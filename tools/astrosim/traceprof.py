#!/usr/bin/env python3
"""traceprof.py -- where an astrosim `cpu` trace spent its instructions:
each line's page (the trace leads with CURPG) and PC to the routine it is in
(the nearest label before it in that page's part of the listing).

    traceprof.py TRACE [LISTING [N]]    (LISTING: build/astrocade/fujiblst.lst)

Counts instructions, not cycles. MARK=ADDR (hex) also counts the passes: the
times that address ran (e.g. the arena tick's entry, page 4's 200DH).
"""
import collections
import os
import re
import sys


def labels(lst):
    page, lab = None, collections.defaultdict(list)
    for l in open(lst, errors="replace"):
        m = re.match(r"\s*\d+:\t([0-9A-F]{4})\s+([0-9A-F]+)?\s*\t([A-Za-z_][A-Za-z0-9_]*):", l)
        if not m:
            continue
        a, name = int(m.group(1), 16), m.group(3).upper()
        mm = re.match(r"MB_P(\d+)$", name)
        if mm:
            page = int(mm.group(1))
        elif name == "MB_R":
            page = "R"
        lab[page].append((a, name))
    for v in lab.values():
        v.sort()
    return lab


def main():
    trace = sys.argv[1]
    lst = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "..", "build", "astrocade", "fujiblst.lst")
    top = int(sys.argv[3]) if len(sys.argv) > 3 else 30
    lab = labels(lst)
    mark = int(os.environ["MARK"], 16) if os.environ.get("MARK") else None
    hist, marks, tot = collections.Counter(), 0, 0
    import bisect
    keys = {pg: [a for a, _ in v] for pg, v in lab.items()}
    for line in open(trace):
        m = re.match(r"([0-9A-F]{2}) ([0-9A-F]{4}):", line)
        if not m:
            continue
        pg, pc = int(m.group(1), 16), int(m.group(2), 16)
        tot += 1
        if pc >= 0x3000:
            pg = "R"
        elif pc < 0x2000:
            hist[("BIOS", "")] += 1
            continue
        if mark is not None and pc == mark:
            marks += 1
        k = keys.get(pg, [])
        i = bisect.bisect_right(k, pc) - 1
        hist[(pg, lab[pg][i][1] if i >= 0 else "?")] += 1
    print("instructions %d%s" % (tot, (", marks %d (%d a mark)" % (marks, tot // max(1, marks))) if mark else ""))
    for (pg, name), n in hist.most_common(top):
        print("%7d %5.1f%%  %s:%s" % (n, 100.0 * n / tot, pg, name))


if __name__ == "__main__":
    main()

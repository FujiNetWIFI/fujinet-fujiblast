#!/usr/bin/env python3
"""traceprof.py -- a MAME trace of the Studio II build (s2sim "trace FILE N",
S2SIM_DEBUG=1: each line "ADDR: MNEMONIC ...") charged to the routines of
the listing: where a pass's instructions go. The 1802's instructions take
2 machine cycles each (the long branches and skips 3, banned here), so a
count is a time; DMA steals the same share of every routine.

    tools/s2sim/traceprof.py TRACE [LISTING] [TOP]

A routine is a label right after an RFIT (or a leaf's), the rest charged to
the routine before; --labels charges every label instead.
"""
import bisect
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
LINE = re.compile(r"^(?:\(\d+\))?\s*\d+/\s*([0-9A-F]+) :(.*)$")
LABEL = re.compile(r"^\s*(?:[0-9A-F]{2} )*\s*([A-Za-z_][A-Za-z0-9_]*):")


def routines(lst, every):
    out, want = {}, False
    for raw in open(lst, errors="replace"):
        m = LINE.match(raw)
        if not m:
            continue
        rest = m.group(2)
        if re.search(r"\(MACRO\)\[\d+\]\s+RFIT\s", rest):
            want = True
            continue
        lm = LABEL.match(rest)
        if lm and (want or every):
            out.setdefault(int(m.group(1), 16), lm.group(1))
            want = False
    return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    every = "--labels" in sys.argv
    trace = args[0]
    lst = args[1] if len(args) > 1 else os.path.join(ROOT, "build", "studio2", "fujiblst.lst")
    top = int(args[2]) if len(args) > 2 else 30
    rs = routines(lst, every)
    addrs = sorted(rs)
    counts, total, entries = {}, 0, {}
    for line in open(trace, errors="replace"):
        m = re.match(r"\s*([0-9A-F]{4}): (\S+)", line)
        if not m:
            continue
        a = int(m.group(1), 16)
        i = bisect.bisect_right(addrs, a) - 1
        name = rs[addrs[i]] if i >= 0 else "?"
        if a < 0x0800:
            name = "runtime:" + name
        counts[name] = counts.get(name, 0) + 1
        if i >= 0 and addrs[i] == a:
            entries[name] = entries.get(name, 0) + 1
        total += 1
    print("%d instructions" % total)
    for name, n in sorted(counts.items(), key=lambda kv: -kv[1])[:top]:
        e = entries.get(name, 0)
        print("%8d %5.1f%% %6d entered  %s" % (n, 100.0 * n / total, e, name))


if __name__ == "__main__":
    main()

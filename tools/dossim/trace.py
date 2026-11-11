#!/usr/bin/env python3
"""trace.py -- instructions per function from a MAME trace (dossim `trace`).

usage: trace.py TRACE MAPFILE SYMS DGROUP

TRACE has linear addresses; DGROUP is the segment dossim printed. The image
starts at DGROUP minus the map's DGROUP; SYMS is tools/dosrig/syms.sh's
table. Counts instructions, and REP string bytes are not expanded (a
`rep movsw` counts once), so they say where the code runs, not the time.
"""
import bisect
import collections
import re
import sys


def main():
    trace, mapfile, symfile, dg = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4], 16)
    m = open(mapfile).read()
    dgrel = int(re.search(r"^DGROUP\s+([0-9a-f]{4}):0000", m, re.M).group(1), 16)
    base = (dg - dgrel) * 16
    syms = sorted((int(o, 16), n) for o, n in re.findall(r"^([0-9a-f]{4}) (\S+)$", open(symfile).read(), re.M))
    offs = [s[0] for s in syms]
    c = collections.Counter()
    n = 0
    for line in open(trace):
        mm = re.match(r"([0-9A-F]+):", line)
        if not mm:
            continue
        a = int(mm.group(1), 16) - base
        n += 1
        if 0 <= a < 0x10000:
            k = bisect.bisect_right(offs, a) - 1
            c[syms[k][1] if k >= 0 else "?"] += 1
        elif a >= 0x10000 and a < 0x20000:
            c["(the image, past 64K: the UART handler's segment?)"] += 1
        else:
            c["(outside: BIOS, DOS, FUJINET.SYS) %05X" % (int(mm.group(1), 16) & 0xF0000)] += 1
    for name, h in c.most_common(45):
        print("%5.1f%%  %8d  %s" % (100.0 * h / n, h, name))
    print(n, "instructions")


if __name__ == "__main__":
    main()

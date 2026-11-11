#!/usr/bin/env python3
"""prof86.py -- the STATS build's profile (A:\\PROF.BIN, tools/dosrig/86box.sh):
where the timer's interrupt found the CPU, by 64 bytes of code, put on the
functions of the wlink map (static functions count with the public one
before them; use -b for the buckets themselves).

usage: prof86.py PROF.BIN MAPFILE [-b] [-s SYMS]

-s SYMS: tools/dosrig/syms.sh's table (static functions too) instead of the map.
"""
import bisect
import collections
import re
import struct
import sys


def main():
    data = open(sys.argv[1], "rb").read()
    hist = struct.unpack("<512H", data[:1024])
    other = struct.unpack("<H", data[1024:1026])[0]
    uart = struct.unpack("<H", data[1026:1028])[0] if len(data) >= 1028 else 0
    if "-s" in sys.argv:
        syms = sorted((int(o, 16), n) for o, n in re.findall(
            r"^([0-9a-f]{4}) (\S+)$", open(sys.argv[sys.argv.index("-s") + 1]).read(), re.M))
    else:
        syms = sorted((int(o, 16), n) for o, n in re.findall(
            r"^0000:([0-9a-f]{4})[+*]?\s+(\S+)\s*$", open(sys.argv[2]).read(), re.M))
    offs = [s[0] for s in syms]
    total = sum(hist) + other + uart
    if "-b" in sys.argv:
        for b, h in sorted(enumerate(hist), key=lambda x: -x[1])[:30]:
            if h:
                k = bisect.bisect_right(offs, b * 64) - 1
                print("%5.1f%%  %04X  %s" % (100.0 * h / total, b * 64, syms[k][1] if k >= 0 else "?"))
        return
    by = collections.Counter()
    for b, h in enumerate(hist):
        if not h:
            continue
        # a bucket's samples shared by the functions in it, by bytes
        lo, hi = b * 64, b * 64 + 64
        k = bisect.bisect_right(offs, lo) - 1
        cuts = [lo] + [o for o in offs if lo < o < hi] + [hi]
        for a, z in zip(cuts, cuts[1:]):
            k = bisect.bisect_right(offs, a) - 1
            by[syms[k][1] if k >= 0 else "?"] += h * (z - a) / 64.0
    by["(outside: BIOS, DOS)"] = other
    by["(the UART's interrupt handler)"] = uart
    for name, h in by.most_common(45):
        print("%5.1f%%  %s" % (100.0 * h / total, name))
    print("%d samples" % total)


if __name__ == "__main__":
    main()

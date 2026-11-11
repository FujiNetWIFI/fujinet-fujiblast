#!/usr/bin/env python3
"""prof.py -- a flat profile from dossim's `pc N` samples.

usage: dossim.py ... | prof.py MAPFILE    (or prof.py MAPFILE < samples)

Each "PC CCCC:IIII DS DDDD" line is one sample. The code segment the game
was loaded at is the CS seen most often; samples elsewhere (the BIOS, DOS)
are counted by segment. Offsets map to the wlink map's code symbols.
"""
import bisect
import collections
import re
import sys


def code_symbols(mapfile):
    syms = []
    for off, name in re.findall(r"^0000:([0-9a-f]{4})[+*]?\s+(\S+)\s*$", open(mapfile).read(), re.M):
        syms.append((int(off, 16), name))
    syms.sort()
    return syms


def main():
    syms = code_symbols(sys.argv[1])
    offs = [s[0] for s in syms]
    samples = [(int(c, 16), int(i, 16)) for c, i in re.findall(r"^PC ([0-9A-F]{4}):([0-9A-F]{4})", sys.stdin.read(), re.M)]
    if not samples:
        sys.exit("no samples")
    cs = collections.Counter(c for c, _ in samples).most_common(1)[0][0]
    hits = collections.Counter()
    for c, i in samples:
        if c != cs:
            hits["(seg %04X)" % c] += 1
            continue
        k = bisect.bisect_right(offs, i) - 1
        hits[syms[k][1] if k >= 0 else "?"] += 1
    n = len(samples)
    for name, h in hits.most_common(30):
        print("%5.1f%%  %5d  %s" % (100.0 * h / n, h, name))
    print("%d samples, game CS %04X" % (n, cs))


if __name__ == "__main__":
    main()

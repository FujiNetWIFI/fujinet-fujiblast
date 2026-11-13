#!/usr/bin/env python3
"""prof.py -- chfsim's PC samples ("PC xxxx DC0 xxxx page n" lines) charged
to the listing's labels: where the time goes.

    chfsim.py SCEN OUT | prof.py LISTING
"""
import re
import sys

lst = open(sys.argv[1], encoding="latin-1").read().splitlines()
# code labels with their address and page: "(1)  12/4A4C : ...   LABEL: op"
labels = {}                     # page (None: fixed) -> [(addr, name)]
page = None
for line in lst:
    m = re.match(r"(?:\(\d+\))?\s+\d+/\s*([0-9A-F]+) :[^;]*?\s([A-Z_][A-Z0-9_]*):", line)
    if not m:
        continue
    addr, name = int(m.group(1), 16), m.group(2)
    pm = re.match(r"MB_P(\d)(END)?$", name)
    if pm:
        page = None if pm.group(2) else int(pm.group(1))
        continue
    labels.setdefault(page if addr >= 0x4800 else None, []).append((addr, name))
import bisect
for k in labels:
    labels[k].sort()
hist = {}
n = 0
for line in sys.stdin:
    m = re.match(r"PC ([0-9A-F]{4}) DC0 [0-9A-F]{4} page (\d+)", line)
    if not m:
        continue
    pc = int(m.group(1), 16)
    pg = int(m.group(2)) if pc >= 0x4800 else None
    tab = labels.get(pg, [])
    i = bisect.bisect_right([a for a, _ in tab], pc) - 1
    name = tab[i][1] if i >= 0 else "?"
    if pg is not None:
        name = "p%d:%s" % (pg, name)
    hist[name] = hist.get(name, 0) + 1
    n += 1
for k, v in sorted(hist.items(), key=lambda x: -x[1])[:30]:
    print("%5.1f%%  %s" % (100.0 * v / n, k))
print(n, "samples")

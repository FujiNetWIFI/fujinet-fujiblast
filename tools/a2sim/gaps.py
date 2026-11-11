#!/usr/bin/env python3
"""gaps.py -- the longest stretches without a frame_tick in an a2sim trace
(the //e's blank is polled: more than ~4 ms, ~1300 instructions, between
looks loses a jiffy). Each gap is shown with the functions it spent its
time in.

usage: gaps.py MAPFILE TRACE [N]
"""
import re
import sys
from bisect import bisect_right

mapfile, trace = sys.argv[1:3]
top = int(sys.argv[3]) if len(sys.argv) > 3 else 10
text = open(mapfile).read()
part = text.split("Exports list by value:")[1].split("Imports list:")[0]
syms = sorted((int(a, 16), n) for n, a in re.findall(r"(\S+)\s+([0-9A-F]{6})\s+R?LA", part)
              if not n.startswith("_ax_"))
addrs = [a for a, _ in syms]
tick = dict((n, a) for a, n in syms)["_frame_tick"]


def name(pc):
    i = bisect_right(addrs, pc) - 1
    return syms[i][1] if i >= 0 else "?"


gaps = []
n = 0
where = {}
last = None
for line in open(trace):
    m = re.match(r"([0-9A-F]{4}): ", line)
    if m:
        pc = int(m.group(1), 16)
        k = 1
        last = pc
    else:
        m = re.search(r"loops for (\d+) instructions", line)
        if not m or last is None:
            continue
        pc, k = last, int(m.group(1))
    if pc == tick:
        gaps.append((n, where))
        n = 0
        where = {}
        continue
    n += k
    f = name(pc)
    where[f] = where.get(f, 0) + k
for n, w in sorted(gaps, key=lambda g: -g[0])[:top]:
    parts = ", ".join(f"{f} {c}" for f, c in sorted(w.items(), key=lambda x: -x[1])[:5])
    print(f"{n:6d}  {parts}")

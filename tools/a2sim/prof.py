#!/usr/bin/env python3
"""prof.py -- where the time goes in an a2sim trace: instructions by
function (the map's nearest exported symbol below each PC).

usage: prof.py MAPFILE TRACE [N]
"""
import re
import sys
from bisect import bisect_right

mapfile, trace = sys.argv[1:3]
top = int(sys.argv[3]) if len(sys.argv) > 3 else 25
text = open(mapfile).read()
part = text.split("Exports list by value:")[1].split("Imports list:")[0]
syms = sorted((int(a, 16), n) for n, a in re.findall(r"(\S+)\s+([0-9A-F]{6})\s+R?LA", part)
              if not n.startswith("_ax_"))      # aux art: same addresses as main code
addrs = [a for a, _ in syms]
count = {}
total = 0
last = None
for line in open(trace):
    m = re.match(r"([0-9A-F]{4}): ", line)
    if m:
        pc = int(m.group(1), 16)
        n = 1
        last = pc
    else:
        m = re.search(r"loops for (\d+) instructions", line)
        if not m or last is None:
            continue
        pc, n = last, int(m.group(1))
    i = bisect_right(addrs, pc) - 1
    name = syms[i][1] if i >= 0 else "?"
    if pc >= 0xC000 and pc < 0xD000:
        name = "(I/O)"
    elif pc >= 0xE000 or (pc >= 0xC100 and pc < 0xD000):
        name = "(ROM/ProDOS)"
    count[name] = count.get(name, 0) + n
    total += n
for name, n in sorted(count.items(), key=lambda x: -x[1])[:top]:
    print(f"{n:8d} {100 * n / total:5.1f}%  {name}")
print(f"{total:8d} instructions")

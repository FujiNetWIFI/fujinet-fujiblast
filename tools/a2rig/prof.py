#!/usr/bin/env python3
"""prof.py -- an a2rig `prof` file (a PC a frame) or `xprof` file (PC and
count, every instruction) by function.

usage: prof.py MAPFILE PCFILE [N]
"""
import re
import sys
from bisect import bisect_right

mapfile, pcs = sys.argv[1:3]
top = int(sys.argv[3]) if len(sys.argv) > 3 else 25
text = open(mapfile).read()
part = text.split("Exports list by value:")[1].split("Imports list:")[0]
syms = sorted((int(a, 16), n) for n, a in re.findall(r"(\S+)\s+([0-9A-F]{6})\s+R?LA", part)
              if not n.startswith("_ax_"))
addrs = [a for a, _ in syms]
count, total = {}, 0
for line in open(pcs):
    f = line.split()
    pc = int(f[0], 16)
    w = int(f[1]) if len(f) > 1 else 1
    if 0xC000 <= pc < 0xD000:
        name = "(I/O, slot firmware: SmartPort)"
    elif pc >= 0xE000:
        name = "(ROM/ProDOS)"
    else:
        i = bisect_right(addrs, pc) - 1
        name = syms[i][1] if i >= 0 else "?"
    count[name] = count.get(name, 0) + w
    total += w
for name, n in sorted(count.items(), key=lambda x: -x[1])[:top]:
    print(f"{n:6d} {100 * n / total:5.1f}%  {name}")
print(f"{total:6d} samples")

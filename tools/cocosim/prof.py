#!/usr/bin/env python3
"""prof.py MAPFILE TRACE -- count a MAME trace's instructions (cocosim's
trace step) by function: the nearest symbol at or below each address."""
import bisect
import collections
import re
import sys

syms = []
for name, addr in re.findall(r"Symbol: (\S+) \(.*\) = ([0-9A-F]{4})", open(sys.argv[1]).read()):
    if name.startswith(("L0", "\\", ".", "l_", "s_")) or "funcend" in name or "funcsize" in name:
        continue
    syms.append((int(addr, 16), name))
syms.sort()
keys = [a for a, _ in syms]
count = collections.Counter()
total = 0
for line in open(sys.argv[2], errors="replace"):
    m = re.match(r"([0-9A-F]{4}):", line)
    if not m:
        continue
    a = int(m.group(1), 16)
    i = bisect.bisect_right(keys, a) - 1
    count[syms[i][1] if i >= 0 else "?"] += 1
    total += 1
print("%d instructions" % total)
for name, n in count.most_common(30):
    print("%5.1f%%  %s" % (100.0 * n / total, name))

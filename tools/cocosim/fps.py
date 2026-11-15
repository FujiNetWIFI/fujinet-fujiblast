#!/usr/bin/env python3
"""fps.py COCOSIM_LOG -- the live run's renders/passes peeks (every 300
frames) as draws and passes a second, a line a window."""
import re, sys
r, p, ph = [], [], []
for l in open(sys.argv[1]):
    m = re.match(r"PEEK st ([0-9A-F]{2})", l)
    if m:
        ph.append(int(m.group(1), 16))
    m = re.match(r"PEEK (renders|st_passes) ([0-9A-F]{2}) ([0-9A-F]{2})", l)
    if m:
        v = int(m.group(2) + m.group(3), 16)
        (r if m.group(1) == "renders" else p).append(v)
for i in range(1, min(len(r), len(p))):
    print("window %d: %4.1f draws/s  %4.1f passes/s  phase %s" % (i, (r[i] - r[i-1]) / 5.0, (p[i] - p[i-1]) / 5.0, ph[i] if i < len(ph) else "?"))

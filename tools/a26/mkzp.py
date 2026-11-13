#!/usr/bin/env python3
"""mkzp.py LISTING OUT.lua -- the zero-page symbols of a bank listing as a Lua
table, for the MAME harness (tools/a26sim/live.lua)."""
import re
import sys

syms = {}
for line in open(sys.argv[1]):
    for m in re.finditer(r"\b([A-Z][A-Z0-9_]*)\s*:\s*([0-9A-F]{2,4})\s+[-CS]", line):
        syms.setdefault(m.group(1), int(m.group(2), 16))
out = ["return {"]
for k in sorted(syms):
    if syms[k] < 0x100 or k in ("PFT0", "FNTEXT"):
        out.append("  %s = 0x%X," % (k, syms[k]))
out.append("}")
open(sys.argv[2], "w").write("\n".join(out) + "\n")

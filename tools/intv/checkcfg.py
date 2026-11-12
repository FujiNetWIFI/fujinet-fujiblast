#!/usr/bin/env python3
"""Fail if the Intellivision ROM would land where it mustn't, from as1600's .cfg memory map:
  $2000-$2FFF, $7000-$7FFF, $E000-$EFFF  the ECS's ROM (the ROM uses an ECS when one is attached);
                                           the EXEC also runs whatever is at $7000 at power-up
  $8000-$9FFF                             the FujiNet cartridge's RAM and its mailbox
  $B800-$BFFF, $F800-$FFFF                GRAM shows through there on some consoles
After M.U.L.E.'s (fujinet-multiplayer-mule, clients/intv/tools/checkcfg.py), with the ECS's ROM added.
Prints each segment's use.
usage: checkcfg.py fujiblst.cfg"""
import re
import sys

BAD = [(0x2000, 0x2FFF, "the ECS's ROM"), (0x7000, 0x7FFF, "the ECS's ROM"), (0xE000, 0xEFFF, "the ECS's ROM"),
       (0x8000, 0x9FFF, "the cartridge's RAM and mailbox"), (0xB800, 0xBFFF, "a GRAM mirror"),
       (0xF800, 0xFFFF, "a GRAM mirror")]
bad = 0
words = 0
for ln in open(sys.argv[1]):
    m = re.match(r"\s*\$([0-9A-Fa-f]+)\s*-\s*\$([0-9A-Fa-f]+)\s*=\s*\$([0-9A-Fa-f]+)", ln)
    if not m:
        continue
    lo = int(m.group(3), 16)
    hi = lo + int(m.group(2), 16) - int(m.group(1), 16)
    words += hi - lo + 1
    print("  ROM $%04X-$%04X  %5d words" % (lo, hi, hi - lo + 1))
    for a, b, what in BAD:
        if lo <= b and hi >= a:
            print("checkcfg.py: ROM at $%04X-$%04X overlaps $%04X-$%04X, %s" % (lo, hi, a, b, what))
            bad += 1
print("  %d words of ROM" % words)
sys.exit(1 if bad else 0)

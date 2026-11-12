#!/usr/bin/env python3
"""Where the Adam program ends, from z88dk's map: it must end below C800h, where the image's
bootstrap loads it from (z88dk's adam_bootstrap.asm reads the program's blocks to 0000h from there).

Usage: adam_end.py syn-ack-wire-adam.map"""
import re
import sys

BOOTSTRAP = 0xC800


def symbol(text, name):
    m = re.search(r"^%s\s*=\s*\$([0-9A-Fa-f]+)" % re.escape(name), text, re.M)
    if not m:
        sys.exit("adam_end.py: no %s in the map" % name)
    return int(m.group(1), 16)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    text = open(sys.argv[1]).read()
    end = symbol(text, "__DATA_END_tail")
    bss = symbol(text, "__BSS_END_tail")
    print("%s: the program ends at %04Xh, its variables at %04Xh (%d bytes under the bootstrap)"
          % (sys.argv[1], end, bss, BOOTSTRAP - end))
    if end > BOOTSTRAP:
        sys.exit("adam_end.py: the program runs over the bootstrap at C800h")


if __name__ == "__main__":
    main()

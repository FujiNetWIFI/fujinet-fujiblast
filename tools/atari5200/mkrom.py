#!/usr/bin/env python3
"""mkrom.py -- the 5200 FujiBlast image from ld65's, checked.

    tools/atari5200/mkrom.py MAP IMAGE.bin OUT.a52

The image is a flat 32K cart (src/atari5200/fujiblst-a52.cfg), $4000-$BFFF.
Over every code segment, as fujinet-lib's atari5200-romstamp.py does:
  - no store and no read-modify-write into the cart window ($4000-$BFFF):
    the edge has no R/W;
  - no absolute read of the mailbox's hotspot pages ($B500-$B7FF), nor an
    indexed one from a base that is not page-aligned (a page crossing reads
    the wrong page first), but the lib's fn_tx / fn_regwr (fuji_atari5200_go.o);
  - no JMP ($xxFF);
and the mailbox $FF, "FUJI" version 1 kind 0 (the size) at $BFE0, $02 at
$BFE7, $FF at $BFFD, the entry in the program. A bad image is not written.
"""
import re
import sys

WIN = 0x4000
SIZE = 0x8000
HOT_LO, HOT_HI = 0xB500, 0xB7FF

LEN = [1] * 256
for op in (0x69, 0x29, 0xC9, 0xE0, 0xC0, 0x49, 0xA9, 0xA2, 0xA0, 0x09, 0xE9,
           0xA5, 0xA6, 0xA4, 0x85, 0x86, 0x84, 0x65, 0x25, 0x06, 0x24, 0xC5,
           0xC6, 0x45, 0xE6, 0x46, 0x26, 0x66, 0xE5, 0x05, 0x75, 0x35, 0x16,
           0xD5, 0xD6, 0x55, 0xF6, 0x56, 0x36, 0x76, 0xF5, 0x15, 0xB5, 0xB4,
           0x95, 0x94, 0xB6, 0x96, 0x61, 0x21, 0xC1, 0x41, 0xA1, 0x01, 0xE1,
           0x81, 0x71, 0x31, 0xD1, 0x51, 0xB1, 0x11, 0xF1, 0x91, 0xC4, 0xE4,
           0x10, 0x30, 0x50, 0x70, 0x90, 0xB0, 0xD0, 0xF0):
    LEN[op] = 2
for op in (0x6D, 0x2D, 0x0E, 0x2C, 0xCD, 0xEC, 0xCC, 0xCE, 0x4D, 0xEE, 0x4C,
           0x20, 0xAD, 0xAE, 0xAC, 0x4E, 0x0D, 0x2E, 0x6E, 0xED, 0x8D, 0x8E,
           0x8C, 0x7D, 0x3D, 0x1E, 0xDD, 0xDE, 0x5D, 0xFD, 0xFE, 0x5E, 0xBD,
           0xBC, 0x3E, 0x7E, 0x1D, 0x9D, 0x79, 0x39, 0xD9, 0x59, 0xB9, 0xBE,
           0x19, 0xF9, 0x99, 0x6C):
    LEN[op] = 3

STORES = {0x8D, 0x8E, 0x8C, 0x9D, 0x99}
RMW = {0x0E, 0x1E, 0x2E, 0x3E, 0x4E, 0x5E, 0x6E, 0x7E, 0xCE, 0xDE, 0xEE, 0xFE}
IDX_READS = {0x7D, 0x3D, 0xDD, 0x5D, 0xFD, 0xBD, 0xBC, 0x1D,
             0x79, 0x39, 0xD9, 0x59, 0xB9, 0xBE, 0x19, 0xF9}
JMPS = {0x4C, 0x20, 0x6C}

# code segments
CODE_SEGS = ("STARTUP", "LOWCODE", "ONCE", "CODE", "ROM2")
# who may read the hotspots
HOT_MODS = ("fuji_atari5200_go.o",)


def section(text, title):
    return re.search(title + r":\s*\n-+\s*\n(.*?)\n\n", text, re.S).group(1)


def main():
    mapfile, path, out = sys.argv[1:4]
    img = bytearray(open(path, "rb").read())
    if len(img) != SIZE:
        sys.exit("mkrom: %s is %d bytes, not %d" % (path, len(img), SIZE))
    text = open(mapfile).read()
    segs = {}
    for line in section(text, "Segment list").splitlines():
        p = line.split()
        if len(p) >= 4 and re.fullmatch(r"[0-9A-Fa-f]{6}", p[1]):
            segs[p[0]] = (int(p[1], 16), int(p[2], 16))
    # each module's piece of each code segment: (start, end, module)
    pieces = []
    cur = None
    for line in section(text, "Modules list").splitlines():
        if not line.startswith(" "):
            cur = line.strip().rstrip(":")
            continue
        m = re.match(r"\s+(\w+)\s+Offs=([0-9A-Fa-f]+)\s+Size=([0-9A-Fa-f]+)", line)
        if m and m.group(1) in CODE_SEGS and m.group(1) in segs:
            s = segs[m.group(1)][0] + int(m.group(2), 16)
            pieces.append((s, s + int(m.group(3), 16), cur))

    def module_at(a):
        for s, e, mod in pieces:
            if s <= a < e:
                return mod
        return "?"

    def modname(mod):       # "dir/x.o" or "dir/lib.lib(x.o)" -> "x.o"
        return re.sub(r".*[/(]", "", mod).rstrip(")")

    bad = []
    for name, (s, e) in segs.items():
        if name not in CODE_SEGS or e < s:
            continue
        code = img[s - WIN:e + 1 - WIN]
        pc = 0
        while pc < len(code):
            op = code[pc]
            n = LEN[op]
            if pc + n > len(code):
                break
            if n == 3 and op not in JMPS:
                at = s + pc
                tgt = code[pc + 1] | (code[pc + 2] << 8)
                here = "%s $%04X (%s)" % (name, at, module_at(at))
                if (op in STORES or op in RMW) and WIN <= tgt <= 0xBFFF:
                    bad.append("%s: a store to $%04X: the cart window" % (here, tgt))
                elif HOT_LO <= tgt <= HOT_HI or (op in IDX_READS and HOT_LO <= tgt + 255 and tgt <= HOT_HI):
                    if modname(module_at(at)) not in HOT_MODS:
                        bad.append("%s: a read of the mailbox's hotspots ($%04X)" % (here, tgt))
                    elif op in IDX_READS and tgt & 0xFF:
                        bad.append("%s: an indexed read of $%04X crosses a hotspot page" % (here, tgt))
            if op == 0x6C and (code[pc + 1] == 0xFF):
                bad.append("%s $%04X: JMP ($%02X%02X) hits the page-wrap bug" % (name, s + pc, code[pc + 2], code[pc + 1]))
            pc += n

    mem = lambda a: img[a - WIN]
    if any(mem(a) != 0xFF for a in range(0xB000, 0xB800)):
        bad.append("the mailbox's $B000-$B7FF is not $FF")
    if bytes(mem(0xBFE0 + i) for i in range(4)) != b"FUJI":
        bad.append("no \"FUJI\" claim at $BFE0")
    if (mem(0xBFE4), mem(0xBFE5)) != (1, 0):
        bad.append("the claim is not version 1, kind 0 (from the size)")
    if mem(0xBFE7) != 0x02:
        bad.append("$BFE7 is not $02 (the 2-port BIOS's PAL check)")
    if mem(0xBFFD) != 0xFF:
        bad.append("$BFFD is not $FF (the BIOS's title)")
    lo = segs["STARTUP"][0]
    vec = mem(0xBFFE) | (mem(0xBFFF) << 8)
    if not lo <= vec < 0xB000:
        bad.append("entry $%04X is not in the program" % vec)
    for x in bad:
        print("mkrom: %s: %s" % (path, x), file=sys.stderr)
    if bad:
        sys.exit(1)
    # how full: the ROM ($4000-$AFFF) and ROM2 ($B800-$BFDF), up to the last byte that isn't fill
    used = len(bytes(img[0:0xB000 - WIN]).rstrip(b"\xff"))
    used2 = len(bytes(img[0xB800 - WIN:0xBFE0 - WIN]).rstrip(b"\xff"))
    print("mkrom: ROM  $4000-$%04X %5d of %5d" % (WIN + used - 1, used, 0xB000 - WIN))
    print("mkrom: ROM2 $B800-$%04X %5d of %5d" % (0xB800 + used2 - 1, used2, 0xBFE0 - 0xB800))
    open(out, "wb").write(bytes(img))


if __name__ == "__main__":
    main()

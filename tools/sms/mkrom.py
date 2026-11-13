#!/usr/bin/env python3
"""Make the SMS's image, r2r/sms/fujiblst.sms: a Sega-mapper ROM for the
FujiNet SMS cartridge, from what z88dk linked (src/sms/blast.asm has the map).
From M.U.L.E.'s tools/sms/mkrom.py.

usage: mkrom.py DIR PRODUCT   (DIR/PRODUCT.sms as z88dk made it, or as made
                              before; PRODUCT.map, PRODUCT_*.bin)

  banks 0-1   0000h-7FFFh, fixed: the program; the "FUJI" claim at 7FDCh
              (the cartridge keeps its mailbox on for a claimed image) and
              the SEGA header at 7FF0h, its checksum made again over it
  bank 2      the arena's art (ART1), bank 3 the menus' (ART2), at 8000h

The last 4K of every window bank (B000h-BFFFh) is the cartridge's mailbox
while the image runs: FFh here, as the cartridge wants it. The cartridge
picks the mapper from the image's first 32K (its heuristic, ported from
fujinet-firmware pico/sms/firmware/src/smsmap.c): the image is refused
unless that says Sega. A failing image is deleted.
"""
import os
import re
import sys

BANK = 0x4000
WIN = 0x3000
CLAIM = 0x7FDC
FENCE = (0x7FD8, 0x7FF0)


def symbols(path):
    out = {}
    for line in open(path):
        m = re.match(r"(\S+)\s+=\s+\$([0-9A-Fa-f]+)", line)
        if m:
            out[m.group(1)] = int(m.group(2), 16)
    return out


def mapper(img):
    """smsmap_heuristic: the mapper an unknown image gets."""
    n = dict.fromkeys(["0002", "8000", "a000", "ffff", "3ffe", "4000", "6000"], 0)
    keys = {0xFFFF: "ffff", 0x0002: "0002", 0x0003: "0002", 0x0004: "0002", 0x8000: "8000",
            0xA000: "a000", 0x3FFE: "3ffe", 0x4000: "4000", 0x6000: "6000"}
    i = 0
    while i < 0x8000:
        if img[i] == 0x32:
            a = img[i + 1] | img[i + 2] << 8
            if a in keys:
                n[keys[a]] += 1
                i += 3
                continue
        i += 1
    if len(img) > 0x10000 and (n["0002"] > n["ffff"] + 2 or (n["0002"] and not n["ffff"])):
        return "Zemina", n
    if n["8000"] > n["ffff"] + 2 or (n["8000"] and not n["ffff"]):
        return "Codemasters", n
    if n["a000"] > n["ffff"] + 2 or (n["a000"] and not n["ffff"]):
        return "Korean", n
    if n["3ffe"] > n["ffff"] + 2 or n["3ffe"]:
        return "4 PAK", n
    if n["4000"] and n["6000"] and n["8000"] and n["a000"]:
        return "Janggun", n
    return "Sega", n


def main():
    d, product = sys.argv[1:3]
    path = os.path.join(d, product + ".sms")
    img = open(path, "rb").read()
    sym = symbols(os.path.join(d, product + ".map"))
    end = sym["__ROMABLE_END_tail"] + (sym["__DATA_END_tail"] - sym["__DATA_head"])
    bss = sym["__BSS_END_tail"]
    errors = []
    if end > FENCE[0]:
        errors.append("the program ends at %04Xh, past the claim's fence (%04Xh)" % (end, FENCE[0]))
    if bss > 0xDA00:
        errors.append("the variables end at %04Xh: little room for the stack" % bss)
    if len(img) < 2 * BANK or img[0x7FF0:0x7FF8] != b"TMR SEGA":
        errors.append("z88dk's image has no SEGA header at 7FF0h")
    out = bytearray(img[:2 * BANK])
    out[CLAIM:CLAIM + 4] = b"\xff" * 4    # (made again: this may be a made one)
    if any(b != 0xFF for b in out[FENCE[0]:FENCE[1]]):
        errors.append("7FD8h-7FEFh isn't free for the claim")
    sizes = []
    for name in ["ART1", "ART2"]:
        data = open(os.path.join(d, "%s_%s.bin" % (product, name)), "rb").read()
        if len(data) > WIN:
            errors.append("%s is %d bytes: over the window's %d" % (name, len(data), WIN))
        out += data[:WIN].ljust(BANK, b"\xff")
        sizes.append("%s %d" % (name, len(data)))
    out[CLAIM:CLAIM + 4] = b"FUJI"
    s = sum(out[:0x7FF0]) & 0xFFFF
    out[0x7FFA] = s & 0xFF
    out[0x7FFB] = s >> 8
    kind, n = mapper(out)
    if kind != "Sega":
        errors.append("the cartridge would take it for a %s mapper (stores: %s)" %
                      (kind, ", ".join("%s %d" % kv for kv in n.items() if kv[1])))
    if errors:
        for e in errors:
            print("mkrom.py: " + e, file=sys.stderr)
        os.remove(path)
        sys.exit(1)
    open(path, "wb").write(out)
    print("sms: program 0000-%04X, variables C000-%04X; %s; %dK, Sega mapper (%d stores to FFFFh)" %
          (end - 1, bss - 1, ", ".join(sizes), len(out) // 1024, n["ffff"]))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""lwlink-fb.py -- lwlink for CMOC (--lwlink=), with FujiBlast's memory map.

CMOC links with its own script; this swaps in src/coco/fujiblst.link (the
CoCo 1/2) or fujiblst3.link (FB_LINK=coco3), runs the real lwlink, checks
every region's limit, and splits the DECB output into what the CoCo loads:

  <out>.bin     low core (< $8000): LOADM'd by BASIC, has the exec address
  <out>hi.bin   everything from $8000 up (the high core and the art):
                loaded by the game itself (src/coco/bank.c)

Both are DECB .bin files (segments of 00 len addr data, then FF 0000 exec).
"""
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
COCO3 = os.environ.get("FB_LINK") == "coco3"
SCRIPT = os.path.join(HERE, "..", "..", "src", "coco", "fujiblst3.link" if COCO3 else "fujiblst.link")

LOW_START = 0x1F00 if COCO3 else 0x2600
LOW_END = 0x7C00        # the stack is the 1K below $8000 (--initial-s)
HIGH_END = 0xBFF0       # the CPU vectors are at $BFF0 (SAM); the CoCo 3 has no such limit
ART_END = 0xFF00        # (the I/O page above)
if COCO3:
    HIGH_END = 0xC000
    ART_END = 0xFE00
REGIONS = [
    ("low", LOW_START, LOW_END),
    ("high", 0x8000, HIGH_END),
    ("art", 0xE000, ART_END),
]
SECT_ART = ("art", "art2")


def region_of(addr):
    for name, a, b in REGIONS:
        if a <= addr < b:
            return name, a, b
    return None, 0, 0


def read_decb(path):
    data = open(path, "rb").read()
    blocks, i, exe = [], 0, None
    while i < len(data):
        kind = data[i]
        n = (data[i + 1] << 8) | data[i + 2]
        a = (data[i + 3] << 8) | data[i + 4]
        if kind == 0xFF:
            exe = a
            break
        blocks.append((a, data[i + 5:i + 5 + n]))
        i += 5 + n
    return blocks, exe


def write_decb(path, blocks, exe):
    with open(path, "wb") as f:
        for a, b in blocks:
            for k in range(0, len(b), 0xFF00):     # length is 16 bits
                part = b[k:k + 0xFF00]
                f.write(bytes([0, len(part) >> 8, len(part) & 255,
                               (a + k) >> 8, (a + k) & 255]))
                f.write(part)
        f.write(bytes([0xFF, 0, 0, exe >> 8, exe & 255]))


def main():
    args = sys.argv[1:]
    out = mapf = None
    new = []
    for a in args:
        if a.startswith("--script="):
            a = "--script=" + SCRIPT
        elif a.startswith("--output="):
            out = a[len("--output="):]
        elif a.startswith("--map="):
            mapf = a[len("--map="):]
        new.append(a)
    if mapf is None:
        mapf = (out or "a.out") + ".map"
        new.append("--map=" + mapf)
    r = subprocess.call(["lwlink"] + new)
    if r != 0 or out is None:
        return r
    # CMOC deletes its map: keep a copy (tools/cocosim reads it)
    if os.path.isdir("build"):
        shutil.copy(mapf, os.path.join("build", "fujiblst-coco3.map" if COCO3 else "fujiblst-coco.map"))

    secs = {}
    for line in open(mapf):
        m = re.match(r"Section: (\S+) \((.*)\) load at ([0-9A-F]+), length ([0-9A-F]+)", line)
        if not m:
            continue
        name, a, n = m.group(1), int(m.group(3), 16), int(m.group(4), 16)
        s = secs.setdefault(name, [a, a])
        s[0] = min(s[0], a)
        s[1] = max(s[1], a + n)
    bad = 0
    for name, (a, e) in sorted(secs.items(), key=lambda kv: kv[1][0]):
        if e == a:
            continue
        reg, lo, hi = region_of(a)
        if reg is None or e > hi:
            print("lwlink-fb: section %s $%04X-$%04X is outside its region (%s, ends $%04X)"
                  % (name, a, e, reg, hi), file=sys.stderr)
            bad = 1
    if bad:
        return 1

    blocks, exe = read_decb(out)
    low = [(a, b) for a, b in blocks if a < 0x8000]
    base = out[:-4] if out.endswith(".bin") else out
    if COCO3:
        # the CoCo 3's art is in MMU blocks (bank.c): each block that starts at
        # $E000 begins a file: <base>a.bin (block 6), <base>b.bin (block 7)
        high, arts = [], [[], []]
        k = -1
        for a, b in blocks:
            if a < 0x8000:
                continue
            if a == 0xE000:
                k += 1
            if a >= 0xE000:
                if k > 1:
                    print("lwlink-fb: more than 2 art blocks", file=sys.stderr)
                    return 1
                arts[max(k, 0)].append((a, b))
            else:
                high.append((a, b))
        write_decb(base + "a.bin", arts[0], exe)
        write_decb(base + "b.bin", arts[1], exe)
    else:
        high = [(a, b) for a, b in blocks if a >= 0x8000]
    write_decb(out, low, exe)
    write_decb(base + "hi.bin", high, exe)

    def total(bl):
        return sum(len(b) for _, b in bl)
    print("%s memory: low $%04X-$%04X (bss to $%04X, limit $%04X)"
          % ("coco3" if COCO3 else "coco", LOW_START, secs.get("rwdata", [0, 0])[1],
             secs.get("bss", [0, 0])[1], LOW_END))
    print("  low %5d  high %5d (code to $%04X, limit $%04X)  art %5d (to $%04X, limit $%04X)"
          % (total(low), total(high), secs.get("hirodata", secs.get("hicode", [0, 0x8000]))[1], HIGH_END,
             secs.get("art", [0, 0])[1] - secs.get("art", [0, 0])[0], secs.get("art", [0, 0xE000])[1], ART_END))
    return 0


if __name__ == "__main__":
    sys.exit(main())

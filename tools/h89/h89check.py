#!/usr/bin/env python3
"""h89check.py -- the H89 build's image and memory budget (no overlays).

  h89check.py BASE --os cpm|hdos --top ADDR

BASE is the link's output without its suffix (r2r/h89-cpm/fujiblst):
BASE.map and BASE.com (CP/M) or BASE.abs (HDOS).

CP/M  The image is loaded at 0100H. z88dk writes the variables (BSS) into
      the .COM as zeros; the startup clears them anyway, so the file is cut
      where they begin (the H17 loads 20 s of 46K: every K counts). The
      variables and a stack must end under --top: the BDOS of a 62K CP/M,
      the one FNDISK needs.
HDOS  The .ABS (header FF 00, load, length, entry) must end where the
      variables begin (CRT_TRIM_BSS), and they must end under --top: what
      HDOS leaves with FN: loaded (src/h89/tpa.asm checks it on the
      machine, with whatever drivers are loaded).

On a failure BASE.com/.abs is removed, so the next make links again.
"""
import argparse
import os
import re
import sys

STACK = 384             # CP/M: the stack is under the BDOS (measure it: h89sim)


def fail(base, msg):
    for ext in (".com", ".abs"):
        try:
            os.remove(base + ext)
        except OSError:
            pass
    sys.exit("h89check: " + msg)


def read_map(path):
    glob = {}
    for line in open(path):
        m = re.match(r"^(\w+)\s*=\s*\$([0-9A-Fa-f]+)\s*;", line)
        if m:
            glob[m.group(1)] = int(m.group(2), 16)
    return glob


def main():
    ap = argparse.ArgumentParser(usage=__doc__)
    ap.add_argument("base")
    ap.add_argument("--os", choices=["cpm", "hdos"], required=True)
    ap.add_argument("--top", type=lambda v: int(v, 0), required=True)
    a = ap.parse_args()
    base = a.base
    glob = read_map(base + ".map")
    bss, end = glob["__BSS_head"], glob["__BSS_END_tail"]
    if a.os == "cpm":
        img = open(base + ".com", "rb").read()
        if len(img) == end - 0x100:
            img = img[:bss - 0x100]
            open(base + ".com", "wb").write(img)
        if len(img) != bss - 0x100:
            fail(base, "%s.com is %d bytes for an image of 0100H-%04XH" % (base, len(img), bss - 1))
        need = end + STACK
        print("h89check: image 0100H-%04XH (%d bytes), variables %04XH-%04XH, "
              "a stack to %04XH: %d bytes under %04XH" % (bss - 1, len(img), bss, end - 1, need,
                                                         a.top - need, a.top))
    else:
        img = open(base + ".abs", "rb").read()
        load, length = img[2] | img[3] << 8, img[4] | img[5] << 8
        if img[:2] != b"\xff\x00" or len(img) < 8 + length or load + length != bss:
            fail(base, "%s.abs is not a header and the image up to BSS (%04XH, %d bytes, BSS at %04XH)"
                 % (base, load, len(img), bss))
        need = end
        print("h89check: image %04XH-%04XH (%d bytes), variables %04XH-%04XH: %d bytes under %04XH"
              % (load, bss - 1, length, bss, end - 1, a.top - need, a.top))
    if need > a.top:
        fail(base, "the program ends at %04XH, past %04XH" % (need, a.top))


if __name__ == "__main__":
    main()

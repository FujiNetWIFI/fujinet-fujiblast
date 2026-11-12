#!/usr/bin/env python3
"""traceprof.py -- a MAME debugger trace (o2sim `trace`) of the 8048 summed
into cycles per routine.

    traceprof.py TRACE BUILDDIR [TOP] [FRAMES]

The trace names no 2K page, so an instruction at $800 and up is placed in the
page whose listing has the same mnemonic there, staying in the page it was
in while that fits. $600-$7FF is the arena overlay (arena.lst) when its
listing fits, else the text overlay's. Cycles: 2 for jumps, calls, returns,
MOVX/MOVP, immediates, port I/O; 1 for the rest (the 8048's).
"""
import collections
import glob
import os
import re
import sys

LINE = re.compile(r"^(?:\(\d+\))?\s*\d+/\s*([0-9A-F]{3,4}) : ((?:[0-9A-F]{2} )*)\s*(.*)$")
LAB = re.compile(r"^([a-z_][a-z_0-9]*):")
TWO = {"jmp", "call", "ret", "retr", "movx", "movp", "movp3", "djnz", "in", "outl", "ins",
       "jmpp", "movd", "anld", "orld", "strt", "stop"}


def load(path):
    code, labs = {}, []
    for line in open(path, errors="ignore"):
        m = LINE.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        src = m.group(3).split(";")[0].strip()
        lm = LAB.match(src)
        if lm:
            labs.append((addr, lm.group(1)))
            src = src[len(lm.group(0)):].strip()
        if src and m.group(2).strip():
            code[addr] = src.split()[0].lower()
    return code, sorted(labs)


def cycles(mn, ops):
    if mn in TWO or mn.startswith("j") or "#" in ops:
        return 2
    return 1


def main(trace, bdir, top=40, frames=None):
    pages = {}
    for f in glob.glob(os.path.join(bdir, "fb_p*.lst")):
        n = int(re.search(r"fb_p(\d+)\.lst", f).group(1))
        pages[n] = load(f)
    arena = load(os.path.join(bdir, "arena.lst"))
    fixed = pages[0]
    cur = 0
    by = collections.Counter()
    total = 0
    for line in open(trace, errors="ignore"):
        m = re.match(r"\s*(\d+):([0-9A-F]+): (\S+)\s*(.*)", line)
        if not m:
            continue
        addr = int(m.group(2), 16) | int(m.group(1)) << 11  # (bank 1: $800 up)
        mn, ops = m.group(3).lower(), m.group(4)
        c = cycles(mn, ops)
        total += c
        if addr < 0x400:
            by["BIOS $%03X" % (addr & 0xFF0)] += c
            continue
        if addr < 0x600:
            src = fixed
            tag = "fixed"
        elif addr < 0x800:
            src = arena if arena[0].get(addr) == mn else fixed
            tag = "arena" if src is arena else "text"
        else:
            if pages.get(cur, ({}, []))[0].get(addr) != mn:
                for n, (code, _) in pages.items():
                    if code.get(addr) == mn:
                        cur = n
                        break
            src = pages.get(cur, ({}, []))
            tag = "p%d" % cur
        name = "?"
        for a, n in src[1]:
            if a <= addr:
                name = n
            else:
                break
        by["%s %s" % (tag, name)] += c
    per = (" a frame: %d" % (total // int(frames))) if frames else ""
    for k, n in by.most_common(int(top)):
        print("%5.1f%%  %7d  %s" % (100.0 * n / total, n, k))
    print("%d cycles%s" % (total, per))


if __name__ == "__main__":
    main(*sys.argv[1:5])

#!/usr/bin/env python3
"""traceprof.py -- a MAME debugger trace (arcsim `cpu`) of the 2650 summed
into cycles per routine (the Odyssey 2's tools/o2sim/traceprof.py, for the
Arcadia build).

    traceprof.py TRACE LSTDIR [TOP] [FRAMES]

The trace names no 2K page, so an instruction in the window ($0800-$0FFF)
is placed in the page whose listing has the same mnemonic there, staying in
the page it was in while that fits. A loop MAME folds ("loops for N
instructions") is charged N times the last instruction's cycles. Cycles (a cycle is 3 clocks): 2 for an
instruction of one or two bytes (a register or an immediate), 3 for a
relative one or a branch, 4 for an absolute one, 2 more when indirect.
"""
import collections
import glob
import os
import re
import sys

LINE = re.compile(r"^\(\d+\)\s+\d+/\s*([0-9A-F]+) : ((?:[0-9A-F]{2} )+)\s*(.*)$")
LAB = re.compile(r"^([A-Z_][A-Z0-9_]*):")
BR = {"BCTR", "BCFR", "BCTA", "BCFA", "BSTR", "BSFR", "BSTA", "BSFA", "BRNR", "BRNA",
      "BIRR", "BIRA", "BDRR", "BDRA", "BSNR", "BSNA", "BXA", "BSXA", "ZBRR", "ZBSR",
      "RETC", "RETE"}


def cycles(src, nbytes):
    op = src.split()[0].split(",")[0].upper()
    ind = "*" in src
    if op in BR:
        c = 3
    elif op.endswith("R") and op[:-1] in ("LOD", "STR", "ADD", "SUB", "AND", "IOR", "EOR", "COM"):
        c = 3
    elif nbytes >= 3:
        c = 4
    else:
        c = 2
    return c + (2 if ind else 0)


def load(path):
    code, labs = {}, []
    for line in open(path, errors="ignore"):
        m = LINE.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        src = m.group(3).split(";")[0].strip()
        src = re.sub(r"^\(MACRO\)\[\d+\]\s*", "", src)
        lm = LAB.match(src)
        if lm:
            labs.append((addr, lm.group(1)))
            src = src[len(lm.group(0)):].strip()
        if src:
            code[addr] = (src.split()[0].split(",")[0].lower(), cycles(src, len(m.group(2).split())))
    return code, sorted(labs)


def main():
    trace, lst = sys.argv[1], sys.argv[2]
    top = int(sys.argv[3]) if len(sys.argv) > 3 else 30
    frames = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    pages = {}
    for p in sorted(glob.glob(os.path.join(lst, "fb_p*.lst"))):
        n = int(re.search(r"_p(\d+)\.lst$", p).group(1))
        pages[n] = load(p)
    fixed_code, fixed_labs = pages[0]
    cur = 0
    per = collections.Counter()
    total = 0
    last, lastcyc = None, 3
    for line in open(trace, errors="ignore"):
        lm = re.search(r"\(loops for (\d+) instructions\)", line)
        if lm and last:                 # MAME folds a repeating loop: its instructions
            n = int(lm.group(1)) * lastcyc
            per[last] += n
            total += n
            continue
        m = re.match(r"^(?:\d+:)?([0-9A-F]{4}): (\S+)", line)
        if not m:
            continue
        a, mn = int(m.group(1), 16), m.group(2).lower().split(",")[0]
        if 0x0800 <= a < 0x1000:
            if pages[cur][0].get(a, ("", 0))[0] != mn:
                for n, (c, _) in pages.items():
                    if c.get(a, ("", 0))[0] == mn:
                        cur = n
                        break
            code, labs = pages[cur]
            where = "p%d" % cur
        else:
            code, labs = fixed_code, fixed_labs
            where = "A" if a < 0x0800 else "B"
        cyc = code.get(a, ("", 2))[1]
        lab = "?"
        for la, ln in labs:
            if la > a:
                break
            lab = ln
        last = "%s %s" % (where, lab)
        lastcyc = cyc
        per[last] += cyc
        total += cyc
    print("traceprof: %d cycles (%d clocks)%s" % (total, total * 3,
          ", %d a frame of 4972" % (total // frames) if frames else ""))
    for k, v in per.most_common(top):
        print("  %6d %5.1f%%  %s" % (v, 100.0 * v / total, k))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""traceprof.py -- a MAME trace of the Channel F build (chfsim "trace FILE":
each line "PG PC: mnemonic", PG the page mapped) charged to the listing's
labels in F8 clocks (MAME f8.cpp's counts): where a pass's time goes.

    traceprof.py TRACE LISTING [TOP]
"""
import bisect
import re
import sys

# clocks by mnemonic (f8.cpp); branches counted taken (+2 at most)
CLK = {}
for m in ("LR", "LIS", "CLR", "COM", "INC", "SL", "SR", "LNK", "AS", "XS", "NS", "NOP", "LISU", "LISL", "ASD"):
    CLK[m] = 4
for m in ("XDC", "POP", "DI", "EI"):
    CLK[m] = 8
for m in ("LM", "ST", "ADC", "AM", "AMD", "NM", "OM", "XM", "CM", "LI", "NI", "OI", "XI", "AI", "CI"):
    CLK[m] = 10
for m in ("BZ", "BNZ", "BC", "BNC", "BP", "BM", "BNO", "BR", "BT", "BF", "BNOV"):
    CLK[m] = 14
CLK.update({"DS": 6, "BR7": 10, "PK": 16, "JMP": 22, "DCI": 24, "PI": 26, "IN": 16, "OUT": 16})


def clocks(mn, ops):
    if mn in ("OUTS", "INS"):
        return 8 if ops.strip() in ("0", "1", "$0", "$1") else 16
    if mn == "LR":
        o = ops.replace(" ", "").upper()
        if o in ("K,P", "P,K", "DC,Q", "Q,DC", "DC,H", "H,DC", "P0,Q"):
            return 16
    return CLK.get(mn, 4)


def main():
    trace, lst = sys.argv[1], sys.argv[2]
    top = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    labels, page = {}, None
    for line in open(lst, encoding="latin-1"):
        m = re.match(r"(?:\(\d+\))?\s+\d+/\s*([0-9A-F]+) :[^;]*?\s([A-Z_][A-Z0-9_]*):", line)
        if not m:
            continue
        addr, name = int(m.group(1), 16), m.group(2)
        pm = re.match(r"MB_P(\d)(END)?$", name)
        if pm:
            page = None if pm.group(2) else int(pm.group(1))
            continue
        labels.setdefault(page if addr >= 0x4800 else None, []).append((addr, name))
    keys = {}
    for k in labels:
        labels[k].sort()
        keys[k] = [a for a, _ in labels[k]]
    hist, total = {}, 0
    for line in open(trace, encoding="latin-1"):
        m = re.match(r"([0-9A-F]{2}) ([0-9A-F]{4}): (\S+)\s*(.*)", line)
        if not m:
            continue
        pg, pc, mn, ops = int(m.group(1), 16), int(m.group(2), 16), m.group(3).upper(), m.group(4)
        k = pg if pc >= 0x4800 else None
        tab = labels.get(k, [])
        i = bisect.bisect_right(keys.get(k, []), pc) - 1
        name = tab[i][1] if i >= 0 else "?%04X" % pc
        if k is not None:
            name = "p%d:%s" % (k, name)
        c = clocks(mn, ops)
        hist[name] = hist.get(name, 0) + c
        total += c
    for k, v in sorted(hist.items(), key=lambda x: -x[1])[:top]:
        print("%7d %5.1f%%  %s" % (v, 100.0 * v / total, k))
    print("%d clocks" % total)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""checksize.py -- per-region ROM budgets and the RAM ledger, from the listing.
(M.U.L.E.'s clients/astrocade/tools/checksize.py, for FujiBlast: the regions
are found from the fences, and the RAM check counts the vector and SCRATCH.)

fujiblst.asm brackets every module with an MB_* fence label. A region is a
page (MB_P<n>) or the resident half (MB_R); its fences are the MB_ labels
that start with the region's prefix, in address order, closed by the
region's END fence. Every page is PHASEd to 2000H, so each region is
budgeted on its own: a page 4096 bytes, the resident half 2816 (3000H-3AFFH).

RAM: ram.inc hands out variables upward from the end of the picture
(RAMBASE) to RAMEND; RAMTOP is the first byte that isn't theirs (SCRATCH,
then the IM 2 vector, then the stack from STKLOW to STACK). RAMEND > RAMTOP
fails the build.

Usage: checksize.py fujiblst.lst
"""

import re
import sys

WARN_SPARE = 100


def read_symbols(path):
    syms = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()
    tail = text[text.rfind("Symbol Table:"):]
    for name, val in re.findall(r"\b([a-z_][a-z0-9_]*)\s+=?\s*([0-9a-f]{1,5})\+?",
                                tail):
        syms[name.upper()] = int(val, 16)
    return syms


def main():
    if len(sys.argv) != 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    syms = read_symbols(sys.argv[1])
    regions = sorted({m.group(1) for n in syms
                      for m in [re.match(r"^(MB_P\d+|MB_R)(?:_|END$|$)", n)] if m},
                     key=lambda p: (p != "MB_P0", p != "MB_R",
                                    int(p[4:]) if p[4:].isdigit() else 0))
    bad = 0
    print("checksize: region budgets")
    for pre in regions:
        budget = 2816 if pre == "MB_R" else 4096
        region = "resident" if pre == "MB_R" else "page " + pre[4:]
        fences = sorted((a, n) for n, a in syms.items()
                        if n.startswith(pre + "_") or n == pre + "END" or n == pre)
        if len(fences) < 2:
            continue
        for (addr, name), (nxt, _) in zip(fences, fences[1:]):
            if nxt - addr:
                print(f"  {region:<9} {name[len(pre):].lstrip('_') or '(head)':<10}"
                      f" {nxt - addr:5} bytes")
        used = fences[-1][0] - fences[0][0]
        spare = budget - used
        print(f"  {region:<9} {'total':<10} {used:5} of {budget} ({spare} spare)")
        if spare < 0:
            print(f"checksize: {region} is {-spare} bytes over", file=sys.stderr)
            bad = 1
        elif spare < WARN_SPARE and region == "resident":
            print(f"checksize: {region} headroom under {WARN_SPARE} bytes",
                  file=sys.stderr)
    if "RAMEND" in syms and "RAMTOP" in syms:
        base = syms.get("RAMBASE", 0)
        used = syms["RAMEND"] - base
        room = syms["RAMTOP"] - base
        print(f"  RAM       variables  {used:5} of {room} "
              f"({room - used} spare), stack {syms.get('STACK', 0) - syms.get('STKLOW', 0)}")
        if used > room:
            print(f"checksize: RAM is {used - room} bytes over", file=sys.stderr)
            bad = 1
    else:
        print("checksize: no RAMEND/RAMTOP in the listing", file=sys.stderr)
        bad = 1
    return bad


if __name__ == "__main__":
    sys.exit(main())

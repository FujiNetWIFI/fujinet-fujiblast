#!/usr/bin/env python3
"""gen.py -- the 160B probe's data (probe.s): the DLL, a display list per
zone, the char pages, the bombers' pixels; and probe.json for check.py.

Zone z has: the run (28 cells of 160B indirect, CW=1, from hpos 24, cells
alternating P0 and P1 colour), optionally the left panel (4 cells, hpos 8,
P2) and right panel (3 cells, hpos 140, P3), and NB bombers (160B direct,
4 bytes wide = 8 px, hpos 28 + 24 k, palettes P4..P7).

    gen.py data.bin probe.json
"""
import json
import sys

# (run cells, left, right, bombers)
LOADS = []
for nb in range(5):
    for lr in ((0, 0), (1, 0), (1, 1)):
        LOADS.append((28, lr[0], lr[1], nb))
LOADS += [(26, 1, 1, 4), (24, 1, 1, 4), (22, 1, 1, 4), (20, 1, 1, 4)]
LOADS += [(28, 0, 0, 0)] * (24 - len(LOADS))
RUN_X, RUN_N = 24, 28
LEFT_X, LEFT_N = 8, 4
RIGHT_X, RIGHT_N = 140, 3
BOMB_X = [28 + 24 * k for k in range(4)]
MAP = 0x8800            # run map: 28 chars; left map 4; right map 3
CB = 0x9000             # char pages
BOMB = 0xA000           # bomber data pages (8 pages)
DLB = 0x8A00            # display lists, 64 bytes a zone
DLLB = 0x8900


def ext(addr, ind, pal, w, hpos):
    # [lo, WM|ext|IND, hi, pal<<5|-w, hpos]
    return [addr & 0xFF, 0xC0 | (0x20 if ind else 0), addr >> 8, (pal << 5) | ((-w) & 0x1F), hpos]


def main():
    out_s, out_j = sys.argv[1], sys.argv[2]
    # char pages: 8 pages at CB, each: code 0: $50,$50  2: $55,$55  4: $5A,$5A  6: $5F,$5F
    page = [0] * 256
    for code, b in ((0, 0x50), (2, 0x55), (4, 0x5A), (6, 0x5F)):
        page[code] = page[code + 1] = b
    # bombers: 4 bytes each, palette k of group 1: $50 | k*5
    bpage = [0xFF] * 256
    for k in range(4):
        for i in range(4):
            bpage[k * 4 + i] = 0x50 | (k * 5)
    # layout in ROM: we emit .org-less data at fixed addresses using .res/.align from the cfg:
    # RODATA is 256-aligned at $8000+code; use a single 24K block built here.
    img = bytearray([0xFF] * 0x7000)           # $8000-$EFFF
    def put(addr, data):
        img[addr - 0x8000:addr - 0x8000 + len(data)] = bytes(data)
    run_map = [0 if i % 2 == 0 else 2 for i in range(RUN_N)]
    put(MAP, run_map)
    put(MAP + 0x40, [4] * LEFT_N)
    put(MAP + 0x50, [6] * RIGHT_N)
    for p in range(8):
        put(CB + p * 256, page)
        put(BOMB + p * 256, bpage)
    zones = []
    for z, (n, L, R, nb) in enumerate(LOADS):
        dl = []
        cyc = 16
        # the run: leave out the cells over n: it starts at hpos 24, ends at 24 + 4 n
        dl += ext(MAP, True, 0, n, RUN_X)
        cyc += 10 + 9 * n
        if L:
            dl += ext(MAP + 0x40, True, 0, LEFT_N, LEFT_X)
            cyc += 10 + 9 * LEFT_N
        if R:
            dl += ext(MAP + 0x50, True, 0, RIGHT_N, RIGHT_X)
            cyc += 10 + 9 * RIGHT_N
        for k in range(nb):
            dl += ext(BOMB + k * 4, False, 4, 4, BOMB_X[k])
            cyc += 10 + 3 * 4
        dl += [0, 0]
        cyc += 6
        assert len(dl) <= 64
        put(DLB + z * 64, dl)
        zones.append(dict(run=n, left=L, right=R, bombers=nb, dma=cyc))
    # the DLL: top blank 16 + 9 lines, 24 zones of 8, bottom 4 x 16
    nulldl = 0x9800
    put(nulldl, [0, 0])
    dll = [(0x0F, nulldl), (0x08, nulldl)]
    dll += [(0x07, DLB + z * 64) for z in range(24)]
    dll += [(0x0F, nulldl)] * 4
    d = []
    for fl, a in dll:
        d += [fl, a >> 8, a & 0xFF]
    put(DLLB, d)
    open(out_s, "wb").write(img[:0x7000])      # $8000-$EFFF
    json.dump(dict(zones=zones, run_x=RUN_X, run_n=RUN_N, left_x=LEFT_X, right_x=RIGHT_X, bomb_x=BOMB_X), open(out_j, "w"), indent=1)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""mkgfx_nes.py OUTDIR -- gfx/nes/*.png and gfx/namco.bin -> gen/nes/gfxdata.{c,h}
for the NES port (src/nes/gfx.c).

CHR is 2 bits a pixel, 16 bytes a cell (8 bytes of plane 0, then plane 1).
Three pattern sets, each at most 256 cells:
  arena set (BG table):  cells 0-63 the Namco font (ASCII 32-95; 0 is the
                         blank), 64 the wall, then every cell of every tile
                         frame, deduplicated
  menu set  (BG table):  the font, the logo's cells, the trophy's 4
  sprite set (sprite table, 8x16): pairs of cells (top, bottom), flips shared
The tiles' animation is name-table writes (tile_cell[tile][phase][TL TR BL BR]),
so no pattern is ever rewritten while the game runs.

Colours: palettes come from drawart_nes (the one source), written as the
2C02's colour numbers. The generated .c is const data in the ART bank.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import png  # noqa: E402
import drawart_nes as N  # noqa: E402
from artlayout import TILES, SPRITES  # noqa: E402

ROOT = os.path.join(HERE, "..")
GFX = os.path.join(ROOT, "gfx", "nes")


def load(name):
    im = png.Image(os.path.join(GFX, name))
    assert im.idx is not None, name + ": not a palette PNG"
    return im.idx


def slot_cell(idx, x0, y0):
    return tuple(tuple(idx[y0 + y][x0 + x] & 3 for x in range(8)) for y in range(8))


def chr_bytes(c):
    p0 = bytes(sum(((c[y][x] & 1) << (7 - x)) for x in range(8)) for y in range(8))
    p1 = bytes(sum((((c[y][x] >> 1) & 1) << (7 - x)) for x in range(8)) for y in range(8))
    return p0 + p1


def flip(c):
    return tuple(tuple(reversed(r)) for r in c)


class Set:
    def __init__(self, name, limit=256):
        self.name, self.limit = name, limit
        self.cells = []
        self.at = {}

    def add(self, c):
        if c not in self.at:
            assert len(self.cells) < self.limit, f"{self.name}: out of cells"
            self.at[c] = len(self.cells)
            self.cells.append(c)
        return self.at[c]

    def data(self):
        return b"".join(chr_bytes(c) for c in self.cells)


def font_cells():
    d = open(os.path.join(ROOT, "gfx", "namco.bin"), "rb").read()
    assert len(d) == 768
    out = []
    for g in range(64):                         # ASCII 32-95
        out.append(tuple(tuple(1 if d[g * 8 + y] >> (7 - x) & 1 else 0 for x in range(8))
                         for y in range(8)))
    return out


def arr(name, data, per=16, fmt="0x%02X", ctype="const uint8_t"):
    lines = []
    for i in range(0, len(data), per):
        lines.append("    " + ", ".join(fmt % v for v in data[i:i + per]) + ",")
    return f"{ctype} {name}[{len(data)}] = {{\n" + "\n".join(lines) + "\n};\n"


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    tiles_idx = load("tiles.png")
    cells_idx = load("cells.png")
    logo_idx = load("logo.png")
    spr_idx = load("sprites.png")
    big_idx = load("bigwin.png")

    # ---- arena set
    arena = Set("arena")
    for c in font_cells():
        arena.cells.append(c)
        arena.at.setdefault(c, len(arena.cells) - 1)
    assert arena.at[arena.cells[0]] == 0 and arena.cells[0] == tuple((0,) * 8 for _ in range(8))
    WALL = arena.add(slot_cell(cells_idx, 0, 0))
    ntiles = len(TILES)
    tile_cell = []
    tile_pal = []
    for i, name in enumerate(TILES):
        # frames in the sheet: the tool wrote len(frames) of them; a frame
        # not written is all index 0 (the floor): repeat the ones that are
        written = N.tile_frames()[name]
        nfr = len(written)
        pal = N.TILE_PAL[name]
        tile_pal.append(pal)
        phases = []
        for ph in range(4):
            f = ph % nfr
            cs = []
            for (dy, dx) in ((0, 0), (0, 8), (8, 0), (8, 8)):
                c = slot_cell(tiles_idx, f * 16 + dx, i * 16 + dy)
                cs.append(arena.add(c))
            phases.append(cs)
        tile_cell.append(phases)
    arena_n = len(arena.cells)

    # ---- menu set: the font, the logo's cells, the trophy's
    menu = Set("menu")
    for c in font_cells():
        menu.cells.append(c)
        menu.at.setdefault(c, len(menu.cells) - 1)
    lw = len(logo_idx[0]) // 8
    logo_cell, logo_pal = [], []
    for r in range(3):
        logo_pal.append(max(logo_idx[r * 8 + y][x] // 4 for y in range(8) for x in range(len(logo_idx[0]))))
        logo_cell.append([menu.add(slot_cell(logo_idx, cx * 8, r * 8)) for cx in range(lw)])
    ti = TILES.index("trophy")
    trophy = [menu.add(slot_cell(tiles_idx, dx, ti * 16 + dy)) for (dy, dx) in ((0, 0), (0, 8), (8, 0), (8, 8))]
    menu_n = len(menu.cells)

    # ---- sprites: 8x16 pairs, flips shared
    spr = Set("sprites", 128)          # pairs
    pairs = {}

    def pair(top, bot):
        a, b = (top, bot), (flip(top), flip(bot))
        if a in pairs:
            return pairs[a], 0
        if b in pairs:
            return pairs[b], 0x40
        assert len(spr.cells) < 128, "sprite pairs"
        spr.cells.append(a)
        pairs[a] = len(spr.cells) - 1
        return pairs[a], 0

    def sprite_halves(idx, x0, y0):
        res = []
        for sx in (0, 8):
            top = slot_cell(idx, x0 + sx, y0)
            bot = slot_cell(idx, x0 + sx, y0 + 8)
            res.append(pair(top, bot))
        return res

    frames = [sprite_halves(spr_idx, i * 16, 0) for i in range(len(SPRITES))]
    big = []
    for k in (0, 1):
        q = []
        for sy in (0, 16):
            for sx in (0, 8, 16, 24):
                top = slot_cell(big_idx, k * 32 + sx, sy)
                bot = slot_cell(big_idx, k * 32 + sx, sy + 8)
                q.append(pair(top, bot))
        big.append(q)
    spr_chr = b"".join(chr_bytes(t) + chr_bytes(b) for (t, b) in spr.cells)
    # 8x16 sprites on the table at $1000: tile byte = 2 * pair + 1
    def sprite_tile(p):
        return 2 * p[0] + 1, p[1]

    # ---- palettes
    def pal4(backdrop, pals):
        o = []
        for p in pals:
            o += [backdrop] + list(p)
        return o
    pal_arena = pal4(*N.BG_ARENA)
    pal_title = pal4(*N.BG_TITLE)
    pal_menu = pal4(*N.BG_MENU)
    pal_spr = pal4(N.BG_ARENA[0], N.OPTIONS[N.OPTION][0])

    # ---- write
    h = ["/* gfxdata.h -- generated by tools/mkgfx_nes.py: do not edit. */",
         "#ifndef GFXDATA_H", "#define GFXDATA_H", "#include <stdint.h>", "",
         f"#define ARENA_CELLS {arena_n}", f"#define MENU_CELLS {menu_n}",
         f"#define SPR_PAIRS {len(spr.cells)}", f"#define N_TILEGFX {ntiles}",
         f"#define N_FRAMES {len(SPRITES)}", f"#define CH_WALL {WALL}", f"#define LOGO_W {lw}",
         f"#define BLACK_TOP 0x{0x0F:02X}", ""]
    codes = ["/* gfxcodes.h -- generated by tools/mkgfx_nes.py: do not edit. */",
             "#ifndef GFXCODES_H", "#define GFXCODES_H", ""]
    for i, name in enumerate(SPRITES):
        codes.append(f"#define FR_{name.upper()} {i}")
    codes += ["", "#endif", ""]
    open(os.path.join(out, "gfxcodes.h"), "w").write("\n".join(codes))
    h.append('#include "gfxcodes.h"')
    h += ["",
          "extern const uint8_t arena_chr[], menu_chr[], spr_chr[];",
          "extern const uint8_t tile_cell[N_TILEGFX][4][4];      /* [tile][phase][TL TR BL BR] */",
          "extern const uint8_t tile_pal[N_TILEGFX];",
          "extern const uint8_t logo_cell[3][LOGO_W], logo_pal[3], trophy_cell[4];",
          "extern const uint8_t spr_frame[N_FRAMES][2][2];      /* [frame][left, right][tile, attr flip] */",
          "extern const uint8_t spr_big[2][8][2];                /* [frame][2x4 sprites row-major][tile, flip] */",
          "extern const uint8_t pal_arena[16], pal_title[16], pal_menu[16], pal_spr[16];",
          "", "#endif", ""]
    c = ["/* gfxdata.c -- generated by tools/mkgfx_nes.py: do not edit. */",
         '#include "gfxdata.h"', '#pragma rodata-name ("ART")', ""]
    c.append(arr("arena_chr", list(arena.data()), ctype="const uint8_t"))
    c.append(arr("menu_chr", list(menu.data()), ctype="const uint8_t"))
    c.append(arr("spr_chr", list(spr_chr), ctype="const uint8_t"))
    c.append('#pragma rodata-name ("RODATA")')
    c.append("const uint8_t tile_cell[N_TILEGFX][4][4] = {\n" + "\n".join(
        "    {" + ", ".join("{%s}" % ",".join(map(str, p)) for p in t) + "}," for t in tile_cell) + "\n};\n")
    c.append(arr("tile_pal", tile_pal, fmt="%d"))
    c.append("const uint8_t logo_cell[3][LOGO_W] = {\n" + "\n".join(
        "    {" + ",".join(map(str, r)) + "}," for r in logo_cell) + "\n};\n")
    c.append(arr("logo_pal", logo_pal, fmt="%d"))
    c.append(arr("trophy_cell", trophy, fmt="%d"))
    c.append("const uint8_t spr_frame[N_FRAMES][2][2] = {\n" + "\n".join(
        "    {" + ", ".join("{%d,0x%02X}" % sprite_tile(hv) for hv in f) + "}," for f in frames) + "\n};\n")
    c.append("const uint8_t spr_big[2][8][2] = {\n" + "\n".join(
        "    {" + ", ".join("{%d,0x%02X}" % sprite_tile(hv) for hv in q) + "}," for q in big) + "\n};\n")
    for nme, p in (("pal_arena", pal_arena), ("pal_title", pal_title), ("pal_menu", pal_menu), ("pal_spr", pal_spr)):
        c.append(arr(nme, p, fmt="0x%02X"))
    open(os.path.join(out, "gfxdata.h"), "w").write("\n".join(h))
    open(os.path.join(out, "gfxdata.c"), "w").write("\n".join(c))
    print(f"gen/nes: arena set {arena_n}/256 cells, menu set {menu_n}/256, "
          f"{len(spr.cells)} sprite pairs ({len(spr_chr)} bytes), tiles {ntiles}")


if __name__ == "__main__":
    main()

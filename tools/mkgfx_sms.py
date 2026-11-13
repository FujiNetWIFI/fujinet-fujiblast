#!/usr/bin/env python3
"""mkgfx_sms.py OUTDIR -- gfx/sms/*.png and gfx/namco.bin -> gen/sms/gfxdata.{c,h}
and gfxcodes.h, for the Sega Master System port (not written yet).

Tiles are the VDP's: 32 bytes, a row 4 bytes (bit planes 0-3, bit 7 the
leftmost pixel). A name table word is little endian: bits 0-8 the tile, 9
hflip, 10 vflip, 11 palette (0: the background's, 1: the sprites'), 12
priority. Three pattern sets, which the port loads with the display off (the
patterns in gfxart1.c and gfxart2.c, one segment each: the two art banks):

  arena set (BG tiles, from BG_BASE = 256: the sprites use bank 0, tiles
            0-255): plain floor, plain black, the wall, then every cell of
            every tile frame, a cell and its flips one tile. ARENA_STATIC
            of them; the port makes the HUD's glyph cells after them (up to
            BG_TILES in all).
  menu set  (BG tiles): plain blue, the logo's cells, the trophy's.
  sprite sets (tiles 0-255, 8x8, no flip, so a seat's patterns are its own):
            "arena" (walk and die frames) and "menu" (down, right, win, and
            the big cheer): each seat's cells, tile 0 left empty. A frame is
            4 tile numbers (top left, top right, bottom left, bottom right),
            SPR_NONE for a blank cell (no sprite needed).

The tiles' animation is name table writes: tile_cell[tile][phase][cell] are
words, a tile of fewer frames repeating over the 4 phases. Text is made at
run time from font_gfx (the Namco font, ASCII 32-95, 8 bytes a glyph, a set
bit a pixel) in the palette entries the TEXT_* defines name.

Colours: drawart_sms's palettes as the VDP's CRAM bytes (00BBGGRR).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import png  # noqa: E402
import drawart_sms as S  # noqa: E402
from artlayout import TILES, SPRITES  # noqa: E402

ROOT = os.path.join(HERE, "..")
GFX = os.path.join(ROOT, "gfx", "sms")
BG_BASE = 256
BG_TILES = 192
SPR_NONE = 0xFF
T = 16


def load(name):
    im = png.Image(os.path.join(GFX, name))
    assert im.idx is not None, name + ": not a palette PNG"
    return im.idx


def cell(idx, x0, y0):
    return tuple(tuple(idx[y0 + y][x0 + x] for x in range(8)) for y in range(8))


def chr4(c):
    out = bytearray()
    for y in range(8):
        for p in range(4):
            out.append(sum(((c[y][x] >> p) & 1) << (7 - x) for x in range(8)))
    return bytes(out)


def unchr4(b):
    return tuple(tuple(sum(((b[y * 4 + p] >> (7 - x)) & 1) << p for p in range(4)) for x in range(8))
                 for y in range(8))


def flipped(c, hf, vf):
    if hf:
        c = S.hflip(c)
    if vf:
        c = S.vflip(c)
    return c


class BgSet:
    """Background tiles: a cell and its flips are one."""

    def __init__(self, name):
        self.name = name
        self.cells = []
        self.at = {}

    def word(self, c, pal=0):
        s = S.canon(c)
        if s not in self.at:
            self.at[s] = len(self.cells)
            self.cells.append(s)
        for hf in (0, 1):
            for vf in (0, 1):
                if flipped(s, hf, vf) == c:
                    return self.at[s] + BG_BASE | hf << 9 | vf << 10 | pal << 11
        raise AssertionError("no flip of the canonical cell")

    def data(self):
        return b"".join(chr4(c) for c in self.cells)

    def decode(self, w):
        s = self.cells[(w & 0x1FF) - BG_BASE]
        return flipped(unchr4(chr4(s)), w >> 9 & 1, w >> 10 & 1)


class SprSet:
    """Sprite tiles: no flips; tile 0 is left empty."""

    def __init__(self, name):
        self.name = name
        self.cells = [None]
        self.at = {}

    def tile(self, c):
        if not any(p for r in c for p in r):
            return SPR_NONE
        if c not in self.at:
            assert len(self.cells) < 255, self.name + ": out of sprite tiles"
            self.at[c] = len(self.cells)
            self.cells.append(c)
        return self.at[c]

    def data(self):
        return b"".join(chr4(c) if c else bytes(32) for c in self.cells)


def arr(name, data, per=16, fmt="0x%02X", ctype="const uint8_t"):
    lines = []
    for i in range(0, len(data), per):
        lines.append("    " + ", ".join(fmt % v for v in data[i:i + per]) + ",")
    return f"{ctype} {name}[{len(data)}] = {{\n" + "\n".join(lines) + "\n};\n"


def font_rows():
    d = open(os.path.join(ROOT, "gfx", "namco.bin"), "rb").read()
    assert len(d) == 768
    return list(d[:64 * 8])                         # ASCII 32-95


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    tiles_idx = load("tiles.png")
    cells_idx = load("cells.png")
    logo_idx = load("logo.png")
    trophy_idx = load("trophy.png")
    spr_idx = load("sprites.png")
    big_idx = load("bigwin.png")
    frames = S.tile_frames()

    # ---- the arena set
    arena = BgSet("arena")
    w_floor = arena.word(tuple((S.FLOOR,) * 8 for _ in range(8)))
    w_black = arena.word(tuple((S.BLACK,) * 8 for _ in range(8)))
    w_wall = arena.word(cell(cells_idx, 0, 0))
    assert cell(cells_idx, 8, 0) == tuple((S.FLOOR,) * 8 for _ in range(8))
    tile_cell, tile_src = [], []
    for i, name in enumerate(TILES):
        if name == "trophy":
            tile_cell.append([[w_floor] * 4 for _ in range(4)])
            tile_src.append(None)
            continue
        nfr = len(frames[name])
        phases = []
        for ph in range(4):
            f = ph % nfr
            phases.append([arena.word(cell(tiles_idx, f * T + dx, i * T + dy))
                           for (dy, dx) in ((0, 0), (0, 8), (8, 0), (8, 8))])
        tile_cell.append(phases)
        tile_src.append(nfr)
    assert len(arena.cells) + S.ARENA_TEXT <= BG_TILES, f"arena set {len(arena.cells)} + text over {BG_TILES}"

    # ---- the menu set
    menu = BgSet("menu")
    w_blue = menu.word(tuple((0,) * 8 for _ in range(8)))
    lw = len(logo_idx[0]) // 8
    logo_cell = [[menu.word(cell(logo_idx, cx * 8, r * 8)) for cx in range(lw)] for r in range(3)]
    trophy_cell = [menu.word(cell(trophy_idx, dx, dy)) for (dy, dx) in ((0, 0), (0, 8), (8, 0), (8, 8))]
    assert len(menu.cells) + 64 <= BG_TILES, "menu set over budget"

    # ---- the sprite sets
    arena_names = [n for n in SPRITES if not n.startswith("win")]
    menu_names = ["down0", "right0", "right1", "right2", "win0", "win1"]
    sets = {}
    for label, names in (("arena", arena_names), ("menu", menu_names)):
        ss = SprSet(label)
        table = []
        for seat in range(4):
            per = []
            for fi, name in enumerate(SPRITES):
                if name in names:
                    per.append([ss.tile(cell(spr_idx, fi * T + dx, seat * T + dy))
                                for (dy, dx) in ((0, 0), (0, 8), (8, 0), (8, 8))])
                else:
                    per.append([SPR_NONE] * 4)
            table.append(per)
        sets[label] = (ss, table)
    big = []
    ms = sets["menu"][0]
    for seat in range(4):
        per = []
        for k in (0, 1):
            per.append([ms.tile(cell(big_idx, k * 32 + cx * 8, seat * 32 + cy * 8))
                        for cy in range(4) for cx in range(4)])
        big.append(per)

    # ---- check: the data back into pixels, against drawart_sms's own rendering
    for i, name in enumerate(TILES):
        if tile_src[i] is None:
            continue
        for ph in range(4):
            src = S.render(frames[name][ph % tile_src[i]])
            got = [[0] * T for _ in range(T)]
            for k, (dy, dx) in enumerate(((0, 0), (0, 8), (8, 0), (8, 8))):
                c = arena.decode(tile_cell[i][ph][k])
                for y in range(8):
                    for x in range(8):
                        got[dy + y][dx + x] = c[y][x]
            assert got == src, f"tile {name} phase {ph} does not round-trip"
    lg = S.logo()
    for r in range(3):
        for cx in range(lw):
            c = menu.decode(logo_cell[r][cx])
            assert c == cell(lg, cx * 8, r * 8), f"logo cell {r},{cx}"
    rf = S.role_frames()
    for seat in range(4):
        for label, (ss, table) in sets.items():
            for fi, name in enumerate(SPRITES):
                for k, (dy, dx) in enumerate(((0, 0), (0, 8), (8, 0), (8, 8))):
                    t = table[seat][fi][k]
                    if name not in (arena_names if label == "arena" else menu_names):
                        assert t == SPR_NONE
                        continue
                    want = cell(S.seat_rows(rf[name], seat), dx, dy)
                    if t == SPR_NONE:
                        assert not any(p for r in want for p in r)
                    else:
                        assert unchr4(ss.data()[t * 32:t * 32 + 32]) == want, f"sprite {label} {seat} {name}"

    # ---- write
    codes = ["/* gfxcodes.h -- generated by tools/mkgfx_sms.py: do not edit. */",
             "#ifndef GFXCODES_H", "#define GFXCODES_H", ""]
    for i, name in enumerate(SPRITES):
        codes.append(f"#define FR_{name.upper()} {i}")
    codes += ["", "#endif", ""]
    h = ["/* gfxdata.h -- generated by tools/mkgfx_sms.py: do not edit. */",
         "#ifndef GFXDATA_H", "#define GFXDATA_H", "#include <stdint.h>", '#include "gfxcodes.h"', "",
         f"#define BG_BASE {BG_BASE}                 /* the background's tiles from here (sprites: tiles 0-255) */",
         f"#define BG_TILES {BG_TILES}                /* the background's tiles in all */",
         f"#define ARENA_STATIC {len(arena.cells)}",
         f"#define MENU_STATIC {len(menu.cells)}",
         f"#define ARENA_TEXT {S.ARENA_TEXT}                /* tiles kept for the arena's glyphs, after ARENA_STATIC */",
         f"#define SPR_ARENA_TILES {len(sets['arena'][0].cells)}",
         f"#define SPR_MENU_TILES {len(sets['menu'][0].cells)}",
         f"#define SPR_NONE 0x{SPR_NONE:02X}",
         f"#define N_TILEGFX {len(TILES)}", f"#define N_FRAMES {len(SPRITES)}", f"#define LOGO_W {lw}",
         "",
         "/* name table words of the plain cells (arena set), the plain cell of the menu set */",
         f"#define W_FLOOR 0x{w_floor:04X}", f"#define W_BLACK 0x{w_black:04X}",
         f"#define W_WALL  0x{w_wall:04X}", f"#define W_BLUE  0x{w_blue:04X}", "",
         "/* palette entries (arena background palette) of the text colours */",
         f"#define TEXT_ARENA_FLOOR {S.FLOOR}", f"#define TEXT_ARENA_BLACK {S.BLACK}",
         "#define TEXT_ARENA_WHITE 3",
         "/* seats 0-3 */",
         "#define TEXT_ARENA_SEAT { " + ", ".join(str(v) for v in S.SEAT_TEXT) + " }",
         "#define TEXT_MENU_SEAT { " + ", ".join(str(v) for v in S.MENU_TEXT) + " }",
         f"#define TEXT_MENU_BLUE 0", "#define TEXT_MENU_WHITE 2", "#define TEXT_MENU_GREY 3",
         f"#define TEXT_MENU_HI {S.M_HI}", "#define TEXT_MENU_ORANGE 5",
         f"#define BD_ARENA 0x{S.cram((0, 0, 0)):02X}      /* the backdrop (sprite palette entry 0) */",
         f"#define BD_MENU  0x{S.cram(S.MENU[0]):02X}", "",
         "extern const uint8_t arena_chr[], menu_chr[], spr_arena_chr[], spr_menu_chr[];",
         "extern const uint16_t tile_cell[N_TILEGFX][4][4];     /* [tile][phase][TL TR BL BR]: name table words */",
         "extern const uint16_t logo_cell[3][LOGO_W], trophy_cell[4];",
         "extern const uint8_t sprt_arena[4][N_FRAMES][4];        /* [seat][frame][TL TR BL BR]: tile or SPR_NONE */",
         "extern const uint8_t sprt_menu[4][N_FRAMES][4];",
         "extern const uint8_t sprt_big[4][2][16];                /* [seat][frame][4x4 row-major], in the menu set */",
         "extern const uint8_t pal_arena[16], pal_menu[16], pal_spr[16];     /* CRAM bytes */",
         "extern const uint8_t font_gfx[64 * 8];", "", "#endif", ""]
    # the patterns are in the two art banks (a .c file's consts go to one
    # segment: the Makefile gives each its own): arena in ART1, menus in ART2
    hdr = "/* %s -- generated by tools/mkgfx_sms.py: do not edit. */"
    art1 = [hdr % "gfxart1.c", '#include "gfxdata.h"', "",
            arr("arena_chr", list(arena.data())), arr("spr_arena_chr", list(sets["arena"][0].data()))]
    art2 = [hdr % "gfxart2.c", '#include "gfxdata.h"', "",
            arr("menu_chr", list(menu.data())), arr("spr_menu_chr", list(sets["menu"][0].data()))]
    open(os.path.join(out, "gfxart1.c"), "w").write("\n".join(art1))
    open(os.path.join(out, "gfxart2.c"), "w").write("\n".join(art2))
    c = [hdr % "gfxdata.c", '#include "gfxdata.h"', ""]
    c.append("const uint16_t tile_cell[N_TILEGFX][4][4] = {\n" + "\n".join(
        "    {" + ", ".join("{%s}" % ",".join("0x%04X" % w for w in p) for p in t) + "}," for t in tile_cell) + "\n};\n")
    c.append("const uint16_t logo_cell[3][LOGO_W] = {\n" + "\n".join(
        "    {" + ",".join("0x%04X" % w for w in r) + "}," for r in logo_cell) + "\n};\n")
    c.append(arr("trophy_cell", trophy_cell, fmt="0x%04X", ctype="const uint16_t"))
    for nme, label in (("sprt_arena", "arena"), ("sprt_menu", "menu")):
        table = sets[label][1]
        c.append(f"const uint8_t {nme}[4][N_FRAMES][4] = {{\n" + "\n".join(
            "    {" + ", ".join("{%s}" % ",".join("0x%02X" % t for t in fr) for fr in seat) + "}," for seat in table) + "\n};\n")
    c.append("const uint8_t sprt_big[4][2][16] = {\n" + "\n".join(
        "    {" + ", ".join("{%s}" % ",".join("0x%02X" % t for t in fr) for fr in seat) + "}," for seat in big) + "\n};\n")
    for nme, p in (("pal_arena", S.ARENA), ("pal_menu", S.MENU), ("pal_spr", S.SPR)):
        c.append(arr(nme, [S.cram(e) for e in p]))
    c.append(arr("font_gfx", font_rows()))
    open(os.path.join(out, "gfxcodes.h"), "w").write("\n".join(codes))
    open(os.path.join(out, "gfxdata.h"), "w").write("\n".join(h))
    open(os.path.join(out, "gfxdata.c"), "w").write("\n".join(c))
    print(f"gen/sms: arena set {len(arena.cells)} tiles (+{S.ARENA_TEXT} text, of {BG_TILES}), "
          f"menu set {len(menu.cells)}, sprites arena {len(sets['arena'][0].cells)} / menu "
          f"{len(sets['menu'][0].cells)} of 256")


if __name__ == "__main__":
    main()

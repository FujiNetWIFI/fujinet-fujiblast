#!/usr/bin/env python3
"""mkgfx_tms.py -- gfx/tms/*.png and gfx/namco.bin -> the Adam port's data.

    python3 tools/mkgfx_tms.py OUTDIR    # writes OUTDIR/gfxdata.c, gfxcodes.h

Graphics II used as a character map: a cell is 8 lines of a pattern byte
(1 = the foreground colour) and a colour byte (foreground << 4 | background).
A cell is 16 bytes here: 8 pattern bytes, then 8 colour bytes.

  arena_cells   the arena's character set: code 0 the panel's blank, then
                the wall, the floor, the static tiles, then 4 cells for each
                animated tile kind (their first frame)
  anim_cells    each animated kind's 4 cells for each of 4 animation phases
  tile_chars    per map tile: the 4 codes (TL, TR, BL, BR)
  logo_cells    the logo's distinct cells; logo_map 3 rows of 27 indexes
  trophy_cells  the trophy's 4 cells (blue background)
  spr_a/spr_b   16x16 sprite layers (A: the seat's colour; B: black, in
                front), as the VDP wants a 16x16 pattern: TL, BL, TR, BR
  spr_bigA/B    the two cheering frames doubled to 32x32, as 4 such blocks
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from artlayout import TILES, SPRITES  # noqa: E402
from png import Image  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
TMS = os.path.join(ROOT, "gfx", "tms")

# drawart_tms.py's colours
RGB = [
    None, (0, 0, 0), (33, 200, 66), (94, 220, 120), (84, 85, 237), (125, 118, 252),
    (212, 82, 77), (66, 235, 245), (252, 85, 84), (255, 121, 120), (212, 193, 84),
    (230, 206, 128), (33, 176, 59), (201, 91, 186), (204, 204, 204), (255, 255, 255),
]
INDEX = {c: i for i, c in enumerate(RGB) if c}
POWERUPS = ("fire", "bombup", "speed", "kick", "glove")
# the tiles that animate (the rest are drawn once)
ANIMATED = ["burn", "bomb", "dud", "flameC", "flameH", "flameV", "flameL", "flameR", "flameU", "flameD",
            "fire", "bombup", "speed", "kick", "glove"]


def pixels(img, x0, y0, w, h):
    out = []
    for y in range(h):
        row = []
        for x in range(w):
            p = img.getpixel((x0 + x, y0 + y))[:3]
            if p not in INDEX:
                raise ValueError(f"({x0 + x},{y0 + y}): {p} is not a TMS colour")
            row.append(INDEX[p])
        out.append(row)
    return out


def cell(rows):
    """8 lines of 8 colour indexes -> 16 bytes (pattern, colour)."""
    pat, col = [], []
    for r in rows:
        cs = sorted(set(r))
        if len(cs) > 2:
            raise ValueError(f"a cell line with {len(cs)} colours: {r}")
        bg = cs[0]
        fg = cs[-1]
        b = 0
        for p in r:
            b = b << 1 | (1 if p == fg and fg != bg else 0)
        pat.append(b)
        col.append(fg << 4 | bg)
    return pat + col


def cells_of(px, w, h):
    """The 8x8 cells of a w x h block of pixels, row by row."""
    return [cell([px[cy * 8 + y][cx * 8:cx * 8 + 8] for y in range(8)])
            for cy in range(h // 8) for cx in range(w // 8)]


def tile_frames():
    img = Image(os.path.join(TMS, "tiles.png"))
    out = {}
    for i, name in enumerate(TILES):
        frames = []
        for f in range(4):
            px = pixels(img, f * 16, i * 16, 16, 16)
            if f and all(p == 12 for r in px for p in r):
                break               # an empty column: no more frames
            frames.append(px)
        out[name] = frames
    return out


def phases(name, n):
    if n == 1:
        return [0, 0, 0, 0]
    if n == 2:
        return [0, 0, 1, 1] if name in POWERUPS else [0, 1, 0, 1]
    return [0, 1, 2, 3]


def sprite_layers():
    img = Image(os.path.join(TMS, "sprites.png"))
    role = {(255, 255, 255): "#", (255, 85, 85): "t", (85, 255, 255): "v", (0, 0, 0): "e"}
    frames = []
    for i in range(len(SPRITES)):
        a = [[0] * 16 for _ in range(16)]
        b = [[0] * 16 for _ in range(16)]
        for y in range(16):
            for x in range(16):
                p = img.getpixel((i * 16 + x, y))
                if p[3] < 128:
                    continue
                r = role[p[:3]]
                if r in "#ve":
                    a[y][x] = 1
                if r in "vt":
                    b[y][x] = 1
        frames.append((a, b))
    return frames


def block16(bits, x0=0, y0=0):
    """A 16x16 window of a bit matrix -> the 32-byte pattern (TL, BL, TR, BR)."""
    out = []
    for qx in (0, 1):
        for qy in (0, 1):
            for y in range(8):
                v = 0
                for x in range(8):
                    v = v << 1 | bits[y0 + qy * 8 + y][x0 + qx * 8 + x]
                out.append(v)
    return out


def doubled(bits):
    return [[p for p in r for _ in (0, 1)] for r in bits for _ in (0, 1)]


def carray(decl, rows):
    out = [f"const uint8_t {decl} = {{"]
    for r in rows:
        out.append("    { " + ", ".join(f"0x{b:02X}" for b in r) + " },")
    out.append("};")
    return "\n".join(out)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "gen", "adam")
    os.makedirs(out, exist_ok=True)
    tiles = tile_frames()
    cimg = Image(os.path.join(TMS, "cells.png"))
    wall = cell(pixels(cimg, 0, 0, 8, 8))
    panel = cell(pixels(cimg, 8, 0, 8, 8))

    arena = [panel, wall]               # codes 0, 1
    CH_WALL = 1

    def add(c):
        if c in arena:
            return arena.index(c)
        arena.append(c)
        return len(arena) - 1

    CH_FLOOR = add(cells_of(tiles["floor"][0], 16, 16)[0])
    anim_cells = []                     # [kind][phase][4]
    anim_base = {}
    static_codes = {}
    tile_chars = []
    for name in TILES:
        if name == "floor":
            static_codes[name] = [CH_FLOOR] * 4
        elif name == "trophy":
            static_codes[name] = [0] * 4
        elif name not in ANIMATED:
            static_codes[name] = [add(c) for c in cells_of(tiles[name][0], 16, 16)]
    base = len(arena)
    for k, name in enumerate(ANIMATED):
        n = len(tiles[name])
        ph = phases(name, n)
        per = []
        for p in range(4):
            per.append(cells_of(tiles[name][ph[p]], 16, 16))
        anim_cells.append(per)
        anim_base[name] = base + k * 4
        for j in range(4):
            arena.append(per[0][j])     # (not deduplicated: they change)
    for name in TILES:
        tile_chars.append(static_codes[name] if name in static_codes else
                          [anim_base[name] + j for j in range(4)])

    # the logo
    limg = Image(os.path.join(TMS, "logo.png"))
    assert limg.width == 216 and limg.height == 24, (limg.width, limg.height)
    lpx = pixels(limg, 0, 0, 216, 24)
    lcells, lmap = [], []
    for cy in range(3):
        row = []
        for cx in range(27):
            c = cell([lpx[cy * 8 + y][cx * 8:cx * 8 + 8] for y in range(8)])
            if c not in lcells:
                lcells.append(c)
            row.append(lcells.index(c))
        lmap.append(row)

    trophy = cells_of(tiles["trophy"][0], 16, 16)

    layers = sprite_layers()
    spr_a = [block16(a) for a, b in layers]
    spr_b = [block16(b) for a, b in layers]
    big_a, big_b = [], []
    for f in (SPRITES.index("win0"), SPRITES.index("win1")):
        da, db = doubled(layers[f][0]), doubled(layers[f][1])
        big_a.append([block16(da, qx * 16, qy * 16) for qy in (0, 1) for qx in (0, 1)])
        big_b.append([block16(db, qx * 16, qy * 16) for qy in (0, 1) for qx in (0, 1)])

    font = open(os.path.join(ROOT, "gfx", "namco.bin"), "rb").read()
    assert len(font) == 768

    h = ["/* GENERATED by tools/mkgfx_tms.py from gfx/tms/*.png: do not edit. */",
         "#ifndef GFXCODES_H", "#define GFXCODES_H", "#include <stdint.h>", ""]
    for i, name in enumerate(SPRITES):
        h.append(f"#define FR_{name.upper():8s} {i}")
    h += ["",
          f"#define N_FRAMES     {len(SPRITES)}",
          f"#define N_TILEKINDS      {len(TILES)}",
          f"#define ARENA_CELLS  {len(arena)}",
          f"#define N_ANIM       {len(ANIMATED)}",
          f"#define ANIM_BASE    {base}",
          "#define CH_PANEL     0",
          f"#define CH_WALL      {CH_WALL}",
          f"#define CH_FLOOR     {CH_FLOOR}",
          f"#define LOGO_W       27",
          f"#define LOGO_CELLS   {len(lcells)}",
          f"#define T_TROPHY     {TILES.index('trophy')}",
          ""]
    for k, name in enumerate(ANIMATED):
        h.append(f"#define AK_{name.upper():8s} {k}")
    h += ["",
          "extern const uint8_t arena_cells[ARENA_CELLS][16];",
          "extern const uint8_t anim_cells[N_ANIM][4][4][16];",
          "extern const uint8_t tile_chars[N_TILEKINDS][4];",
          "extern const uint8_t tile_anim_kind[N_TILEKINDS];   /* the AK_ index, 255: static */",
          "extern const uint8_t logo_cells[LOGO_CELLS][16];",
          "extern const uint8_t logo_map[3][LOGO_W];",
          "extern const uint8_t trophy_cells[4][16];",
          "extern const uint8_t spr_a[N_FRAMES][32];",
          "extern const uint8_t spr_b[N_FRAMES][32];",
          "extern const uint8_t spr_bigA[2][4][32];",
          "extern const uint8_t spr_bigB[2][4][32];",
          "extern const uint8_t font_gfx[768];",
          "", "#endif", ""]
    open(os.path.join(out, "gfxcodes.h"), "w").write("\n".join(h))

    c = ["/* GENERATED by tools/mkgfx_tms.py from gfx/tms/*.png and gfx/namco.bin: do not edit. */",
         "#include <stdint.h>", '#include "gfxcodes.h"', "",
         carray("arena_cells[ARENA_CELLS][16]", arena), ""]
    c.append("const uint8_t anim_cells[N_ANIM][4][4][16] = {")
    for per in anim_cells:
        c.append("  {")
        for ph in per:
            c.append("    {")
            for cc in ph:
                c.append("      { " + ", ".join(f"0x{b:02X}" for b in cc) + " },")
            c.append("    },")
        c.append("  },")
    c.append("};")
    c.append("")
    c.append(carray("tile_chars[N_TILEKINDS][4]", tile_chars))
    c.append("")
    c.append("const uint8_t tile_anim_kind[N_TILEKINDS] = { " +
             ", ".join(str(ANIMATED.index(n)) if n in ANIMATED else "255" for n in TILES) + " };")
    c.append("")
    c.append(carray("logo_cells[LOGO_CELLS][16]", lcells))
    c.append("")
    c.append(carray("logo_map[3][LOGO_W]", lmap))
    c.append("")
    c.append(carray("trophy_cells[4][16]", trophy))
    c.append("")
    c.append(carray("spr_a[N_FRAMES][32]", spr_a))
    c.append("")
    c.append(carray("spr_b[N_FRAMES][32]", spr_b))
    c.append("")
    for nm, big in (("spr_bigA", big_a), ("spr_bigB", big_b)):
        c.append(f"const uint8_t {nm}[2][4][32] = {{")
        for f in big:
            c.append("  {")
            for q in f:
                c.append("    { " + ", ".join(f"0x{b:02X}" for b in q) + " },")
            c.append("  },")
        c.append("};")
        c.append("")
    c.append("const uint8_t font_gfx[768] = {")
    for i in range(0, 768, 16):
        c.append("    " + ", ".join(f"0x{b:02X}" for b in font[i:i + 16]) + ",")
    c.append("};")
    c.append("")
    open(os.path.join(out, "gfxdata.c"), "w").write("\n".join(c))
    print(f"{out}: {len(arena)} arena cells ({len(ANIMATED)} animated kinds), {len(lcells)} logo cells, "
          f"{len(SPRITES)} sprite frames")


if __name__ == "__main__":
    main()

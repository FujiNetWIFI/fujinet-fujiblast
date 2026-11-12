#!/usr/bin/env python3
"""mkgfx_a78.py -- the Atari 7800 port's art: MARIA character blocks, the
bombers' REPL pages and the code tables (src/atari7800/DESIGN.md).

    python3 tools/mkgfx_a78.py [OUTDIR]       # default gen/atari7800

Writes repl.bin (30 pages), art_txt.bin, art_big.bin, art_gfx.bin (2K each),
art_anim.bin (A0..A3, 8K), gfxdata.c and gfxcodes.h. The art itself is
tools/drawart_a78.py's (the DHGR designs widened to 8x16 on the 7800 palette).
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "a78"))
import drawart_a78 as A      # noqa: E402
import drawart_cga as C      # noqa: E402
import drawart_dhgr as D     # noqa: E402
from artlayout import TILES, SPRITES  # noqa: E402

ROOT = os.path.join(HERE, "..")
SETS = 4
MAP_TILES = TILES[:18]

# P0..P3: colours 1-3 of each (DESIGN.md)
PAL03 = [(0x05, 0x0F, 0x0A), (0x26, 0x2A, 0x36), (0x4C, 0x5A, 0x75), (0x00, 0x1E, 0x9C)]
PC = {col: (p, i + 1) for p, row in enumerate(PAL03) for i, col in enumerate(row)}
assert len(PC) == 12


class Charset:
    """Even codes 0..254 (CW=1: two bytes a char), code 0 blank; a code's
    16 bytes (8 lines x 2) per set."""

    def __init__(self, name, nsets):
        self.name, self.nsets = name, nsets
        blank = bytes(16)
        self.codes = [tuple([blank] * nsets)]
        self.index = {self.codes[0]: 0}

    def add(self, per_set):
        key = tuple(per_set)
        if key not in self.index:
            if len(self.codes) >= 128:
                sys.exit(f"charset {self.name} full")
            self.index[key] = len(self.codes)
            self.codes.append(key)
        return self.index[key] * 2

    def data(self, s):
        out = bytearray(2048)
        for i, c in enumerate(self.codes):
            for k in range(8):
                out[(7 - k) * 256 + i * 2: (7 - k) * 256 + i * 2 + 2] = c[s][k * 2:k * 2 + 2]
        return bytes(out)


def px160b(col):
    """A pixel's (colour, palette-in-group); the floor/background is 0."""
    if col == A.FLOOR:
        return 0, 0
    c, p = PC[col][1], PC[col][0]
    return c, p


def cell_bytes(rows):
    """A 4x8 cell of 7800 colour bytes (or None) -> 16 bytes of 160B."""
    out = bytearray()
    for r in rows:
        for h in (0, 2):
            c0, p0 = px160b(r[h]) if r[h] is not None else (0, 0)
            c1, p1 = px160b(r[h + 1]) if r[h + 1] is not None else (0, 0)
            out.append(c0 << 6 | c1 << 4 | p0 << 2 | p1)
    return bytes(out)


def col_of(ch):
    return A.TILE_COL[ch]


def tile_cells(frame):
    """An 8x16 tile (strings of DHGR letters) -> its cells TL, TR, BL, BR of 7800 colours."""
    px = [[col_of(c) for c in r] for r in frame]
    return [[r[c * 4:c * 4 + 4] for r in px[h * 8:h * 8 + 8]] for h in (0, 1) for c in (0, 1)]


def tile_codes(cs, frames):
    out = []
    for k in range(4):
        out.append(cs.add([cell_bytes(tile_cells(frames[s % len(frames)])[k]) for s in range(cs.nsets)]))
    return out


def glyph_cell(ch):
    """A font3 glyph (3x7) as a 4x8 cell, white on the floor."""
    g = D.font3()[ord(ch) - 32]
    rows = []
    for y in range(8):
        row = [A.FLOOR] * 4
        if y < 7:
            for x in range(3):
                if g[y][x] == "#":
                    row[x] = 0x0F
        rows.append(row)
    return rows


def wall_cell():
    return [[col_of(c) for c in D.WALL[y % 4]] for y in range(8)]


def font_rows(i):
    """ASCII 32+i's 8 line bytes (Namco)."""
    g = C.glyph(C.namco(), chr(32 + i))
    return [int(r.replace(".", "0").replace("#", "1"), 2) for r in g]


def txt_block():
    out = bytearray(2048)
    for i in range(64):
        for k, b in enumerate(font_rows(i)):
            out[(7 - k) * 256 + i] = b
            out[(7 - k) * 256 + 64 + i] = b ^ 0xFF
    return bytes(out)


def big_block():
    out = bytearray(2048)
    for i in range(64):
        for k, b in enumerate(font_rows(i)):
            v = 0
            for x in range(8):
                v = v << 2 | (2 if b & (0x80 >> x) else 0)
            out[(7 - k) * 256 + 2 * i] = v >> 8
            out[(7 - k) * 256 + 2 * i + 1] = v & 0xFF
    return bytes(out)


def repl_pages():
    img = bytearray(30 * 256)
    fr = A.sprite_sets()
    for f, name in enumerate(SPRITES):
        rows = fr[name]
        assert len(rows) == 16 and all(len(r) == 8 for r in rows), name
        for layer in (0, 1):
            o = f * 2 + layer
            for L in range(16):
                vals = []
                for p in rows[L]:
                    if layer == 0:
                        vals.append({"#": 2, "+": 1}.get(p, 0))
                    else:
                        vals.append({"k": 1, "v": 2}.get(p, 0))
                for b in range(2):
                    v = 0
                    for x in range(4):
                        v = v << 2 | vals[b * 4 + x]
                    img[(22 - L) * 256 + 2 * o + b] = v
    return bytes(img)


def asm_h(vals, per=16):
    return ",\n".join("    " + ", ".join(f"0x{v:02X}" for v in vals[i:i + per]) for i in range(0, len(vals), per))


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "gen", "atari7800")
    os.makedirs(out_dir, exist_ok=True)
    tiles = A.tile_sets()

    arena = Charset("arena", SETS)
    tcodes = {n: tile_codes(arena, tiles[n]) for n in MAP_TILES}
    gl = {}
    gl["wall"] = arena.add([cell_bytes(wall_cell())] * SETS)
    gl["panel"] = 0
    for ch, nm in [(c, c) for c in "0123456789"] + [("X", "X"), ("/", "SLASH"), ("R", "R"), ("N", "N"), ("D", "D")]:
        gl["FONT_" + nm] = arena.add([cell_bytes(glyph_cell(ch))] * SETS)

    menu = Charset("gfx", 1)
    menu_wall = menu.add([cell_bytes(wall_cell())])
    trophy = tile_codes(menu, tiles["trophy"])
    lg = D.logo()
    lw = (len(lg[0]) + 3) // 4
    logo = []
    for r in range(3):
        row = []
        for c in range(lw):
            cells = [[col_of(ch) if A.TILE_COL.get(ch) is not None and ch != "b" else A.FLOOR
                      for ch in lg[r * 8 + y][c * 4:c * 4 + 4].ljust(4, "b")] for y in range(8)]
            row.append(menu.add([cell_bytes(cells)]))
        logo.append(row)

    open(os.path.join(out_dir, "repl.bin"), "wb").write(repl_pages())
    open(os.path.join(out_dir, "art_txt.bin"), "wb").write(txt_block())
    open(os.path.join(out_dir, "art_big.bin"), "wb").write(big_block())
    open(os.path.join(out_dir, "art_gfx.bin"), "wb").write(menu.data(0))
    open(os.path.join(out_dir, "art_anim.bin"), "wb").write(b"".join(arena.data(s) for s in range(SETS)))

    h = ["/* gfxcodes.h -- made by tools/mkgfx_a78.py; do not edit. */",
         "#ifndef GFXCODES_H", "#define GFXCODES_H", "#include <stdint.h>",
         "#define TXTB 0x6000", "#define BIGB 0x4800", "#define GFXB 0x4000",
         "#define A0B 0x4000", "#define A1B 0x4800", "#define A2B 0x5000", "#define A3B 0x5800",
         "#define CB_TXT 0x60", "#define CB_BIG 0x48", "#define CB_GFX 0x40", "#define CB_A0 0x40",
         "#define REPLB 0x8000"]
    for name, code in gl.items():
        h.append(f"#define CH_{name.upper():<9} 0x{code:02X}")
    h.append(f"#define CH_MENU_WALL 0x{menu_wall:02X}")
    h.append(f"#define LOGO_W {lw}")
    for i, name in enumerate(SPRITES):
        h.append(f"#define FR_{name.upper():<8} {i}")
    h.append(f"#define N_FRAMES {len(SPRITES)}")
    h += ["extern const uint8_t tile_chars[][4];     /* by map tile: TL, TR, BL, BR */",
          "extern const uint8_t logo_chars[3][LOGO_W];",
          "extern const uint8_t trophy_chars[4];",
          "extern const uint8_t pal_p0_3[12];          /* P0..P3 colours 1-3 (NTSC) */",
          "extern const uint8_t seat_pal[4][3];         /* P4..P7 colours 1-3 */",
          "#endif"]
    open(os.path.join(out_dir, "gfxcodes.h"), "w").write("\n".join(h) + "\n")

    c = ["/* gfxdata.c -- made by tools/mkgfx_a78.py; do not edit. */", '#include "gfxcodes.h"',
         "const uint8_t tile_chars[][4] = {"]
    for n in MAP_TILES:
        c.append("    { " + ", ".join(f"0x{b:02X}" for b in tcodes[n]) + " },   /* " + n + " */")
    c.append("};")
    c.append("const uint8_t logo_chars[3][LOGO_W] = {")
    for row in logo:
        c.append("  {\n" + asm_h(row, 16) + "\n  },")
    c.append("};")
    c.append("const uint8_t trophy_chars[4] = { " + ", ".join(f"0x{b:02X}" for b in trophy) + " };")
    c.append("const uint8_t pal_p0_3[12] = { " + ", ".join(f"0x{v:02X}" for row in PAL03 for v in row) + " };")
    c.append("const uint8_t seat_pal[4][3] = {")
    for light, shade in A.SEAT:
        c.append(f"    {{ 0x{shade:02X}, 0x{light:02X}, 0x{light:02X} }},")
    c.append("};")
    open(os.path.join(out_dir, "gfxdata.c"), "w").write("\n".join(c) + "\n")
    print(f"arena {len(arena.codes)} codes, gfx {len(menu.codes)} codes, {len(SPRITES)} sprite frames; logo {lw} cells wide")


if __name__ == "__main__":
    main()

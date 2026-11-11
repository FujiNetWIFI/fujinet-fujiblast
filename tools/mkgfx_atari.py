#!/usr/bin/env python3
"""mkgfx_atari.py -- gfx/*.png -> the Atari's charsets and sprites.

    python3 tools/mkgfx_atari.py OUTDIR      # writes OUTDIR/gfxdata.s, gfxcodes.h

Arena: four mode 4 charsets (1K each, segment CHARSETS), the same but for
the animated cells: a tile with n frames shows frame (set mod n). The VBI
turns the sets over (gfxa.s). A cell with yellow (PF3) in it is an inverse
character: its screen byte has bit 7 set. Menus: one more charset (MENUCH):
the logo and the trophy.

Players: 16 bytes a frame, and each frame's visor (the player's missile):
the rows it covers and its left offset (it is always 4 pixels, double
width).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from artlayout import ROLE_RGB, SPRITE_RGB, TILES, SPRITES, GLYPHS  # noqa: E402
from png import Image  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
RGB_ROLE = {v: k for k, v in ROLE_RGB.items()}
BITS = {".": 0, "w": 1, "k": 2, "o": 3, "y": 3}
SETS = 4

# Map tiles, in proto.h's order (T_FLOOR .. T_GLOVE).
MAP_TILES = TILES[:18]


def roles(img, x0, y0, w, h):
    out = []
    for y in range(h):
        row = ""
        for x in range(w):
            p = img.getpixel((x0 + x, y0 + y))[:3]
            if p not in RGB_ROLE:
                sys.exit(f"stray colour {p} at {x0 + x},{y0 + y}")
            row += RGB_ROLE[p]
        out.append(row)
    return out


def cell_bytes(rows):
    """A 4x8 cell -> 8 bytes, and whether it is inverse (has yellow)."""
    s = "".join(rows)
    if "o" in s and "y" in s:
        raise ValueError("a cell has both orange and yellow")
    data = []
    for r in rows:
        b = 0
        for p in r:
            b = b << 2 | BITS[p]
        data.append(b)
    return bytes(data), "y" in s


def cells_of(tile):
    """A 16x8 tile -> its four 4x8 cells: TL, TR, BL, BR."""
    return [[r[c * 4:c * 4 + 4] for r in tile[h * 8:h * 8 + 8]] for h in (0, 1) for c in (0, 1)]


class Charset:
    """Codes 0..127 over SETS sets; a code's content per set."""

    def __init__(self, sets):
        self.sets = sets
        self.codes = []            # [ (bytes per set) ]
        self.index = {}

    def add(self, per_set):
        key = tuple(per_set)
        if key not in self.index:
            if len(self.codes) >= 128:
                sys.exit("charset full")
            self.index[key] = len(self.codes)
            self.codes.append(key)
        return self.index[key]

    def data(self, s):
        out = bytearray(1024)
        for i, c in enumerate(self.codes):
            out[i * 8:i * 8 + 8] = c[s]
        return bytes(out)


def frames_of(img, i):
    """The tile in row i: its frames (blank columns past the first don't count)."""
    fr = [roles(img, f * 8, i * 16, 8, 16) for f in range(SETS)]
    n = SETS
    while n > 1 and all(set(r) == {"."} for r in fr[n - 1]) and TILES[i] != "floor":
        n -= 1
    return fr[:n]


def tile_codes(cs, frames):
    """Screen bytes TL, TR, BL, BR for a tile animated over frames."""
    out = []
    for k in range(4):
        per_set, inv = [], None
        for s in range(cs.sets):
            b, v = cell_bytes(cells_of(frames[s % len(frames)])[k])
            if inv is not None and v != inv:
                sys.exit("a cell is inverse in one frame and not another")
            inv = v
            per_set.append(b)
        out.append(cs.add(per_set) | (0x80 if inv else 0))
    return out


def asm_bytes(data, per=16):
    lines = []
    for i in range(0, len(data), per):
        lines.append("        .byte   " + ",".join(f"${b:02X}" for b in data[i:i + per]))
    return "\n".join(lines)


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "gen", "atari")
    os.makedirs(out_dir, exist_ok=True)
    tiles_img = Image(os.path.join(ROOT, "gfx", "tiles.png"))
    glyph_img = Image(os.path.join(ROOT, "gfx", "glyphs.png"))
    logo_img = Image(os.path.join(ROOT, "gfx", "logo.png"))
    spr_img = Image(os.path.join(ROOT, "gfx", "sprites.png"))

    arena = Charset(SETS)
    blank = bytes(8)
    arena.add([blank] * SETS)               # code 0: the floor's cell
    tcodes = {}
    for i, name in enumerate(TILES):
        if name == "trophy":
            continue
        tcodes[name] = tile_codes(arena, frames_of(tiles_img, i))
    gcodes = {}
    for i, name in enumerate(GLYPHS):
        b, inv = cell_bytes(roles(glyph_img, i * 4, 0, 4, 8))
        gcodes[name] = arena.add([b] * SETS) | (0x80 if inv else 0)

    menu = Charset(1)
    menu.add([blank])
    trophy = tile_codes(menu, frames_of(tiles_img, TILES.index("trophy")))
    wall = cell_bytes(roles(glyph_img, GLYPHS.index("wall") * 4, 0, 4, 8))[0]
    menu_wall = menu.add([wall])
    lw = logo_img.width // 4
    logo = []
    for r in range(3):
        row = []
        for c in range(lw):
            b, inv = cell_bytes(roles(logo_img, c * 4, r * 8, 4, 8))
            row.append(menu.add([b]) | (0x80 if inv else 0))
        logo.append(row)

    # Players.
    frames, visor_rows, visor_x = [], [], []
    for i, name in enumerate(SPRITES):
        body, vis, vx = bytearray(16), bytearray(16), 0
        for y in range(16):
            for x in range(8):
                p = spr_img.getpixel((i * 8 + x, y))
                if p[3] == 0:
                    continue
                if p == SPRITE_RGB["#"]:
                    body[y] |= 0x80 >> x
                elif p == SPRITE_RGB["v"]:
                    vis[y] = 1
                    vx = x if not any(vis[:y]) else min(vx, x)
                else:
                    sys.exit(f"sprite {name}: stray colour {p}")
        frames.append(bytes(body))
        visor_rows.append(bytes(vis))
        visor_x.append(vx)

    s = ["; gfxdata.s -- made by tools/mkgfx_atari.py from gfx/*.png; do not edit.",
         "        .export _tile_chars, _logo_chars, _trophy_chars, _spr_frames, _spr_visor, _spr_visor_x",
         "",
         '        .segment "CHARSETS"']
    for k in range(SETS):
        s.append(f"; arena set {k}")
        s.append(asm_bytes(arena.data(k)))
    s += ['', '        .segment "MENUCH"', asm_bytes(menu.data(0)), "", "        .rodata",
          "_tile_chars:"]
    for name in MAP_TILES:
        s.append(f"        .byte   {','.join(f'${b:02X}' for b in tcodes[name])}     ; {name}")
    s.append("_logo_chars:")
    for row in logo:
        s.append(asm_bytes(bytes(row), 32))
    s.append("_trophy_chars:")
    s.append(asm_bytes(bytes(trophy)))
    s.append("_spr_frames:")
    for i, f in enumerate(frames):
        s.append(asm_bytes(f) + f"     ; {SPRITES[i]}")
    s.append("_spr_visor:")
    for v in visor_rows:
        s.append(asm_bytes(v))
    s.append("_spr_visor_x:")
    s.append(asm_bytes(bytes(visor_x)))
    open(os.path.join(out_dir, "gfxdata.s"), "w").write("\n".join(s) + "\n")

    h = ["/* gfxcodes.h -- made by tools/mkgfx_atari.py; do not edit. */",
         "#ifndef GFXCODES_H", "#define GFXCODES_H", "#include <stdint.h>"]
    for name, code in gcodes.items():
        cname = {"/": "SLASH"}.get(name[4:], name[4:]) if name.startswith("font") else name
        prefix = "CH_FONT_" if name.startswith("font") else "CH_"
        h.append(f"#define {prefix}{cname.upper():<8} 0x{code:02X}")
    h.append(f"#define CH_MENU_WALL 0x{menu_wall:02X}")
    h.append(f"#define LOGO_W {lw}")
    for i, name in enumerate(SPRITES):
        h.append(f"#define FR_{name.upper():<8} {i}")
    h.append(f"#define N_FRAMES {len(SPRITES)}")
    h += ["extern const uint8_t tile_chars[][4];     /* by map tile: TL, TR, BL, BR */",
          "extern const uint8_t logo_chars[3][LOGO_W];",
          "extern const uint8_t trophy_chars[4];",
          "extern const uint8_t spr_frames[][16];",
          "extern const uint8_t spr_visor[][16];",
          "extern const uint8_t spr_visor_x[];",
          "#endif"]
    open(os.path.join(out_dir, "gfxcodes.h"), "w").write("\n".join(h) + "\n")
    print(f"arena: {len(arena.codes)} codes; menu: {len(menu.codes)} codes; {len(frames)} sprite frames")


if __name__ == "__main__":
    main()

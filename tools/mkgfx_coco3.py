#!/usr/bin/env python3
"""mkgfx_coco3.py -- the Adam's art (gfx/tms) and the bombers (gfx/cga) -> the
CoCo 3's data: 320x192 in 16 colours, 4 bits a pixel, the left pixel in the
high nibble.

    python3 tools/mkgfx_coco3.py OUTDIR [--preview DIR]    # OUTDIR/gfxdata.c, gfxcodes.h

The GIME's palette is 16 of 64 colours (2 bits each of red, green, blue: the
codes below, R1 G1 B1 R0 G0 B0), so each of the Adam's 15 colours is one of
the 64 nearest it and the sheet is used pixel for pixel. A second table
gives the composite monitor's codes (intensity, hue).

  tile_gfx    tile frames, 16 lines of 8 bytes
  tile_anim   per tile, the frame for each of the 4 animation phases
  cell_gfx    the wall and the panel: 8 lines of 8 bytes (16 pixels)
  logo_gfx    24 lines of 108 bytes
  spr_data    the bombers in roles (0 eyes, 1 body, 2 trim, 3 visor, a nibble
              each), 16 lines of 9 bytes at each of the 2 pixel phases, then
              the masks (F where clear): 16 lines of 18 bytes a phase
  pal_rgb, pal_cmp  the 16 palette codes
  font_gfx    the Namco font, ASCII 32-127, 8 bytes a glyph, bit 7 left
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from artlayout import TILES, SPRITES  # noqa: E402
from png import Image  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
TMS = os.path.join(ROOT, "gfx", "tms")
CGA = os.path.join(ROOT, "gfx", "cga")

# the palette: the Adam's colours (drawart_tms.py), as GIME RGB codes (levels
# 0 85 170 255 a channel) and composite codes; index 15 is spare
ART = [
    ("black", (0, 0, 0), 0x00, 0x00),
    ("floor", (33, 176, 59), 0x10, 0x1C),          # green
    ("floor2", (33, 200, 66), 0x17, 0x2C),         # brighter green
    ("floor3", (94, 220, 120), 0x1E, 0x3C),        # light green
    ("blue", (84, 85, 237), 0x0F, 0x12),
    ("ltblue", (125, 118, 252), 0x39, 0x22),
    ("dkred", (212, 82, 77), 0x24, 0x06),
    ("red", (252, 85, 84), 0x27, 0x26),
    ("pink", (255, 121, 120), 0x3C, 0x35),
    ("dkyellow", (212, 193, 84), 0x37, 0x29),
    ("tan", (230, 206, 128), 0x3E, 0x39),
    ("magenta", (201, 91, 186), 0x2A, 0x25),
    ("cyan", (66, 235, 245), 0x1F, 0x1B),
    ("grey", (204, 204, 204), 0x38, 0x20),
    ("white", (255, 255, 255), 0x3F, 0x30),
    ("spare", (0, 0, 0), 0x00, 0x00),
]
INDEX = {rgb: i for i, (_, rgb, _, _) in enumerate(ART) if i < 15}
LEVEL = (0, 85, 170, 255)


def show(i):
    """A palette entry as a monitor shows it."""
    c = ART[i][2]
    r = LEVEL[((c >> 5) & 1) * 2 + ((c >> 2) & 1)]
    g = LEVEL[((c >> 4) & 1) * 2 + ((c >> 1) & 1)]
    b = LEVEL[((c >> 3) & 1) * 2 + (c & 1)]
    return (r, g, b)


CGA_ROLE = {(0, 0, 0): 0, (255, 255, 255): 1, (255, 85, 85): 2, (85, 255, 255): 3}
# the bombers' colours (palette indexes): body, trim, visor; the eyes are black
SEAT_COL = [(14, 7, 12), (7, 14, 12), (12, 14, 7), (10, 14, 7)]
POWERUPS = ("fire", "bombup", "speed", "kick", "glove")


def px(img, x, y):
    p = tuple(img.getpixel((x, y)))
    if p[3] < 128 if len(p) > 3 else False:
        return None
    return INDEX[p[:3]]


def pack(row):
    return [row[i] << 4 | row[i + 1] for i in range(0, len(row), 2)]


def tiles():
    img = Image(os.path.join(TMS, "tiles.png"))
    frames, anim = [], []
    for i, name in enumerate(TILES):
        mine = []
        for f in range(4):
            rows = [[px(img, f * 16 + x, i * 16 + y) for x in range(16)] for y in range(16)]
            if f and all(all(v == 1 for v in r) for r in rows):
                break               # an empty frame (the sheet leaves unused ones floor): no more
            mine.append(len(frames))
            frames.append([b for r in rows for b in pack(r)])
        n = len(mine)
        if n == 1:
            order = [0, 0, 0, 0]
        elif n == 2:
            order = [0, 0, 1, 1] if name in POWERUPS else [0, 1, 0, 1]
        elif n == 4:
            order = [0, 1, 2, 3]
        else:
            raise ValueError(f"{name}: {n} frames")
        anim.append([mine[k] for k in order])
    return frames, anim


def cells():
    """The wall and the panel: 8x8 pixels, 8 lines of 4 bytes."""
    img = Image(os.path.join(TMS, "cells.png"))
    return [[b for y in range(8) for b in pack([px(img, i * 8 + x, y) for x in range(8)])] for i in range(2)]


def logo():
    img = Image(os.path.join(TMS, "logo.png"))
    return [pack([px(img, x, y) for x in range(216)]) for y in range(24)]


def sprites():
    """The bombers, 16 pixels wide, on 2-pixel boundaries: a line is 8 bytes
    of roles (a nibble each) and 8 of mask (F where clear)."""
    img = Image(os.path.join(CGA, "sprites.png"))
    out = []
    for i in range(len(SPRITES)):
        data = []
        for y in range(16):
            row, m = [], []
            for x in range(16):
                p = tuple(img.getpixel((i * 16 + x, y)))
                row.append(CGA_ROLE[p[:3]] if p[3] >= 128 else 0)
                m.append(0 if p[3] >= 128 else 15)
            data += pack(row) + pack(m)
        out.append(data)
    return out


def preview(out, frames, anim, lg, spr):
    from PIL import Image as PI
    os.makedirs(out, exist_ok=True)
    S = 4

    def unpack(data, w):
        rows = []
        for y in range(len(data) // w):
            r = []
            for b in data[y * w:(y + 1) * w]:
                r += [b >> 4, b & 15]
            rows.append(r)
        return rows

    def paint(im, x0, y0, rows, tr=None):
        for y, r in enumerate(rows):
            for x, v in enumerate(r):
                if tr is not None and tr[y][x]:
                    continue
                for dx in range(S):
                    for dy in range(S):
                        im.putpixel((x0 * S + x * S + dx, y0 * S + y * S + dy), show(v))

    im = PI.new("RGB", (17 * S * 12, 17 * S * ((len(frames) + 11) // 12)), (0, 0, 0))
    for i, f in enumerate(frames):
        paint(im, (i % 12) * 17, (i // 12) * 17, unpack(f, 8))
    im.save(os.path.join(out, "tiles.png"))
    sheet = PI.new("RGB", (len(SPRITES) * 17 * S, 4 * 17 * S), show(1))
    for seat in range(4):
        for fr in range(len(SPRITES)):
            d = spr[fr]
            rr = unpack([d[y * 16 + k] for y in range(16) for k in range(8)], 8)
            mm = unpack([d[y * 16 + 8 + k] for y in range(16) for k in range(8)], 8)
            body, trim, visor = SEAT_COL[seat]
            cm = {0: 0, 1: body, 2: trim, 3: visor}
            paint(sheet, fr * 17, seat * 17, [[cm[v] for v in r] for r in rr], [[1 if m else 0 for m in r] for r in mm])
    sheet.save(os.path.join(out, "sprites.png"))
    lim = PI.new("RGB", (216 * S, 24 * S), (0, 0, 0))
    paint(lim, 0, 0, unpack([b for r in lg for b in r], 108))
    lim.save(os.path.join(out, "logo.png"))


def carray(name, rows):
    out = [f"const uint8_t {name} = {{"]
    for r in rows:
        out.append("    { " + ", ".join(f"0x{b:02X}" for b in r) + " },")
    out.append("};")
    return "\n".join(out)


def main():
    args = sys.argv[1:]
    prev = None
    if "--preview" in args:
        k = args.index("--preview")
        prev = args[k + 1]
        del args[k:k + 2]
    out = args[0] if args else os.path.join(ROOT, "gen", "coco3")
    os.makedirs(out, exist_ok=True)
    frames, anim = tiles()
    cell = cells()
    lg = logo()
    spr = sprites()
    font = open(os.path.join(ROOT, "gfx", "namco.bin"), "rb").read()
    assert len(font) == 768

    def flat(name, data, per=16):
        o = [f"const uint8_t {name} = {{"]
        for i in range(0, len(data), per):
            o.append("    " + ", ".join(f"0x{b:02X}" for b in data[i:i + per]) + ",")
        o.append("};")
        return "\n".join(o)

    h = ["/* GENERATED by tools/mkgfx_coco3.py: do not edit. */",
         "#ifndef GFXCODES_H", "#define GFXCODES_H", "#include <stdint.h>", ""]
    for i, name in enumerate(SPRITES):
        h.append(f"#define FR_{name.upper():8s} {i}")
    h += ["", f"#define N_SPR_FRAMES {len(SPRITES)}", f"#define N_TFRAMES    {len(frames)}",
          f"#define T_TROPHY     {TILES.index('trophy')}", f"#define N_TGFX       {len(TILES)}",
          "#define CELL_WALL    0", "#define CELL_PANEL   1", "#define LOGO_BYTES   108", "#define LOGO_LINES   24",
          "",
          "extern const uint8_t tile_gfx[N_TFRAMES][128];     /* art2: block 7 at $E000 */",
          "extern const uint8_t spr_data[N_SPR_FRAMES][256];  /* art: block 6 at $E000 */",
          "extern const uint8_t font_gfx[768];                /* art */",
          "extern const uint8_t tile_anim[N_TGFX][4];",
          "extern const uint8_t cell_gfx[2][32];",
          "extern const uint8_t logo_gfx[LOGO_LINES][LOGO_BYTES];",
          "extern const uint8_t pal_rgb[16], pal_cmp[16];",
          "", "#endif", ""]
    open(os.path.join(out, "gfxcodes.h"), "w").write("\n".join(h))
    hdr = ["/* GENERATED by tools/mkgfx_coco3.py: do not edit. */", "#include <stdint.h>", '#include "gfxcodes.h"', ""]
    # the high core: what is read in a few places; block 6 (art): the bombers and the font; block 7 (art2): the tiles
    open(os.path.join(out, "gfxdata.c"), "w").write("\n".join(hdr + [
        carray("tile_anim[N_TGFX][4]", anim), "",
        carray("cell_gfx[2][32]", cell), "",
        carray("logo_gfx[LOGO_LINES][LOGO_BYTES]", lg), "",
        flat("pal_rgb[16]", [a[2] for a in ART]), "",
        flat("pal_cmp[16]", [a[3] for a in ART]), ""]))
    open(os.path.join(out, "gfxspr.c"), "w").write("\n".join(hdr + [
        carray("spr_data[N_SPR_FRAMES][256]", spr), "", flat("font_gfx[768]", font), ""]))
    open(os.path.join(out, "gfxtile.c"), "w").write("\n".join(hdr + [
        carray("tile_gfx[N_TFRAMES][128]", frames), ""]))
    print(f"{out}: {len(frames)} tile frames, {len(SPRITES)} sprite frames")
    if prev:
        preview(prev, frames, anim, lg, spr)


if __name__ == "__main__":
    main()

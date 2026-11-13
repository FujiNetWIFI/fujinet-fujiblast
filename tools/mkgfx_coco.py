#!/usr/bin/env python3
"""mkgfx_coco.py -- the Atari art and the CGA bombers -> the CoCo 1/2's data
(PMODE 4 artifact colour).

    python3 tools/mkgfx_coco.py OUTDIR            # writes OUTDIR/gfxdata.c, gfxcodes.h
    python3 tools/mkgfx_coco.py OUTDIR --preview DIR   # and PNGs of how it looks (needs Pillow)

A PMODE 4 screen is 256 dots by 192 lines, 1 bit a dot. With artifact
colour two dots make a colour pixel of four colours (the second bit pair
01 / 10 are blue and orange, or the other way round on some machines, 11
white, 00 black): 128 pixels across, 4 to a byte, the left one in the top
two bits, exactly the Atari's cell (4 colour clocks, 8 lines). So the
Atari's art is native here: a tile is 8 pixels (2 bytes) by 16 lines.

  tile_gfx    tile frames, 16 lines of 2 bytes
  tile_anim   per tile, the frame for each of the 4 animation phases
  cell_gfx    the arena's wall and the side panel's: 8 lines of 1 byte
  logo_gfx    24 lines of 27 bytes (108 pixels)
  spr_data    the bombers, a frame at each of the 4 pixel phases: 16 lines of
              3 bytes in roles (0 eyes, 1 body, 2 trim, 3 visor), then the 3
              bytes of the mask (11 where clear: the background kept)
  font3_gfx   text: 64 glyphs (ASCII 32-95), 8 lines of a byte (3 pixels, a gap)
  font5_gfx   the HUD's: 8 lines of a 6-bit mask (5 pixels, a gap)
  font_gfx    the Namco font, ASCII 32-127, 8 bytes a glyph, bit 7 left

Colours: 0 black, 1 blue, 2 orange, 3 white (the machine's artifact phase
decides which of 1 and 2 is which; src/coco/gfx.c asks the player).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from artlayout import ROLE_RGB, TILES, SPRITES  # noqa: E402
from png import Image  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

# the Atari tile roles -> colours (floor black, so the field is dark)
ROLE_COL = {".": 0, "w": 3, "k": 1, "o": 2, "y": 3}
# per tile name: roles that differ (the flames are orange round white)
TILE_COL = {
    "flameC": {"y": 2}, "flameH": {"y": 2}, "flameV": {"y": 2},
    "flameL": {"y": 2}, "flameR": {"y": 2}, "flameU": {"y": 2}, "flameD": {"y": 2},
    "bomb": {"y": 2},
    "dud": {"y": 2},
}
POWERUPS = ("fire", "bombup", "speed", "kick", "glove")

# the CGA sprites' roles: black eyes, white body, red trim, cyan visor
CGA_ROLE = {(0, 0, 0): 0, (255, 255, 255): 1, (255, 85, 85): 2, (85, 255, 255): 3}


def role_of(p):
    return min(ROLE_RGB.items(), key=lambda kv: sum((a - b) ** 2 for a, b in zip(kv[1], p[:3])))[0]


def pack(row):
    """Pixel values (a multiple of 4) -> bytes, the left pixel on top."""
    out = []
    for i in range(0, len(row), 4):
        b = 0
        for p in row[i:i + 4]:
            b = b << 2 | p
        out.append(b)
    return out


def tiles():
    img = Image(os.path.join(ROOT, "gfx", "tiles.png"))
    frames, anim = [], []
    for i, name in enumerate(TILES):
        cmap = dict(ROLE_COL)
        cmap.update(TILE_COL.get(name, {}))
        mine = []
        for f in range(4):
            px = [[cmap[role_of(img.getpixel((f * 8 + x, i * 16 + y)))] for x in range(8)] for y in range(16)]
            if f and not any(any(r) for r in px):
                break               # an empty column: no more frames
            mine.append(len(frames))
            frames.append([b for r in px for b in pack(r)])
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
    """The wall and the panel: the Atari's 4x8 cells (light on dark)."""
    img = Image(os.path.join(ROOT, "gfx", "glyphs.png"))
    out = []
    for i in range(2):
        rows = []
        for y in range(8):
            px = [3 if role_of(img.getpixel((i * 4 + x, y))) == "w" else 1 for x in range(4)]
            rows += pack(px)
        out.append(rows)
    return out


def logo():
    img = Image(os.path.join(ROOT, "gfx", "logo.png"))
    assert (img.width, img.height) == (108, 24), (img.width, img.height)
    cmap = {".": 0, "w": 3, "k": 1, "o": 2, "y": 3}
    return [pack([cmap[role_of(img.getpixel((x, y)))] for x in range(108)]) for y in range(24)]


def sprites():
    """The CGA bombers, 16 pixels wide, squeezed to 8: of two pixels the one
    that matters most stays (eyes, visor, trim, body), then each frame
    shifted to the four pixel phases of a byte."""
    img = Image(os.path.join(ROOT, "gfx", "cga", "sprites.png"))
    prio = {0: 3, 3: 2, 2: 1, 1: 0}         # eyes first, then visor, trim, body
    roles, masks = [], []
    for i in range(len(SPRITES)):
        grid = []                           # 16 lines of 8: (role, opaque)
        for y in range(16):
            row = []
            for x in range(8):
                a = img.getpixel((i * 16 + 2 * x, y))
                b = img.getpixel((i * 16 + 2 * x + 1, y))
                cand = [CGA_ROLE[p[:3]] for p in (a, b) if p[3] >= 128]
                row.append((max(cand, key=lambda r: prio[r]), 1) if cand else (0, 0))
            grid.append(row)
        for phase in range(4):
            rr, mm = [], []
            for row in grid:
                line = [(0, 0)] * phase + row
                line += [(0, 0)] * (12 - len(line))
                rr += pack([p[0] for p in line])
                mm += pack([0 if p[1] else 3 for p in line])
            roles.append([b for y in range(16) for b in rr[y * 3:y * 3 + 3] + mm[y * 3:y * 3 + 3]])
            masks.append(mm)
    return roles, masks


def font_masks(name, pitch, w):
    """gfx/hgr/<name> (the Apple's): 64 glyphs (ASCII 32-95), pitch pixels
    apart, 7 lines (+ a blank 8th); the first w pixels of each as masks,
    the leftmost in the top bit."""
    img = Image(os.path.join(ROOT, "gfx", "hgr", name))
    out = []
    for i in range(64):
        rows = []
        for y in range(8):
            b = 0
            for x in range(w):
                if y < 7 and img.getpixel((i * pitch + x, y))[:3] == (255, 255, 255):
                    b |= 1 << (w - 1 - x)
            rows.append(b)
        out.append(rows)
    return out


def carray(name, rows):
    out = [f"const uint8_t {name} = {{"]
    for r in rows:
        out.append("    { " + ", ".join(f"0x{b:02X}" for b in r) + " },")
    out.append("};")
    return "\n".join(out)


def flat(name, data):
    out = [f"const uint8_t {name} = {{"]
    for i in range(0, len(data), 16):
        out.append("    " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",")
    out.append("};")
    return "\n".join(out)


# ---- the preview: colours as a CoCo shows them (NTSC, phase 1) ----------------
SHOW = {0: (0, 0, 0), 1: (40, 90, 255), 2: (255, 120, 20), 3: (255, 255, 255)}
SEAT_ROLES = [(3, 2, 1), (2, 3, 1), (1, 3, 3), ("s", 3, 3)]     # body, trim, visor


def preview(out, frames, anim, cell, lg, roles, masks):
    from PIL import Image as PI
    os.makedirs(out, exist_ok=True)
    S = 6

    def unpack(data, w):
        rows = []
        for y in range(len(data) // w):
            r = []
            for b in data[y * w:(y + 1) * w]:
                r += [b >> 6 & 3, b >> 4 & 3, b >> 2 & 3, b & 3]
            rows.append(r)
        return rows

    def paint(im, x0, y0, rows, tr=None):
        for y, r in enumerate(rows):
            for x, v in enumerate(r):
                if tr is not None and tr[y][x]:
                    continue
                for dx in range(2 * S):
                    for dy in range(S):
                        im.putpixel((x0 * 2 * S + x * 2 * S + dx, y0 * S + y * S + dy), SHOW[v])

    im = PI.new("RGB", (9 * 2 * S * 10, 17 * S * ((len(frames) + 9) // 10)), SHOW[0])
    names = []
    for i, f in enumerate(frames):
        paint(im, (i % 10) * 9, (i // 10) * 17, unpack(f, 2))
    im.save(os.path.join(out, "tiles.png"))

    # the bombers, each seat, the walking frames
    sheet = PI.new("RGB", (len(SPRITES) * 10 * 2 * S, 4 * 17 * S), SHOW[0])
    for seat in range(4):
        for fr in range(len(SPRITES)):
            ph = 0
            d = roles[fr * 4 + ph]
            rr = unpack([d[y * 6 + k] for y in range(16) for k in range(3)], 3)
            mm = unpack([d[y * 6 + 3 + k] for y in range(16) for k in range(3)], 3)
            body, trim, visor = SEAT_ROLES[seat]
            cm = {0: 0, 1: body, 2: trim, 3: visor}
            rows = []
            for y, r in enumerate(rr):
                b = cm[1]
                if b == "s":
                    b = 2 if y % 2 else 3
                rows.append([{0: 0, 1: b, 2: trim, 3: visor}[v] for v in r])
            paint(sheet, fr * 10, seat * 17, rows, [[1 if m == 3 else 0 for m in r] for r in mm])
    sheet.save(os.path.join(out, "sprites.png"))
    lim = PI.new("RGB", (108 * 2 * S, 24 * S), SHOW[0])
    paint(lim, 0, 0, unpack([b for r in lg for b in r], 27))
    lim.save(os.path.join(out, "logo.png"))


def main():
    args = [a for a in sys.argv[1:]]
    prev = None
    if "--preview" in args:
        k = args.index("--preview")
        prev = args[k + 1]
        del args[k:k + 2]
    out = args[0] if args else os.path.join(ROOT, "gen", "coco")
    os.makedirs(out, exist_ok=True)
    frames, anim = tiles()
    cell = cells()
    lg = logo()
    roles, masks = sprites()
    font = open(os.path.join(ROOT, "gfx", "namco.bin"), "rb").read()
    assert len(font) == 768
    # text rows: 3 pixels and a gap, as colour pixels (11 where lit); the HUD: 5 and a gap
    f3 = [[sum(3 << (6 - 2 * x) for x in range(4) if m >> (3 - x) & 1) for m in g] for g in font_masks("font3.png", 4, 4)]
    f5 = font_masks("font5.png", 7, 6)

    h = ["/* GENERATED by tools/mkgfx_coco.py: do not edit. */",
         "#ifndef GFXCODES_H", "#define GFXCODES_H", "#include <stdint.h>", ""]
    for i, name in enumerate(SPRITES):
        h.append(f"#define FR_{name.upper():8s} {i}")
    h += ["",
          f"#define N_SPR_FRAMES {len(SPRITES)}",
          f"#define N_TFRAMES    {len(frames)}",
          f"#define T_TROPHY     {TILES.index('trophy')}",
          f"#define N_TGFX       {len(TILES)}",
          "#define CELL_WALL    0",
          "#define CELL_PANEL   1",
          "#define LOGO_BYTES   27",
          "#define LOGO_LINES   24",
          "",
          "extern const uint8_t tile_gfx[N_TFRAMES][32];",
          "extern const uint8_t tile_anim[N_TGFX][4];",
          "extern const uint8_t cell_gfx[2][8];",
          "extern const uint8_t logo_gfx[LOGO_LINES][LOGO_BYTES];",
          "extern const uint8_t spr_data[N_SPR_FRAMES * 4][96];",
          "extern const uint8_t font_gfx[768];",
          "extern const uint8_t font3_gfx[64][8];",
          "extern const uint8_t font5_gfx[64][8];",
          "", "#endif", ""]
    open(os.path.join(out, "gfxcodes.h"), "w").write("\n".join(h))

    c = ["/* GENERATED by tools/mkgfx_coco.py from gfx/*.png and gfx/namco.bin: do not edit. */",
         "#include <stdint.h>", '#include "gfxcodes.h"', "",
         carray("tile_gfx[N_TFRAMES][32]", frames), "",
         carray("tile_anim[N_TGFX][4]", anim), "",
         carray("cell_gfx[2][8]", cell), "",
         carray("logo_gfx[LOGO_LINES][LOGO_BYTES]", lg), "",
         carray("font3_gfx[64][8]", f3), "",
         carray("font5_gfx[64][8]", f5), ""]
    open(os.path.join(out, "gfxdata.c"), "w").write("\n".join(c))
    # (its own file: the Makefile puts it in the art section, the rest in the high core)
    open(os.path.join(out, "gfxspr.c"), "w").write("\n".join([
        "/* GENERATED by tools/mkgfx_coco.py: do not edit. */", "#include <stdint.h>", '#include "gfxcodes.h"', "",
        carray("spr_data[N_SPR_FRAMES * 4][96]", roles), "",
        flat("font_gfx[768]", font), ""]))
    print(f"{out}: {len(frames)} tile frames, {len(SPRITES)} sprite frames")
    if prev:
        preview(prev, frames, anim, cell, lg, roles, masks)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""drawart_intv.py -- FujiBlast's art for the Intellivision, written out as
gfx/intv/*.png (the INTV port's sheet; tools/mkgfx_intv.py converts it).

An Intellivision card is 8x8 and as big on the screen as the Atari's 8x16
tile, so a tile is a card, in two colours (foreground/background mode:
tools/artlayout_intv.py has each tile's). The designs are the master
sheet's (tools/drawart.py) redrawn for that. The bombers are the master's
own 8x16 frames: a MOB with twice the rows (DOUBLEY) in a card's height,
one colour; the visor is a second MOB. All of it is original art.

    python3 tools/drawart_intv.py              # rewrites gfx/intv/*.png
    python3 tools/drawart_intv.py --preview D  # also mock screens into D
"""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drawart  # noqa: E402
from artlayout_intv import (PAL, TILES, BODIES, VISORS, LOGO_TEXT, LOGO_TOP, LOGO_BOTTOM, SEAT_BODY,  # noqa: E402
                            VISOR, FLOOR, PANEL, DUD_FG, BLACK, WHITE, YELLOW, GREY, RED, CYAN)

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

# ---- tiles (8x8, # the foreground) ------------------------------------------

PILLAR = [
    ".......#",
    ".......#",
    "..###..#",
    "..#....#",
    "..#....#",
    ".......#",
    ".......#",
    "########",
]
BLOCK = [
    "#.......",
    "#.......",
    "########",
    "....#...",
    "....#...",
    "########",
    "#.......",
    "########",
]
WALL = [
    "...#....",
    "########",
    "#.......",
    "########",
    "....#...",
    "########",
    "#.......",
    "########",
]
BURN = [[
    ".#...#..",
    "##..##..",
    "########",
    "...#....",
    "...#....",
    "########",
    "#.......",
    "########",
], [
    "..#...#.",
    "..##..##",
    "########",
    "...#....",
    "...#....",
    "########",
    "#.......",
    "########",
]]
BOMB_BIG = [
    "........",
    "..####..",
    ".##.###.",
    ".#.####.",
    ".######.",
    ".######.",
    "..####..",
    "........",
]
BOMB_SMALL = [
    "........",
    "........",
    "..####..",
    "..#.##..",
    "..####..",
    "...##...",
    "........",
]
SPARK = [["....#...", "...#...."], ["......#.", ".....#.."]]


def bomb(spark, small):
    top = SPARK[spark]
    if small:
        return ["........", top[0], top[1]] + BOMB_SMALL[2:7]
    return [top[0], merge(top[1], BOMB_BIG[1])] + BOMB_BIG[2:]


def merge(a, b):
    return "".join("#" if "#" in (x, y) else "." for x, y in zip(a, b))


DUD_BODY = [
    "..####..",
    ".######.",
    ".######.",
    ".######.",
    "..####..",
]
PUFF = [["...#....", "..#.#...", "........"], ["..#.....", ".#..#...", "........"],
        [".#..#...", "........", "........"], ["........", "...#....", "........"]]


def dud(i):
    return PUFF[i] + DUD_BODY


FLAMES = {
    "C": [[
        "...##...",
        "..####..",
        ".######.",
        "########",
        "########",
        ".######.",
        "..####..",
        "...##...",
    ], [
        "...##...",
        "...##...",
        "..####..",
        "########",
        "########",
        "..####..",
        "...##...",
        "...##...",
    ]],
    "H": [["........", "........", "########", "########", "########", "########", "........", "........"],
          ["........", "........", "........", "########", "########", "........", "........", "........"]],
    "L": [["........", "........", "...#####", "..######", "..######", "...#####", "........", "........"],
          ["........", "........", "........", "...#####", "...#####", "........", "........", "........"]],
}
FLAMES["V"] = [["".join(f[x][y] for x in range(8)) for y in range(8)] for f in FLAMES["H"]]
FLAMES["R"] = [[r[::-1] for r in f] for f in FLAMES["L"]]
FLAMES["U"] = [["".join(f[x][y] for x in range(8)) for y in range(8)] for f in FLAMES["L"]]
FLAMES["D"] = [f[::-1] for f in FLAMES["U"]]

ICONS = {
    "fire": [
        "...#....",
        "...##...",
        "..###.#.",
        ".#####..",
        ".######.",
        "########",
        ".######.",
        "..####..",
    ],
    "bombup": [
        "......#.",
        ".....#..",
        "..###...",
        ".##.##..",
        ".#####..",
        ".#####..",
        "..###...",
        "........",
    ],
    "speed": [
        "........",
        "##..##..",
        ".##..##.",
        "..##..##",
        "..##..##",
        ".##..##.",
        "##..##..",
        "........",
    ],
    "kick": [
        "..###...",
        "..###...",
        "..###...",
        "..####..",
        "..#####.",
        "..######",
        "........",
        "..######",
    ],
    "glove": [
        ".#.#.#..",
        ".#.#.#..",
        ".#####..",
        ".#####.#",
        ".######.",
        ".#####..",
        "..###...",
        "..###...",
    ],
}


def lit(icon):
    """The blink's other half: the icon in its colours swapped."""
    return ["".join("." if p == "#" else "#" for p in r) for r in icon]


TROPHY = [
    "########",
    "#.####.#",
    ".######.",
    "..####..",
    "...##...",
    "...##...",
    "..####..",
    ".######.",
]


def halves():
    b = BOMB_BIG
    return {
        "halfL": ["...." + r[:4] for r in b],
        "halfR": [r[4:] + "...." for r in b],
        "halfU": ["........"] * 4 + b[:4],
        "halfD": b[4:] + ["........"] * 4,
    }


def tile_frames():
    t = {
        "pillar": [PILLAR], "block": [BLOCK], "wall": [WALL], "trophy": [TROPHY],
        "burn": BURN,
        "bomb": [bomb(0, False), bomb(1, False), bomb(0, True), bomb(1, True)],
        "dud": [dud(i) for i in range(4)],
    }
    for k in "CHVLRUD":
        t["flame" + k] = FLAMES[k]
    for name, icon in ICONS.items():
        t[name] = [icon, icon, lit(icon), lit(icon)]
    for k, v in halves().items():
        t[k] = [v]
    return t


# ---- the bombers --------------------------------------------------------------

def bodies():
    """The master's frames, one colour (the visor is a MOB of its own)."""
    fr = drawart.sprite_frames()
    return {k: [r.replace("v", "#") for r in fr[k]] for k in BODIES}


def visors():
    """8x8 cards a MOB shows at half height (8 scanlines), 2 pixels down
    from the body (rows 4-11 of its 16)."""
    fr = drawart.sprite_frames()
    v = lambda f, a: ["".join("#" if p == "v" else "." for p in r) for r in fr[f][a:a + 8]]  # noqa: E731
    return {"front": v("down0", 4), "side": v("right0", 4), "frontlo": v("win1", 4)}


# ---- the logo -----------------------------------------------------------------

def logo():
    """FUJIBLAST: each letter the master's 5x7 font at 2x2, in a 16x16 box
    (2x2 cards); # is the letter."""
    out = []
    for ch in LOGO_TEXT:
        g = drawart.FONT5[ch]
        img = [["."] * 16 for _ in range(16)]
        for r, row in enumerate(g):
            for c, p in enumerate(row):
                if p == "#":
                    for dy in range(2):
                        for dx in range(2):
                            img[1 + r * 2 + dy][3 + c * 2 + dx] = "#"
        out.append(["".join(r) for r in img])
    return out


# ---- write ---------------------------------------------------------------------

def put(img, x0, y0, rows, fg, bg):
    for y, row in enumerate(rows):
        for x, p in enumerate(row):
            img.putpixel((x0 + x, y0 + y), PAL[fg] if p == "#" else PAL[bg])


def main():
    out = os.path.join(ROOT, "gfx", "intv")
    os.makedirs(out, exist_ok=True)
    tiles = tile_frames()
    img = Image.new("RGB", (4 * 8, len(TILES) * 8))
    for i, (name, fg, bg) in enumerate(TILES):
        for f, rows in enumerate(tiles[name]):
            assert len(rows) == 8 and all(len(r) == 8 for r in rows), name
            put(img, f * 8, i * 8, rows, fg, bg)
        for f in range(len(tiles[name]), 4):
            put(img, f * 8, i * 8, tiles[name][f % len(tiles[name])], fg, bg)
    img.save(os.path.join(out, "tiles.png"))

    b, v = bodies(), visors()
    img = Image.new("RGBA", (len(BODIES) * 8 + len(VISORS) * 8, 16), (0, 0, 0, 0))
    for i, k in enumerate(BODIES):
        for y, row in enumerate(b[k]):
            for x, p in enumerate(row):
                if p == "#":
                    img.putpixel((i * 8 + x, y), PAL[WHITE] + (255,))
    for i, k in enumerate(VISORS):
        for y, row in enumerate(v[k]):
            for x, p in enumerate(row):
                if p == "#":
                    img.putpixel(((len(BODIES) + i) * 8 + x, y), PAL[VISOR] + (255,))
    img.save(os.path.join(out, "sprites.png"))

    img = Image.new("RGB", (len(LOGO_TEXT) * 16, 16))
    for i, rows in enumerate(logo()):
        put(img, i * 16, 0, rows[:8], BLACK, LOGO_TOP)
        for y in range(8):
            for x in range(16):
                img.putpixel((i * 16 + x, 8 + y), PAL[LOGO_BOTTOM] if rows[8 + y][x] == "#" else PAL[BLACK])
    img.save(os.path.join(out, "logo.png"))
    print("gfx/intv/tiles.png, sprites.png, logo.png")

    if len(sys.argv) > 2 and sys.argv[1] == "--preview":
        import preview_intv
        d = sys.argv[2]
        os.makedirs(d, exist_ok=True)
        preview_intv.all_previews(d, tiles, b, v, logo())
        print("previews in", d)


if __name__ == "__main__":
    main()

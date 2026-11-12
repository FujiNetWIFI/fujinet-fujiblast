#!/usr/bin/env python3
"""drawart_astro.py -- the Bally Astrocade's art, drawn for its 2-bit
bitmap: gfx/astrocade/tiles.png, sprites.png, logo.png
(tools/astro/mkgfx_astro.py turns them into the build's tables).

    drawart_astro.py            (needs Pillow; the build itself doesn't)

Four colours a pixel, the same four everywhere in the arena (the left
palette): k dark (index 0: the frame, the walls, shades, bombs, visors),
. floor (1), o warm (2), w bright (3). A tile is 8x7 (two bytes by seven
lines: about square, the Astrocade's pixels being taller than wide); a
bomber is 8x7 too, drawn in two roles, # body and v visor, which each
seat colours its own way (mkgfx_astro.py SEATS). Everything is reduced
from the master sheet (gfx/*.png, tools/drawart.py): sixteen Atari
lines become seven.
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
from drawart import FONT5, LOGO_TEXT  # noqa: E402
from artlayout import TILES, SPRITES  # noqa: E402

OUT = os.path.join(HERE, "..", "..", "gfx", "astrocade")

# role -> RGB in the PNGs (mkgfx_astro.py reads them back by these)
ROLE_RGB = {"k": (40, 40, 40), ".": (64, 136, 48), "o": (200, 104, 40), "w": (240, 240, 240)}
SPR_RGB = {"#": (255, 255, 255, 255), "v": (240, 224, 64, 255), ".": (0, 0, 0, 0)}

# ---- tiles: 8x7, one or two frames ------------------------------------------------
PILLAR = [
    "wwwwwwwk",
    "wkkkkkwk",
    "wkwwwwwk",
    "wkwwwwwk",
    "wwwwwwwk",
    "wwwwwwkk",
    "kkkkkkkk",
]
BLOCK = [
    "wwwkwwww",
    "ooookooo",
    "ooookooo",
    "kkkkkkkk",
    "wkwwwwww",
    "okoooooo",
    "kkkkkkkk",
]
BURN = [[
    "..w...w.",
    ".wo..wo.",
    "owwoowwo",
    "kkkkkkkk",
    "wkwwowww",
    "okoowooo",
    "kkkkkkkk",
], [
    ".w...w..",
    ".ow..ow.",
    "owwoowwo",
    "kkkkkkkk",
    "wkwowwww",
    "okowoooo",
    "kkkkkkkk",
]]
BOMB = [[
    ".....w..",
    "....k.o.",
    "..kkkk..",
    ".kwwkkk.",
    ".kwkkkk.",
    ".kkkkkk.",
    "..kkkk..",
], [
    "......o.",
    "....kw.w",
    "..kkkk..",
    ".kwwkkk.",
    ".kwkkkk.",
    ".kkkkkk.",
    "..kkkk..",
]]
DUD = [[
    "...w....",
    "....k...",
    "..kkkk..",
    ".kwwwwk.",
    ".kwkwwk.",
    ".kwwwwk.",
    "..kkkk..",
], [
    "..w.w...",
    "...wk...",
    "..kkkk..",
    ".kwwwwk.",
    ".kwkwwk.",
    ".kwwwwk.",
    "..kkkk..",
]]


def flame(kind, thin):
    """A blast's piece: bands of warm round a bright core (thick, then thin)."""
    hy = (1, 5, 2, 4) if not thin else (2, 4, 3, 3)      # rows: band, core
    vx = (1, 6, 2, 5) if not thin else (2, 5, 3, 4)      # columns: band, core

    def h(x, y):
        if hy[0] <= y <= hy[1]:
            return "w" if hy[2] <= y <= hy[3] else "o"

    def v(x, y):
        if vx[0] <= x <= vx[1]:
            return "w" if vx[2] <= x <= vx[3] else "o"

    def f(x, y):
        if kind == "C":
            r = h(x, y) or v(x, y)
            dx, dy = x - 3.5, (y - 3) * 1.15
            if dx * dx + dy * dy <= (11 if not thin else 7):
                r = "w" if dx * dx + dy * dy <= (5 if not thin else 3) else "o"
            return r
        if kind == "H":
            return h(x, y)
        if kind == "V":
            return v(x, y)
        if kind in "LR":            # an arm's end: the band ends round
            d = x if kind == "L" else 7 - x
            if d == 0 or (d == 1 and y in (hy[0], hy[1])):
                return None
            r = h(x, y)
            return "o" if r == "w" and d < 2 else r
        if kind in "UD":
            d = y if kind == "U" else 6 - y
            if d == 0 and x in (vx[0], vx[1]):
                return None
            r = v(x, y)
            return "o" if r == "w" and d < 1 else r
    return ["".join(f(x, y) or "." for x in range(8)) for y in range(7)]


def panel(icon, lit):
    """A powerup: a dark rounded panel, its 6x5 icon inside, the edge lit or not."""
    e = "o" if lit else "k"
    return [".kkkkkk."] + [e + r.replace(".", "k") + e for r in icon] + [".kkkkkk."]


FIRE = ["..o...", "..oo..", ".owwo.", ".owwo.", "..oo.."]
BOMBUP = ["....o.", "..ww..", ".wwkw.", ".wwww.", "..ww.."]
SPEED = ["ww....", ".oww..", "..ooww", ".oww..", "ww...."]
KICK = [".ooo..", ".ooo..", ".oooo.", ".wwwww", "....ww"]
GLOVE = ["w.w.w.", "wwwww.", "wwwwww", ".www..", ".ooo.."]
TROPHY = [
    "owwwwwwo",
    "o.wwww.o",
    ".owwwwo.",
    "..owwo..",
    "...ww...",
    "..oooo..",
    ".kkkkkk.",
]


def tile_frames():
    """name -> list of frames (each 7 rows of 8), in artlayout's TILES order."""
    t = {
        "floor": [["........"] * 7],
        "pillar": [PILLAR],
        "block": [BLOCK],
        "burn": BURN,
        "bomb": BOMB,
        "dud": DUD,
        "trophy": [TROPHY],
    }
    for k in "CHVLRUD":
        t["flame" + k] = [flame(k, False), flame(k, True)]
    for name, icon in (("fire", FIRE), ("bombup", BOMBUP), ("speed", SPEED),
                       ("kick", KICK), ("glove", GLOVE)):
        t[name] = [panel(icon, True), panel(icon, False)]
    return [t[n] for n in TILES]


# ---- the bombers: 8x7, # body, v visor ------------------------------------------------
DOWN = [
    "..####..",
    ".######.",
    "#vvvvvv#",
    ".######.",
    "#.####.#",
    "..#..#..",
    ".##..##.",
]
UP = DOWN[:2] + ["########"] + DOWN[3:]
RIGHT = [
    "..####..",
    ".######.",
    "####vvv#",
    ".######.",
    "..####..",
    "..#..#..",
    "..#..##.",
]


def legs(f, a, b):
    return f[:5] + [a, b]


def mirror(f):
    return [r[::-1] for r in f]


def sprite_frames():
    fr = {}
    fr["down0"] = DOWN
    fr["down1"] = legs(DOWN, "..#..#..", ".##...#.")
    fr["down2"] = legs(DOWN, "..#..#..", ".#...##.")
    fr["up0"] = UP
    fr["up1"] = legs(UP, "..#..#..", ".##...#.")
    fr["up2"] = legs(UP, "..#..#..", ".#...##.")
    fr["right0"] = RIGHT
    fr["right1"] = legs(RIGHT, ".#....#.", "##....##")
    fr["right2"] = legs(RIGHT, "...##...", "..###...")
    for k in "012":
        fr["left" + k] = mirror(fr["right" + k])
    fr["win0"] = [
        "#.####.#",
        "########",
        "#vvvvvv#",
        ".######.",
        "..####..",
        "..#..#..",
        ".##..##.",
    ]
    fr["win1"] = ["........"] + fr["win0"][:4] + ["..#..#..", ".##..##."]
    fr["die0"] = fr["up0"]
    fr["die1"] = fr["left0"]
    fr["die2"] = [
        "........",
        "........",
        "..####..",
        ".######.",
        "#vvvvvv#",
        "########",
        ".##..##.",
    ]
    fr["die3"] = [
        "........",
        "#..#...#",
        ".#...#..",
        "..#.#.#.",
        "#.......",
        "...#..#.",
        ".#.....#",
    ]
    return [fr[n] for n in SPRITES]


# ---- the title's logo: FONT5 (5x7) at 2x2 a pixel, lit on top, a shadow: 108x16 ------
def logo():
    w = len(LOGO_TEXT) * 12
    img = [["."] * w for _ in range(16)]
    for i, ch in enumerate(LOGO_TEXT):
        for r, row in enumerate(FONT5[ch]):
            for c, p in enumerate(row):
                if p != "#":
                    continue
                top = r == 0 or FONT5[ch][r - 1][c] != "#"
                for dy in range(2):
                    for dx in range(2):
                        x, y = i * 12 + 1 + c * 2 + dx, 1 + r * 2 + dy
                        img[y][x] = "w" if dy == 0 and top else "o"
    out = [row[:] for row in img]
    for y in range(16):
        for x in range(w):
            if img[y][x] == "." and ((x > 0 and img[y][x - 1] in "wo") or (y > 0 and img[y - 1][x] in "wo")):
                out[y][x] = "k"
    return ["".join(r) for r in out]


def save(path, rows, rgb, mode="RGB"):
    h, w = len(rows), len(rows[0])
    img = Image.new(mode, (w, h))
    for y, r in enumerate(rows):
        assert len(r) == w, (path, y, r)
        for x, p in enumerate(r):
            img.putpixel((x, y), rgb[p])
    img.save(path)


def main():
    os.makedirs(OUT, exist_ok=True)
    tiles = tile_frames()
    rows = []
    for frames in tiles:
        for y in range(7):
            rows.append("".join(f[y] for f in frames).ljust(16, "."))
    save(os.path.join(OUT, "tiles.png"), rows, ROLE_RGB)
    spr = sprite_frames()
    save(os.path.join(OUT, "sprites.png"), ["".join(f[y] for f in spr) for y in range(7)],
         SPR_RGB, "RGBA")
    save(os.path.join(OUT, "logo.png"), logo(), ROLE_RGB)
    print("drawart_astro: gfx/astrocade/tiles.png, sprites.png, logo.png")


if __name__ == "__main__":
    main()

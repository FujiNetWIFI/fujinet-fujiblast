#!/usr/bin/env python3
"""drawart.py -- FujiBlast's original art, written out as the master PNGs in
gfx/ (the sheet every port starts from).

Pixels are the Atari's mode 4 pixels: one colour clock wide, one scanline
tall. A tile is 8x16, a character cell 4x8, a player 8x16.

Tile and glyph colours (by role; tools/mkgfx_atari.py maps them back):
  .  floor (background)      w  light (PF0)      k  dark (PF1)
  o  orange (PF2)            y  yellow (PF3, inverse cells)
A 4x8 cell may use o or y, not both. Sprites: # body (the player's colour),
v visor (the missiles, PF3), . clear.

    python3 tools/drawart.py        # rewrites gfx/*.png
"""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from artlayout import ROLE_RGB, SPRITE_RGB, TILES, SPRITES, GLYPHS, LOGO_TEXT  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

# ---- tiles (8x16), by frame ------------------------------------------------

PILLAR = [
    "wwwwwwwk",
    "wkwwwkwk",
    "wwwwwwwk",
    "wwkkkkwk",
    "wwkwwwwk",
    "wwkwwwwk",
    "wwkwwwwk",
    "wwkwwwwk",
    "wwkwwwwk",
    "wwkwwwwk",
    "wwkwwwwk",
    "wwwwwwwk",
    "wwwwwwwk",
    "wkwwwkwk",
    "wkkkkkkk",
    "kkkkkkkk",
]

BLOCK = [
    "wwwkwwww",
    "ooookooo",
    "ooookooo",
    "ooookooo",
    "kkkkkkkk",
    "wkwwwwww",
    "okoooooo",
    "okoooooo",
    "okoooooo",
    "kkkkkkkk",
    "wwwwwkww",
    "oooookoo",
    "oooookoo",
    "oooookoo",
    "kkkkkkkk",
    "kkkkkkkk",
]

BURN = [[
    "..y...y.",
    ".yy..yy.",
    ".yyy.yyy",
    "yywyyywy",
    "ywwykwwy",
    "kkkkkkkk",
    "ykyyyyyy",
    "ykywwyyy",
    "ykyyyyyy",
    "kkkkkkkk",
    "yyyyykyy",
    "ywyyykyy",
    "yyyyykwy",
    "yyyyykyy",
    "kkkkkkkk",
    "kkkkkkkk",
], [
    ".y...y..",
    ".yy..yy.",
    "yyy.yyy.",
    "ywyyywyy",
    "wwykwwyy",
    "kkkkkkkk",
    "ykyywyyy",
    "ykywwwyy",
    "ykyyyyyy",
    "kkkkkkkk",
    "yyywykyy",
    "yywwykyy",
    "yyyyykyy",
    "yyyyykwy",
    "kkkkkkkk",
    "kkkkkkkk",
]]


def bomb(spark, small):
    body = [
        "..kkkk..",
        ".kkkkkk.",
        "kwwkkkkk",
        "kwkkkkkk",
        "kkkkkkkk",
        "kkkkkkkk",
        "kkkkkkkk",
        "kkkkkkkk",
        ".kkkkkk.",
        ".kkkkkk.",
        "..kkkk..",
    ]
    if small:
        body = [
            "........",
            "..kkkk..",
            ".kwwkkk.",
            ".kwkkkk.",
            ".kkkkkk.",
            ".kkkkkk.",
            ".kkkkkk.",
            ".kkkkkk.",
            "..kkkk..",
            "........",
            "........",
        ]
    top = [
        ["......y.", ".....yy.", ".....w..", "....w..."],
        [".....y.y", "......y.", ".....w..", "....w..."],
    ][spark]
    return top + body + ["........"]


def dud(puff):
    body = [
        "..kkkk..",
        ".kwwwwk.",
        "kwkwwwwk",
        "kwwwwwwk",
        "kwwwwwwk",
        "kwwwwwwk",
        "kwwwwwwk",
        "kwwwwwwk",
        ".kwwwwk.",
        ".kwwwwk.",
        "..kkkk..",
    ]
    top = [
        ["....w...", "...www..", "....w...", "....k..."],
        ["...w....", "..ww.w..", "...ww...", "....k..."],
        ["..ww....", ".w..w...", "........", "....k..."],
        ["........", "........", "....w...", "....k..."],
    ][puff]
    return top + body + ["........"]


def shape(fn):
    """A tile from fn(x, y) -> role."""
    return ["".join(fn(x, y) for x in range(8)) for y in range(16)]


def flame(kind, thin):
    # Bands: horizontal rows ry0..ry1 (core cy0..cy1), vertical columns.
    ry0, ry1, cy0, cy1 = (3, 12, 6, 9) if not thin else (4, 11, 7, 8)
    rx0, rx1, cx0, cx1 = (1, 6, 3, 4) if not thin else (2, 5, 3, 4)

    def h(x, y, x0=0, x1=7):
        if not (x0 <= x <= x1 and ry0 <= y <= ry1):
            return None
        return "w" if cy0 <= y <= cy1 else "y"

    def v(x, y, y0=0, y1=15):
        if not (y0 <= y <= y1 and rx0 <= x <= rx1):
            return None
        return "w" if cx0 <= x <= cx1 else "y"

    def tip_h(x, y, toward):  # arm end: the band narrows to a rounded tip
        d = x if toward < 0 else 7 - x          # 0 at the tip
        shrink = max(0, 3 - d) * (2 if not thin else 1)
        if not (ry0 + shrink <= y <= ry1 - shrink):
            return None
        return "w" if cy0 <= y <= cy1 and d >= 2 else "y"

    def tip_v(x, y, toward):
        d = y if toward < 0 else 15 - y
        shrink = 1 if d < 3 else 0
        if d < 1:
            shrink = 2
        if not (rx0 + shrink <= x <= rx1 - shrink):
            return None
        return "w" if cx0 <= x <= cx1 and d >= 3 else "y"

    def f(x, y):
        r = None
        if kind == "C":
            dx, dy = (x - 3.5) / 4.0, (y - 7.5) / 8.0
            if dx * dx + dy * dy <= (0.95 if not thin else 0.8):
                r = "w" if dx * dx + dy * dy <= 0.35 else "y"
            r = r or h(x, y) or v(x, y)
        elif kind == "H":
            r = h(x, y)
        elif kind == "V":
            r = v(x, y)
        elif kind == "L":
            r = tip_h(x, y, -1)
        elif kind == "R":
            r = tip_h(x, y, 1)
        elif kind == "U":
            r = tip_v(x, y, -1)
        elif kind == "D":
            r = tip_v(x, y, 1)
        return r or "."
    return shape(f)


def panel(icon, lit):
    """A powerup: a dark rounded panel, the icon (6x12) inside."""
    edge = "o" if lit else "k"
    rows = [".kkkkkk."]
    for r in range(14):
        inner = icon[r - 1] if 1 <= r <= 12 else "......"
        inner = inner.replace(".", "k")
        rows.append(edge + inner + edge)
    rows.append(".kkkkkk.")
    return rows


FIRE = [
    "...o..",
    "...oo.",
    "..ooo.",
    ".oooo.",
    ".oowoo",
    "oowwoo",
    "owwwwo",
    "owwwwo",
    "owwwwo",
    ".owwo.",
    "..oo..",
    "......",
]
BOMBUP = [
    "....o.",
    "...o.o",
    "..ww..",
    ".wwww.",
    "wwkwww",
    "wkwwww",
    "wwwwww",
    "wwwwww",
    "wwwwww",
    ".wwww.",
    "..ww..",
    "......",
]
SPEED = [
    "......",
    "ww....",
    "oww...",
    ".oww..",
    "..oww.",
    "...oww",
    "...oww",
    "..oww.",
    ".oww..",
    "oww...",
    "ww....",
    "......",
]
KICK = [
    "......",
    ".ooo..",
    ".ooo..",
    ".ooo..",
    ".ooo..",
    ".oooo.",
    ".ooooo",
    ".ooooo",
    ".wwwww",
    "......",
    "....ww",
    "....ww",
]
GLOVE = [
    "w.w.w.",
    "w.w.w.",
    "w.w.w.",
    "wwwww.",
    "wwwww.",
    "wwwwww",
    "wwwwww",
    "wwwww.",
    ".www..",
    ".ooo..",
    ".ooo..",
    "......",
]

TROPHY = [
    "........",
    "oooooooo",
    "owoooooo",
    ".wooooo.",
    ".wooooo.",
    ".wooooo.",
    "..wooo..",
    "...oo...",
    "...oo...",
    "...oo...",
    "..woooo.",
    ".kkkkkk.",
    ".kwwwwk.",
    ".kkkkkk.",
    "........",
    "........",
]


def tile_frames():
    """name -> list of frames (each 16 rows of 8)."""
    t = {
        "floor": [["........"] * 16],
        "pillar": [PILLAR],
        "block": [BLOCK],
        "burn": BURN,
        "bomb": [bomb(0, False), bomb(1, False), bomb(0, True), bomb(1, True)],
        "dud": [dud(i) for i in range(4)],
        "trophy": [TROPHY],
    }
    for k in "CHVLRUD":
        t["flame" + k] = [flame(k, False), flame(k, True)]
    for name, icon in (("fire", FIRE), ("bombup", BOMBUP), ("speed", SPEED), ("kick", KICK), ("glove", GLOVE)):
        t[name] = [panel(icon, False), panel(icon, False), panel(icon, True), panel(icon, True)]
    return t


# ---- single cells (4x8) ----------------------------------------------------

FONT3 = {
    "0": ["###", "#.#", "#.#", "#.#", "#.#", "#.#", "###"],
    "1": [".#.", "##.", ".#.", ".#.", ".#.", ".#.", "###"],
    "2": ["###", "..#", "..#", "###", "#..", "#..", "###"],
    "3": ["###", "..#", "..#", ".##", "..#", "..#", "###"],
    "4": ["#.#", "#.#", "#.#", "###", "..#", "..#", "..#"],
    "5": ["###", "#..", "#..", "###", "..#", "..#", "###"],
    "6": ["###", "#..", "#..", "###", "#.#", "#.#", "###"],
    "7": ["###", "..#", "..#", ".#.", ".#.", ".#.", ".#."],
    "8": ["###", "#.#", "#.#", "###", "#.#", "#.#", "###"],
    "9": ["###", "#.#", "#.#", "###", "..#", "..#", "###"],
    "x": ["...", "...", "#.#", ".#.", "#.#", "...", "..."],
    "/": ["..#", "..#", ".#.", ".#.", ".#.", "#..", "#.."],
    "R": ["##.", "#.#", "#.#", "##.", "#.#", "#.#", "#.#"],
    "N": ["#.#", "###", "###", "###", "#.#", "#.#", "#.#"],
    "D": ["##.", "#.#", "#.#", "#.#", "#.#", "#.#", "##."],
}


def glyph_cells():
    g = {
        "wall": ["wwwk", "wwwk", "wwwk", "kkkk", "wkww", "wkww", "wkww", "kkkk"],
        "panel": ["kkkk"] * 8,
    }
    for ch, rows in FONT3.items():
        g["font" + ch] = ["kkkk"] + [r.replace("#", "w").replace(".", "k") + "k" for r in rows]
    return g


# ---- the logo --------------------------------------------------------------

FONT5 = {
    "F": ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    "U": ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "J": ["..###", "...#.", "...#.", "...#.", "#..#.", "#..#.", ".##.."],
    "I": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"],
    "B": ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    "L": ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "S": [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
}


def logo():
    """The title: each font pixel is 2 wide and 3 tall, lit on top, with a
    shadow under and to the right; a letter is 12 pixels (3 cells) wide."""
    w = len(LOGO_TEXT) * 12
    img = [["."] * w for _ in range(24)]
    for i, ch in enumerate(LOGO_TEXT):
        for r, row in enumerate(FONT5[ch]):
            for c, p in enumerate(row):
                if p != "#":
                    continue
                top = r == 0 or FONT5[ch][r - 1][c] != "#"
                for dy in range(3):
                    for dx in range(2):
                        x, y = i * 12 + 1 + c * 2 + dx, 1 + r * 3 + dy
                        img[y][x] = "w" if dy == 0 and top else "o"
    # shadows: an unlit pixel right of or under a lit one goes dark
    out = [row[:] for row in img]
    for y in range(24):
        for x in range(w):
            if img[y][x] == "." and ((x > 0 and img[y][x - 1] in "wo") or (y > 0 and img[y - 1][x] in "wo")):
                out[y][x] = "k"
    return ["".join(r) for r in out]


# ---- the players -----------------------------------------------------------

def sprite_frames():
    down = [
        "...##...",
        "..####..",
        ".######.",
        "########",
        "##vvvv##",
        "##vvvv##",
        ".######.",
        "..####..",
        ".######.",
        "########",
        "#.####.#",
        "#.####.#",
        "..####..",
        "..#..#..",
        ".##..##.",
        ".##..##.",
    ]
    up = down[:4] + ["########", "########"] + down[6:]
    right = [
        "...##...",
        "..####..",
        ".######.",
        "########",
        "####vvvv",
        "####vvvv",
        ".######.",
        "..####..",
        "..####..",
        ".######.",
        ".#####..",
        "..####..",
        "..####..",
        "..#..#..",
        "..#..##.",
        ".##..##.",
    ]

    def legs(f, a, b):
        g = f[:13]
        return g + [a, b, b]

    def mirror(f):
        return [r[::-1] for r in f]

    frames = {}
    frames["down0"] = down
    frames["down1"] = legs(down, "..#..#..", ".##...#.")
    frames["down2"] = legs(down, "..#..#..", ".#...##.")
    frames["up0"] = up
    frames["up1"] = legs(up, "..#..#..", ".##...#.")
    frames["up2"] = legs(up, "..#..#..", ".#...##.")
    frames["right0"] = right
    frames["right1"] = right[:13] + ["..#..#..", ".#....#.", "##....##"]
    frames["right2"] = right[:13] + ["...##...", "...##...", "..###..."]
    for k in ("0", "1", "2"):
        frames["left" + k] = mirror(frames["right" + k])
    frames["win0"] = [
        "#..##..#",
        "#.####.#",
        "########",
        "########",
        "##vvvv##",
        "##vvvv##",
        ".######.",
        "..####..",
        "..####..",
        "..####..",
        "..####..",
        "..####..",
        "..####..",
        "..#..#..",
        ".##..##.",
        ".##..##.",
    ]
    frames["win1"] = ["........"] + frames["win0"][:12] + ["..#..#..", ".##..##.", ".##..##."]
    frames["die0"] = frames["up0"]
    frames["die1"] = frames["left0"]
    frames["die2"] = [
        "........", "........", "........", "...##...", "..####..", ".######.", "##vvvv##", "##vvvv##",
        ".######.", "..####..", "#.####.#", "########", ".##..##.", "........", "........", "........"]
    frames["die3"] = [
        "........", "........", "........", "........", "........", "#..#...#", ".#...#..", "..#.#.#.",
        "#.......", "...#..#.", ".#.....#", "#..#.#..", "........", "........", "........", "........"]
    return frames


# ---- write ---------------------------------------------------------------

def put(img, x0, y0, rows, rgb):
    for y, row in enumerate(rows):
        for x, p in enumerate(row):
            img.putpixel((x0 + x, y0 + y), rgb[p])


def main():
    gfx = os.path.join(ROOT, "gfx")
    os.makedirs(gfx, exist_ok=True)

    tiles = tile_frames()
    img = Image.new("RGB", (4 * 8, len(TILES) * 16), ROLE_RGB["."])
    for i, name in enumerate(TILES):
        for f, rows in enumerate(tiles[name]):
            assert len(rows) == 16 and all(len(r) == 8 for r in rows), name
            put(img, f * 8, i * 16, rows, ROLE_RGB)
    img.save(os.path.join(gfx, "tiles.png"))

    cells = glyph_cells()
    img = Image.new("RGB", (len(GLYPHS) * 4, 8), ROLE_RGB["."])
    for i, name in enumerate(GLYPHS):
        put(img, i * 4, 0, cells[name], ROLE_RGB)
    img.save(os.path.join(gfx, "glyphs.png"))

    rows = logo()
    img = Image.new("RGB", (len(rows[0]), 24), ROLE_RGB["."])
    put(img, 0, 0, rows, ROLE_RGB)
    img.save(os.path.join(gfx, "logo.png"))

    frames = sprite_frames()
    img = Image.new("RGBA", (len(SPRITES) * 8, 16), SPRITE_RGB["."])
    for i, name in enumerate(SPRITES):
        assert len(frames[name]) == 16 and all(len(r) == 8 for r in frames[name]), name
        put(img, i * 8, 0, frames[name], SPRITE_RGB)
    img.save(os.path.join(gfx, "sprites.png"))
    print("wrote gfx/tiles.png glyphs.png logo.png sprites.png")


if __name__ == "__main__":
    main()

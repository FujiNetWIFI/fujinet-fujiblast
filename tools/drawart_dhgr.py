#!/usr/bin/env python3
"""drawart_dhgr.py -- FujiBlast's art for Apple II double hi-res, written out
as gfx/dhgr/*.png (the Apple port's sheet; tools/mkgfx_apple2.py converts it).

A DHGR colour pixel is a quarter of a 7-pixel group, about 1.8 times as wide
as it is tall, so a tile is 7x16: the Atari tile's size on the screen. The
designs are the master sheet's (tools/drawart.py), redrawn in DHGR's 16
colours. All of it is original art.

Colours, one letter each (lo-res colour numbers):
  k black   m magenta   b dark blue  p purple   g dark green  d dark grey
  u blue    l light blue n brown     o orange   e light grey  i pink
  G green   y yellow    a aqua       w white    . floor (dark green)
Sprites: # body, + body shade (the seat's two colours), v visor (yellow),
k black, . clear.

    python3 tools/drawart_dhgr.py              # rewrites gfx/dhgr/*.png
    python3 tools/drawart_dhgr.py --preview D  # also mock screens into D
"""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from artlayout import TILES, SPRITES  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

# The 16 DHGR colours (lo-res numbering), as fujinet-picture-display's
# DHGR server shows them.
PAL16 = [
    (0x00, 0x00, 0x00), (0xDD, 0x00, 0x33), (0x00, 0x00, 0x99), (0xAA, 0x00, 0xFF),
    (0x00, 0x77, 0x22), (0x55, 0x55, 0x55), (0x22, 0x22, 0xFF), (0x66, 0xAA, 0xFF),
    (0x88, 0x55, 0x00), (0xFF, 0x66, 0x00), (0xAA, 0xAA, 0xAA), (0xFF, 0x99, 0x88),
    (0x00, 0xFF, 0x44), (0xFF, 0xFF, 0x00), (0x44, 0xFF, 0xCC), (0xFF, 0xFF, 0xFF),
]
CODE = {"k": 0, "m": 1, "b": 2, "p": 3, "g": 4, "d": 5, "u": 6, "l": 7,
        "n": 8, "o": 9, "e": 10, "i": 11, "G": 12, "y": 13, "a": 14, "w": 15,
        ".": 4}
FLOOR = 4

# The seats: body and shade colours. Sprite roles in the PNG: these RGBs
# stand for "body"/"shade"; mkgfx_apple2.py reads them back as roles.
SEAT_COLOURS = [(15, 10), (11, 1), (7, 6), (12, 8)]   # white, red, blue, green
SPRITE_ROLE_RGB = {
    ".": (0, 0, 0, 0),
    "#": (255, 255, 255, 255),
    "+": (128, 128, 128, 255),
    "v": PAL16[13] + (255,),
    "k": (0, 0, 0, 255),
}
VISOR = 13

W = 7          # a tile's width


def rgb(c):
    return PAL16[CODE[c]]


# ---- tiles (7x16) ---------------------------------------------------------

PILLAR = [
    "wwwwwwd",
    "weeeeed",
    "weeeeed",
    "weewedk",
    "weeeedk",
    "weeeedk",
    "weeeedk",
    "weeeedk",
    "weeeedk",
    "weeeedk",
    "weeeedk",
    "weeeedk",
    "weewedk",
    "weeeedk",
    "eddddkk",
    "dkkkkkk",
]

BLOCK = [
    "yyynyyy",
    "ooonooo",
    "ooonooo",
    "ooonooo",
    "nnnnnnn",
    "ynyyyyn",
    "onoooon",
    "onoooon",
    "onoooon",
    "nnnnnnn",
    "yyyynyy",
    "oooonoo",
    "oooonoo",
    "oooonoo",
    "nnnnnnn",
    "kkkkkkk",
]

BURN = [[
    ".y...y.",
    ".yy.yy.",
    "yyoyyoy",
    "ywoyowy",
    "omwoomo",
    "nnnnnnn",
    "ymyyoym",
    "omowwoo",
    "momoomo",
    "nnnnnnn",
    "oymoyoo",
    "mmwomyo",
    "omoommo",
    "mmommmo",
    "nnnnnnn",
    "kkkkkkk",
], [
    "..y.y..",
    ".yy.yy.",
    "yoyyoyy",
    "owyyowo",
    "omooomw",
    "nnnnnnn",
    "oymoyyo",
    "ooowwmo",
    "omoomom",
    "nnnnnnn",
    "yomoymo",
    "omwmmoy",
    "omomoom",
    "omommmm",
    "nnnnnnn",
    "kkkkkkk",
]]


def bomb(spark, small):
    body = [
        "..kkk..",
        ".kkkkk.",
        "kwdkkkk",
        "kdkkkkk",
        "kkkkkkk",
        "kkkkkkk",
        "kkkkkkk",
        "kkkkkkd",
        ".kkkkd.",
        ".kkkkk.",
        "..kkk..",
    ]
    if small:
        body = [
            ".......",
            "..kkk..",
            ".kwdkk.",
            ".kdkkk.",
            ".kkkkk.",
            ".kkkkk.",
            ".kkkkd.",
            ".kkkkk.",
            "..kkk..",
            ".......",
            ".......",
        ]
    top = [
        ["....y..", "...yoy.", "....n..", "...n..."],
        ["...w.y.", "....o..", "...yn..", "...n..."],
    ][spark]
    return top + body + ["......."]


def dud(puff):
    body = [
        "..ddd..",
        ".deeed.",
        "dewded.",
        "deeeeed",
        "dededed",
        "deeeeed",
        "dededed",
        "deeeeed",
        ".deeed.",
        ".ddddd.",
        "..ddd..",
    ]
    top = [
        ["...w...", "..wew..", "...e...", "...n..."],
        ["..w....", ".we.w..", "..ee...", "...n..."],
        [".ee....", "e..e...", ".......", "...n..."],
        [".......", ".......", "...e...", "...n..."],
    ][puff]
    return top + body + ["......."]


def shape(fn):
    return ["".join(fn(x, y) for x in range(W)) for y in range(16)]


def flame(kind, thin):
    """Arms: a white core, yellow, orange, red at the edge. Thin: the
    flicker frame, one pixel narrower all round."""
    t = 1 if thin else 0
    cy, cx = 7.5, 3.0

    def band(d, half):   # d: distance from the band's centre line
        half -= t
        if d > half:
            return None
        if d <= half - 3:
            return "w"
        if d <= half - 2:
            return "y"
        if d <= half - 1:
            return "o"
        return "m"

    def h(x, y):
        return band(abs(y - cy), 5.0)

    def v(x, y):
        return band(abs(x - cx), 3.0)

    def f(x, y):
        r = None
        if kind == "C":
            dx, dy = (x - cx) / 3.6, (y - cy) / 7.5
            q = dx * dx + dy * dy
            lim = 1.0 if not thin else 0.8
            if q <= lim:
                r = "w" if q <= lim * 0.3 else "y" if q <= lim * 0.6 else "o" if q <= lim * 0.85 else "m"
            r = r or h(x, y) or v(x, y)
        elif kind == "H":
            r = h(x, y)
        elif kind == "V":
            r = v(x, y)
        elif kind in "LR":
            d = x if kind == "L" else W - 1 - x      # 0 at the tip
            r = band(abs(y - cy), 5.0 - max(0, 3 - d) * 1.4)
        elif kind in "UD":
            d = y if kind == "U" else 15 - y
            r = band(abs(x - cx), 3.0 - max(0, 3 - d) * 0.7)
        return r or "."
    return shape(f)


def panel(icon, lit):
    """A powerup: a dark blue panel with a rim (lit: yellow), the icon
    (5x12) inside."""
    edge = "y" if lit else "p"
    rows = ["." + edge * 5 + "."]
    for r in range(14):
        inner = icon[r - 1] if 1 <= r <= 12 else "....."
        rows.append(edge + inner.replace(".", "b") + edge)
    rows.append("." + edge * 5 + ".")
    return rows


FIRE = [
    "..m..",
    "..mo.",
    ".moo.",
    ".mooo",
    "moyoo",
    "moyyo",
    "oyywo",
    "oywwo",
    "oywwo",
    ".oyo.",
    "..o..",
    ".....",
]
BOMBUP = [
    "...y.",
    "..o.y",
    ".nkk.",
    "kkkkk",
    "kwdkk",
    "kdkkk",
    "kkkkk",
    "kkkkk",
    ".kkk.",
    ".....",
    "..w..",
    ".www.",
]
SPEED = [
    ".....",
    "aw...",
    ".aw..",
    "..aw.",
    "...aw",
    "...aw",
    "..aw.",
    ".aw..",
    "aw...",
    ".....",
    ".....",
    ".....",
]
KICK = [
    ".....",
    ".nn..",
    ".nn..",
    ".nn..",
    ".nn..",
    ".nnn.",
    ".nnnn",
    ".nnnn",
    ".wwww",
    ".....",
    "...ww",
    "...ww",
]
GLOVE = [
    "i.i.i",
    "i.i.i",
    "i.i.i",
    "iiiii",
    "iiiii",
    "iiiiw",
    "iiiiw",
    "iiii.",
    ".iii.",
    ".lll.",
    ".lll.",
    ".....",
]

TROPHY = [
    ".......",
    "yyyyyyy",
    "ywyyyyo",
    ".wyyyo.",
    ".wyyyo.",
    ".yyyyo.",
    "..yyo..",
    "...o...",
    "...o...",
    "...o...",
    "..yyo..",
    ".nnnnn.",
    ".nwwwn.",
    ".nnnnn.",
    ".......",
    ".......",
]


def tile_frames():
    t = {
        "floor": [["......."] * 16],
        "pillar": [PILLAR],
        "block": [BLOCK],
        "burn": BURN,
        "bomb": [bomb(0, False), bomb(1, False), bomb(0, True), bomb(1, True)],
        "dud": [dud(i) for i in range(4)],
        "trophy": [TROPHY],
    }
    for k in "CHVLRUD":
        t["flame" + k] = [flame(k, False), flame(k, True)]
    for name, icon in (("fire", FIRE), ("bombup", BOMBUP), ("speed", SPEED),
                       ("kick", KICK), ("glove", GLOVE)):
        t[name] = [panel(icon, False), panel(icon, True)]
    return t


# The arena's frame: a 4x4 stone pattern (the side walls are 4 px wide, the
# top and bottom 4 lines tall).
WALL = ["eeed", "dddk", "edee", "kdkd"]

# ---- fonts -----------------------------------------------------------------
# ASCII 32-95. FONT3 (3x7) is the text rows' (35 columns of 4 px); FONT5
# (5x7) the BIG rows' (20 columns of 7 px).

F5 = {
    " ": [".....", ".....", ".....", ".....", ".....", ".....", "....."],
    "!": ["..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."],
    '"': [".#.#.", ".#.#.", ".....", ".....", ".....", ".....", "....."],
    "#": [".#.#.", "#####", ".#.#.", ".#.#.", "#####", ".#.#.", "....."],
    "'": ["..#..", "..#..", ".#...", ".....", ".....", ".....", "....."],
    "(": ["...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#."],
    ")": [".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..."],
    "*": [".....", "#.#.#", ".###.", "#####", ".###.", "#.#.#", "....."],
    "+": [".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."],
    ",": [".....", ".....", ".....", ".....", "..##.", "...#.", "..#.."],
    "-": [".....", ".....", ".....", "#####", ".....", ".....", "....."],
    ".": [".....", ".....", ".....", ".....", ".....", ".##..", ".##.."],
    "/": ["....#", "....#", "...#.", "..#..", ".#...", "#....", "#...."],
    "0": [".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."],
    "1": ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."],
    "2": [".###.", "#...#", "....#", "..##.", ".#...", "#....", "#####"],
    "3": ["#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."],
    "4": ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."],
    "5": ["#####", "#....", "####.", "....#", "....#", "#...#", ".###."],
    "6": ["..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."],
    "7": ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."],
    "8": [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."],
    "9": [".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."],
    ":": [".....", ".##..", ".##..", ".....", ".##..", ".##..", "....."],
    ";": [".....", ".##..", ".##..", ".....", ".##..", "..#..", ".#..."],
    "<": ["...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#."],
    "=": [".....", ".....", "#####", ".....", "#####", ".....", "....."],
    ">": [".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..."],
    "?": [".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."],
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "B": ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    "C": [".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."],
    "D": ["####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."],
    "E": ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    "F": ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    "G": [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"],
    "H": ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "I": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"],
    "J": ["..###", "...#.", "...#.", "...#.", "#..#.", "#..#.", ".##.."],
    "K": ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"],
    "L": ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    "M": ["#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"],
    "N": ["#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"],
    "O": [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "P": ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    "Q": [".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"],
    "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    "S": [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    "U": ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "V": ["#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."],
    "W": ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"],
    "X": ["#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"],
    "Y": ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
    "Z": ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"],
    "[": [".###.", ".#...", ".#...", ".#...", ".#...", ".#...", ".###."],
    "]": [".###.", "...#.", "...#.", "...#.", "...#.", "...#.", ".###."],
    "_": [".....", ".....", ".....", ".....", ".....", ".....", "#####"],
}

F3 = {
    " ": ["...", "...", "...", "...", "...", "...", "..."],
    "!": [".#.", ".#.", ".#.", ".#.", ".#.", "...", ".#."],
    '"': ["#.#", "#.#", "...", "...", "...", "...", "..."],
    "'": [".#.", ".#.", "#..", "...", "...", "...", "..."],
    "(": [".#.", "#..", "#..", "#..", "#..", "#..", ".#."],
    ")": [".#.", "..#", "..#", "..#", "..#", "..#", ".#."],
    "*": ["...", "#.#", ".#.", "###", ".#.", "#.#", "..."],
    "+": ["...", "...", ".#.", "###", ".#.", "...", "..."],
    ",": ["...", "...", "...", "...", "...", ".#.", "#.."],
    "-": ["...", "...", "...", "###", "...", "...", "..."],
    ".": ["...", "...", "...", "...", "...", "...", ".#."],
    "/": ["..#", "..#", ".#.", ".#.", ".#.", "#..", "#.."],
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
    ":": ["...", ".#.", ".#.", "...", ".#.", ".#.", "..."],
    ";": ["...", ".#.", ".#.", "...", ".#.", ".#.", "#.."],
    "<": ["..#", ".#.", "#..", "#..", "#..", ".#.", "..#"],
    "=": ["...", "...", "###", "...", "###", "...", "..."],
    ">": ["#..", ".#.", "..#", "..#", "..#", ".#.", "#.."],
    "?": ["###", "..#", "..#", ".#.", ".#.", "...", ".#."],
    "A": [".#.", "#.#", "#.#", "###", "#.#", "#.#", "#.#"],
    "B": ["##.", "#.#", "#.#", "##.", "#.#", "#.#", "##."],
    "C": [".##", "#..", "#..", "#..", "#..", "#..", ".##"],
    "D": ["##.", "#.#", "#.#", "#.#", "#.#", "#.#", "##."],
    "E": ["###", "#..", "#..", "##.", "#..", "#..", "###"],
    "F": ["###", "#..", "#..", "##.", "#..", "#..", "#.."],
    "G": [".##", "#..", "#..", "#.#", "#.#", "#.#", ".##"],
    "H": ["#.#", "#.#", "#.#", "###", "#.#", "#.#", "#.#"],
    "I": ["###", ".#.", ".#.", ".#.", ".#.", ".#.", "###"],
    "J": ["..#", "..#", "..#", "..#", "..#", "#.#", ".#."],
    "K": ["#.#", "#.#", "##.", "#..", "##.", "#.#", "#.#"],
    "L": ["#..", "#..", "#..", "#..", "#..", "#..", "###"],
    "M": ["#.#", "###", "###", "#.#", "#.#", "#.#", "#.#"],
    "N": ["##.", "#.#", "#.#", "#.#", "#.#", "#.#", "#.#"],
    "O": [".#.", "#.#", "#.#", "#.#", "#.#", "#.#", ".#."],
    "P": ["##.", "#.#", "#.#", "##.", "#..", "#..", "#.."],
    "Q": [".#.", "#.#", "#.#", "#.#", "#.#", "##.", ".##"],
    "R": ["##.", "#.#", "#.#", "##.", "#.#", "#.#", "#.#"],
    "S": [".##", "#..", "#..", ".#.", "..#", "..#", "##."],
    "T": ["###", ".#.", ".#.", ".#.", ".#.", ".#.", ".#."],
    "U": ["#.#", "#.#", "#.#", "#.#", "#.#", "#.#", "###"],
    "V": ["#.#", "#.#", "#.#", "#.#", "#.#", "#.#", ".#."],
    "W": ["#.#", "#.#", "#.#", "#.#", "###", "###", "#.#"],
    "X": ["#.#", "#.#", "#.#", ".#.", "#.#", "#.#", "#.#"],
    "Y": ["#.#", "#.#", "#.#", ".#.", ".#.", ".#.", ".#."],
    "Z": ["###", "..#", "..#", ".#.", "#..", "#..", "###"],
    "[": ["##.", "#..", "#..", "#..", "#..", "#..", "##."],
    "]": [".##", "..#", "..#", "..#", "..#", "..#", ".##"],
    "_": ["...", "...", "...", "...", "...", "...", "###"],
}


def font_table(font, width):
    """ASCII 32-95; a character the font lacks is blank."""
    blank = ["." * width] * 7
    return [font.get(chr(c), blank) for c in range(32, 96)]


def font3():
    return font_table(F3, 3)


def font5():
    return font_table(F5, 5)


# ---- the logo --------------------------------------------------------------

LOGO_TEXT = "FUJIBLAST"


def logo():
    """140x24 on the menu's dark blue: FONT5 letters, each font pixel 2 wide
    and 3 tall, lit white on top, yellow to orange to red down the letter,
    a black shadow under and to the right. 12 px a letter, centred."""
    f5 = font5()
    w = 140
    img = [["b"] * w for _ in range(24)]
    x0 = (w - len(LOGO_TEXT) * 12) // 2
    shade = ["y", "y", "y", "o", "o", "m", "m"]
    lit = [[False] * w for _ in range(24)]
    for i, ch in enumerate(LOGO_TEXT):
        g = f5[ord(ch) - 32]
        for r, row in enumerate(g):
            for c, p in enumerate(row):
                if p != "#":
                    continue
                top = r == 0 or g[r - 1][c] != "#"
                for dy in range(3):
                    for dx in range(2):
                        x, y = x0 + i * 12 + 1 + c * 2 + dx, 1 + r * 3 + dy
                        img[y][x] = "w" if dy == 0 and top else shade[r]
                        lit[y][x] = True
    out = [row[:] for row in img]
    for y in range(24):
        for x in range(w):
            if not lit[y][x] and ((x > 0 and lit[y][x - 1]) or (y > 0 and lit[y - 1][x])):
                out[y][x] = "k"
    return ["".join(r) for r in out]


# ---- the bombers (7x16) ----------------------------------------------------
# A round helmet (the seat's colour, shaded on the right), a yellow visor
# with two dark eyes, a suit with a black belt, black gloves and boots.

def sprite_frames():
    down = [
        "..###..",
        ".####+.",
        "#####++",
        "#####++",
        "#vkvkv+",
        "#vvvvv+",
        ".####+.",
        "..k+k..",
        ".##+##.",
        "#######",
        "#.kkk.#",
        "k.##+.k",
        "..#.+..",
        "..#.+..",
        ".kk.kk.",
        ".kk.kk.",
    ]
    up = [
        "..###..",
        ".####+.",
        "#####++",
        "#####++",
        "#####++",
        "#+###++",
        ".#+++#.",
        "..k+k..",
        ".##+##.",
        "#######",
        "#.kkk.#",
        "k.##+.k",
        "..#.+..",
        "..#.+..",
        ".kk.kk.",
        ".kk.kk.",
    ]
    right = [
        "..###..",
        ".####+.",
        "#####++",
        "####+++",
        "##+vkvv",
        "##+vvvv",
        ".####+.",
        "..k+k..",
        "..##+..",
        ".###++.",
        ".#kkk..",
        "..k#+..",
        "..#++..",
        "..#.+..",
        "..k.kk.",
        ".kk.kkk",
    ]

    def legs(f, a, b, c):
        return f[:13] + [a, b, c]

    def mirror(f):
        return [r[::-1] for r in f]

    fr = {}
    fr["down0"] = down
    fr["down1"] = legs(down, "..#.+..", ".kk..+.", ".kk.kk.")
    fr["down2"] = legs(down, "..#.+..", ".#..kk.", ".kk.kk.")
    fr["up0"] = up
    fr["up1"] = legs(up, "..#.+..", ".kk..+.", ".kk.kk.")
    fr["up2"] = legs(up, "..#.+..", ".#..kk.", ".kk.kk.")
    fr["right0"] = right
    fr["right1"] = right[:13] + [".#...+.", ".k...kk", "kk...kk"]
    fr["right2"] = right[:13] + ["..#+...", "..kk...", "..kkk.."]
    for k in "012":
        fr["left" + k] = mirror(fr["right" + k])
    # cheering: arms up, then a hop
    fr["win0"] = [
        "k.###.k",
        "#####+#",
        "#####++",
        "#####++",
        "#vkvkv+",
        "#vvvvv+",
        ".####+.",
        "..k+k..",
        "..#+#..",
        "..###..",
        "..kkk..",
        "..##+..",
        "..#.+..",
        "..#.+..",
        ".kk.kk.",
        ".kk.kk.",
    ]
    fr["win1"] = ["......."] + fr["win0"][:12] + [".#...+.", ".kk.kk.", "......."]
    # knocked out: a spin (back, side), dazed and squashed, then blown away
    fr["die0"] = fr["up0"]
    fr["die1"] = fr["left0"]
    fr["die2"] = [
        ".......",
        "..v.v..",
        ".v.v.v.",
        "..###..",
        ".####+.",
        "#####++",
        "#vkvkv+",
        "#vvvvv+",
        ".####+.",
        "#######",
        "k.kkk.k",
        ".##.+#.",
        ".kk.kk.",
        ".......",
        ".......",
        ".......",
    ]
    fr["die3"] = [
        ".......",
        ".......",
        ".......",
        "...k...",
        ".k...k.",
        "..k.k..",
        "k.#.+.k",
        ".#.v.+.",
        "k.+.#.k",
        "..k.k..",
        ".k...k.",
        "...k...",
        ".......",
        ".......",
        ".......",
        ".......",
    ]
    return fr


# ---- writing ---------------------------------------------------------------

def put(img, x0, y0, rows, colour):
    for y, row in enumerate(rows):
        for x, p in enumerate(row):
            c = colour(p)
            if c is not None:
                img.putpixel((x0 + x, y0 + y), c)


def write_sheet(out):
    os.makedirs(out, exist_ok=True)
    tiles = tile_frames()
    img = Image.new("RGB", (4 * W, len(TILES) * 16), PAL16[FLOOR])
    for i, name in enumerate(TILES):
        frames = tiles[name]
        for f, rows in enumerate(frames):
            assert len(rows) == 16 and all(len(r) == W for r in rows), (name, f)
            put(img, f * W, i * 16, rows, rgb)
    img.save(os.path.join(out, "tiles.png"))

    img = Image.new("RGB", (4, 4), PAL16[0])
    put(img, 0, 0, WALL, rgb)
    img.save(os.path.join(out, "wall.png"))

    for name, glyphs, gw in (("font3", font3(), 4), ("font5", font5(), 7)):
        assert len(glyphs) == 64, (name, len(glyphs))
        img = Image.new("RGB", (64 * gw, 8), PAL16[0])
        for i, g in enumerate(glyphs):
            assert len(g) == 7, (name, i)
            put(img, i * gw, 0, g, lambda p: PAL16[15] if p == "#" else None)
        img.save(os.path.join(out, name + ".png"))

    rows = logo()
    img = Image.new("RGB", (len(rows[0]), 24), PAL16[2])
    put(img, 0, 0, rows, rgb)
    img.save(os.path.join(out, "logo.png"))

    frames = sprite_frames()
    img = Image.new("RGBA", (len(SPRITES) * W, 16), SPRITE_ROLE_RGB["."])
    for i, name in enumerate(SPRITES):
        f = frames[name]
        assert len(f) == 16 and all(len(r) == W for r in f), name
        put(img, i * W, 0, f, lambda p: SPRITE_ROLE_RGB[p] if p != "." else None)
    img.save(os.path.join(out, "sprites.png"))


# ---- previews: mock screens at the DHGR pixel aspect ----------------------

def sprite_rgb(p, seat):
    body, shade = SEAT_COLOURS[seat]
    return {"#": PAL16[body], "+": PAL16[shade], "v": PAL16[VISOR], "k": PAL16[0]}.get(p)


def text(scr, x, y, s, colour, glyphs, gw):
    for ch in s:
        g = glyphs[ord(ch.upper()) - 32]
        for r, row in enumerate(g):
            for c, p in enumerate(row):
                if p == "#" and 0 <= x + c < 140:
                    scr[y + r][x + c] = colour
        x += gw


def mock_arena():
    scr = [[0] * 140 for _ in range(192)]
    tiles = tile_frames()
    f3, f5 = font3(), font5()
    # HUD: four names and wins in seat colours, the timer in the middle
    hud = [("ANN1", 0), ("BOB0", 1), ("2:41", 15), ("CAT2", 2), ("DAN1", 3)]
    x = 0
    for s, c in hud:
        col = SEAT_COLOURS[c][0] if c < 4 else 15
        text(scr, x + (2 if len(s) < 4 else 0), 0, s, col, f5, 7)
        x += 28
    # walls
    for y in range(8, 192):
        for xx in range(24, 123):
            inside = 28 <= xx <= 118 and 12 <= y <= 187
            if not inside:
                scr[y][xx] = CODE[WALL[y % 4][xx % 4]]
    # the map: pillars on odd tiles, some blocks, one of every tile
    import random
    rnd = random.Random(7)
    grid = [["floor"] * 13 for _ in range(11)]
    for r in range(11):
        for c in range(13):
            if r % 2 and c % 2:
                grid[r][c] = "pillar"
            elif rnd.random() < 0.55 and not (r < 2 and c < 2) and not (r > 8 and c > 10):
                grid[r][c] = "block"
    for (c, r) in [(0, 0), (1, 0), (0, 1), (12, 10), (11, 10), (12, 9), (12, 0), (0, 10)]:
        grid[r][c] = "floor"
    # a cross of fire
    grid[4][6] = "flameC"
    for c in (4, 5, 7, 8):
        grid[4][c] = "flameH"
    grid[4][3], grid[4][9] = "flameL", "flameR"
    grid[2][6], grid[6][6] = "flameU", "flameD"
    grid[3][6] = grid[5][6] = "flameV"
    grid[4][2] = "burn"
    grid[8][2], grid[8][4] = "bomb", "dud"
    for i, p in enumerate(("fire", "bombup", "speed", "kick", "glove")):
        grid[0][4 + i] = p
    grid[10][6] = "bomb"
    for r in range(11):
        for c in range(13):
            name = grid[r][c]
            fr = tiles[name][0] if name not in ("fire", "bombup", "speed", "kick", "glove") \
                else tiles[name][1 if c % 2 else 0]
            for y, row in enumerate(fr):
                for xx, p in enumerate(row):
                    scr[12 + r * 16 + y][28 + c * 7 + xx] = CODE[p]
    # the side panels: stats on the left, the round on the right
    for i, name in enumerate(("fire", "bombup", "speed", "kick", "glove")):
        y0 = 16 + i * 32
        for y, row in enumerate(tiles[name][1]):
            for xx, p in enumerate(row):
                scr[y0 + y][1 + xx] = CODE[p]
        if i < 3:
            text(scr, 9, y0 + 8, "X" + str([3, 2, 1][i]), 15, f3, 4)
    text(scr, 125, 20, "RND", 15, f3, 4)
    text(scr, 129, 30, "3", 13, f3, 4)
    # four bombers, one walking between tiles
    frames = sprite_frames()
    for seat, (name, px, py) in enumerate([("down0", 28 + 0 * 7, 12), ("right1", 28 + 12 * 7 - 3, 12 + 10 * 16),
                                           ("left0", 28 + 6 * 7 + 4, 12 + 8 * 16), ("up2", 28 + 12 * 7, 12 + 6)]):
        for y, row in enumerate(frames[name]):
            for xx, p in enumerate(row):
                c = sprite_rgb(p, seat)
                if c is not None:
                    scr[py + y][px + xx] = PAL16.index(c)
    return scr


def mock_title():
    scr = [[2] * 140 for _ in range(192)]
    f3, f5 = font3(), font5()
    lg = logo()
    for y, row in enumerate(lg):
        for x, p in enumerate(row):
            scr[16 + y][x] = CODE[p]
    s = "FOR FUJINET"
    text(scr, (140 - len(s) * 7) // 2, 56, s, 13, f5, 7)
    s = "PRESS FIRE"
    text(scr, (35 - len(s)) // 2 * 4, 136, s, 15, f3, 4)
    s = "BOMBS, BLOCKS AND BLASTS FOR 2-4"
    text(scr, (35 - len(s)) // 2 * 4, 160, s, 15, f3, 4)
    frames = sprite_frames()
    for seat in range(4):
        for y, row in enumerate(frames[["right0", "right1", "right0", "right2"][seat]]):
            for xx, p in enumerate(row):
                c = sprite_rgb(p, seat)
                if c is not None:
                    scr[88 + y][20 + seat * 28 + xx] = PAL16.index(c)
    s = "ROUND 3 OF 5: ANN WINS!"
    text(scr, (35 - len(s)) // 2 * 4, 176, s, 14, f3, 4)
    return scr


def mock_sprites():
    frames = sprite_frames()
    scr = [[4] * (len(SPRITES) * 8 + 2) for _ in range(4 * 20 + 4)]
    for seat in range(4):
        for i, name in enumerate(SPRITES):
            for y, row in enumerate(frames[name]):
                for xx, p in enumerate(row):
                    c = sprite_rgb(p, seat)
                    if c is not None:
                        scr[2 + seat * 20 + y][1 + i * 8 + xx] = PAL16.index(c)
    return scr


def save_screen(scr, path, sx=8, sy=4):
    """Each DHGR pixel as sx by sy (8x4 ~ the 1.8:1 aspect, doubled)."""
    h, w = len(scr), len(scr[0])
    img = Image.new("RGB", (w * sx, h * sy))
    px = img.load()
    for y in range(h):
        for x in range(w):
            c = PAL16[scr[y][x]]
            for dy in range(sy):
                for dx in range(sx):
                    px[x * sx + dx, y * sy + dy] = c
    img.save(path)


def main():
    write_sheet(os.path.join(ROOT, "gfx", "dhgr"))
    print("wrote gfx/dhgr/tiles.png wall.png font3.png font5.png logo.png sprites.png")
    if len(sys.argv) > 2 and sys.argv[1] == "--preview":
        d = sys.argv[2]
        os.makedirs(d, exist_ok=True)
        save_screen(mock_arena(), os.path.join(d, "arena.png"))
        save_screen(mock_title(), os.path.join(d, "title.png"))
        save_screen(mock_sprites(), os.path.join(d, "sprites.png"), 14, 8)
        print("previews in", d)


if __name__ == "__main__":
    main()

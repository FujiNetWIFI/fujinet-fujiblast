#!/usr/bin/env python3
"""drawart_cga.py -- FujiBlast's art for CGA (320x200, 4 colours), written
out as gfx/cga/*.png (the MS-DOS port's sheet; tools/mkgfx_cga.py converts
it).

A CGA pixel is half an Atari colour clock, so a tile is 16x16, the Atari
tile's size on the screen. The designs are the master sheet's
(tools/drawart.py), redrawn in CGA's four colours. All of it is original art.

Colours are the pixel values, one letter each:
  .  0, the background (any of the 16, set per screen through the BIOS)
  c  1   r  2   w  3   (in mode 5: cyan, red, white)
Sprites are in roles; each seat colours them its own way (SEATS):
  #  body    t  trim (gloves, boots, belt)    v  visor    e  eyes (pixel 0)
  .  clear
Text is the Namco arcade font (gfx/namco.bin, 8x8).

    python3 tools/drawart_cga.py              # rewrites gfx/cga/*.png
    python3 tools/drawart_cga.py --preview D  # also mock screens into D
"""
import os
import random
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from artlayout import TILES, SPRITES, LOGO_TEXT  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

# The 16 RGBI colours as an RGB monitor (the 5153) shows them.
RGBI = [
    (0x00, 0x00, 0x00), (0x00, 0x00, 0xAA), (0x00, 0xAA, 0x00), (0x00, 0xAA, 0xAA),
    (0xAA, 0x00, 0x00), (0xAA, 0x00, 0xAA), (0xAA, 0x55, 0x00), (0xAA, 0xAA, 0xAA),
    (0x55, 0x55, 0x55), (0x55, 0x55, 0xFF), (0x55, 0xFF, 0x55), (0x55, 0xFF, 0xFF),
    (0xFF, 0x55, 0x55), (0xFF, 0x55, 0xFF), (0xFF, 0xFF, 0x55), (0xFF, 0xFF, 0xFF),
]

# The foreground sets a real CGA has, as pixel values 1-3 (RGBI numbers).
TRIPLES = {
    "mode5": (11, 12, 15),      # mode 5, bright: light cyan, light red, white
    "pal1": (11, 13, 15),       # mode 4 palette 1, bright: + light magenta
    "pal0": (10, 12, 14),       # mode 4 palette 0, bright: green, red, yellow
}

# The sheet's own key: the pixel values drawn in mode 5's colours on black.
KEY = [RGBI[0], RGBI[11], RGBI[12], RGBI[15]]
VAL = {".": 0, "c": 1, "r": 2, "w": 3}

# Sprite roles in sprites.png (mkgfx_cga.py reads them back as roles).
ROLE_RGBA = {
    ".": (0, 0, 0, 0),
    "#": (255, 255, 255, 255),
    "t": (255, 85, 85, 255),
    "v": (85, 255, 255, 255),
    "e": (0, 0, 0, 255),
}
# Per seat: the pixel value for body, trim, visor (the eyes are 0). Body
# "s" is the fourth seat's stripes: 1 on even lines, 2 on odd.
SEATS = [("w", "r", "c"), ("r", "w", "c"), ("c", "w", "r"), ("s", "w", "w")]

T = 16          # a tile is 16x16


def shape(fn, w=T, h=T):
    return ["".join(fn(x, y) for x in range(w)) for y in range(h)]


def check(name, rows, w=T, h=T):
    assert len(rows) == h and all(len(r) == w for r in rows), (name, [len(r) for r in rows])


# ---- tiles (16x16) ----------------------------------------------------------

PILLAR = [
    ".wwwwwwwwwwwwww.",
    "wwwwwwwwwwwwwwwc",
    "wwcwcwcwcwcwcwcc",
    "wwwcwcwcwcwcwccc",
    "wwcwcwcwcwcwcwcc",
    "wwwcwcwcwcwcwccc",
    "wwcwcwcwcwcwcwcc",
    "wwwcwcwcwcwcwccc",
    "wwcwcwcwcwcwcwcc",
    "wwwcwcwcwcwcwccc",
    "wwcwcwcwcwcwcwcc",
    "wwwcwcwcwcwcwccc",
    "wwcwcwcwcwcwcwcc",
    "wwwcccccccccccc.",
    "wcccccccccccccc.",
    ".cccccccccccccc.",
]


def bricks(fill="r", top="w"):
    """Three courses of brick, 5 lines each, joints staggered by 4 px."""
    rows = []
    for course in range(3):
        joints = (7, 15) if course % 2 == 0 else (3, 11)
        for line in range(5):
            if line == 4:
                rows.append("." * T)
                continue
            rows.append("".join("." if x in joints else (top if line == 0 else fill) for x in range(T)))
    rows.append("." * T)
    return rows


BLOCK = bricks()


def burn(f):
    """The block on fire: bricks gone white-hot, tongues of flame on top."""
    rnd = random.Random(11 + f)
    b = [list(r) for r in bricks("r", "w")]
    for y in range(T):
        for x in range(T):
            if b[y][x] == "r" and rnd.random() < 0.35:
                b[y][x] = "w"
    tongues = [3, 8, 12] if f == 0 else [1, 6, 10, 14]
    for tx in tongues:
        h = rnd.randint(4, 6)
        for y in range(h):
            half = 1 if y < h - 2 else 0
            for x in range(tx - half, tx + half + 1):
                if 0 <= x < T:
                    b[y][x] = "w" if y >= h - 2 else "r"
    return ["".join(r) for r in b]


def ball(cx, cy, rx, ry, x, y):
    dx, dy = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
    return dx * dx + dy * dy


def bomb(spark, small):
    """A steel bomb: shine top left, a red fuse, a spark on the tip."""
    rx, ry = (6.5, 5.6) if not small else (5.5, 4.8)
    cx, cy = 8.0, 10.0

    def f(x, y):
        q = ball(cx, cy, rx, ry, x, y)
        if q <= 1.0:
            s = ball(cx - rx * 0.45, cy - ry * 0.45, rx * 0.35, ry * 0.3, x, y)
            return "w" if s <= 1.0 else "c"
        return "."
    rows = [list(r) for r in shape(f)]
    top = int(cy - ry) - 1
    fuse = [(10, top), (11, top - 1), (11, top - 2)]
    for x, y in fuse:
        if 0 <= y:
            rows[y][x] = "r"
    tip = top - 3
    sparks = [[(11, tip), (10, tip - 1), (12, tip - 1)], [(11, tip), (12, tip), (11, tip - 1), (10, tip)]][spark]
    for i, (x, y) in enumerate(sparks):
        if 0 <= y:
            rows[y][x] = "w" if i == 0 else "r"
    return ["".join(r) for r in rows]


def dud(puff):
    """A dead bomb: stippled, a wisp of smoke instead of a spark."""
    rx, ry, cx, cy = 6.5, 5.6, 8.0, 10.0

    def f(x, y):
        if ball(cx, cy, rx, ry, x, y) <= 1.0:
            return "c" if (x + y) % 2 == 0 else "."
        return "."
    rows = [list(r) for r in shape(f)]
    top = int(cy - ry) - 1
    rows[top][10] = "r"
    rows[top - 1][11] = "r"
    smoke = [
        [(11, 2), (10, 1), (12, 1), (11, 0)],
        [(10, 2), (11, 1), (9, 0), (10, 0)],
        [(12, 2), (12, 1), (13, 0)],
        [(11, 2)],
    ][puff]
    for x, y in smoke:
        rows[y][x] = "w"
    return ["".join(r) for r in rows]


def flame(kind, thin):
    """Arms: white at the core, red at the edge. Thin: the flicker frame,
    a pixel narrower all round."""
    t = 1 if thin else 0
    cy, cx = 7.5, 7.5

    def wob(a):
        """The edge's flicker along the arm (a: position along it)."""
        return (0.0, 0.8, 0.0, -0.8)[(a + (2 if thin else 0)) % 4]

    def band(d, half, a=0):
        half -= t
        if d > half + wob(a):
            return None
        return "w" if d <= half - 2.5 - wob(a + 1) * 0.5 else "r"

    def h(x, y):
        return band(abs(y - cy), 5.5, x)

    def v(x, y):
        return band(abs(x - cx), 5.5, y)

    def f(x, y):
        r = None
        if kind == "C":
            q = ball(cx + 0.5, cy + 0.5, 7.6 - t, 7.6 - t, x, y)
            if q <= 1.0:
                r = "w" if q <= 0.45 else "r"
            r = r or h(x, y) or v(x, y)
        elif kind == "H":
            r = h(x, y)
        elif kind == "V":
            r = v(x, y)
        elif kind in "LR":
            d = x if kind == "L" else T - 1 - x      # 0 at the tip
            r = band(abs(y - cy), 5.5 - max(0, 5 - d) * 0.9, x)
        elif kind in "UD":
            d = y if kind == "U" else T - 1 - y
            r = band(abs(x - cx), 5.5 - max(0, 5 - d) * 0.9, y)
        return r or "."
    return shape(f)


def panel(icon, lit):
    """A powerup: a red panel with a rim (lit: white, else cyan), the icon
    (12x12) inside."""
    edge = "w" if lit else "c"
    rows = ["." + edge * 14 + "."]
    for r in range(14):
        inner = icon[r - 1] if 1 <= r <= 12 else "." * 12
        rows.append(edge + "r" + inner.replace(".", "r") + "r" + edge)
    rows.append("." + edge * 14 + ".")
    return rows


FIRE = [
    "......w.....",
    ".....ww.....",
    ".....www..w.",
    "..w.wwww.ww.",
    "..wwwwwwwww.",
    ".wwwwwrwwww.",
    ".wwwwrrrwww.",
    "wwwwrrrrrwww",
    "wwwrrcrrrwww",
    "wwwrccccrww.",
    ".wwwccccwww.",
    "..wwwwwwww..",
]
BOMBUP = [
    "........w...",
    ".......w.w..",
    "......c.....",
    "....cccc..w.",
    "...cwwcc.www",
    "..cwwccc..w.",
    "..cwcccccc..",
    "..cccccccc..",
    "..cccccccc..",
    "...cccccc...",
    "....cccc....",
    "............",
]
SPEED = [
    "............",
    "ww....ww....",
    "cww...cww...",
    ".cww...cww..",
    "..cww...cww.",
    "...cww...cww",
    "...cww...cww",
    "..cww...cww.",
    ".cww...cww..",
    "cww...cww...",
    "ww....ww....",
    "............",
]
KICK = [
    "..www.......",
    "..www.......",
    "..www.......",
    "..www.......",
    "..www.......",
    "..wwww......",
    "..wwwwww....",
    "..wwwwwwww..",
    "..wwwwwwwww.",
    "..ccccccccc.",
    "........ww..",
    ".......ww...",
]
GLOVE = [
    "..w.w.w.....",
    "..w.w.w.w...",
    "..w.w.w.w...",
    "..wwwwwww...",
    "w.wwwwwww...",
    "wwwwwwwww...",
    ".wwwwwwww...",
    "..wwwwwww...",
    "...wwwww....",
    "...ccccc....",
    "...ccccc....",
    "............",
]

TROPHY = [
    "................",
    "..wwwwwwwwwwww..",
    "cww.wwwwwwwwcwwc",
    "c.w.wwwwwwwwcw.c",
    "c..wwwwwwwwwc..c",
    ".c.wwwwwwwwwc.c.",
    "..cwwwwwwwwwcc..",
    "....wwwwwwcc....",
    "......wwcc......",
    "......wwc.......",
    "......wwc.......",
    ".....wwwcc......",
    "....rrrrrrrr....",
    "....rwwwwwwr....",
    "....rrrrrrrr....",
    "................",
]


def tile_frames():
    t = {
        "floor": [["." * T] * T],
        "pillar": [PILLAR],
        "block": [BLOCK],
        "burn": [burn(0), burn(1)],
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


# The arena's frame, an 8x8 cell of stone; and the side panels' cell.
WALL = [
    "wwwwwwwc",
    "wcccccc.",
    "wcccccc.",
    "c.......",
    "wwwcwwww",
    "ccc.wccc",
    "ccc.wccc",
    "........",
]
PANEL = ["." * 8] * 8

# ---- the Namco font --------------------------------------------------------


def namco():
    """ASCII 32-127, 8x8, as rows of '#' and '.'."""
    d = open(os.path.join(ROOT, "gfx", "namco.bin"), "rb").read()
    assert len(d) == 768, len(d)
    return [[format(b, "08b").replace("0", ".").replace("1", "#") for b in d[i * 8:i * 8 + 8]]
            for i in range(96)]


def glyph(font, ch):
    c = ord(ch.upper())
    return font[c - 32] if 32 <= c < 128 else font[0]


# ---- the logo --------------------------------------------------------------


def logo():
    """216x24: the Namco letters at 3x, lit white on top, red, a cyan
    shadow down and to the right. 24 px a letter (3 cells)."""
    font = namco()
    w = len(LOGO_TEXT) * 24
    lit = [[None] * w for _ in range(24)]
    for i, ch in enumerate(LOGO_TEXT):
        g = glyph(font, ch)
        for r, row in enumerate(g):
            for c, p in enumerate(row):
                if p != "#":
                    continue
                top = r == 0 or g[r - 1][c] != "#"
                for dy in range(3):
                    for dx in range(3):
                        lit[r * 3 + dy][i * 24 + c * 3 + dx] = "w" if top and dy == 0 else "r"
    out = [["."] * w for _ in range(24)]
    for y in range(24):
        for x in range(w):
            if lit[y][x]:
                out[y][x] = lit[y][x]
            elif (x >= 2 and y >= 2 and lit[y - 2][x - 2]) or (x >= 1 and y >= 1 and lit[y - 1][x - 1]):
                out[y][x] = "c"
    return ["".join(r) for r in out]


# ---- the bombers (16x16) ---------------------------------------------------
# A round helmet in the seat's colour, a visor across it with two eyes, a
# suit with a belt, gloves and boots in the trim colour.

HEAD = [
    ".....######.....",
    "...##########...",
    "..############..",
    ".##############.",
    ".#vvvvvvvvvvvv#.",
    ".#vveevvvveevv#.",
    ".#vveevvvveevv#.",
    ".##############.",
    "..############..",
]
BACK = [
    ".....######.....",
    "...##########...",
    "..############..",
    ".##############.",
    ".##############.",
    ".##############.",
    ".######tt######.",
    ".######tt######.",
    "..############..",
]
SIDE = [
    ".....######.....",
    "...##########...",
    "..############..",
    ".#######vvvvvvv.",
    ".######vvvvveev.",
    ".######vvvvveev.",
    ".#######vvvvvvv.",
    "..############..",
    "...##########...",
]
TORSO = [
    "...t########t...",
    "..tt########tt..",
    "..tt#tttttt#tt..",
]
TORSO_SIDE = [
    ".....######.....",
    ".....###tt##....",
    ".....tttttt.....",
]
LEGS = {
    "stand": ["....###..###....", "....###..###....", "...tttt..tttt...", "...tttt..tttt..."],
    "stepL": ["....###..###....", "...tttt..###....", "...tttt..tttt...", ".........tttt..."],
    "stepR": ["....###..###....", "....###..tttt...", "...tttt..tttt...", "...tttt........."],
    "side0": [".....######.....", ".....###.###....", ".....tttt.ttt...", ".....tttt.tttt.."],
    "side1": ["....###...###...", "...###.....###..", "..tttt......ttt.", "..ttt.......tttt"],
    "side2": [".....#####......", "......####......", ".....tttttt.....", ".....ttttttt...."],
}


def mirror(f):
    return [r[::-1] for r in f]


def sprite_frames():
    fr = {}
    for k, legs in (("0", "stand"), ("1", "stepL"), ("2", "stepR")):
        fr["down" + k] = HEAD + TORSO + LEGS[legs]
        fr["up" + k] = BACK + TORSO + LEGS[legs]
    for k, legs in (("0", "side0"), ("1", "side1"), ("2", "side2")):
        fr["right" + k] = SIDE + TORSO_SIDE + LEGS[legs]
        fr["left" + k] = mirror(fr["right" + k])
    # cheering: gloves up, then a hop
    arms = [
        "tt............tt",
        "tt....####....tt",
        ".#..########..#.",
        ".#.##########.#.",
        ".##vvvvvvvvvv##.",
        "..#vveevveevv#..",
        "..#vveevveevv#..",
        "..############..",
        "...##########...",
    ]
    fr["win0"] = arms + ["....########....", "....########....", "....#tttttt#...."] + LEGS["stand"]
    fr["win1"] = fr["win0"][1:12] + ["....###..###....", "...tttt..tttt...", "...tttt..tttt...", "." * 16, "." * 16]
    fr["win1"] = fr["win1"][:16]
    # knocked out: a spin (back, side), dazed and squashed, then blown apart
    fr["die0"] = fr["up0"]
    fr["die1"] = fr["left0"]
    fr["die2"] = [
        "." * 16,
        "...v...v...v....",
        "..vvv.vvv.vvv...",
        "...v...v...v....",
        "....########....",
        "..############..",
        ".##############.",
        ".#vveevvvveevv#.",
        ".#vvvvvvvvvvvv#.",
        ".##############.",
        "tt############tt",
        "tt#tttttttttt#tt",
        "..tttttttttttt..",
        "..tttt....tttt..",
        "." * 16,
        "." * 16,
    ]
    fr["die3"] = [
        "." * 16,
        "....t......t....",
        ".....#....#.....",
        "..#...........#.",
        "...v...##...v...",
        ".......##.......",
        ".#...vv..vv...#.",
        ".....vv..vv.....",
        ".......##.......",
        "...t...##...t...",
        "..#...........#.",
        ".....#....#.....",
        "....t......t....",
        "." * 16,
        "." * 16,
        "." * 16,
    ]
    return fr


# ---- writing ---------------------------------------------------------------


def put(img, x0, y0, rows, colour):
    for y, row in enumerate(rows):
        for x, p in enumerate(row):
            c = colour(p)
            if c is not None:
                img.putpixel((x0 + x, y0 + y), c)


def key(p):
    return KEY[VAL[p]]


def write_sheet(out):
    os.makedirs(out, exist_ok=True)
    tiles = tile_frames()
    img = Image.new("RGB", (4 * T, len(TILES) * T), KEY[0])
    for i, name in enumerate(TILES):
        for f, rows in enumerate(tiles[name]):
            check(name, rows)
            put(img, f * T, i * T, rows, key)
    img.save(os.path.join(out, "tiles.png"))

    img = Image.new("RGB", (16, 8), KEY[0])
    for i, cell in enumerate((WALL, PANEL)):
        check("cell", cell, 8, 8)
        put(img, i * 8, 0, cell, key)
    img.save(os.path.join(out, "cells.png"))

    rows = logo()
    img = Image.new("RGB", (len(rows[0]), 24), KEY[0])
    put(img, 0, 0, rows, key)
    img.save(os.path.join(out, "logo.png"))

    frames = sprite_frames()
    img = Image.new("RGBA", (len(SPRITES) * T, T), ROLE_RGBA["."])
    for i, name in enumerate(SPRITES):
        check(name, frames[name])
        put(img, i * T, 0, frames[name], lambda p: ROLE_RGBA[p] if p != "." else None)
    img.save(os.path.join(out, "sprites.png"))


# ---- previews: mock screens as a CGA on an RGB monitor ---------------------
# A screen is 200 rows of 320 pixel values (0-3) plus the BIOS palette it
# would set: the triple, and the background.

class Screen:
    def __init__(self, bg):
        self.px = [[0] * 320 for _ in range(200)]
        self.bg = bg

    def plot(self, x, y, v):
        if 0 <= x < 320 and 0 <= y < 200:
            self.px[y][x] = v

    def rows(self, x0, y0, rows, colour=lambda p: VAL.get(p)):
        for y, row in enumerate(rows):
            for x, p in enumerate(row):
                v = colour(p)
                if v is not None:
                    self.plot(x0 + x, y0 + y, v)

    def text(self, col, row, s, v, font, inverse=None):
        for i, ch in enumerate(s):
            g = glyph(font, ch)
            for y in range(8):
                for x in range(8):
                    on = g[y][x] == "#"
                    if inverse is not None:
                        self.plot((col + i) * 8 + x, row * 8 + y, v if on else inverse)
                    elif on:
                        self.plot((col + i) * 8 + x, row * 8 + y, v)

    def big(self, col, row, s, v, font):
        """BIG text: 20 columns, each glyph doubled in width."""
        for i, ch in enumerate(s):
            g = glyph(font, ch)
            for y in range(8):
                for x in range(8):
                    if g[y][x] == "#":
                        vv = v if v != "s" else (1 if y % 2 == 0 else 2)
                        self.plot((col + i) * 16 + x * 2, row * 8 + y, vv)
                        self.plot((col + i) * 16 + x * 2 + 1, row * 8 + y, vv)

    def sprite(self, x0, y0, rows, seat, big=False):
        body, trim, visor = SEATS[seat]
        n = 2 if big else 1
        for y, row in enumerate(rows):
            for x, p in enumerate(row):
                if p == ".":
                    continue
                for dy in range(n):
                    yy = y0 + y * n + dy
                    if p == "#":
                        v = (1 if yy % 2 == 0 else 2) if body == "s" else VAL[body]
                    elif p == "t":
                        v = VAL[trim]
                    elif p == "v":
                        v = VAL[visor]
                    else:
                        v = 0
                    for dx in range(n):
                        self.plot(x0 + x * n + dx, yy, v)

    def save(self, path, triple, sx=5, sy=6):
        """Each CGA pixel as sx by sy (5x6: the 1.2:1 pixel of a 4:3 screen)."""
        pal = [RGBI[self.bg]] + [RGBI[c] for c in TRIPLES[triple]]
        img = Image.new("RGB", (320 * sx, 200 * sy))
        px = img.load()
        for y in range(200):
            for x in range(320):
                c = pal[self.px[y][x]]
                for dy in range(sy):
                    for dx in range(sx):
                        px[x * sx + dx, y * sy + dy] = c
        img.save(path)


HUD = [("ANN", 0, "1"), ("BOB", 1, "0"), ("CAT", 2, "2"), ("DAN", 3, "1")]
SEAT_V = ["w", "r", "c", "s"]


def mock_arena(bg):
    s = Screen(bg)
    font = namco()
    tiles = tile_frames()
    frames = sprite_frames()
    # the HUD: seat s owns BIG columns 0, 4, 12, 16; the timer is 8-11
    for (name, seat, wins), col in zip(HUD, (0, 4, 12, 16)):
        s.big(col, 0, name, VAL.get(SEAT_V[seat], "s") if SEAT_V[seat] != "s" else "s", font)
        s.big(col + 3, 0, wins, VAL["w"], font)
    s.big(8, 0, "2:41", VAL["w"], font)
    # the frame and the side panels
    for r in range(1, 25):
        for c in range(40):
            wall = (r in (1, 24) and 6 <= c <= 33) or (c in (6, 33))
            inside = 7 <= c <= 32 and 2 <= r <= 23
            if wall:
                s.rows(c * 8, r * 8, WALL)
            elif not inside:
                s.rows(c * 8, r * 8, PANEL)
    # the map: pillars on odd tiles, blocks, one of every tile
    rnd = random.Random(7)
    grid = [["floor"] * 13 for _ in range(11)]
    for r in range(11):
        for c in range(13):
            if r % 2 and c % 2:
                grid[r][c] = "pillar"
            elif rnd.random() < 0.5 and not (r < 2 and c < 2) and not (r > 8 and c > 10):
                grid[r][c] = "block"
    for (c, r) in [(0, 0), (1, 0), (0, 1), (12, 10), (11, 10), (12, 9), (12, 0), (0, 10), (6, 8), (5, 8), (7, 8)]:
        grid[r][c] = "floor"
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
            fr = tiles[name][(c % 2) if len(tiles[name]) > 1 and name not in ("bomb", "dud") else 0]
            if name == "bomb" and c == 6:
                fr = tiles[name][2]
            s.rows(56 + c * 16, 16 + r * 16, fr)
    # a sliding bomb, half a tile over
    s.rows(56 + 5 * 16 + 8, 16 + 8 * 16, tiles["bomb"][1])
    # the stats panel: icons at rows 3+4i, col 1; counts beside
    for i, name in enumerate(("fire", "bombup", "speed", "kick", "glove")):
        s.rows(8, (3 + i * 4) * 8, tiles[name][1])
        if i < 3:
            s.text(3, 4 + i * 4, "X" + str([3, 2, 1][i]), VAL["w"], font)
    s.text(35, 3, "RND", VAL["w"], font)
    s.text(36, 4, " 3", VAL["c"], font)
    # four bombers, two between tiles
    for seat, (name, px, py) in enumerate([("down0", 56, 16), ("right1", 56 + 11 * 16 + 6, 16 + 10 * 16),
                                           ("left2", 56 + 6 * 16 + 9, 16 + 6 * 16), ("up1", 56 + 12 * 16, 16 + 3 * 16 + 4)]):
        s.sprite(px, py, frames[name], seat)
    return s


def mock_title(bg):
    s = Screen(bg)
    font = namco()
    s.rows((320 - 216) // 2, 24, logo())
    s.big((20 - 11) // 2, 8, "FOR FUJINET", VAL["w"], font)
    frames = sprite_frames()
    for seat in range(4):
        s.sprite(40 + seat * 80, 96, frames[["right0", "right1", "right0", "right2"][seat]], seat)
    s.text((40 - 10) // 2, 18, "PRESS FIRE", VAL["w"], font)
    msg = "BOMBS, BLOCKS AND BLASTS FOR 2-4"
    s.text((40 - len(msg)) // 2, 21, msg, VAL["c"], font)
    s.text((40 - 10) // 2, 23, "ESC: LOBBY", VAL["r"], font)
    return s


def mock_wait(bg):
    s = Screen(bg)
    font = namco()
    frames = sprite_frames()
    s.big((20 - 10) // 2, 1, "ROOM BLAST1", VAL["w"], font)
    names = ["ANN", "BOB", "CAT", None]
    for seat in range(4):
        r = 4 + seat * 4
        if names[seat]:
            s.big(4, r, names[seat], VAL[SEAT_V[seat]] if SEAT_V[seat] != "s" else "s", font)
            if seat == 0:
                s.big(14, r, "HOST", VAL[SEAT_V[seat]], font)
            s.sprite(16, r * 8, frames["down0"], seat)
        else:
            s.big(4, r, "--------", "s", font)
    s.text(12, 20, "ROUNDS: 5", VAL["w"], font)
    s.text(23, 20, "(LEFT/RIGHT)", VAL["c"], font)
    s.text((40 - 19) // 2, 22, "PRESS FIRE TO START", VAL["r"], font, inverse=VAL["w"])
    s.text((40 - 22) // 2, 24, "ESC: LEAVE    M: SOUND", VAL["w"], font)
    return s


def mock_results(bg):
    s = Screen(bg)
    font = namco()
    frames = sprite_frames()
    tiles = tile_frames()
    s.big((20 - 8) // 2, 1, "ANN WINS!", VAL["w"], font)
    wins = [3, 1, 2, 0]
    for k, (name, seat) in enumerate((("ANN", 0), ("BOB", 1), ("CAT", 2), ("DAN", 3))):
        r = 4 + k * 5
        v = VAL[SEAT_V[seat]] if SEAT_V[seat] != "s" else "s"
        s.big(4, r, name, v, font)
        s.big(13, r, str(wins[k]), v, font)
        for t in range(wins[k]):
            s.rows((8 + t * 2) * 8, (r + 1) * 8, tiles["trophy"][0])
        s.sprite(16, r * 8, frames["down0"], seat)
    s.sprite(272, 96, frames["win0"], 0, big=True)
    s.text(8, 24, "BACK TO THE ROOM IN 9", VAL["w"], font)
    return s


def mock_sprites(bg):
    s = Screen(bg)
    frames = sprite_frames()
    for seat in range(4):
        for i, name in enumerate(SPRITES):
            s.sprite(4 + i * 17, 8 + seat * 24, frames[name], seat)
    tiles = tile_frames()
    y = 112
    x = 4
    for name in TILES:
        for f in tiles[name]:
            if x + 16 > 316:
                x, y = 4, y + 20
            s.rows(x, y, f)
            x += 17
    return s


# Each option: the triple, and the background of the arena, the title and
# the other screens (RGBI numbers, set with INT 10h AH=0Bh BH=0).
OPTIONS = {
    "a": ("mode5", 2, 1, 0),        # green floor, blue title, black menus
    "b": ("mode5", 0, 1, 1),        # black floor, blue title and menus
    "c": ("pal1", 2, 1, 0),         # palette 1 (magenta for red), green floor
}


def main():
    write_sheet(os.path.join(ROOT, "gfx", "cga"))
    print("wrote gfx/cga/tiles.png cells.png logo.png sprites.png")
    if len(sys.argv) > 2 and sys.argv[1] == "--preview":
        d = sys.argv[2]
        os.makedirs(d, exist_ok=True)
        for opt, (triple, arena, title, menus) in OPTIONS.items():
            mock_arena(arena).save(os.path.join(d, opt + "-arena.png"), triple)
            mock_title(title).save(os.path.join(d, opt + "-title.png"), triple)
            mock_wait(menus).save(os.path.join(d, opt + "-wait.png"), triple)
            mock_results(menus).save(os.path.join(d, opt + "-results.png"), triple)
            mock_sprites(arena).save(os.path.join(d, opt + "-sheet.png"), triple, 4, 5)
        print("previews in", d)


if __name__ == "__main__":
    main()

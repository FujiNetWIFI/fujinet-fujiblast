#!/usr/bin/env python3
"""drawart_nes.py -- FujiBlast's art for the NES (2C02): 2 bits a pixel,
written out as gfx/nes/*.png (the NES port's converter reads them).

The NES's rules, which the art keeps to:
  BG       8x8 cells, 2 bits a pixel; one palette per 16x16 attribute block;
           4 palettes of 3 colours plus one shared backdrop (slot 0).
  sprites  8x16; 4 palettes of 3 colours plus clear; 8 a line, 64 in all;
           they flip horizontally.
A tile is 16x16, one attribute block (the arena sits on even rows and
columns), so each tile picks its own palette. A bomber is two 8x16 sprites
in its seat's palette: four abreast make the 8 a line, so no overlay layer.

All of it is original art (the game is not Bomberman: no antenna, no
pom-pom, our own bricks and pillars).

Pixels are slots, one character each: . 0 (the backdrop: the floor), 1 2 3.
In every BG palette slot 1 is its text colour, so one copy of the Namco font
(gfx/namco.bin) in slot 1 writes any palette's colour.
Bombers are drawn in roles; each colour option (OPTIONS) maps them to slots:
  B body   S shade   H shine   V visor   E eyes   K black (outline, gloves,
  boots, belt)   . clear

    python3 tools/drawart_nes.py                # rewrites gfx/nes/*.png
    python3 tools/drawart_nes.py --preview D    # also mock screens into D
"""
import math
import os
import random
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drawart_cga as C  # noqa: E402  (the Namco font, the icon shapes)
from artlayout import TILES, SPRITES, LOGO_TEXT  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

# The 2C02 as FujiNet Go NES shows it (MesenCE's default "2C02" palette,
# Core/NES/NesDefaultVideoFilter.cpp).
NES_RGB = [
    0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00,
    0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
    0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
    0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
    0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22,
    0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
    0xFFFEFF, 0xC0DFFF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5,
    0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000,
]
# $0D is "blacker than black" (some TVs lose sync); the rest are duplicate
# blacks. $0F is the black to use.
BAD = {0x0D, 0x0E, 0x1D, 0x1E, 0x1F, 0x2E, 0x2F, 0x3E, 0x3F}


def rgb(n):
    v = NES_RGB[n]
    return (v >> 16, (v >> 8) & 255, v & 255)


# ---- palettes ---------------------------------------------------------------
# A set: the backdrop and four palettes of three (slots 1-3). Palette k
# serves the same seat on every screen: SEAT_PAL[seat] (slot 1 = its colour).

BG_ARENA = (0x1A, [
    (0x30, 0x10, 0x0F),     # P0 stone: white, grey, black (seat 0's white)
    (0x26, 0x16, 0x06),     # P1 brick: light red, red, dark red (seat 1)
    (0x27, 0x30, 0x16),     # P2 fire: orange, white, red (seat 3)
    (0x21, 0x30, 0x02),     # P3 items: sky blue, white, dark blue (seat 2)
])
BG_TITLE = (0x02, [
    (0x30, 0x10, 0x0F),     # T0 text: white
    (0x30, 0x38, 0x0F),     # T1 the logo's top: white, pale yellow, black shadow
    (0x27, 0x16, 0x0F),     # T2 the logo's bottom: orange, red, black shadow
    (0x21, 0x11, 0x30),     # T3 sky text
])
BG_MENU = (0x02, [
    (0x30, 0x10, 0x0F),     # M0 seat 0: white, grey, black
    (0x26, 0x16, 0x30),     # M1 seat 1: light red, red, white
    (0x27, 0x30, 0x17),     # M2 seat 3: orange, white, brown (the trophy)
    (0x21, 0x11, 0x30),     # M3 seat 2: sky blue, blue, white
])
SEAT_PAL = [0, 1, 3, 2]

# The seats' colours and their shades.
SEAT_COL = [0x30, 0x26, 0x21, 0x27]       # white, light red, sky blue, orange
SEAT_SHADE = [0x10, 0x16, 0x11, 0x17]     # grey, red, blue, brown

# Bomber colour options: the four sprite palettes and the roles' slots.
OPTIONS = {
    # A: black visor, white eyes, a white shine (seat 0: grey for white)
    "a": ([(0x30, 0x0F, 0x10)] + [(c, 0x0F, 0x30) for c in SEAT_COL[1:]],
          {"B": 1, "S": 1, "H": 3, "V": 2, "E": 3, "K": 2}),
    # B: shaded body, black visor, eyes in the seat's colour (the Adam look)
    "b": ([(c, s, 0x0F) for c, s in zip(SEAT_COL, SEAT_SHADE)],
          {"B": 1, "S": 2, "H": 1, "V": 3, "E": 1, "K": 3}),
    # C: white visor, black eyes (seat 0: a grey visor)
    "c": ([(0x30, 0x10, 0x0F)] + [(c, 0x30, 0x0F) for c in SEAT_COL[1:]],
          {"B": 1, "S": 1, "H": 2, "V": 2, "E": 3, "K": 3}),
}
OPTION = "a"            # the one sprites.png is written in (the user's pick)

# Each tile's palette (in BG_ARENA; the trophy's in BG_MENU, the same slots).
TILE_PAL = {
    "floor": 0, "pillar": 0, "block": 1, "burn": 2, "bomb": 0, "dud": 0,
    "fire": 2, "bombup": 0, "speed": 3, "kick": 1, "glove": 3, "trophy": 2,
}
for _k in "CHVLRUD":
    TILE_PAL["flame" + _k] = 2

T = 16


def shape(fn, w=T, h=T):
    return ["".join(fn(x, y) for x in range(w)) for y in range(h)]


def check(name, rows, w=T, h=T, chars=".123"):
    assert len(rows) == h and all(len(r) == w for r in rows), (name, [len(r) for r in rows])
    bad = {p for r in rows for p in r} - set(chars)
    assert not bad, (name, bad)


# ---- tiles (16x16) ----------------------------------------------------------

# P0: 1 white, 2 grey, 3 black. A raised stone, an engraved panel in it.
PILLAR = [
    "1111111111111112",
    "1111111111111123",
    "1122222222222233",
    "1122222222222233",
    "1122333333312233",
    "1122322222212233",
    "1122322222212233",
    "1122322222212233",
    "1122322222212233",
    "1122322222212233",
    "1122322222212233",
    "1122311111112233",
    "1122222222222233",
    "1122222222222233",
    "1233333333333333",
    "2333333333333333",
]


def bricks(face, top, mortar):
    """Four courses of 3 lines and a joint line, joints staggered by 4 px:
    the pattern repeats every 8 px, so blocks side by side join up."""
    rows = []
    for course in range(4):
        joints = (7, 15) if course % 2 == 0 else (3, 11)
        for line in range(4):
            if line == 3:
                rows.append(mortar * T)
                continue
            r = ""
            for x in range(T):
                if x in joints:
                    r += mortar
                elif line == 0 or (x - 1) % T in joints:
                    r += top
                else:
                    r += face
            rows.append(r)
    return rows


# P1: 1 light red (the lit top and left of each brick), 2 red, 3 dark red.
BLOCK = bricks("2", "1", "3")


def burn(f):
    """P2 (1 orange, 2 white, 3 red): the block on fire. The bricks glow
    orange in red joints, white-hot embers come and go, tongues of flame
    lick up off the top."""
    rnd = random.Random(5 + f)
    b = [list(r) for r in bricks("1", "2", "3")]
    for y in range(T):
        for x in range(T):
            if b[y][x] == "1" and rnd.random() < 0.12:
                b[y][x] = "2"
            elif b[y][x] == "2" and rnd.random() < 0.5:
                b[y][x] = "1"
    tongues = [(3, 6), (8, 7), (12, 5)] if f == 0 else [(1, 5), (6, 7), (10, 6), (14, 5)]
    for y in range(4):
        b[y] = ["." if p != "3" or y < 3 else p for p in b[y]]
    for tx, h in tongues:
        for y in range(h):
            half = 0 if y < 2 else 1
            for x in range(tx - half, tx + half + 1):
                if 0 <= x < T:
                    b[y][x] = "2" if (x == tx and 2 <= y < h - 1) else "1" if y < h - 1 else "3"
    return ["".join(r) for r in b]


def ball(cx, cy, r, x, y):
    dx, dy = x + 0.5 - cx, y + 0.5 - cy
    return (dx * dx + dy * dy) / (r * r)


def bomb(spark, small):
    """P0: a black ball, a white shine in a grey halo, a grey rim of
    reflected light low on the right, a grey fuse, a white spark."""
    r = 5.0 if small else 6.0
    cx, cy = 8.0, 10.0
    sx, sy = cx - r * 0.42, cy - r * 0.42

    def f(x, y):
        if ball(cx, cy, r, x, y) > 1.0:
            return "."
        s = ball(sx, sy, 1.0, x, y)
        if s <= 1.0:
            return "1"
        if s <= 3.6:
            return "2"
        dx, dy = x + 0.5 - cx, y + 0.5 - cy
        if ball(cx, cy, r, x, y) > 0.62 and dx + dy > r * 0.95:
            return "2"
        return "3"
    rows = [list(row) for row in shape(f)]
    top = int(cy - r)
    for x, y in ((8, top - 1), (9, top - 2)):
        rows[y][x] = "2"
    tx, ty = 10, top - 3
    if spark == 0:
        pts = [(tx, ty, "1"), (tx - 1, ty - 1, "2"), (tx + 1, ty - 1, "2"), (tx + 1, ty + 1, "2")]
    else:
        pts = [(tx, ty, "1"), (tx, ty - 1, "1"), (tx + 1, ty, "1"), (tx + 2, ty, "2"), (tx, ty - 2, "2")]
    for x, y, c in pts:
        if 0 <= x < T and 0 <= y < T:
            rows[y][x] = c
    return ["".join(row) for row in rows]


def dud(puff):
    """P0: a dead bomb, grey and dull with a black rim, its fuse burnt to a
    stub; a white puff of smoke drifting up."""
    r, cx, cy = 6.0, 8.0, 10.0

    def f(x, y):
        q = ball(cx, cy, r, x, y)
        if q > 1.0:
            return "."
        if q > 0.7:
            return "3"
        if ball(cx - 2.4, cy - 2.4, 1.0, x, y) <= 1.2:
            return "1"
        return "3" if (x + y) % 2 == 0 and q > 0.25 else "2"
    rows = [list(row) for row in shape(f)]
    rows[3][8] = "3"
    smoke = [
        [(9, 2, "1"), (10, 1, "1"), (9, 1, "2"), (11, 0, "2")],
        [(10, 1, "1"), (11, 0, "1"), (10, 0, "2"), (9, 2, "2")],
        [(11, 0, "1"), (12, 0, "2"), (10, 1, "2")],
        [(9, 2, "2")],
    ][puff]
    for x, y, c in smoke:
        rows[y][x] = c
    return ["".join(row) for row in rows]


def flame(kind, thin):
    """P2 (1 orange, 2 white, 3 red): a white-hot core, orange, a ragged
    red edge. Thin: the flicker frame, a pixel narrower, the rag moved on.
    The rag repeats every 8 px, so arms join up tile to tile."""
    t = 1 if thin else 0
    cy = cx = 7.5
    rag_t = (0.0, 0.9, 0.3, -0.6, 0.7, -0.3, 1.0, 0.2)

    def rag(a):
        return rag_t[(a * 3 + (4 if thin else 0)) % 8]

    def band(d, half, a=0):
        half -= t
        if d > half + rag(a):
            return None
        if d <= half * 0.36:
            return "2"
        if d <= half * 0.7 + rag(a + 3) * 0.5:
            return "1"
        return "3"

    def h(x, y):
        return band(abs(y - cy), 6.0, x)

    def v(x, y):
        return band(abs(x - cx), 6.0, y)

    def f(x, y):
        r = None
        if kind == "C":
            ang = int((math.atan2(y - cy, x - cx) + math.pi) / (2 * math.pi) * 16) % 16
            q = ball(cx + 0.5, cy + 0.5, 7.4 - t + rag(ang), x, y)
            if q <= 1.0:
                r = "2" if q <= 0.22 else "1" if q <= 0.55 else "3"
            r = r or h(x, y) or v(x, y)
        elif kind == "H":
            r = h(x, y)
        elif kind == "V":
            r = v(x, y)
        elif kind in "LR":
            d = x if kind == "L" else T - 1 - x      # 0 at the tip
            r = band(abs(y - cy), 6.0 - max(0, 5 - d) * 1.0, x)
        elif kind in "UD":
            d = y if kind == "U" else T - 1 - y
            r = band(abs(x - cx), 6.0 - max(0, 5 - d) * 1.0, y)
        return r or "."
    return shape(f)


def panel(icon, lit, m, pslot, rim_lit, rim_dim):
    """A powerup: a panel in slot pslot, a rim (lit or dim), the 12x12 icon
    (drawart_cga's shapes, its colours mapped by m) inside."""
    edge = rim_lit if lit else rim_dim
    rows = ["." + edge * 14 + "."]
    for r in range(14):
        inner = icon[r - 1] if 1 <= r <= 12 else "." * 12
        inner = "".join(m.get(p, pslot) for p in inner)
        rows.append(edge + pslot + inner + pslot + edge)
    rows.append("." + edge * 14 + ".")
    return rows


# name: (icon, colour map, panel slot, rim lit, rim dim) in the tile's palette
POWERUPS = {
    "fire": (C.FIRE, {"w": "1", "r": "2", "c": "2"}, "3", "2", "1"),      # P2: red panel, yellow flame
    "bombup": (C.BOMBUP, {"c": "3", "w": "1"}, "2", "1", "3"),          # P0: grey panel, black bomb
    "speed": (C.SPEED, {"w": "2", "c": "1"}, "3", "2", "1"),            # P3: dark blue, white chevrons
    "kick": (C.KICK, {"w": "1", "c": "2"}, "3", "1", "2"),              # P1: dark red, light red boot
    "glove": (C.GLOVE, {"w": "2", "c": "1"}, "3", "2", "1"),            # P3: dark blue, white glove
}

# M2 (shown in the arena's P2): 1 the orange cup, 2 white shine and
# plaque, 3 the brown shade (red in P2).
TROPHY = [
    "................",
    "..111111111113..",
    "3312111111111333",
    "3.121111111113.3",
    "3.121111111113.3",
    ".31211111111133.",
    "...2111111113...",
    "....21111113....",
    ".....211113.....",
    "......2113......",
    ".......13.......",
    ".......13.......",
    ".....211113.....",
    "....33333333....",
    "....32222223....",
    "....33333333....",
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
    for name, (icon, m, ps, rl, rd) in POWERUPS.items():
        t[name] = [panel(icon, False, m, ps, rl, rd), panel(icon, True, m, ps, rl, rd)]
    for name, frames in t.items():
        for i, f in enumerate(frames):
            check(f"{name}[{i}]", f)
    return t


# The arena's frame (P0): an 8x8 cell of stone, white tops, grey faces,
# black joints; and the side panel's cell (the floor).
WALL = [r.replace("w", "1").replace("c", "2").replace(".", "3") for r in C.WALL]
PANEL = ["." * 8] * 8


# ---- the logo ---------------------------------------------------------------

def logo():
    """216x24 (27 cells x 3): the Namco letters at 3x, 24 px a letter, a
    black shadow down and to the right. Lines 0-15 are one attribute row
    (T1: the top lit white, then yellow), lines 16-23 the next (T2: orange,
    then red). Returns (rows of slots, the palette of each line)."""
    font = C.namco()
    w = len(LOGO_TEXT) * 24
    lit = [[None] * w for _ in range(24)]
    for i, ch in enumerate(LOGO_TEXT):
        g = C.glyph(font, ch)
        for r, row in enumerate(g):
            for c, p in enumerate(row):
                if p != "#":
                    continue
                top = r == 0 or g[r - 1][c] != "#"
                for dy in range(3):
                    for dx in range(3):
                        y = r * 3 + dy
                        if y < 16:
                            v = "1" if top and dy == 0 else "2"
                        else:
                            v = "1" if y < 20 else "2"
                        lit[y][i * 24 + c * 3 + dx] = v
    out = [["."] * w for _ in range(24)]
    for y in range(24):
        for x in range(w):
            if lit[y][x]:
                out[y][x] = lit[y][x]
            elif (x >= 2 and y >= 2 and lit[y - 2][x - 2]) or (x >= 1 and y >= 1 and lit[y - 1][x - 1]):
                out[y][x] = "3"
    return ["".join(r) for r in out], [1] * 16 + [2] * 8


# ---- the bombers (16x16) ----------------------------------------------------
# A round helmet (lit from the top left: a shine, the right side shaded), a
# visor across it with two eyes, a suit with a belt and buckle, black
# gloves and boots, all in a black outline.

HEAD_DOWN = [
    ".....KKKKKK.....",
    "...KKBHHBBSKK...",
    "..KBHHBBBBBSSK..",
    ".KBHBBBBBBBBSSK.",
    ".KVVVVVVVVVVVVK.",
    ".KVVEEVVVVEEVVK.",
    ".KVVEEVVVVEEVVK.",
    ".KBBBBBBBBBBSSK.",
    "..KKBBBBBBBSKK..",
]
HEAD_UP = [
    ".....KKKKKK.....",
    "...KKBHHBBSKK...",
    "..KBHHBBBBBSSK..",
    ".KBHBBBBBBBBSSK.",
    ".KBBBBBBBBBBSSK.",
    ".KBBBBBBBBBBSSK.",
    ".KBBBBBBBBBBSSK.",
    "..KBBBBBBBBBSK..",
    "...KKKKKKKKKK...",
]
HEAD_SIDE = [
    ".....KKKKKK.....",
    "...KKBHHBBSKK...",
    "..KBHHBBBBBSSK..",
    ".KBHBBBBBBBBSSK.",
    ".KBBBBBBBBVVVVK.",
    ".KBBBBBBBBVVEVK.",
    ".KBBBBBBBBVVVVK.",
    ".KBBBBBBBBBBSSK.",
    "..KKBBBBBBBSKK..",
]
TORSO = [
    ".KBBKBBBBBBKBSK.",
    ".KKKKBBBBBSKKKK.",
    "....KKKHHKKK....",
]
TORSO_SIDE = [
    "....KBBBBBSK....",
    "....KBKKKBSK....",
    "....KKKHHKKK....",
]
LEGS = {
    "stand": ["....KBBKKBSK....", "....KBBKKBSK....", "...KKKKKKKKKK...", "...KKKK..KKKK..."],
    "stepL": ["....KBBKKBSK....", "...KKKKKKBSK....", "...KKKK.KKKKK...", "........KKKKK..."],
    "stepR": ["....KBBKKBSK....", "....KBBKKKKKK...", "...KKKKK.KKKK...", "...KKKKK........"],
    "side0": [".....KBBBSK.....", ".....KBKBSK.....", ".....KKKKKKK....", ".....KKKKKKKK..."],
    "side1": ["....KBBKKBSK....", "...KBBK..KBSK...", "..KKKK....KKKK..", "..KKK.....KKKKK."],
    "side2": [".....KBBBSK.....", "......KBSK......", ".....KKKKKK.....", ".....KKKKKKK...."],
}


def mirror(f):
    return [r[::-1] for r in f]


def sprite_frames():
    fr = {}
    for k, legs in (("0", "stand"), ("1", "stepL"), ("2", "stepR")):
        fr["down" + k] = HEAD_DOWN + TORSO + LEGS[legs]
        fr["up" + k] = HEAD_UP + TORSO + LEGS[legs]
    for k, legs in (("0", "side0"), ("1", "side1"), ("2", "side2")):
        fr["right" + k] = HEAD_SIDE + TORSO_SIDE + LEGS[legs]
        fr["left" + k] = mirror(fr["right" + k])
    # cheering: arms up, the eyes smiling; then a hop (the feet tucked up)
    fr["win0"] = [
        "KK............KK",
        "KK...KKKKKK...KK",
        "KB.KKBHHBBSKK.BK",
        "KBKBHHBBBBBSSKBK",
        "KBKVVVVVVVVVVKBK",
        "KBKVVEVVVVEVVKBK",
        "KBKVEVEVVEVEVKBK",
        "KBKBBBBBBBBSSKBK",
        "KBBKBBBBBBBSKBBK",
        ".KBBBBBBBBBBSBK.",
        "..KKBBBBBBBSKK..",
        "....KKKHHKKK....",
    ] + LEGS["stand"]
    fr["win1"] = fr["win0"][:12] + ["....KBBKKBSK....", "...KKKK..KKKK...", "." * 16, "." * 16]
    # knocked out: a spin (back, side), dazed and squashed, then blown apart
    fr["die0"] = fr["up0"]
    fr["die1"] = fr["left0"]
    fr["die2"] = ["." * 16] * 2 + [
        "................",
        "..H....H....H...",
        ".HHH..HHH..HHH..",
        "..H....H....H...",
        ".....KKKKKK.....",
        "...KKBHHBBSKK...",
        "..KBBBBBBBBBSK..",
        ".KVVVVVVVVVVVVK.",
        ".KVEEEVVVVEEEVK.",
        ".KBBBBBBBBBBSSK.",
        "KKKBBBBBBBBBSKKK",
        "KK.KKKKKKKKKK.KK",
        "...KBBK..KBSK...",
        "..KKKKK..KKKKK..",
    ]
    fr["die3"] = [
        "................",
        "....K......K....",
        ".....B....B.....",
        "..K...........K.",
        "...H...BB...H...",
        ".......BS.......",
        ".B...VV..VV...B.",
        ".....VE..EV.....",
        ".......BS.......",
        "...K...KK...K...",
        "..B...........B.",
        ".....B....B.....",
        "....K......K....",
        "................",
        "................",
        "................",
    ]
    for name in SPRITES:
        check(name, fr[name], chars=".BSHVEK")
    return fr


def doubled(rows):
    return ["".join(p * 2 for p in r) for r in rows for _ in (0, 1)]


def slots(frame, opt):
    """A role frame -> slot rows for a colour option."""
    m = OPTIONS[opt][1]
    return ["".join("." if p == "." else str(m[p]) for p in r) for r in frame]


# ---- a model NES screen -------------------------------------------------------

def cell_of(rows, x0, y0):
    return tuple(tuple(int(c) if c != "." else 0 for c in r[x0:x0 + 8]) for r in rows[y0:y0 + 8])


BLANK = tuple((0,) * 8 for _ in range(8))


def flip(cell):
    return tuple(tuple(reversed(r)) for r in cell)


def check_set(name, backdrop, pals):
    assert len(pals) == 4 and all(len(p) == 3 for p in pals), name
    for n in [backdrop] + [c for p in pals for c in p]:
        assert 0 <= n < 64 and n not in BAD, f"{name}: colour ${n:02X} is not one to use"


class Nes:
    """A 32x30 name table, a 16x15 attribute grid, 4+4 palettes and an OAM
    list of 8x16 sprites, rendered as a 2C02 would. A block asked for two
    palettes, a ninth sprite on a line or a 65th sprite is an error."""

    def __init__(self, name, bg, spr):
        self.name = name
        self.backdrop, self.bg = bg
        self.spr = spr
        check_set(name + " BG", self.backdrop, self.bg)
        check_set(name + " sprites", self.backdrop, self.spr)
        self.nt = [[BLANK] * 32 for _ in range(30)]
        self.at = [[None] * 16 for _ in range(15)]
        self.why = {}
        self.oam = []
        self.split = None       # (line, backdrop above it): a mid-frame backdrop write

    def attr(self, col, row, pal, what):
        b = (col // 2, row // 2)
        cur = self.at[b[1]][b[0]]
        if cur is not None and cur != pal:
            raise ValueError(f"{self.name}: block {b} wants palette {pal} for {what}, "
                             f"has {cur} for {self.why[b]}")
        self.at[b[1]][b[0]] = pal
        self.why[b] = what

    def cell(self, col, row, cell, pal, what):
        assert 0 <= col < 32 and 0 <= row < 30, (self.name, what, col, row)
        self.nt[row][col] = cell
        if cell != BLANK:
            self.attr(col, row, pal, what)

    def rows(self, col, row, rows, pal, what, claim=False):
        """Slot rows (multiples of 8) at a cell position. claim: ask for the
        palette even where blank (a tile owns its block)."""
        for cy in range(len(rows) // 8):
            for cx in range(len(rows[0]) // 8):
                self.cell(col + cx, row + cy, cell_of(rows, cx * 8, cy * 8), pal, what)
                if claim:
                    self.attr(col + cx, row + cy, pal, what)

    def tile(self, col, row, rows, pal, what):
        assert col % 2 == 0 and row % 2 == 0, (self.name, what, "a tile off the attribute grid")
        self.rows(col, row, rows, pal, what, claim=True)

    def text(self, col, row, s, pal, fg=1, bg=0):
        font = C.namco()
        for i, ch in enumerate(s):
            g = C.glyph(font, ch)
            cell = tuple(tuple(fg if p == "#" else bg for p in r) for r in g)
            self.cell(col + i, row, cell, pal, f"text {s!r}")

    def sprite(self, x, y, rows, pal, hflip=False):
        """Slot rows, 8n wide and 16m tall, as 8x16 sprites."""
        h, w = len(rows), len(rows[0])
        assert h % 16 == 0 and w % 8 == 0
        for sy in range(0, h, 16):
            for sx in range(0, w, 8):
                top, bot = cell_of(rows, sx, sy), cell_of(rows, sx, sy + 8)
                xx = x + (w - 8 - sx if hflip else sx)
                if hflip:
                    top, bot = flip(top), flip(bot)
                if top == BLANK and bot == BLANK:
                    continue
                self.oam.append((xx, y + sy, top, bot, pal))

    def render(self):
        px = [[self.split[1] if self.split and y < self.split[0] else self.backdrop] * 256
              for y in range(240)]
        for row in range(30):
            for col in range(32):
                cell = self.nt[row][col]
                if cell == BLANK:
                    continue
                pal = self.bg[self.at[row // 2][col // 2]]
                for y in range(8):
                    for x in range(8):
                        v = cell[y][x]
                        if v:
                            px[row * 8 + y][col * 8 + x] = pal[v - 1]
        assert len(self.oam) <= 64, (self.name, len(self.oam), "sprites")
        self.per_line = [0] * 240
        for (x, y, top, bot, pal) in self.oam:
            for yy in range(y, min(y + 16, 240)):
                self.per_line[yy] += 1
        worst = max(self.per_line)
        assert worst <= 8, (self.name, f"{worst} sprites on line {self.per_line.index(worst)}")
        for (x, y, top, bot, pal) in reversed(self.oam):      # OAM 0 in front
            for yy in range(16):
                r = (top if yy < 8 else bot)[yy % 8]
                for xx in range(8):
                    v = r[xx]
                    if v and 0 <= x + xx < 256 and 0 <= y + yy < 240:
                        px[y + yy][x + xx] = self.spr[pal][v - 1]
        return px

    def report(self):
        cells = {c for r in self.nt for c in r} | {BLANK}
        spr = set()
        for (_, _, top, bot, _) in self.oam:
            pair = (top, bot)
            spr.add(min(pair, (flip(top), flip(bot))))
        return (f"{self.name}: {len(cells)} BG cells, {len(spr)} sprite pairs, "
                f"{len(self.oam)} sprites, {max(self.per_line)} a line at most")

    def save(self, path, scale=3):
        px = self.render()
        img = Image.new("RGB", (256 * scale, 224 * scale))
        out = img.load()
        for y in range(224):
            for x in range(256):
                c = rgb(px[y + 8][x])
                for dy in range(scale):
                    for dx in range(scale):
                        out[x * scale + dx, y * scale + dy] = c
        img.save(path)
        print(" ", self.report())


# ---- budgets ------------------------------------------------------------------

def budgets(tiles, frames):
    """The arena screen's CHR: static cells (one 256-cell table less a 1K
    bank of 64 for the animated tiles, whose 4 frames sit in 4 such banks,
    as the Atari's 4 charsets) and the bombers' 8x16 pairs."""
    font = C.namco()
    static = {BLANK}
    for name in ("floor", "pillar", "block"):
        for f in tiles[name]:
            static |= {cell_of(f, x, y) for y in (0, 8) for x in (0, 8)}
    static.add(cell_of(WALL, 0, 0))
    for c in range(32, 96):
        g = font[c - 32]
        static.add(tuple(tuple(1 if p == "#" else 0 for p in r) for r in g))
    anim = [n for n in TILES if len(tiles[n]) > 1]
    slots_n = 4 * len(anim)
    banks = []
    for k in range(4):
        banks.append(len({cell_of(tiles[n][k % len(tiles[n])], x, y)
                          for n in anim for y in (0, 8) for x in (0, 8)}))
    every = set(static)
    for n in TILES:
        for f in tiles[n]:
            every |= {cell_of(f, x, y) for y in (0, 8) for x in (0, 8)}
    pairs = set()
    for name in SPRITES:
        rows = slots(frames[name], OPTION)
        for sx in (0, 8):
            top, bot = cell_of(rows, sx, 0), cell_of(rows, sx, 8)
            pairs.add(min((top, bot), (flip(top), flip(bot))))
    bigpairs = set()
    for k in (0, 1):
        big = slots(doubled(frames["win%d" % k]), OPTION)
        for sy in (0, 16):
            for sx in range(0, 32, 8):
                top, bot = cell_of(big, sx, sy), cell_of(big, sx, sy + 8)
                bigpairs.add(min((top, bot), (flip(top), flip(bot))))
    print(f"arena CHR: {len(static)} static cells (font 64) of 192; "
          f"{slots_n} animated cell slots of 64 ({len(anim)} tiles), distinct per bank {banks}; "
          f"{len(every)} distinct cells in all the tiles' frames")
    print(f"sprite CHR: {len(pairs)} 8x16 pairs for the 18 frames (flips shared), "
          f"{len(bigpairs)} more for the big cheer; {2 * (len(pairs) + len(bigpairs))} of 256 cells")
    assert len(static) <= 192 and slots_n <= 64 and 2 * (len(pairs) + len(bigpairs)) <= 256


# ---- writing ------------------------------------------------------------------

def indexed(w, h, colours, transparent=None):
    """A palette image, padded to 256 entries so it saves 8 bits a pixel
    (tools/png.py reads only those)."""
    img = Image.new("P", (w, h), 0)
    flat = []
    for c in colours:
        flat += list(c)
    flat += [0] * (768 - len(flat))
    img.putpalette(flat)
    if transparent is not None:
        img.info["transparency"] = transparent
    return img


def set_colours(backdrop, pals):
    out = []
    for p in pals:
        out += [rgb(backdrop)] + [rgb(c) for c in p]
    return out


def write_sheet(out):
    os.makedirs(out, exist_ok=True)
    tiles = tile_frames()
    # tiles.png: index = 4 x palette + slot, shown in the arena's colours
    img = indexed(4 * T, len(TILES) * T, set_colours(*BG_ARENA))
    for i, name in enumerate(TILES):
        pal = TILE_PAL[name]
        for f, rows in enumerate(tiles[name]):
            for y, r in enumerate(rows):
                for x, p in enumerate(r):
                    img.putpixel((f * T + x, i * T + y), 4 * pal + (0 if p == "." else int(p)))
    img.save(os.path.join(out, "tiles.png"))

    img = indexed(16, 8, set_colours(*BG_ARENA))
    for i, cell in enumerate((WALL, PANEL)):
        check("cell", cell, 8, 8)
        for y, r in enumerate(cell):
            for x, p in enumerate(r):
                img.putpixel((i * 8 + x, y), 0 if p == "." else int(p))
    img.save(os.path.join(out, "cells.png"))

    rows, line_pal = logo()
    img = indexed(len(rows[0]), 24, set_colours(*BG_TITLE))
    for y, r in enumerate(rows):
        for x, p in enumerate(r):
            img.putpixel((x, y), 4 * line_pal[y] + (0 if p == "." else int(p)))
    img.save(os.path.join(out, "logo.png"))

    # sprites.png / bigwin.png: slots in OPTION's seat 0 colours, 0 clear
    frames = sprite_frames()
    seat0 = OPTIONS[OPTION][0][0]
    key = [(255, 0, 255)] + [rgb(c) for c in seat0]
    img = indexed(len(SPRITES) * T, T, key, transparent=0)
    for i, name in enumerate(SPRITES):
        for y, r in enumerate(slots(frames[name], OPTION)):
            for x, p in enumerate(r):
                img.putpixel((i * T + x, y), 0 if p == "." else int(p))
    img.save(os.path.join(out, "sprites.png"))
    img = indexed(64, 32, key, transparent=0)
    for k in (0, 1):
        for y, r in enumerate(slots(doubled(frames["win%d" % k]), OPTION)):
            for x, p in enumerate(r):
                img.putpixel((k * 32 + x, y), 0 if p == "." else int(p))
    img.save(os.path.join(out, "bigwin.png"))
    budgets(tiles, frames)


# ---- previews -------------------------------------------------------------------

AX, AY = 4, 4          # the arena's first cell: column 4, row 4 (even: the attribute grid)


def sprites_for(opt):
    return OPTIONS[opt][0]


def bomber(s, x, y, name, seat, opt, big=False):
    frames = sprite_frames()
    f = frames[name]
    if name.startswith("left") and not big:
        s.sprite(x, y, slots(frames["right" + name[4:]], opt), seat, hflip=True)
        return
    if big:
        f = doubled(f)
    s.sprite(x, y, slots(f, opt), seat)


def mock_arena(opt):
    s = Nes("arena-" + opt, BG_ARENA, sprites_for(opt))
    tiles = tile_frames()
    # the status bar is black (rows 0-3, lines 0-31), the rest the floor's
    # green: the port writes the backdrop colour again at line 32 (MMC3 IRQ)
    s.split = (32, 0x0F)
    # the HUD, row 2 on the black: names in their seat's palette, the wins
    # and the clock in white (P0), each in blocks of its own
    for (name, seat, wins), col in zip(C.HUD, (0, 6, 20, 26)):
        s.text(col, 2, name, SEAT_PAL[seat])
        s.text(col + 4, 2, wins, 0)
    s.text(13, 2, "2:41", 0)
    # walls: column 3, column 30, row 26 (no top wall, as on the TMS)
    for r in range(AY, 27):
        s.rows(3, r, WALL, 0, "wall")
        s.rows(30, r, WALL, 0, "wall")
    for c in range(3, 31):
        s.rows(c, 26, WALL, 0, "wall")
    # the map: pillars on odd tiles, blocks, one of every tile
    rnd = random.Random(7)
    grid = [["floor"] * 13 for _ in range(11)]
    for r in range(11):
        for c in range(13):
            if r % 2 and c % 2:
                grid[r][c] = "pillar"
            elif rnd.random() < 0.5 and not (r < 2 and c < 2) and not (r > 8 and c > 10):
                grid[r][c] = "block"
    for (c, r) in [(0, 0), (1, 0), (0, 1), (12, 10), (11, 10), (12, 9), (12, 0), (0, 10),
                   (6, 8), (5, 8), (7, 8), (12, 6), (11, 6), (6, 6), (12, 5), (12, 7)]:
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
            fr = tiles[name]
            f = fr[(c % 2) if len(fr) > 1 and name not in ("bomb", "dud") else 0]
            if name == "bomb" and c == 6:
                f = fr[2]
            s.tile(AX + c * 2, AY + r * 2, f, TILE_PAL[name], name)
    # a kicked bomb half way between tiles (5,8) and (6,8): gfx_bomb_at's
    # half-tile step, over two floor blocks (floor tiles are P0, as bombs)
    s.rows(AX + 11, AY + 16, tiles["bomb"][1], 0, "moving bomb")
    # the side panel: the powerups, their counts in white beside them, the round
    for i, name in enumerate(("fire", "bombup", "speed", "kick", "glove")):
        row = 6 + i * 4
        s.tile(0, row, tiles[name][1], TILE_PAL[name], name)
        if i < 3:
            s.text(2, row + 1, "321"[i], 0)
    s.text(0, 24, "RND", 0)
    s.text(1, 26, "3", 2)
    # four bombers, two of them on the same lines, one walking between tiles
    x0, y0 = AX * 8, AY * 8
    for seat, (name, px, py) in enumerate([("down0", 0, 0), ("right1", 11 * 16 + 6, 10 * 16),
                                           ("left2", 6 * 16 + 9, 6 * 16), ("up1", 12 * 16, 6 * 16 + 5)]):
        bomber(s, x0 + px, y0 + py, name, seat, opt)
    return s


def mock_title(opt):
    s = Nes("title-" + opt, BG_TITLE, sprites_for(opt))
    rows, line_pal = logo()
    for cy in range(3):
        band = rows[cy * 8:cy * 8 + 8]
        for cx in range(len(rows[0]) // 8):
            s.cell(2 + cx, 4 + cy, cell_of(band, cx * 8, 0), line_pal[cy * 8], "logo")
    s.text(10, 9, "FOR FUJINET", 0)
    for seat in range(4):
        bomber(s, 24 + seat * 56, 96, ["right0", "right1", "right0", "right2"][seat], seat, opt)
    s.text(11, 18, "PRESS FIRE", 0)
    s.text(0, 21, "BOMBS, BLOCKS AND BLASTS FOR 2-4", 3)
    s.text(11, 24, "ESC: LOBBY", 2)
    return s


def mock_wait(opt):
    s = Nes("wait-" + opt, BG_MENU, sprites_for(opt))
    s.text(10, 3, "ROOM BLAST1", 0)
    names = ["ANN", "BOB", "CAT", None]
    for seat in range(4):
        r = 6 + seat * 4                                # rows 6, 10, 14, 18: blocks of their own
        pal = SEAT_PAL[seat]
        if names[seat]:
            s.text(6, r, names[seat], pal)
            if seat == 0:
                s.text(14, r, "HOST", pal)
            bomber(s, 24, r * 8 - 4, "down0", seat, opt)
        else:
            s.text(6, r, "--------", 0, fg=2)
    s.text(9, 21, "ROUNDS: 5", 0)
    s.text(9, 22, "(LEFT/RIGHT)", 0, fg=2)
    s.text(6, 24, "PRESS FIRE TO START", 1, fg=2, bg=3)
    s.text(5, 26, "ESC: LEAVE    M: SOUND", 0)
    return s


def mock_results(opt):
    s = Nes("results-" + opt, BG_MENU, sprites_for(opt))
    tiles = tile_frames()
    s.text(11, 2, "ANN WINS!", 0)
    wins = [3, 1, 2, 0]
    for k, (name, seat) in enumerate((("ANN", 0), ("BOB", 1), ("CAT", 2), ("DAN", 3))):
        r = 5 + k * 4                                   # odd: the trophies' row is even
        pal = SEAT_PAL[seat]
        s.text(6, r, name, pal)
        s.text(11, r, str(wins[k]), pal)
        for t in range(wins[k]):
            s.tile(6 + t * 2, r + 1, tiles["trophy"][0], TILE_PAL["trophy"], "trophy")
        bomber(s, 24, r * 8 - 4, "down0", seat, opt)
    bomber(s, 192, 88, "win0", 0, opt, big=True)
    s.text(5, 26, "BACK TO THE ROOM IN 9", 0)
    return s


def mock_sheet():
    """Not a NES screen: every tile frame in its palette, then every bomber
    frame in every seat for each option, on the floor."""
    tiles = tile_frames()
    frames = sprite_frames()
    back, bg = BG_ARENA
    cols = 18
    w = cols * 18 + 4
    h = 4 + 4 * 18 + 8 + len(OPTIONS) * (4 * 18 + 10)
    img = Image.new("RGB", (w, h), rgb(back))
    x, y = 2, 2
    for name in TILES:
        pal = bg[TILE_PAL[name]] if name != "trophy" else BG_MENU[1][TILE_PAL[name]]
        for f in tiles[name]:
            if x + 16 > w - 2:
                x, y = 2, y + 18
            for yy, r in enumerate(f):
                for xx, p in enumerate(r):
                    if p != ".":
                        img.putpixel((x + xx, y + yy), rgb(pal[int(p) - 1]))
            x += 18
    y += 26
    for opt in OPTIONS:
        pals = sprites_for(opt)
        for seat in range(4):
            for i, name in enumerate(SPRITES):
                for yy, r in enumerate(slots(frames[name], opt)):
                    for xx, p in enumerate(r):
                        if p != ".":
                            img.putpixel((2 + i * 18 + xx, y + yy), rgb(pals[seat][int(p) - 1]))
            y += 18
        y += 10
    return img.resize((w * 4, h * 4), Image.NEAREST)


def main():
    write_sheet(os.path.join(ROOT, "gfx", "nes"))
    print("wrote gfx/nes/tiles.png cells.png logo.png sprites.png bigwin.png")
    if len(sys.argv) > 2 and sys.argv[1] == "--preview":
        d = sys.argv[2]
        os.makedirs(d, exist_ok=True)
        for opt in OPTIONS:
            mock_arena(opt).save(os.path.join(d, f"arena-{opt}.png"))
        mock_title(OPTION).save(os.path.join(d, "title.png"))
        mock_wait(OPTION).save(os.path.join(d, "wait.png"))
        mock_results(OPTION).save(os.path.join(d, "results.png"))
        mock_sheet().save(os.path.join(d, "sheet.png"))
        print("previews in", d)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""drawart_sms.py -- FujiBlast's art for the Sega Master System (Mode 4),
written out as gfx/sms/*.png (the SMS port's converter reads them).

The SMS's rules, which the art keeps to:
  tiles    8x8, 4 bits a pixel (16 colours); any cell picks one of two
           16-colour palettes (CRAM 0-15 and 16-31) out of 64 colours (2 bits
           a channel: 0/85/170/255); a name table entry can flip a cell.
           448 tiles in all, shared by the background and the sprites; the
           sprites reach only 256 of them (one bank), so the background gets
           the other 192.
  sprites  64, 8 a line, 8x8 here, NO flip, all in the sprite palette (CRAM
           16-31); colour 0 is transparent. So a bomber is 4 sprites, a
           line crossing two of them, and each seat has its own copy of the
           patterns (the palette is shared).
  the backdrop colour is a sprite-palette entry.
A tile is 16x16 (2x2 cells), the screen 32x24 cells: the Adam's layout, the
HUD a black status bar on row 0. All of it is original art (the game is not
Bomberman: no antenna, no pom-pom, our own bricks and pillars).

Pixels are palette entries; the art is drawn in tone letters (TONE, MTONE).
Bombers are drawn in roles: B body, S shade, H highlight, V visor, E eyes,
K outline, Y buckle; each seat maps them to its own sprite-palette entries.
Text is the Namco font (gfx/namco.bin), a cell a character, made when first
drawn (the port's glyph cache): the previews count them.

    python3 tools/drawart_sms.py                # rewrites gfx/sms/*.png
    python3 tools/drawart_sms.py --preview D    # also mock screens into D
"""
import math
import os
import random
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drawart_cga as C  # noqa: E402  (the Namco font, the icon shapes)
import drawart_nes as N  # noqa: E402  (the bombers' silhouettes, geometry)
from artlayout import TILES, SPRITES, LOGO_TEXT  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

# ---- colours ----------------------------------------------------------------
# 2 bits a channel: the VDP's 64 colours, as a monitor shows them.
LEVEL = (0, 85, 170, 255)


def rgb(c):
    return tuple(LEVEL[v] for v in c)


def cram(c):
    return c[0] | c[1] << 2 | c[2] << 4


# Arena background palette (CRAM 0-15), tone letter -> entry.
ARENA = [
    (0, 2, 0), (0, 1, 0), (0, 0, 0), (3, 3, 3), (2, 2, 2), (1, 1, 1),     # floor, floor dark, black, white, greys
    (3, 2, 2), (3, 1, 1), (2, 0, 0), (1, 0, 0),                           # brick: light, face, shade, mortar
    (3, 3, 0), (3, 2, 0), (3, 1, 0),                                      # flame: yellow, orange, red
    (0, 0, 2), (1, 1, 3), (1, 2, 3),                                      # panel blue, light blue, sky
]
TONE = {"F": 0, "D": 1, "k": 2, "w": 3, "l": 4, "g": 5, "H": 6, "R": 7, "r": 8, "d": 9,
        "y": 10, "o": 11, "f": 12, "b": 13, "B": 14, "s": 15}
# The HUD's text colours (entries of ARENA): seat 0-3, the timer, the floor's
SEAT_TEXT = [3, 6, 15, 11]
BLACK, FLOOR = 2, 0

# Menu background palette: the title, waiting room and results.
MENU = [
    (0, 0, 2), (0, 0, 0), (3, 3, 3), (2, 2, 2), (3, 3, 0), (3, 2, 0), (3, 1, 0), (3, 0, 0),
    (3, 2, 2), (1, 2, 3), (2, 1, 0), (3, 3, 2), (1, 1, 3), (2, 2, 3), (1, 0, 0), (2, 3, 3),
]
MTONE = {"F": 0, "k": 1, "w": 2, "l": 3, "y": 4, "o": 5, "f": 6, "R": 7, "p": 8, "s": 9,
         "g": 10, "c": 11, "B": 12, "v": 13, "d": 14, "q": 15}
MENU_TEXT = [2, 8, 9, 5]            # seat 0-3 text: white, pink, sky, orange
M_HI = 4                            # a highlight (yellow)

# Sprite palette (CRAM 16-31): 0 transparent (and the backdrop), shared black,
# white, two greys, then a light/body/shade trio for seats 1-3, a buckle.
SPR = [
    (0, 0, 0), (0, 0, 0), (3, 3, 3), (2, 2, 2), (1, 1, 1),
    (3, 2, 2), (3, 1, 1), (2, 0, 0),            # seat 1: pink
    (2, 3, 3), (1, 2, 3), (0, 1, 2),            # seat 2: sky
    (3, 3, 1), (3, 2, 0), (2, 1, 0),            # seat 3: orange
    (3, 3, 0), (3, 0, 0),                       # buckle yellow, spare red
]
SEAT_ROLE = [
    {"H": 2, "B": 2, "S": 3},
    {"H": 5, "B": 6, "S": 7},
    {"H": 8, "B": 9, "S": 10},
    {"H": 11, "B": 12, "S": 13},
]
COMMON_ROLE = {"K": 1, "V": 1, "E": 2, "Y": 14}
T = 16

# ---- the floor's texture: the same in every tile, so cells dedupe ------------------

_rnd = random.Random(5)
SPECK = set()
for _ in range(7):
    _x, _y = _rnd.randrange(1, 14), _rnd.randrange(1, 15)
    SPECK |= {(_x, _y), (_x + 1, _y)}


def floor_at(x, y):
    return "D" if (x, y) in SPECK else "F"


def render(rows, tones=TONE):
    """Rows of tone letters ('.': the floor's texture) -> rows of entries."""
    out = []
    for y, r in enumerate(rows):
        out.append([tones[floor_at(x, y) if p == "." else p] for x, p in enumerate(r)])
    return out


def shape(fn, w=T, h=T):
    return ["".join(fn(x, y) for x in range(w)) for y in range(h)]


def check(name, rows, w=T, h=T, chars=None):
    assert len(rows) == h and all(len(r) == w for r in rows), (name, [len(r) for r in rows])
    if chars:
        bad = {p for r in rows for p in r} - set(chars)
        assert not bad, (name, bad)


def ball(cx, cy, r, x, y):
    dx, dy = x + 0.5 - cx, y + 0.5 - cy
    return (dx * dx + dy * dy) / (r * r)


# ---- tiles (16x16) ---------------------------------------------------------------

def pillar():
    """A raised stone: lit top and left, a dark right and bottom, a recessed
    panel in the face."""
    g = [["l"] * T for _ in range(T)]
    for y in range(T):
        for x in range(T):
            if x < 2 or y < 2:
                g[y][x] = "w"
            if x >= 14 or y >= 14:
                g[y][x] = "g"
            if x == 15 or y == 15:
                g[y][x] = "k"
    for y in range(4, 12):
        for x in range(4, 12):
            g[y][x] = "l"
            if y == 4 or x == 4:
                g[y][x] = "g"
            if y == 11 or x == 11:
                g[y][x] = "w"
    g[4][11] = g[11][4] = "l"
    g[3][3] = "g"
    return ["".join(r) for r in g]


def bricks(top, face, low, mortar):
    """Four courses of a lit top line, two face lines, a shaded line and a
    mortar line, joints staggered by 4 px; the pattern repeats every 8 px."""
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
                elif line == 0 or (line == 1 and (x - 1) % T in joints):
                    r += top
                elif line == 2:
                    r += low
                else:
                    r += face
            rows.append(r)
    return rows


def burn(f):
    """The block on fire: its top course gone, the bricks glowing, embers,
    tongues of flame licking up."""
    rnd = random.Random(5 + f)
    b = [list(r) for r in bricks("y", "o", "f", "r")]
    for y in range(T):
        for x in range(T):
            if b[y][x] == "o" and rnd.random() < 0.10:
                b[y][x] = "y"
            elif b[y][x] == "y" and rnd.random() < 0.35:
                b[y][x] = "w"
    for y in range(3):
        b[y] = ["."] * T
    for x in range(T):
        if b[3][x] != "r":
            b[3][x] = "."
    tongues = [(3, 6), (8, 7), (12, 5)] if f == 0 else [(1, 5), (6, 7), (10, 6), (14, 5)]
    for tx, h in tongues:
        for y in range(h):
            half = 0 if y < 2 else 1
            for x in range(tx - half, tx + half + 1):
                if 0 <= x < T:
                    b[y][x] = "w" if (x == tx and 2 <= y < h - 2) else "y" if (x == tx or y < h - 1) else "o"
                    if y >= h - 1:
                        b[y][x] = "f"
    return ["".join(r) for r in b]


def bomb(spark, small):
    """A black bomb, a white shine fading through two greys, a grey rim of
    light low on the right, a grey fuse, a spark."""
    r = 5.0 if small else 6.0
    cx, cy = 8.0, 10.0
    sx, sy = cx - r * 0.42, cy - r * 0.42

    def f(x, y):
        q = ball(cx, cy, r, x, y)
        if q > 1.0:
            return "."
        s = ball(sx, sy, 1.0, x, y)
        if s <= 1.0:
            return "w"
        if s <= 2.6:
            return "l"
        if s <= 6.0:
            return "g"
        dx, dy = x + 0.5 - cx, y + 0.5 - cy
        if q > 0.62 and dx + dy > r * 0.95:
            return "g"
        return "k"
    rows = [list(row) for row in shape(f)]
    top = int(cy - r)
    for x, y, c in ((8, top - 1, "l"), (9, top - 2, "l")):
        rows[y][x] = c
    tx, ty = 10, top - 3
    if spark == 0:
        pts = [(tx, ty, "w"), (tx - 1, ty - 1, "y"), (tx + 1, ty - 1, "o"), (tx + 1, ty + 1, "y")]
    else:
        pts = [(tx, ty, "w"), (tx, ty - 1, "y"), (tx + 1, ty, "y"), (tx + 2, ty, "o"), (tx, ty - 2, "o")]
    for x, y, c in pts:
        if 0 <= x < T and 0 <= y < T:
            rows[y][x] = c
    return ["".join(row) for row in rows]


def dud(puff):
    """A dead bomb: dull grey, dithered, a black rim and a dim shine; the
    fuse burnt to a stub, a puff of smoke drifting up."""
    r, cx, cy = 6.0, 8.0, 10.0

    def f(x, y):
        q = ball(cx, cy, r, x, y)
        if q > 1.0:
            return "."
        if q > 0.72:
            return "k"
        if ball(cx - 2.4, cy - 2.4, 1.0, x, y) <= 1.3:
            return "w"
        return "g" if (x + y) % 2 == 0 and q > 0.2 else "l"
    rows = [list(row) for row in shape(f)]
    rows[3][8] = "k"
    smoke = [
        [(9, 2, "w"), (10, 1, "w"), (9, 1, "l"), (11, 0, "l")],
        [(10, 1, "w"), (11, 0, "w"), (10, 0, "l"), (9, 2, "l")],
        [(11, 0, "w"), (12, 0, "l"), (10, 1, "l")],
        [(9, 2, "l")],
    ][puff]
    for x, y, c in smoke:
        rows[y][x] = c
    return ["".join(row) for row in rows]


def flame(kind, thin):
    """White-hot core, yellow, orange, a ragged red edge. Thin: the flicker
    frame, a pixel narrower, the rag moved along. The rag repeats every 8
    px, so arms join up from tile to tile."""
    t = 1 if thin else 0
    cy = cx = 7.5
    rag_t = (0.0, 0.9, 0.3, -0.6, 0.7, -0.3, 1.0, 0.2)

    def rag(a):
        return rag_t[(a * 3 + (4 if thin else 0)) % 8]

    def band(d, half, a=0):
        half -= t
        if d > half + rag(a):
            return None
        if d <= half * 0.30:
            return "w"
        if d <= half * 0.58:
            return "y"
        if d <= half * 0.82 + rag(a + 3) * 0.5:
            return "o"
        return "f"

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
                r = "w" if q <= 0.2 else "y" if q <= 0.45 else "o" if q <= 0.75 else "f"
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


def panel(icon, lit, m, base, rim_lit, rim_dim):
    """A powerup: a panel (base tone), a rim (lit or dim), the 12x12 icon
    (drawart_cga's shapes, its tones mapped by m) inside, a lighter line
    along the panel's top."""
    edge = rim_lit if lit else rim_dim
    rows = ["." + edge * 14 + "."]
    for r in range(14):
        inner = icon[r - 1] if 1 <= r <= 12 else "." * 12
        inner = "".join(m.get(p, base) for p in inner)
        rows.append(edge + base + inner + base + edge)
    rows.append("." + edge * 14 + ".")
    return rows


# name: (icon, tone map, panel base, lit rim, dim rim)
POWERUPS = {
    "fire": (C.FIRE, {"w": "o", "r": "y", "c": "w"}, "r", "y", "H"),
    "bombup": (C.BOMBUP, {"c": "k", "w": "w"}, "g", "w", "l"),
    "speed": (C.SPEED, {"w": "w", "c": "s"}, "b", "s", "B"),
    "kick": (C.KICK, {"w": "H", "c": "w"}, "d", "H", "R"),
    "glove": (C.GLOVE, {"w": "H", "c": "s"}, "b", "s", "B"),
}

# the trophy, in the menu palette: y cup, c shine, o shade, d dark base, w plaque
TROPHY = [
    "................",
    "..yyyyyyyyyyyy..",
    "oyycyyyyyyyyoyyo",
    "o.ycyyyyyyyyyo.o",
    "o.ycyyyyyyyyyo.o",
    ".oycyyyyyyyyyoo.",
    "...cyyyyyyyyoo..",
    "....cyyyyyyoo...",
    ".....cyyyyoo....",
    "......cyyoo.....",
    ".......yo.......",
    ".......yo.......",
    ".....cyyyyoo....",
    "....dddddddd....",
    "....dwwwwwwd....",
    "....dddddddd....",
]

# the arena's frame: an 8x8 stone cell (white tops, grey faces, black joints)
WALL = ["wwwwwwwk", "wllllllk", "wllllllg", "kkkkkkkk", "wwwkwwww", "lllklllg", "lllklllg", "kkkkkkkk"]


def tile_frames():
    t = {
        "floor": [["." * T] * T],
        "pillar": [pillar()],
        "block": [bricks("H", "R", "r", "d")],
        "burn": [burn(0), burn(1)],
        "bomb": [bomb(0, False), bomb(1, False), bomb(0, True), bomb(1, True)],
        "dud": [dud(i) for i in range(4)],
    }
    for k in "CHVLRUD":
        t["flame" + k] = [flame(k, False), flame(k, True)]
    for name, (icon, m, base, rl, rd) in POWERUPS.items():
        t[name] = [panel(icon, False, m, base, rl, rd), panel(icon, True, m, base, rl, rd)]
    for name, frames in t.items():
        for i, f in enumerate(frames):
            check(f"{name}[{i}]", f, chars=set(TONE) | {"."})
    return t


# ---- the logo ---------------------------------------------------------------------

def logo():
    """216x24 in the menu palette: the Namco letters at 3x, a gradient down
    each letter (cream, yellow, orange, red), a white lit top, a black shadow
    down and to the right. 24 px a letter, 27 cells."""
    font = C.namco()
    w = len(LOGO_TEXT) * 24
    grad = ["c", "y", "y", "o", "o", "f", "R", "R"]
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
                        lit[r * 3 + dy][i * 24 + c * 3 + dx] = "w" if top and dy == 0 else grad[r]
    out = [["F"] * w for _ in range(24)]
    for y in range(24):
        for x in range(w):
            if lit[y][x]:
                out[y][x] = lit[y][x]
            elif (x >= 2 and y >= 2 and lit[y - 2][x - 2]) or (x >= 1 and y >= 1 and lit[y - 1][x - 1]):
                out[y][x] = "k"
    return [[MTONE[p] for p in r] for r in out]


# ---- the bombers (16x16) ------------------------------------------------------------

def role_frames():
    """The NES's silhouettes (drawart_nes.sprite_frames), the belt's buckle
    in its own role."""
    fr = N.sprite_frames()
    out = {}
    for name in SPRITES:
        rows = [r.replace("KKKHHKKK", "KKKYYKKK") for r in fr[name]]
        check(name, rows, chars=".BSHVEKY")
        out[name] = rows
    return out


def seat_rows(rows, seat):
    m = dict(SEAT_ROLE[seat])
    m.update(COMMON_ROLE)
    return [[0 if p == "." else m[p] for p in r] for r in rows]


def doubled(rows):
    return [[p for p in r for _ in (0, 1)] for r in rows for _ in (0, 1)]


# ---- cells --------------------------------------------------------------------------

def cell_of(rows, x0, y0):
    return tuple(tuple(rows[y0 + y][x0 + x] for x in range(8)) for y in range(8))


def hflip(c):
    return tuple(tuple(reversed(r)) for r in c)


def vflip(c):
    return tuple(reversed(c))


def canon(c):
    """The cell and its flips are one tile (a name table entry flips it)."""
    return min(c, hflip(c), vflip(c), hflip(vflip(c)))


def cells16(rows):
    return [cell_of(rows, x, y) for y in (0, 8) for x in (0, 8)]


def sprite_cells(rows):
    """The sprites of a 16-multiple picture: (x, y, cell), blank cells skipped."""
    out = []
    for y in range(0, len(rows), 8):
        for x in range(0, len(rows[0]), 8):
            c = cell_of(rows, x, y)
            if any(p for r in c for p in r):
                out.append((x, y, c))
    return out


# ---- a model SMS screen -------------------------------------------------------------

BLANK = tuple((0,) * 8 for _ in range(8))


class Sms:
    """A 32x24 name table (cell, palette 0 or 1, flips), the two palettes and
    a list of 8x8 sprites (all in palette 1), rendered as the VDP would. More
    than 8 sprites on a line, more than 64 sprites or a colour off the 2-bit
    grid is an error. Cells made as text are counted apart (the port's glyph
    cache)."""

    def __init__(self, name, pal0, pal1):
        self.name, self.pal = name, [pal0, pal1]
        for p in self.pal:
            assert len(p) == 16 and all(len(c) == 3 and all(0 <= v < 4 for v in c) for c in p), name
        self.nt = [[None] * 32 for _ in range(24)]
        self.static, self.dyn = set(), set()
        self.spr = []

    def put(self, col, row, cell, pal=0, text=False):
        assert 0 <= col < 32 and 0 <= row < 24, (self.name, col, row)
        self.nt[row][col] = (cell, pal)
        (self.dyn if text else self.static).add(canon(cell))

    def rows(self, col, row, rows, pal=0):
        for cy in range(len(rows) // 8):
            for cx in range(len(rows[0]) // 8):
                self.put(col + cx, row + cy, cell_of(rows, cx * 8, cy * 8), pal)

    def fill(self, cell, c0=0, r0=0, c1=31, r1=23):
        for r in range(r0, r1 + 1):
            for c in range(c0, c1 + 1):
                if self.nt[r][c] is None:
                    self.put(c, r, cell)

    def text(self, col, row, s, fg, bg):
        font = C.namco()
        for i, ch in enumerate(s):
            g = C.glyph(font, ch)
            cell = tuple(tuple(fg if g[y][x] == "#" else bg for x in range(8)) for y in range(8))
            self.put(col + i, row, cell, 0, text=True)

    def sprite(self, x, y, rows):
        for dx, dy, c in sprite_cells(rows):
            self.spr.append((x + dx, y + dy, c))

    def render(self):
        px = [[self.pal[1][0]] * 256 for _ in range(192)]
        for r in range(24):
            for c in range(32):
                e = self.nt[r][c]
                if e is None:
                    continue
                cell, p = e
                for y in range(8):
                    for x in range(8):
                        px[r * 8 + y][c * 8 + x] = self.pal[p][cell[y][x]]
        assert len(self.spr) <= 64, (self.name, len(self.spr), "sprites")
        per = [0] * 192
        for (x, y, c) in self.spr:
            for yy in range(y, min(y + 8, 192)):
                if yy >= 0:
                    per[yy] += 1
        worst = max(per)
        assert worst <= 8, (self.name, f"{worst} sprites on line {per.index(worst)}")
        self.per_line = per
        for (x, y, c) in reversed(self.spr):             # sprite 0 in front
            for yy in range(8):
                for xx in range(8):
                    v = c[yy][xx]
                    if v and 0 <= x + xx < 256 and 0 <= y + yy < 192:
                        px[y + yy][x + xx] = self.pal[1][v]
        return px

    def report(self):
        blank = sum(1 for r in self.nt for e in r if e is None)
        sp = {canon(c) for (_, _, c) in self.spr}
        return (f"{self.name}: {len(self.static)} static cells, {len(self.dyn)} text cells, "
                f"{len(sp)} sprite cells, {len(self.spr)} sprites, {max(self.per_line)} a line at most"
                + (f", {blank} cells never written" if blank else ""))

    def save(self, path, scale=3):
        px = self.render()
        img = Image.new("RGB", (256 * scale, 192 * scale))
        out = img.load()
        for y in range(192):
            for x in range(256):
                c = rgb(px[y][x])
                for dy in range(scale):
                    for dx in range(scale):
                        out[x * scale + dx, y * scale + dy] = c
        img.save(path)
        print(" ", self.report())


# ---- budgets ------------------------------------------------------------------------

BG_BUDGET = 192                 # the tiles the sprites' bank leaves the background
ARENA_TEXT = 48                 # the HUD's and the panel's glyph cells (made at run time)


def spr_pal(backdrop):
    p = list(SPR)
    p[0] = backdrop
    return p


def budgets(tiles, rf):
    wall = cell_of([[TONE[p] for p in r] for r in WALL], 0, 0)
    static = {canon(wall), canon(tuple((FLOOR,) * 8 for _ in range(8))), canon(tuple((BLACK,) * 8 for _ in range(8)))}
    for name, frames in tiles.items():
        for f in frames:
            static |= {canon(c) for c in cells16(render(f))}
    print(f"arena BG: {len(static)} static cells (flips shared) + {ARENA_TEXT} for text of {BG_BUDGET}")
    assert len(static) + ARENA_TEXT <= BG_BUDGET, "arena set over budget"
    arena_frames = [n for n in SPRITES if not n.startswith("win")]
    menu_frames = ["down0", "right0", "right1", "right2", "win0", "win1"]
    for label, names in (("arena", arena_frames), ("menu", menu_frames)):
        per = []
        for seat in range(4):
            cs = set()
            for n in names:
                cs |= {c for (_, _, c) in sprite_cells(seat_rows(rf[n], seat))}
            per.append(len(cs))
        extra = 0
        if label == "menu":
            for seat in range(4):
                cs = set()
                for k in (0, 1):
                    cs |= {c for (_, _, c) in sprite_cells(doubled(seat_rows(rf["win%d" % k], seat)))}
                extra += len(cs)
        total = sum(per) + extra + 1
        print(f"{label} sprites: {per} cells a seat, {extra} for the big cheer, {total} of 256")
        assert total <= 256, f"{label} sprite set over budget"


# ---- writing ------------------------------------------------------------------------

def indexed(w, h, pal, transparent=None):
    """A palette image (8 bits a pixel; tools/png.py reads only those)."""
    img = Image.new("P", (w, h), 0)
    flat = []
    for c in pal:
        flat += list(rgb(c))
    flat += [0] * (768 - len(flat))
    img.putpalette(flat)
    if transparent is not None:
        img.info["transparency"] = transparent
    return img


def put(img, x0, y0, rows):
    for y, r in enumerate(rows):
        for x, v in enumerate(r):
            img.putpixel((x0 + x, y0 + y), v)


def write_sheet(out):
    os.makedirs(out, exist_ok=True)
    tiles = tile_frames()
    rf = role_frames()
    img = indexed(4 * T, len(TILES) * T, ARENA)
    put(img, 0, 0, [[FLOOR] * (4 * T) for _ in range(len(TILES) * T)])
    for i, name in enumerate(TILES):
        if name == "trophy":
            continue                                   # trophy.png, in the menu palette
        for f, rows in enumerate(tiles[name]):
            put(img, f * T, i * T, render(rows))
    img.save(os.path.join(out, "tiles.png"))

    img = indexed(16, 8, ARENA)
    put(img, 0, 0, [[TONE[p] for p in r] for r in WALL])
    put(img, 8, 0, [[FLOOR] * 8 for _ in range(8)])
    img.save(os.path.join(out, "cells.png"))

    rows = logo()
    img = indexed(len(rows[0]), 24, MENU)
    put(img, 0, 0, rows)
    img.save(os.path.join(out, "logo.png"))

    img = indexed(T, T, MENU)
    put(img, 0, 0, [[MTONE["F"] if p == "." else MTONE[p] for p in r] for r in TROPHY])
    img.save(os.path.join(out, "trophy.png"))

    # sprites.png: a row a seat, the frames in SPRITES order, entries of the sprite palette
    img = indexed(len(SPRITES) * T, 4 * T, SPR, transparent=0)
    for seat in range(4):
        for i, name in enumerate(SPRITES):
            put(img, i * T, seat * T, seat_rows(rf[name], seat))
    img.save(os.path.join(out, "sprites.png"))
    img = indexed(64, 4 * 32, SPR, transparent=0)
    for seat in range(4):
        for k in (0, 1):
            put(img, k * 32, seat * 32, doubled(seat_rows(rf["win%d" % k], seat)))
    img.save(os.path.join(out, "bigwin.png"))
    budgets(tiles, rf)


# ---- previews: mock screens -----------------------------------------------------------

HUD = [("ANN", 0, "1"), ("BOB", 1, "0"), ("CAT", 2, "2"), ("DAN", 3, "1")]


def mock_arena():
    s = Sms("arena", ARENA, spr_pal((0, 0, 0)))
    tiles = tile_frames()
    rf = role_frames()
    black = tuple((BLACK,) * 8 for _ in range(8))
    floor = tuple((FLOOR,) * 8 for _ in range(8))
    wall = cell_of([[TONE[p] for p in r] for r in WALL], 0, 0)
    for c in range(32):
        s.put(c, 0, black)
    # the HUD: seat s's name and wins, the timer, on the black bar
    for (name, seat, wins), col in zip(HUD, (1, 7, 18, 24)):
        s.text(col, 0, name + wins, SEAT_TEXT[seat], BLACK)
    s.text(13, 0, "2:41", 3, BLACK)
    for r in range(1, 24):
        for c in range(32):
            if c < 4:
                s.put(c, r, floor)
            elif c in (4, 31) or r == 23:
                s.put(c, r, wall)
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
            s.rows(5 + c * 2, 1 + r * 2, render(f))
    s.rows(5 + 11, 1 + 16, render(tiles["bomb"][1]))         # a kicked bomb half way between two tiles
    for i, name in enumerate(("fire", "bombup", "speed", "kick", "glove")):
        row = 3 + i * 4
        s.rows(0, row, render(tiles[name][1]))
        if i < 3:
            s.text(2, row + 1, "321"[i], 3, FLOOR)
    s.text(0, 21, "RND", 3, FLOOR)
    s.text(1, 22, "3", SEAT_TEXT[3], FLOOR)
    x0, y0 = 40, 8
    for seat, (name, px, py) in enumerate([("down0", 0, 0), ("right1", 11 * 16 + 6, 10 * 16),
                                           ("left2", 6 * 16 + 9, 6 * 16), ("up1", 12 * 16, 6 * 16 + 5)]):
        s.sprite(x0 + px, y0 + py, seat_rows(rf[name], seat))
    return s


def menu_screen(name):
    return Sms(name, MENU, spr_pal(MENU[0]))


def mock_title():
    s = menu_screen("title")
    rf = role_frames()
    blue = tuple((0,) * 8 for _ in range(8))
    s.fill(blue)
    lg = logo()
    s.rows(2, 3, lg)
    s.text(10, 8, "FOR FUJINET", M_HI, 0)
    for seat in range(4):
        s.sprite(24 + seat * 56, 88, seat_rows(rf[["right0", "right1", "right0", "right2"][seat]], seat))
    s.text(11, 16, "PRESS FIRE", 2, 0)
    s.text(0, 19, "BOMBS, BLOCKS AND BLASTS FOR 2-4", 3, 0)
    s.text(11, 22, "ESC: LOBBY", 5, 0)
    return s


def mock_wait():
    s = menu_screen("wait")
    rf = role_frames()
    s.fill(tuple((0,) * 8 for _ in range(8)))
    s.text(10, 1, "ROOM BLAST1", 2, 0)
    names = ["ANN", "BOB", "CAT", None]
    for seat in range(4):
        r = 4 + seat * 4
        if names[seat]:
            s.text(6, r, names[seat], MENU_TEXT[seat], 0)
            if seat == 0:
                s.text(14, r, "HOST", MENU_TEXT[seat], 0)
            s.sprite(24, r * 8 - 4, seat_rows(rf["down0"], seat))
        else:
            s.text(6, r, "--------", 3, 0)
    s.text(9, 19, "ROUNDS: 5", 2, 0)
    s.text(9, 20, "(LEFT/RIGHT)", 3, 0)
    s.text(6, 22, "PRESS FIRE TO START", M_HI, 0)
    s.text(5, 23, "ESC: LEAVE    M: SOUND", 2, 0)
    return s


def mock_results():
    s = menu_screen("results")
    rf = role_frames()
    s.fill(tuple((0,) * 8 for _ in range(8)))
    trophy = [[MTONE["F"] if p == "." else MTONE[p] for p in r] for r in TROPHY]
    s.text(11, 0, "ANN WINS!", MENU_TEXT[0], 0)
    wins = [3, 1, 2, 0]
    for k, (name, seat) in enumerate((("ANN", 0), ("BOB", 1), ("CAT", 2), ("DAN", 3))):
        r = 3 + k * 5
        s.text(6, r, name, MENU_TEXT[seat], 0)
        s.text(11, r, str(wins[k]), MENU_TEXT[seat], 0)
        for t in range(wins[k]):
            s.rows(7 + t * 2, r + 1, trophy)
        s.sprite(24, r * 8 - 4, seat_rows(rf["down0"], seat))
    s.sprite(200, 80, doubled(seat_rows(rf["win0"], 0)))
    s.text(5, 23, "BACK TO THE ROOM IN 9", 2, 0)
    return s


def mock_sheet():
    """Not a screen: every tile frame, then every bomber frame in all four
    seats, on the floor, enlarged."""
    tiles = tile_frames()
    rf = role_frames()
    cols = 18
    w = cols * 18 + 4
    h = 4 + 5 * 18 + 8 + 4 * 18 + 4
    img = Image.new("RGB", (w, h), rgb(ARENA[FLOOR]))
    x, y = 2, 2
    for name in TILES:
        if name == "trophy":
            continue
        for f in tiles[name]:
            if x + 16 > w - 2:
                x, y = 2, y + 18
            for yy, r in enumerate(render(f)):
                for xx, v in enumerate(r):
                    img.putpixel((x + xx, y + yy), rgb(ARENA[v]))
            x += 18
    y += 26
    pal = spr_pal(ARENA[FLOOR])
    for seat in range(4):
        for i, name in enumerate(SPRITES):
            for yy, r in enumerate(seat_rows(rf[name], seat)):
                for xx, v in enumerate(r):
                    if v:
                        img.putpixel((2 + i * 18 + xx, y + yy), rgb(pal[v]))
        y += 18
    return img.resize((w * 4, h * 4), Image.NEAREST)


def main():
    write_sheet(os.path.join(ROOT, "gfx", "sms"))
    print("wrote gfx/sms/tiles.png cells.png logo.png trophy.png sprites.png bigwin.png")
    if len(sys.argv) > 2 and sys.argv[1] == "--preview":
        d = sys.argv[2]
        os.makedirs(d, exist_ok=True)
        for n, f in (("arena", mock_arena), ("title", mock_title), ("wait", mock_wait), ("results", mock_results)):
            s = f()
            s.save(os.path.join(d, n + ".png"))
        mock_sheet().save(os.path.join(d, "sheet.png"))
        print("previews in", d)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""drawart_vga.py -- FujiBlast's art for VGA and MCGA (mode 13h: 320x200,
256 colours), written out as gfx/vga/*.png (tools/mkgfx_vga.py converts
them for src/msdos/vga).

The geometry is the CGA build's: a cell 8x8, a tile 16x16, a bomber 16x16
(the winner's cheer 32x32). The designs are the master sheet's, as every
port draws them (the bombers are drawart_nes's silhouettes, the icons
drawart_cga's shapes); here they are shaded with the palette's ramps
(tools/vgapal.py: where every colour lives). All of it is original art.

Every PNG is indexed, its palette the DAC. Pixels are DAC entries, except:
  sprites.png, bigwin.png   a row a seat; entry SEAT + 16*seat + code (0 clear)
  icons.png, trophy.png, logo.png   0 where the backdrop shows through

    python3 tools/drawart_vga.py              # rewrites gfx/vga/*.png
    python3 tools/drawart_vga.py --preview D  # also mock screens into D
"""
import math
import os
import random
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drawart_cga as C  # noqa: E402  (the Namco font, the icon shapes)
import drawart_nes as N  # noqa: E402  (the bombers' silhouettes)
import vgapal as V  # noqa: E402
from artlayout import TILES, SPRITES, LOGO_TEXT  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
T = 16
PAL = V.palette()


def grid(w, h, v=0):
    return [[v] * w for _ in range(h)]


def clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


def norm(v):
    m = math.sqrt(sum(c * c for c in v))
    return tuple(c / m for c in v)


LIGHT = norm((-0.55, -0.75, 0.62))     # from the upper left, in front
HALF = norm((LIGHT[0], LIGHT[1], LIGHT[2] + 1))


def hsh(*a):
    """A repeatable hash of integers, 0-1."""
    h = 2166136261
    for v in a:
        h = ((h ^ (v & 0xFFFF)) * 16777619) & 0xFFFFFFFF
    h ^= h >> 13
    h = (h * 0x5BD1E995) & 0xFFFFFFFF
    h ^= h >> 15
    return (h & 0xFFFF) / 65536


# ---- the floor: grass, the same in every tile so they join ---------------------

def _grass():
    g = grid(T, T)
    two = 2 * math.pi / T
    for y in range(T):
        for x in range(T):
            n = (0.55 * math.sin(two * (x + 2 * y) + 0.3) + 0.35 * math.sin(two * (3 * x - y) + 1.1)
                 + 0.25 * math.sin(two * (2 * x + 3 * y) + 2.0))
            g[y][x] = 3 + (1 if n > 0.45 else -1 if n < -0.55 else 0)
    rnd = random.Random(21)
    for _ in range(11):                     # blades: a lit tip, a dark root
        x, y = rnd.randrange(T), rnd.randrange(T)
        g[y][x] = 6 + (rnd.random() < 0.4)
        g[(y + 1) % T][x] = 5
        g[(y + 2) % T][x] = 2
    for _ in range(6):
        g[rnd.randrange(T)][rnd.randrange(T)] = 1
    return [[V.GRASS + v for v in r] for r in g]


GRASS = _grass()


def floor():
    return [r[:] for r in GRASS]


def over_floor(fn):
    """A tile from fn(x, y) -> entry or None (the grass shows)."""
    return [[fn(x, y) if fn(x, y) is not None else GRASS[y][x] for x in range(T)] for y in range(T)]


# ---- pillar: a raised steel block with a recessed panel --------------------------

def pillar():
    P = V.PILLAR

    def f(x, y):
        if x == 15 or y == 15:
            return P + 0
        if x == 0 or y == 0:
            return P + 11 if x + y < 15 else P + 3
        if x >= 13 or y >= 13:
            return P + (2 if x + y > 16 else 9) if (x < 2 or y < 2) else P + (2 if x == 14 or y == 14 else 3)
        if x < 2 or y < 2:
            return P + 10 if x + y < 15 else P + 3
        if 4 <= x <= 11 and 4 <= y <= 11:          # the recess
            if x == 4 or y == 4:
                return P + 3 if x + y < 16 or x == 4 else P + 8
            if x == 11 or y == 11:
                return P + 9
            if (x, y) in ((7, 7), (8, 8)):
                return P + 9
            if (x, y) in ((8, 7), (7, 8)):
                return P + 4
            return P + 5 + (1 if y < 7 else 0)
        return P + clamp(8 - (y - 2) // 4 - (1 if x > 9 else 0), 5, 8)
    return [[f(x, y) for x in range(T)] for y in range(T)]


# ---- brick: four courses, each brick its own shade, mortar between ---------------

def bricks(seed=3):
    B = V.BRICK
    rnd = random.Random(seed)
    g = grid(T, T)
    for course in range(4):
        joints = (7, 15) if course % 2 == 0 else (3, 11)
        bases = [5 + rnd.choice((-1, 0, 0, 1)) for _ in range(3)]
        for line in range(4):
            y = course * 4 + line
            for x in range(T):
                if line == 3:
                    g[y][x] = B + 0
                    continue
                if x in joints:
                    g[y][x] = B + 1 if line == 0 else B + 0
                    continue
                k = sum(1 for j in joints if x > j) % 3
                b = bases[k]
                v = b + 2 if line == 0 else b - 2 if line == 2 else b
                if (x - 1) % T in joints:
                    v += 1
                if (x + 1) % T in joints:
                    v -= 1
                if line == 1 and rnd.random() < 0.18:
                    v += rnd.choice((-1, 1))
                g[y][x] = B + 2 + clamp(v, 0, 9)
    return g


def fire_at(level, x, y, salt=0):
    """A fire pixel: its level and a cycling group from 2x2 blocks."""
    return V.FIRE + clamp(level, 0, 3) * 4 + int(hsh(x // 2, y // 2, salt) * 4)


def burn(f):
    """The block on fire: its top course gone, the bricks glowing in the
    fire's colours (hot spots white), tongues of flame licking up from them."""
    rnd = random.Random(5 + f)
    base = bricks()
    g = grid(T, T, None)
    for y in range(4, T):
        for x in range(T):
            v = base[y][x] - V.BRICK
            if v <= 1:
                g[y][x] = V.BRICK + v
            else:
                lv = 3 if v < 5 else 2 if v < 8 else 1
                if rnd.random() < 0.10:
                    lv -= 1
                g[y][x] = fire_at(lv, x, y, f)
    tongues = [(3, 6), (8, 8), (12, 5)] if f == 0 else [(2, 5), (6, 8), (10, 7), (14, 5)]
    for tx, h in tongues:
        for k in range(h):                          # from the bricks (line 7) up
            y = 7 - k
            r = k / h
            half = 2.4 * (1 - r) ** 0.8 + 0.3
            cx = tx + round(math.sin(k * 0.9 + f * 2 + tx) * 0.7 * r)
            for x in range(cx - 3, cx + 4):
                d = abs(x - cx) / half
                if 0 <= x < T and d <= 1:
                    lv = 0 if d < 0.4 and r < 0.6 else 1 if d < 0.7 else 2
                    g[y][x] = fire_at(min(3, lv + (1 if r > 0.75 else 0)), x, y, f)
    return [[g[y][x] if g[y][x] is not None else GRASS[y][x] for x in range(T)] for y in range(T)]


# ---- bombs ----------------------------------------------------------------------------

def sphere_shade(nx, ny):
    nz = math.sqrt(max(0.0, 1 - nx * nx - ny * ny))
    d = max(0.0, nx * LIGHT[0] + ny * LIGHT[1] + nz * LIGHT[2])
    s = max(0.0, nx * HALF[0] + ny * HALF[1] + nz * HALF[2]) ** 28
    rim = max(0.0, nx * 0.7 + ny * 0.7) * (1 - nz)
    return d, s, rim


def bomb(spark, small):
    """Black steel with a white glint, a cool rim of light low on the right,
    a steel cap, a fuse and its spark (fire: it flickers with the cycle)."""
    r = 5.2 if small else 6.2
    cx, cy = 8.0, 10.0 if not small else 10.4
    top = int(cy - r)

    def f(x, y):
        dx, dy = (x + 0.5 - cx) / r, (y + 0.5 - cy) / r
        q = dx * dx + dy * dy
        if q > 1.0:
            return V.BOMB + 0 if q < 1.22 and dy > -0.3 else None
        d, s, rim = sphere_shade(dx, dy)
        v = 0.6 + d * 3.2 + rim * 2.6
        if s > 0.5:
            return V.BOMB + 7
        if s > 0.18:
            return V.BOMB + 6
        return V.BOMB + clamp(int(v), 1, 5)
    g = [[f(x, y) for x in range(T)] for y in range(T)]
    for x in (7, 8, 9):                             # the cap
        g[top][x] = V.FUSE + (3 if x == 7 else 2)
        if top - 1 >= 0:
            g[top - 1][x] = V.FUSE + (3 if x < 9 else 2)
    fuse = [(9, top - 2), (10, top - 3), (10, top - 4)] if not small else [(9, top - 2), (10, top - 3)]
    for i, (x, y) in enumerate(fuse):
        if y >= 0:
            g[y][x] = V.FUSE + (1 if i % 2 == 0 else 0)
    tx, ty = fuse[-1][0] + 1, fuse[-1][1] - 1
    if spark == 0:
        pts = [(tx, ty, 0), (tx - 1, ty, 2), (tx + 1, ty, 2), (tx, ty - 1, 1), (tx, ty + 1, 3)]
    else:
        pts = [(tx, ty, 0), (tx - 1, ty - 1, 1), (tx + 1, ty + 1, 2), (tx + 1, ty - 1, 1), (tx - 1, ty + 1, 3),
               (tx + 2, ty, 3)]
    for x, y, lv in pts:
        if 0 <= x < T and 0 <= y < T:
            g[y][x] = V.GREY + 14 if lv == 0 else fire_at(lv, x, y, 7)
    return [[g[y][x] if g[y][x] is not None else GRASS[y][x] for x in range(T)] for y in range(T)]


def dud(puff):
    """A dead bomb: dull stone, cracked, no glint; the fuse burnt to a stub,
    a puff of smoke drifting up."""
    r, cx, cy = 6.0, 8.0, 10.0

    def f(x, y):
        dx, dy = (x + 0.5 - cx) / r, (y + 0.5 - cy) / r
        q = dx * dx + dy * dy
        if q > 1.0:
            return V.GREY + 1 if q < 1.2 and dy > -0.3 else None
        d, _, _ = sphere_shade(dx, dy)
        v = d * 3.6 + (0.5 if (x + y) % 2 else 0)
        return V.DUD + clamp(int(v), 0, 3)
    g = [[f(x, y) for x in range(T)] for y in range(T)]
    for x, y in ((6, 8), (7, 9), (7, 10), (8, 11), (10, 9), (11, 10)):    # cracks
        g[y][x] = V.GREY + 1
    g[3][8] = g[4][8] = V.GREY + 2
    smoke = [
        [(9, 2, 13), (10, 1, 14), (9, 1, 11), (11, 0, 10)],
        [(10, 1, 14), (11, 0, 13), (10, 0, 11), (9, 2, 10)],
        [(11, 0, 13), (12, 0, 11), (10, 1, 10)],
        [(9, 2, 11)],
    ][puff]
    for x, y, v in smoke:
        g[y][x] = V.GREY + v
    return [[g[y][x] if g[y][x] is not None else GRASS[y][x] for x in range(T)] for y in range(T)]


# ---- flames: levels from the core out, ragged edges, groups that cycle -------------

def flame(kind, thin, salt):
    t = 1 if thin else 0
    cy = cx = 7.5
    rag_t = (0.0, 0.9, 0.3, -0.6, 0.7, -0.3, 1.0, 0.2)

    def rag(a):
        return rag_t[(a * 3 + (4 if thin else 0)) % 8]

    def band(d, half, a=0):
        half -= t
        if d > half + rag(a):
            return None
        if d <= half * 0.28:
            return 0
        if d <= half * 0.55:
            return 1
        if d <= half * 0.80 + rag(a + 3) * 0.5:
            return 2
        return 3

    def h(x, y):
        return band(abs(y - cy), 6.2, x)

    def v(x, y):
        return band(abs(x - cx), 6.2, y)

    def f(x, y):
        r = None
        if kind == "C":
            ang = int((math.atan2(y - cy, x - cx) + math.pi) / (2 * math.pi) * 16) % 16
            dx, dy = x - cx, y - cy
            q = (dx * dx + dy * dy) / (7.4 - t + rag(ang)) ** 2
            if q <= 1.0:
                r = 0 if q <= 0.2 else 1 if q <= 0.45 else 2 if q <= 0.75 else 3
            hv = [z for z in (h(x, y), v(x, y)) if z is not None]
            if hv:
                r = min(hv + ([r] if r is not None else []))
        elif kind == "H":
            r = h(x, y)
        elif kind == "V":
            r = v(x, y)
        elif kind in "LR":
            d = x if kind == "L" else T - 1 - x
            r = band(abs(y - cy), 6.2 - max(0, 5 - d) * 1.0, x)
        elif kind in "UD":
            d = y if kind == "U" else T - 1 - y
            r = band(abs(x - cx), 6.2 - max(0, 5 - d) * 1.0, y)
        return None if r is None else fire_at(r, x, y, salt)
    return over_floor(f)


# ---- powerups: a bevelled panel, a rim (lit: gold), a shaded icon -----------------

PANELS = {"fire": 0, "bombup": 4, "speed": 8, "kick": 12, "glove": 16}
CHEV, BOOT, HAND, LIT = V.PWR + 20, V.PWR + 23, V.PWR + 26, V.PWR + 29


def icon_px(name, ch, x, y):
    """An icon's pixel (12x12 shapes from drawart_cga), shaded."""
    if ch == ".":
        return None
    if name == "fire":
        return fire_at({"w": 2, "r": 1, "c": 0}[ch], x, y, 3)
    if name == "bombup":
        if ch == "w":
            return V.GREY + 15 if y > 2 else V.GREY + 13
        dx, dy = (x + 0.5 - 5.5) / 4.6, (y + 0.5 - 6.8) / 4.6
        d, s, rim = sphere_shade(clamp(dx, -1, 1), clamp(dy, -1, 1))
        return V.BOMB + (6 if s > 0.3 else clamp(int(0.6 + d * 3.2 + rim * 2), 1, 5))
    if name == "speed":
        return CHEV + (2 if ch == "w" and y < 6 else 1 if ch == "w" else 0)
    if name == "kick":
        if ch == "c":
            return V.GREY + 3
        return BOOT + (2 if x < 4 and y < 8 else 1 if x < 7 or y < 9 else 0)
    if name == "glove":
        if ch == "c":
            return CHEV + 1 if y == 9 else CHEV
        return HAND + (2 if x < 5 and y < 6 else 1 if y < 7 else 0)
    raise ValueError(name)


ICONS = {"fire": C.FIRE, "bombup": C.BOMBUP, "speed": C.SPEED, "kick": C.KICK, "glove": C.GLOVE}


def panel(name, lit, bg=None):
    """16x16; bg(x, y) shows at the rounded corners (None: 0)."""
    b = V.PWR + PANELS[name]
    icon = ICONS[name]
    g = grid(T, T)
    for y in range(T):
        for x in range(T):
            corner = (x in (0, 15)) and (y in (0, 15))
            if corner:
                g[y][x] = bg(x, y) if bg else 0
            elif x in (0, 15) or y in (0, 15):
                g[y][x] = (LIT + (2 if x + y < 15 else 1)) if lit else b + 0
            elif x == 1 or y == 1:
                g[y][x] = b + 3
            elif x == 14 or y == 14:
                g[y][x] = b + 0
            else:
                g[y][x] = b + (2 if y < 6 else 1)
            if 2 <= x < 14 and 2 <= y < 14:
                p = icon_px(name, icon[y - 2][x - 2], x - 2, y - 2)
                if p is not None:
                    g[y][x] = p
    return g


# ---- the trophy: gold, a shine down the left, on a dark plinth ------------------

def trophy():
    """16x16 for the menus (0: the backdrop)."""
    g = grid(T, T)
    for y, row in enumerate(C.TROPHY):
        for x, ch in enumerate(row):
            if ch == ".":
                continue
            if y >= 12:
                g[y][x] = V.TROPHY + (5 if ch == "w" else 7)
                continue
            if ch == "c":
                g[y][x] = V.TROPHY + 2
                continue
            k = (x - 7.5) / 5.5
            v = 4.2 - k * 2.4 - abs(k) * 1.2
            if x in (4, 5) and 2 <= y <= 7:
                v = 6
            g[y][x] = V.TROPHY + clamp(int(round(v)), 0, 6)
    return g


def tile_frames():
    t = {
        "floor": [floor()],
        "pillar": [pillar()],
        "block": [bricks()],
        "burn": [burn(0), burn(1)],
        "bomb": [bomb(0, False), bomb(1, False), bomb(0, True), bomb(1, True)],
        "dud": [dud(i) for i in range(4)],
        "trophy": [[[trophy()[y][x] or GRASS[y][x] for x in range(T)] for y in range(T)]],
    }
    for i, k in enumerate("CHVLRUD"):
        t["flame" + k] = [flame(k, False, i), flame(k, True, i + 8)]
    for name in PANELS:
        t[name] = [panel(name, False, lambda x, y: GRASS[y][x]), panel(name, True, lambda x, y: GRASS[y][x])]
    for name, frames in t.items():
        for i, f in enumerate(frames):
            assert len(f) == T and all(len(r) == T for r in f), (name, i)
            assert all(isinstance(v, int) and 0 <= v < 256 for r in f for v in r), (name, i)
    return t


# ---- the arena's frame (8x8 stone) and the side panels' cell -------------------

def wall():
    W = V.WALL
    g = grid(8, 8)
    for y in range(8):
        course, line = y // 4, y % 4
        joint = 7 if course == 0 else 3
        for x in range(8):
            if line == 3 or x == joint:
                g[y][x] = W + 0
            elif line == 0 or (x - 1) % 8 == joint:
                g[y][x] = W + 6 if line == 0 else W + 5
            elif line == 2 or (x + 1) % 8 == joint:
                g[y][x] = W + 2
            else:
                g[y][x] = W + 4 - (1 if hsh(x, y, 9) < 0.25 else 0)
    return g


def panel_cell():
    return [[V.PANEL + V.PANEL_LINES[y]] * 8 for y in range(8)]


# ---- the logo -------------------------------------------------------------------

LOGO_W, LOGO_H = 224, 24


def logo():
    """The Namco letters at 3x: a gradient down each (white-hot, yellow,
    orange, red), lit left edges, a dark outline and a shadow down and to the
    right; 0 where the title's sky shows."""
    font = C.namco()
    lit = grid(LOGO_W, LOGO_H, None)
    x0, y0 = 2, 1
    grad = [0, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 6]
    for i, ch in enumerate(LOGO_TEXT):
        g = C.glyph(font, ch)
        for r, row in enumerate(g):
            for c, p in enumerate(row):
                if p != "#":
                    continue
                for dy in range(3):
                    for dx in range(3):
                        y = y0 + r * 3 + dy
                        lit[y][x0 + i * 24 + c * 3 + dx] = grad[min(r * 3 + dy, len(grad) - 1)]
    out = grid(LOGO_W, LOGO_H)
    on = lambda x, y: 0 <= x < LOGO_W and 0 <= y < LOGO_H and lit[y][x] is not None  # noqa: E731
    for y in range(LOGO_H):
        for x in range(LOGO_W):
            if on(x, y):
                v = lit[y][x]
                if not on(x - 1, y) or not on(x, y - 1):
                    v = max(0, v - 2)
                elif not on(x + 1, y) or not on(x, y + 1):
                    v = min(6, v + 1)
                out[y][x] = V.LOGO + v
    edge = [[False] * LOGO_W for _ in range(LOGO_H)]
    for y in range(LOGO_H):
        for x in range(LOGO_W):
            if not on(x, y) and any(on(x + dx, y + dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                out[y][x] = V.LOGO + 7
                edge[y][x] = True
    for y in range(LOGO_H - 1, -1, -1):
        for x in range(LOGO_W - 1, -1, -1):
            if out[y][x] == 0:
                for dx, dy in ((2, 1), (1, 1)):
                    sx, sy = x - dx, y - dy
                    if sx >= 0 and sy >= 0 and (on(sx, sy) or edge[sy][sx]):
                        out[y][x] = V.LOGO + 8
                        break
    return out


# ---- the bombers ----------------------------------------------------------------

def role_frames():
    """drawart_nes's silhouettes, the belt's buckle in its own role."""
    fr = N.sprite_frames()
    return {n: [r.replace("KKKHHKKK", "KKKYYKKK") for r in fr[n]] for n in SPRITES}


def blur(mask, sigma):
    h, w = len(mask), len(mask[0])
    rad = int(sigma * 2.5) + 1
    k = [math.exp(-(i * i) / (2 * sigma * sigma)) for i in range(-rad, rad + 1)]
    s = sum(k)
    k = [v / s for v in k]
    tmp = [[sum(k[j + rad] * (mask[y][x + j] if 0 <= x + j < w else 0) for j in range(-rad, rad + 1))
            for x in range(w)] for y in range(h)]
    return [[sum(k[j + rad] * (tmp[y + j][x] if 0 <= y + j < h else 0) for j in range(-rad, rad + 1))
             for x in range(w)] for y in range(h)]


def epx(rows):
    """Scale2x of a role frame (strings), for the 32x32 cheer."""
    h, w = len(rows), len(rows[0])
    at = lambda x, y: rows[y][x] if 0 <= x < w and 0 <= y < h else "."  # noqa: E731
    out = [[None] * (w * 2) for _ in range(h * 2)]
    for y in range(h):
        for x in range(w):
            p, a, b, c, d = at(x, y), at(x, y - 1), at(x + 1, y), at(x - 1, y), at(x, y + 1)
            e1 = a if (c == a and c != d and a != b) else p
            e2 = b if (a == b and a != c and b != d) else p
            e3 = c if (d == c and d != b and c != a) else p
            e4 = d if (b == d and b != a and d != c) else p
            out[y * 2][x * 2], out[y * 2][x * 2 + 1] = e1, e2
            out[y * 2 + 1][x * 2], out[y * 2 + 1][x * 2 + 1] = e3, e4
    return ["".join(r) for r in out]


def shade_frame(rows, scale=1):
    """A role frame -> codes (vgapal C_*): the body lit from the upper left
    by normals from its blurred silhouette, H/S nudging it; a glassy visor;
    boots, belt and buckle; the outline."""
    h, w = len(rows), len(rows[0])
    body = set("BHSVEY")
    mask = [[1.0 if rows[y][x] in body else 0.0 for x in range(w)] for y in range(h)]
    fld = blur(mask, 1.25 * scale)
    out = [[0] * w for _ in range(h)]
    vis = [[rows[y][x] in "VE" for x in range(w)] for y in range(h)]
    for y in range(h):
        for x in range(w):
            ch = rows[y][x]
            if ch == ".":
                continue
            if ch == "K":
                if y >= 14 * scale:
                    out[y][x] = V.C_BOOT + (1 if y < 15 * scale else 0)
                elif "Y" in rows[y]:
                    out[y][x] = V.C_BOOT
                else:
                    out[y][x] = V.C_OUT
                continue
            if ch == "Y":
                out[y][x] = V.C_BUCKLE
                continue
            if ch == "E":
                out[y][x] = V.C_EYE
                continue
            if ch == "V":
                top = y == 0 or not vis[y - 1][x]
                bottom = y == h - 1 or not vis[y + 1][x]
                xs = [xx for xx in range(w) if vis[y][xx]]
                left = (x - xs[0]) / max(1, xs[-1] - xs[0])
                out[y][x] = V.C_VIS + (2 if top and left < 0.45 else 1 if top or (not bottom and left < 0.2) else 0)
                continue
            if ch == "H" and not any(0 <= x + dx < w and 0 <= y + dy < h and rows[y + dy][x + dx] in "BS"
                                     for dx in (-1, 0, 1) for dy in (-1, 0, 1)):
                out[y][x] = V.C_BUCKLE          # die2's dizzy stars
                continue
            g = lambda xx, yy: fld[clamp(yy, 0, h - 1)][clamp(xx, 0, w - 1)]  # noqa: E731
            gx = g(x + 1, y) - g(x - 1, y)
            gy = g(x, y + 1) - g(x, y - 1)
            n = norm((-gx * 2.4 * scale, -gy * 2.4 * scale, 1.0))
            d = max(0.0, sum(n[i] * LIGHT[i] for i in range(3)))
            s = max(0.0, sum(n[i] * HALF[i] for i in range(3))) ** 20
            v = 0.15 + d * 0.95 + s * 0.6 + (0.18 if ch == "H" else -0.18 if ch == "S" else 0)
            out[y][x] = V.C_BODY + clamp(int(round(v * 6)), 0, 6)
    return out


def sprite_codes():
    rf = role_frames()
    return {n: shade_frame(rf[n]) for n in SPRITES}


def big_codes():
    rf = role_frames()
    return [shade_frame(epx(rf["win%d" % k]), 2) for k in (0, 1)]


def seated(codes, seat):
    return [[V.SEAT + 16 * seat + c if c else 0 for c in r] for r in codes]


# ---- writing ----------------------------------------------------------------------

def indexed(w, h, transparent=None):
    img = Image.new("P", (w, h), 0)
    flat = []
    for c in PAL:
        flat += list(V.to8(V.to6(c)))
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
    img = indexed(4 * T, len(TILES) * T)
    for i, name in enumerate(TILES):
        for f, rows in enumerate(tiles[name]):
            put(img, f * T, i * T, rows)
    img.save(os.path.join(out, "tiles.png"))

    img = indexed(16, 8)
    put(img, 0, 0, wall())
    put(img, 8, 0, panel_cell())
    img.save(os.path.join(out, "cells.png"))

    img = indexed(5 * T, T, transparent=0)
    for i, name in enumerate(PANELS):
        put(img, i * T, 0, panel(name, True))
    img.save(os.path.join(out, "icons.png"))

    img = indexed(T, T, transparent=0)
    put(img, 0, 0, trophy())
    img.save(os.path.join(out, "trophy.png"))

    img = indexed(LOGO_W, LOGO_H, transparent=0)
    put(img, 0, 0, logo())
    img.save(os.path.join(out, "logo.png"))

    codes = sprite_codes()
    img = indexed(len(SPRITES) * T, 4 * T, transparent=0)
    for seat in range(4):
        for i, name in enumerate(SPRITES):
            put(img, i * T, seat * T, seated(codes[name], seat))
    img.save(os.path.join(out, "sprites.png"))

    big = big_codes()
    img = indexed(64, 4 * 32, transparent=0)
    for seat in range(4):
        for k in (0, 1):
            put(img, k * 32, seat * 32, seated(big[k], seat))
    img.save(os.path.join(out, "bigwin.png"))


# ---- previews: mock screens as mode 13h shows them --------------------------------

class Screen:
    """200 lines of 320 DAC entries; the backdrop (line map) and the 32
    backdrop colours it would load."""

    def __init__(self, line=V.MENU_LINE, back=V.MENU_BACK):
        self.line, self.back = line, back
        self.px = [[line[y]] * 320 for y in range(200)]

    def plot(self, x, y, v):
        if 0 <= x < 320 and 0 <= y < 200:
            self.px[y][x] = v

    def rows(self, x0, y0, rows, key=False):
        for y, r in enumerate(rows):
            for x, v in enumerate(r):
                if key and not v:
                    v = self.line[y0 + y] if 0 <= y0 + y < 200 else 0
                self.plot(x0 + x, y0 + y, v)

    def sprite(self, x0, y0, codes):
        for y, r in enumerate(codes):
            for x, v in enumerate(r):
                if v:
                    self.plot(x0 + x, y0 + y, v)

    def text(self, col, row, s, style, big=False, bg=None):
        font = C.namco()
        fg = V.STYLES[style]
        w = 16 if big else 8
        for i, ch in enumerate(s):
            g = C.glyph(font, ch)
            for y in range(8):
                yy = row * 8 + y
                b = bg[y] if bg else self.line[yy]
                for x in range(w):
                    on = g[y][x // 2 if big else x] == "#"
                    self.plot((col + i) * w + x, yy, fg[y] if on else b)

    def centre(self, row, s, style, big=False):
        self.text(((20 if big else 40) - len(s)) // 2, row, s, style, big)

    def save(self, path):
        pal = list(PAL)
        for i, c in enumerate(self.back):
            pal[V.BACK + i] = c
        rgb = [V.to8(V.to6(c)) for c in pal]
        img = Image.new("RGB", (320, 200))
        img.putdata([rgb[v] for r in self.px for v in r])
        img = img.resize((960, 600), Image.NEAREST).resize((960, 720), Image.NEAREST)
        img.save(path)


HUD = [("ANN", 0, "1"), ("BOB", 1, "0"), ("CAT", 2, "2"), ("DAN", 3, "1")]


def mock_arena():
    s = Screen(V.ARENA_LINE)
    tiles = tile_frames()
    codes = sprite_codes()
    for (name, seat, wins), col in zip(HUD, (0, 4, 12, 16)):
        s.text(col, 0, name + wins, V.ST_SEAT + seat, big=True)
    s.text(8, 0, "2:41", V.ST_SEAT, big=True)
    w = wall()
    pc = panel_cell()
    for r in range(1, 25):
        for c in range(40):
            if (r in (1, 24) and 6 <= c <= 33) or c in (6, 33):
                s.rows(c * 8, r * 8, w)
            elif not (7 <= c <= 32 and 2 <= r <= 23):
                s.rows(c * 8, r * 8, pc)
    rnd = random.Random(7)
    g = [["floor"] * 13 for _ in range(11)]
    for r in range(11):
        for c in range(13):
            if r % 2 and c % 2:
                g[r][c] = "pillar"
            elif rnd.random() < 0.5 and not (r < 2 and c < 2) and not (r > 8 and c > 10):
                g[r][c] = "block"
    for (c, r) in [(0, 0), (1, 0), (0, 1), (12, 10), (11, 10), (12, 9), (12, 0), (0, 10), (6, 8), (5, 8), (7, 8)]:
        g[r][c] = "floor"
    g[4][6] = "flameC"
    for c in (4, 5, 7, 8):
        g[4][c] = "flameH"
    g[4][3], g[4][9] = "flameL", "flameR"
    g[2][6], g[6][6] = "flameU", "flameD"
    g[3][6] = g[5][6] = "flameV"
    g[4][2] = "burn"
    g[8][2], g[8][4] = "bomb", "dud"
    for i, p in enumerate(PANELS):
        g[0][4 + i] = p
    g[10][6] = "bomb"
    for r in range(11):
        for c in range(13):
            name = g[r][c]
            fr = tiles[name]
            f = fr[1] if name in PANELS else fr[(c % 2) if len(fr) > 1 and name not in ("bomb", "dud") else 0]
            if name == "bomb" and c == 6:
                f = fr[2]
            s.rows(56 + c * 16, 16 + r * 16, f)
    s.rows(56 + 5 * 16 + 8, 16 + 8 * 16, tiles["bomb"][1])
    for i, name in enumerate(PANELS):
        s.rows(8, (3 + i * 4) * 8, panel(name, True), key=True)
        if i < 3:
            s.text(3, 4 + i * 4, "X" + str([3, 2, 1][i]), V.ST_SILVER)
    s.text(35, 3, "RND", V.ST_SILVER)
    s.text(36, 4, " 3", V.ST_SILVER)
    for seat, (name, px, py) in enumerate([("down0", 56, 16), ("right1", 56 + 11 * 16 + 6, 16 + 10 * 16),
                                           ("left2", 56 + 6 * 16 + 9, 16 + 6 * 16),
                                           ("up1", 56 + 12 * 16, 16 + 3 * 16 + 4)]):
        s.sprite(px, py, seated(codes[name], seat))
    return s


def mock_title():
    s = Screen(V.MENU_LINE, V.TITLE_BACK)
    codes = sprite_codes()
    s.rows((320 - LOGO_W) // 2, 24, logo(), key=True)
    s.centre(8, "FOR FUJINET", V.ST_GOLD, big=True)
    for seat in range(4):
        s.sprite(40 + seat * 80, 96, seated(codes[["right0", "right1", "right0", "right2"][seat]], seat))
    s.centre(18, "PRESS FIRE", V.ST_SILVER)
    s.centre(21, "BOMBS, BLOCKS AND BLASTS FOR 2-4", V.ST_SILVER)
    s.centre(23, "ESC: LOBBY", V.ST_SILVER)
    return s


def mock_wait():
    s = Screen()
    codes = sprite_codes()
    s.centre(1, "ROOM BLAST1", V.ST_GOLD, big=True)
    names = ["ANN", "BOB", "CAT", None]
    for seat in range(4):
        r = 4 + seat * 4
        if names[seat]:
            s.text(4, r, names[seat], V.ST_SEAT + seat, big=True)
            if seat == 0:
                s.text(14, r, "HOST", V.ST_SEAT + seat, big=True)
            s.sprite(16, r * 8, seated(codes["down0"], seat))
        else:
            s.text(4, r, "--------", V.ST_SEAT + seat, big=True)
    s.text(12, 20, "ROUNDS: 5", V.ST_SILVER)
    s.text(23, 20, "(LEFT/RIGHT)", V.ST_SILVER)
    s.text((40 - 19) // 2, 22, "PRESS FIRE TO START", V.ST_INV, bg=V.INV_BAR)
    s.centre(24, "ESC: LEAVE    M: SOUND", V.ST_SILVER)
    return s


def mock_results():
    s = Screen()
    codes = sprite_codes()
    big = big_codes()
    s.centre(1, "ANN WINS!", V.ST_SEAT, big=True)
    wins = [3, 1, 2, 0]
    tr = trophy()
    for k, (name, seat) in enumerate((("ANN", 0), ("BOB", 1), ("CAT", 2), ("DAN", 3))):
        r = 4 + k * 5
        s.text(4, r, name, V.ST_SEAT + seat, big=True)
        s.text(13, r, str(wins[k]), V.ST_SEAT + seat, big=True)
        for t in range(wins[k]):
            s.rows((8 + t * 2) * 8, (r + 1) * 8, tr, key=True)
        s.sprite(16, r * 8, seated(codes["down0"], seat))
    s.sprite(272, 96, seated(big[0], 0))
    s.text(8, 24, "BACK TO THE ROOM IN 9", V.ST_SILVER)
    return s


def mock_sheet():
    """Every tile frame, the icons and trophy, every bomber frame in each
    seat, the cheer: on the floor, enlarged."""
    tiles = tile_frames()
    codes = sprite_codes()
    big = big_codes()
    w, h = 18 * 18 + 4, 5 * 18 + 4 * 18 + 2 * 34 + 12
    px = [[GRASS[y % T][x % T] for x in range(w)] for y in range(h)]

    def blit(x0, y0, rows, key=False):
        for y, r in enumerate(rows):
            for x, v in enumerate(r):
                if not (key and not v):
                    px[y0 + y][x0 + x] = v
    x, y = 2, 2
    for name in TILES:
        for f in tiles[name]:
            if x + 16 > w - 2:
                x, y = 2, y + 18
            blit(x, y, f)
            x += 18
    for name in PANELS:
        blit(x, y, panel(name, True), key=True)
        x += 18
    blit(x, y, trophy(), key=True)
    y += 22
    for seat in range(4):
        for i, name in enumerate(SPRITES):
            blit(2 + i * 18, y, seated(codes[name], seat), key=True)
        y += 18
    y += 2
    for seat in range(4):
        for k in (0, 1):
            blit(2 + (seat * 2 + k) * 36, y, seated(big[k], seat), key=True)
    rgb = [V.to8(V.to6(c)) for c in PAL]
    img = Image.new("RGB", (w, h))
    img.putdata([rgb[v] for r in px for v in r])
    return img.resize((w * 4, h * 4), Image.NEAREST)


def mock_palette():
    rgb = [V.to8(V.to6(c)) for c in PAL]
    img = Image.new("RGB", (16, 16))
    img.putdata(rgb)
    return img.resize((512, 512), Image.NEAREST)


def main():
    write_sheet(os.path.join(ROOT, "gfx", "vga"))
    print("wrote gfx/vga/tiles.png cells.png icons.png trophy.png logo.png sprites.png bigwin.png")
    if len(sys.argv) > 2 and sys.argv[1] == "--preview":
        d = sys.argv[2]
        os.makedirs(d, exist_ok=True)
        for n, f in (("arena", mock_arena), ("title", mock_title), ("wait", mock_wait), ("results", mock_results)):
            f().save(os.path.join(d, n + ".png"))
        mock_sheet().save(os.path.join(d, "sheet.png"))
        mock_palette().save(os.path.join(d, "palette.png"))
        print("previews in", d)


if __name__ == "__main__":
    main()

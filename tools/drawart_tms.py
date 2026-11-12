#!/usr/bin/env python3
"""drawart_tms.py -- FujiBlast's art for the TMS9918A (the Coleco Adam):
Graphics II, 256x192, 15 colours, but only 2 colours in any 8-pixel run of
a line. Written out as gfx/tms/*.png (tools/mkgfx_tms.py converts it).

The designs are drawart_cga.py's (original art), recoloured per tile and
then squeezed to the TMS rule: any 8x1 run holding more than 2 colours
keeps its two commonest, the rest turning into the nearer of the two.
Sprites are two hardware layers, each one colour:
  A  the seat's colour: body, and the visor's whole area
  B  black, in front: visor (eyes cut out, so A shows), gloves, boots, belt
Text is the Namco arcade font (gfx/namco.bin), one 8x8 cell a character.

    python3 tools/drawart_tms.py                # rewrites gfx/tms/*.png
    python3 tools/drawart_tms.py --preview D    # also mock screens into D
"""
import os
import sys
from collections import Counter

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import drawart_cga as C  # noqa: E402
from artlayout import TILES, SPRITES  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

# TMS9918A colours 1-15 (index 0 is transparent), as a monitor shows them.
RGB = [
    None, (0, 0, 0), (33, 200, 66), (94, 220, 120), (84, 85, 237), (125, 118, 252),
    (212, 82, 77), (66, 235, 245), (252, 85, 84), (255, 121, 120), (212, 193, 84),
    (230, 206, 128), (33, 176, 59), (201, 91, 186), (204, 204, 204), (255, 255, 255),
]
FLOOR, BLACK, BLUE, WHITE = 12, 1, 4, 15
# The seats: white, red, cyan, yellow (the visor layer is black).
SEAT_COL = [15, 8, 7, 11]


def dist(a, b):
    return sum((x - y) ** 2 for x, y in zip(RGB[a], RGB[b]))


stats = Counter()


def squeeze(rows, what):
    """rows: lists of colour indices. Every aligned 8-pixel run -> at most 2."""
    for y, row in enumerate(rows):
        for x0 in range(0, len(row), 8):
            run = row[x0:x0 + 8]
            cnt = Counter(run)
            if len(cnt) <= 2:
                continue
            keep = [c for c, _ in cnt.most_common(2)]
            for i, c in enumerate(run):
                if c not in keep:
                    row[x0 + i] = min(keep, key=lambda k: dist(c, k))
                    stats[what] += 1
    return rows


def paint(rows, m):
    return [[m[p] for p in r] for r in rows]


# ---- tiles -----------------------------------------------------------------

ICON_ACCENT = {"fire": (11, 10), "bombup": (7, 5), "speed": (15, 14), "kick": (3, 2), "glove": (13, 6)}


def tile_sheet():
    src = C.tile_frames()
    out = {}
    plain = {
        "floor": {".": FLOOR},
        "pillar": {".": FLOOR, "w": 15, "c": 14, "r": 14},
        "block": {".": 6, "w": 9, "r": 8, "c": 8},
        "burn": {".": 6, "w": 11, "r": 8, "c": 8},
        "bomb": {".": FLOOR, "w": 15, "c": 1, "r": 8},
        "dud": {".": FLOOR, "w": 15, "c": 14, "r": 10},
        "trophy": {".": BLUE, "w": 11, "c": 10, "r": 6},
    }
    for k in "CHVLRUD":
        plain["flame" + k] = {".": 8, "w": 11, "r": 8, "c": 8}
    # a charred block: dark red bricks, black joints, embers on the courses'
    # tops and tongues of flame (yellow tips) leaping off the top
    def burn(f):
        rows = [list(r) for r in C.bricks("r", "w")]
        rows = [[{"w": 8, "r": 6, ".": 1}[p] for p in r] for r in rows]
        for tx, h in zip(([3, 8, 12] if f == 0 else [1, 6, 10, 14]), ([5, 6, 4] if f == 0 else [4, 6, 5, 4])):
            for y in range(h):
                for x in range(max(0, tx - 1), min(16, tx + 2)):
                    if abs(x - tx) <= (1 if y > 1 else 0) or y >= 2:
                        rows[y][x] = 11 if y >= h - 3 and abs(x - tx) == 0 else 8
        return rows
    src["burn"] = [None, None]
    plain["burn"] = None
    for name, frames in src.items():
        if name == "burn":
            out[name] = [squeeze(burn(0), "tile burn"), squeeze(burn(1), "tile burn")]
        elif name in ICON_ACCENT:
            a, d = ICON_ACCENT[name]
            # frame 0 unlit (dim), frame 1 lit
            out[name] = [squeeze(paint(frames[0], {".": BLUE, "w": d, "c": d, "r": BLUE}), "tile " + name),
                         squeeze(paint(frames[1], {".": BLUE, "w": a, "c": a, "r": BLUE}), "tile " + name)]
        else:
            out[name] = [squeeze(paint(f, plain[name]), "tile " + name) for f in frames]
    return out


# k black, g grey, w white: stone with black mortar, every 8-run <= 2 colours
WALL_ROWS = ["wwwwwwwk", "gggggggk", "gggggggk", "kkkkkkkk",
             "wwwkwwww", "gggkgggg", "gggkgggg", "kkkkkkkk"]
WALL_M = {"w": 15, "g": 14, "k": 1}
CELLS = [paint(WALL_ROWS, WALL_M), [[BLACK] * 8 for _ in range(8)]]


# ---- logo: white top, red body, dark red shadow, on black ---------------------

def logo_rows():
    rows = C.logo()
    m = {".": BLACK, "w": 15, "r": 8, "c": 6}
    return squeeze(paint(rows, m), "logo")


# ---- sprites ---------------------------------------------------------------

def layers(frame):
    """frame (CGA roles) -> (A, B) as lists of 0/1 rows."""
    a = [[1 if p in "#ve" else 0 for p in r] for r in frame]
    b = [[1 if p in "vt" else 0 for p in r] for r in frame]
    return a, b


def doubled(rows):
    return [[p for p in r for _ in (0, 1)] for r in rows for _ in (0, 1)]


# ---- writing ---------------------------------------------------------------

def put(img, x0, y0, rows):
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            img.putpixel((x0 + x, y0 + y), RGB[c])


def write_sheet(out):
    os.makedirs(out, exist_ok=True)
    tiles = tile_sheet()
    img = Image.new("RGB", (4 * 16, len(TILES) * 16), RGB[FLOOR])
    for i, name in enumerate(TILES):
        for f, rows in enumerate(tiles[name]):
            put(img, f * 16, i * 16, rows)
    img.save(os.path.join(out, "tiles.png"))
    img = Image.new("RGB", (16, 8), RGB[BLACK])
    for i, cell in enumerate(CELLS):
        put(img, i * 8, 0, cell)
    img.save(os.path.join(out, "cells.png"))
    rows = logo_rows()
    img = Image.new("RGB", (len(rows[0]), 24), RGB[BLACK])
    put(img, 0, 0, rows)
    img.save(os.path.join(out, "logo.png"))
    # sprites keep the CGA sheet's roles (# body, t trim, v visor, e eyes)
    frames = C.sprite_frames()
    img = Image.new("RGBA", (len(SPRITES) * 16, 16), C.ROLE_RGBA["."])
    for i, name in enumerate(SPRITES):
        C.check(name, frames[name])
        C.put(img, i * 16, 0, frames[name], lambda p: C.ROLE_RGBA[p] if p != "." else None)
    img.save(os.path.join(out, "sprites.png"))


# ---- previews: mock screens, 256x192, the TMS's own pixels -----------------

class Screen:
    def __init__(self, bg):
        self.px = [[bg] * 256 for _ in range(192)]
        self.bg = bg

    def plot(self, x, y, c):
        if 0 <= x < 256 and 0 <= y < 192:
            self.px[y][x] = c

    def rows(self, x0, y0, rows):
        for y, r in enumerate(rows):
            for x, c in enumerate(r):
                self.plot(x0 + x, y0 + y, c)

    def text(self, col, row, s, fg, bg=None):
        font = C.namco()
        for i, ch in enumerate(s):
            g = C.glyph(font, ch)
            for y in range(8):
                for x in range(8):
                    if g[y][x] == "#":
                        self.plot((col + i) * 8 + x, row * 8 + y, fg)
                    elif bg is not None:
                        self.plot((col + i) * 8 + x, row * 8 + y, bg)

    def sprite(self, x0, y0, frame, seat, big=False):
        a, b = layers(frame)
        if big:
            a, b = doubled(a), doubled(b)
        for rows, col in ((a, SEAT_COL[seat]), (b, BLACK)):
            for y, r in enumerate(rows):
                for x, p in enumerate(r):
                    if p:
                        self.plot(x0 + x, y0 + y, col)

    def save(self, path, scale=3):
        img = Image.new("RGB", (256 * scale, 192 * scale))
        px = img.load()
        for y in range(192):
            for x in range(256):
                c = RGB[self.px[y][x]]
                for dy in range(scale):
                    for dx in range(scale):
                        px[x * scale + dx, y * scale + dy] = c
        img.save(path)


def mock_arena():
    import random
    s = Screen(FLOOR)
    tiles = tile_sheet()
    frames = C.sprite_frames()
    for y in range(0, 8):                       # HUD row
        for x in range(256):
            s.plot(x, y, BLACK)
    for (name, seat, wins), col in zip((("ANN", 0, "1"), ("BOB", 1, "0"), ("CAT", 2, "2"), ("DAN", 3, "1")),
                                       (2, 8, 18, 24)):
        s.text(col, 0, name, SEAT_COL[seat])
        s.text(col + 4, 0, wins, WHITE)
    s.text(13, 0, "2:41", WHITE)
    for r in range(1, 24):                      # panel and walls
        for c in range(32):
            wall = c in (4, 31) or r == 23
            if wall and c >= 4:
                s.rows(c * 8, r * 8, CELLS[0])
            elif c < 4:
                s.rows(c * 8, r * 8, CELLS[1])
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
            s.rows(40 + c * 16, 8 + r * 16, fr)
    s.rows(40 + 5 * 16 + 8, 8 + 8 * 16, tiles["bomb"][1])
    for i, name in enumerate(("fire", "bombup", "speed", "kick", "glove")):
        s.rows(0, (3 + i * 4) * 8, tiles[name][1])
        if i < 3:
            s.text(2, 4 + i * 4, ["3", "2", "1"][i].rjust(2), WHITE)
    s.text(0, 21, "RND", WHITE)
    s.text(2, 22, "3", 7)
    for seat, (name, px, py) in enumerate([("down0", 40, 8), ("right1", 40 + 11 * 16 + 6, 8 + 10 * 16),
                                           ("left2", 40 + 6 * 16 + 9, 8 + 6 * 16), ("up1", 40 + 12 * 16, 8 + 3 * 16 + 4)]):
        s.sprite(px, py, frames[name], seat)
    return s


def mock_title():
    s = Screen(BLUE)
    for y in range(24, 48):
        for x in range(256):
            s.plot(x, y, BLACK)
    lg = logo_rows()
    s.rows(16, 24, lg)
    s.text(10, 8, "FOR FUJINET", WHITE)
    frames = C.sprite_frames()
    for seat in range(4):
        s.sprite(24 + seat * 56, 96, frames[["right0", "right1", "right0", "right2"][seat]], seat)
    s.text(11, 18, "PRESS FIRE", WHITE)
    s.text(0, 21, "BOMBS, BLOCKS AND BLASTS FOR 2-4", 7)
    s.text(11, 23, "ESC: LOBBY", 9)
    return s


def mock_wait():
    s = Screen(BLUE)
    frames = C.sprite_frames()
    s.text(10, 1, "ROOM BLAST1", WHITE)
    names = ["ANN", "BOB", "CAT", None]
    for seat in range(4):
        r = 4 + seat * 3
        if names[seat]:
            s.text(6, r, names[seat], SEAT_COL[seat])
            if seat == 0:
                s.text(14, r, "HOST", SEAT_COL[seat])
            s.sprite(24, r * 8 - 4, frames["down0"], seat)
        else:
            s.text(6, r, "--------", 14)
    s.text(9, 17, "ROUNDS: 5", WHITE)
    s.text(9, 18, "(LEFT/RIGHT)", 7)
    s.text(6, 21, "PRESS FIRE TO START", 8, WHITE)
    s.text(5, 23, "ESC: LEAVE    M: SOUND", WHITE)
    return s


def mock_results():
    s = Screen(BLUE)
    frames = C.sprite_frames()
    tiles = tile_sheet()
    s.text(11, 0, "ANN WINS!", WHITE)
    wins = [3, 1, 2, 0]
    for k, (name, seat) in enumerate((("ANN", 0), ("BOB", 1), ("CAT", 2), ("DAN", 3))):
        r = 3 + k * 4
        s.text(6, r, name, SEAT_COL[seat])
        s.text(11, r, str(wins[k]), SEAT_COL[seat])
        for t in range(wins[k]):
            s.rows(56 + t * 16, (r + 1) * 8, tiles["trophy"][0])
        s.sprite(24, r * 8 - 4, frames["down0"], seat)
    s.sprite(200, 80, frames["win0"], 0, big=True)
    s.text(5, 23, "BACK TO THE ROOM IN 9", WHITE)
    return s


def mock_sheet():
    s = Screen(FLOOR)
    frames = C.sprite_frames()
    for seat in range(4):
        for i, name in enumerate(SPRITES[:14]):
            s.sprite(2 + i * 17 % 255, 4 + seat * 20 + (i // 14) * 0, frames[name], seat) if i < 14 else None
    tiles = tile_sheet()
    x, y = 2, 90
    for name in TILES:
        for f in tiles[name]:
            if x + 16 > 254:
                x, y = 2, y + 18
            s.rows(x, y, f)
            x += 17
    return s


def main():
    write_sheet(os.path.join(ROOT, "gfx", "tms"))
    print("wrote gfx/tms/tiles.png cells.png logo.png sprites.png")
    print("squeezed pixels:", dict(stats))
    if len(sys.argv) > 2 and sys.argv[1] == "--preview":
        d = sys.argv[2]
        os.makedirs(d, exist_ok=True)
        for n, f in (("arena", mock_arena), ("title", mock_title), ("wait", mock_wait),
                     ("results", mock_results), ("sheet", mock_sheet)):
            f().save(os.path.join(d, n + ".png"))
        print("previews in", d)


if __name__ == "__main__":
    main()

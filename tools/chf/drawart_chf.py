#!/usr/bin/env python3
"""drawart_chf.py -- the Fairchild Channel F's art, drawn for its 2-bit
bitmap and per-row palette: gfx/channelf/tiles.png and sprites.png
(tools/chf/mkgfx_chf.py turns them into the build's tables), and with
--preview DIR mock screens of the looks at MAME's scale.

    drawart_chf.py [--preview DIR]        (needs Pillow; the build doesn't)

A pixel is a value 0-3. Value 0 is the row's background, which its
palette colours (cols 125/126): pal0 black, pal1 light blue, pal2 light
grey, pal3 light green. Values 1-3 are blue, red and green in pal1-3 and
white in pal0. So the arena's inks are fixed and a row's palette only
picks its floor: roles '.' floor (0), 'b' blue (1), 'r' red (2), 'g'
green (3).

A tile is 6x5 (about square: the Channel F's pixels are taller than
wide). Its bottom row and right column are mostly floor, so the seam
rows' palette draws a grid for free. Every frame of an animation differs
from the one before in a few pixels: the console writes a pixel at a
time, about 150 clocks each (mkgfx_chf.py counts them and fails the
build past its targets).

A bomber is 6x5 in two roles, '#' body (the seat's ink) and 'v' visor
(opaque floor, a hole showing the background colour); '.' is clear.
Its runs are solid so a 1-pixel step changes few pixels.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
sys.path.insert(0, os.path.join(HERE, ".."))
from artlayout import TILES, SPRITES  # noqa: E402

OUT = os.path.join(ROOT, "gfx", "channelf")

# role -> RGB in the PNGs (mkgfx_chf.py reads them back by these)
ROLE_RGB = {".": (224, 224, 224), "b": (75, 63, 243), "r": (255, 49, 83), "g": (2, 204, 93)}
SPR_RGB = {"#": (255, 255, 255, 255), "v": (224, 224, 224, 255), ".": (0, 0, 0, 0)}
ROLE_VAL = {".": 0, "b": 1, "r": 2, "g": 3}

TW, TH = 6, 5

# ---- tiles ------------------------------------------------------------------------
PILLAR = [
    "bbbbbb",
    "b.bbbb",
    "bbbbbb",
    "bbbbbb",
    "......",
]
BLOCK = [
    "rr.rrr",
    "rr.rrr",
    "......",
    "rrrr.r",
    "......",
]
BURN = [[
    "r...r.",
    "gg.ggg",
    "......",
    "gggg.g",
    "......",
], [
    "..r..r",
    "gg.ggg",
    "......",
    "gggg.g",
    "......",
]]
# the spark moves (2 pixels), the highlight blinks (1)
_B = [".bbbb.", "bb.bbb", "bbbbbb", ".bbbb."]
_BH = [".bbbb.", "bbbbbb", "bbbbbb", ".bbbb."]
BOMB = [["...r.."] + _B, ["....r."] + _B, ["...r.."] + _BH, ["....r."] + _BH]
_D = [".gggg.", "gg.ggg", "gggggg", ".gggg."]
DUD = [["..g..."] + _D, [".g...."] + _D, ["...g.."] + _D, ["..g..."] + _D]


def flame(kind, thin):
    """A blast's piece: a red band round a floor-coloured (white-hot) core;
    thin = cooled, the core filled red (one row or column of change)."""
    core = "r" if thin else "."

    def h(x, y):                # the horizontal band: rows 1-3
        if y in (1, 3):
            return "r"
        if y == 2:
            return core

    def v(x, y):                # the vertical band: cols 1-3
        if x in (1, 3):
            return "r"
        if x == 2:
            return core

    def f(x, y):
        if kind == "C":             # both bands, their cores crossing
            if x == 2 or y == 2:
                return core
            if x in (1, 3) or y in (1, 3):
                return "r"
            return None
        if kind == "H":
            return h(x, y)
        if kind == "V":
            return v(x, y)
        if kind == "R":             # the arm comes from the left, ends here
            if x > 3:
                return None
            if x == 3:
                return "r" if 1 <= y <= 3 else None
            return h(x, y)
        if kind == "L":             # the arm comes from the right
            if x < 1:
                return None
            if x == 1:
                return "r" if 1 <= y <= 3 else None
            return h(x, y)
        if kind == "U":             # the arm comes from below
            if y < 1:
                return None
            if y == 1:
                return "r" if 1 <= x <= 3 else None
            return v(x, y)
        if kind == "D":             # the arm comes from above
            if y > 3:
                return None
            if y == 3:
                return "r" if 1 <= x <= 3 else None
            return v(x, y)
    return ["".join(f(x, y) or "." for x in range(TW)) for y in range(TH)]


# a powerup: its letter in blue on a green plate, a corner spark blinking
LETTER = {
    "F": ["###", "#..", "##.", "#..", "#.."],
    "B": ["##.", "#.#", "##.", "#.#", "##."],
    "S": ["###", "#..", "###", "..#", "###"],
    "K": ["#.#", "#.#", "##.", "#.#", "#.#"],
    "G": ["###", "#..", "#.#", "#.#", "###"],
}


def plate(letter, lit):
    rows = []
    for y, r in enumerate(LETTER[letter]):
        row = "g" + "".join("b" if c == "#" else "g" for c in r) + "g"
        row += ("r" if lit else ".") if y == 0 else "."
        rows.append(row)
    return rows


TROPHY = [
    "rgggr.",
    ".ggg..",
    "..g...",
    ".ggg..",
    "......",
]


def tile_frames():
    """name -> list of frames (each TH rows of TW), in artlayout's TILES order."""
    t = {
        "floor": [["." * TW] * TH],
        "pillar": [PILLAR],
        "block": [BLOCK],
        "burn": BURN,
        "bomb": BOMB,
        "dud": DUD,
        "trophy": [TROPHY],
    }
    for k in "CHVLRUD":
        t["flame" + k] = [flame(k, False), flame(k, True)]
    for name, letter in (("fire", "F"), ("bombup", "B"), ("speed", "S"),
                         ("kick", "K"), ("glove", "G")):
        t[name] = [plate(letter, True), plate(letter, False)]
    return [t[n] for n in TILES]


# ---- the bombers: 6x5, # body, v visor (opaque floor) ---------------------------
DOWN = [
    ".####.",
    "##vv##",
    ".####.",
    "######",
    ".#..#.",
]
UP = [DOWN[0], "######"] + DOWN[2:]
RIGHT = [
    ".####.",
    "###vv#",
    ".####.",
    "..###.",
    "..#.#.",
]


def legs(f, row):
    return f[:4] + [row]


def mirror(f):
    return [r[::-1] for r in f]


def sprite_frames():
    fr = {}
    fr["down0"] = DOWN
    fr["down1"] = legs(DOWN, "#...#.")
    fr["down2"] = legs(DOWN, ".#...#")
    fr["up0"] = UP
    fr["up1"] = legs(UP, "#...#.")
    fr["up2"] = legs(UP, ".#...#")
    fr["right0"] = RIGHT
    fr["right1"] = legs(RIGHT, ".#..#.")
    fr["right2"] = legs(RIGHT, "..##..")
    for k in "012":
        fr["left" + k] = mirror(fr["right" + k])
    fr["win0"] = [
        "#.##.#",
        "##vv##",
        ".####.",
        "..##..",
        ".#..#.",
    ]
    fr["win1"] = [
        ".####.",
        "##vv##",
        "######",
        ".####.",
        ".#..#.",
    ]
    fr["die0"] = fr["up0"]
    fr["die1"] = fr["left0"]
    fr["die2"] = [
        "......",
        ".####.",
        "##vv##",
        "######",
        ".#..#.",
    ]
    fr["die3"] = [
        "#..#..",
        "..#..#",
        "#....#",
        ".#.#..",
        "#...#.",
    ]
    return [fr[n] for n in SPRITES]


# ---- PNGs --------------------------------------------------------------------------
def save(path, rows, rgb, mode="RGB"):
    from PIL import Image
    h, w = len(rows), len(rows[0])
    img = Image.new(mode, (w, h))
    for y, r in enumerate(rows):
        assert len(r) == w, (path, y, r)
        for x, p in enumerate(r):
            img.putpixel((x, y), rgb[p])
    img.save(path)


# ---- the screen: what the build lays out (src/channelf/src/look.inc) ------------------
AX0, AY0 = 8, 5                 # the arena's top-left pixel
PANX = 88                       # the panel's first text column
SEATINK = [1, 2, 3, None]       # blue, red, green, red/blue stripes
STRIPE = (2, 1)                 # seat 3: red on even pixel rows, blue on odd

# MAME's colours (channelf_v.cpp), and the palettes' four
BLACK, WHITE, RED, GREEN, BLUE = (16, 16, 16), (253, 253, 253), (255, 49, 83), (2, 204, 93), (75, 63, 243)
LTGREY, LTGREEN, LTBLUE = (224, 224, 224), (145, 255, 166), (206, 208, 255)
PAL = [[BLACK, WHITE, WHITE, WHITE], [LTBLUE, BLUE, RED, GREEN],
       [LTGREY, BLUE, RED, GREEN], [LTGREEN, BLUE, RED, GREEN]]

# looks: (floor palette, seam palette) for the arena's rows
LOOKS = {"A": (2, 3), "B": (3, 2), "C": (1, 2)}


class Screen:
    def __init__(self):
        self.v = [[0] * 128 for _ in range(64)]
        self.pal = [0] * 64

    def put(self, x, y, val):
        if 0 <= x < 128 and 0 <= y < 64:
            self.v[y][x] = val

    def text(self, x, y, s, ink):
        sys.path.insert(0, HERE)
        from mkfont import GLYPHS
        for i, ch in enumerate(s):
            for r, line in enumerate(GLYPHS[ch].split(";")):
                for c, p in enumerate(line):
                    if p == "#":
                        val = ink if ink is not None else STRIPE[(y + r) & 1]
                        self.put(x + 4 * i + c, y + r, val)

    def png(self, path, scale=2):
        """The visible 102x58 at TV aspect: a pixel 3 wide, 4 tall (0.76)."""
        from PIL import Image
        img = Image.new("RGB", (102 * 3, 58 * 4))
        for y in range(58):
            pal = PAL[self.pal[y + 4]]
            for x in range(102):
                rgb = pal[self.v[y + 4][x + 4]]
                for dy in range(4):
                    for dx in range(3):
                        img.putpixel((x * 3 + dx, y * 4 + dy), rgb)
        if scale != 1:
            img = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
        img.save(path)


def arena_screen(look, flash_row=None, freeze=False):
    """The canned state (tools/o2/mkcanned.py) drawn as the build will."""
    sys.path.insert(0, os.path.join(HERE, "..", "o2"))
    from mkcanned import canned, proto
    p = proto(os.path.join(ROOT, "src", "proto.h"))
    st = canned(p)
    tiles = tile_frames()
    spr = sprite_frames()
    sc = Screen()
    floor, seam = LOOKS[look]
    for y in range(AY0, AY0 + 11 * TH):
        sc.pal[y] = seam if (y - AY0) % TH == TH - 1 else floor
        if flash_row is not None and (y - AY0) // TH == flash_row:
            sc.pal[y] = 0
        if freeze:
            sc.pal[y] = 0
    for y in range(AY0, AY0 + 11 * TH):     # the side walls
        sc.put(AX0 - 1, y, 1)
        sc.put(AX0 + 13 * TW, y, 1)
    for t in range(p["TILES"]):
        k = st[p["OFF_MAP"] + t]
        art = tiles[k][0]
        tx, ty = AX0 + (t % 13) * TW, AY0 + (t // 13) * TH
        for y in range(TH):
            for x in range(TW):
                sc.put(tx + x, ty + y, ROLE_VAL[art[y][x]])
    for s in range(4):
        o = p["OFF_PLAYERS"] + s * p["PLAYER_SIZE"]
        x, y = st[o + p["P_X"]], st[o + p["P_Y"]]
        alive = st[p["OFF_ALIVE"]] >> s & 1
        fr = spr[SPRITES.index("down0" if alive else "die3")]
        px, py = AX0 + x * TW // 16, AY0 + y * TH // 16
        for r in range(TH):
            for c in range(TW):
                ch = fr[r][c]
                if ch == "#":
                    ink = SEATINK[s] if SEATINK[s] else STRIPE[(py + r) & 1]
                    sc.put(px + c, py + r, ink)
                elif ch == "v":
                    sc.put(px + c, py + r, 0)
    # the panel: 4 seats, clock, round, our stats
    for s in range(4):
        n = bytes(st[p["OFF_NAMES"] + s * 8:p["OFF_NAMES"] + s * 8 + 3]).decode()
        if not st[p["OFF_ALIVE"]] >> s & 1:
            n = "-  "
        sc.text(PANX, AY0 + 6 * s, n + str(st[p["OFF_PLAYERS"] + s * p["PLAYER_SIZE"] + p["P_WINS"]]),
                SEATINK[s])
    t = st[p["OFF_TIMER"]]
    sc.text(PANX, AY0 + 24, "%d:%02d" % (t // 60, t % 60), 1)
    sc.text(PANX, AY0 + 30, "RD %d" % st[p["OFF_ROUND"]], 2)
    sc.text(PANX, AY0 + 42, "F2B1", 3)
    sc.text(PANX, AY0 + 48, "S1KG", 3)
    return sc


def frames_sheet(path):
    """The bomber frames and tiles, big, on each arena palette."""
    from PIL import Image
    spr = sprite_frames()
    tiles = tile_frames()
    cells = [(f, None) for f in spr] + [(f, "t") for frames in tiles for f in frames]
    cols = 18
    rows = (len(cells) + cols - 1) // cols
    sc_w, sc_h = 8 * 2 * 4, 7 * 4 * 4
    img = Image.new("RGB", (cols * sc_w, rows * sc_h * 3))
    for band, pal in enumerate((2, 3, 1)):
        for i, (f, kind) in enumerate(cells):
            cx, cy = (i % cols) * sc_w, (band * rows + i // cols) * sc_h
            for y in range(7):
                for x in range(8):
                    val = 0
                    if 1 <= y <= TH and 1 <= x <= TW:
                        ch = f[y - 1][x - 1]
                        val = (1 if ch == "#" else 0) if kind is None else ROLE_VAL[ch]
                    rgb = PAL[pal][val]
                    for dy in range(16):
                        for dx in range(8):
                            img.putpixel((cx + x * 8 + dx, cy + y * 16 + dy), rgb)
    img.save(path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--preview", metavar="DIR")
    a = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    tiles = tile_frames()
    rows = []
    for frames in tiles:
        for y in range(TH):
            rows.append("".join(f[y] for f in frames).ljust(4 * TW, "."))
    save(os.path.join(OUT, "tiles.png"), rows, ROLE_RGB)
    spr = sprite_frames()
    save(os.path.join(OUT, "sprites.png"), ["".join(f[y] for f in spr) for y in range(TH)],
         SPR_RGB, "RGBA")
    print("drawart_chf: gfx/channelf/tiles.png, sprites.png")
    if a.preview:
        os.makedirs(a.preview, exist_ok=True)
        for look in LOOKS:
            arena_screen(look).png(os.path.join(a.preview, "look%s.png" % look))
            arena_screen(look, flash_row=5).png(os.path.join(a.preview, "look%s-flash.png" % look))
            arena_screen(look, freeze=True).png(os.path.join(a.preview, "look%s-freeze.png" % look))
        frames_sheet(os.path.join(a.preview, "frames.png"))
        print("drawart_chf: previews in", a.preview)


if __name__ == "__main__":
    main()

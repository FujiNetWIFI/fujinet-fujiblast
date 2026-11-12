#!/usr/bin/env python3
"""drawart_a78.py -- FujiBlast's art for the Atari 7800: MARIA 160B.

160B objects pick each pixel's colour from 4 palettes of 3 colours (12 + the
background), so a tile may use 12 colours at once, a bomber another 12. This
takes the Apple II double hi-res designs (tools/drawart_dhgr.py, 7x16, 16
colours), widens them to 8x16 (the Atari's tile: 160 pixels a line) and maps
them onto the NTSC 7800 palette. All of it is original art.

    python3 tools/drawart_a78.py --preview DIR     # mock screens into DIR
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "a78"))
import drawart_dhgr as D      # noqa: E402
from palette import NTSC       # noqa: E402

FLOOR = 0xC5
# DHGR letter -> 7800 colour (hue << 4 | luminance)
TILE_COL = {
    "k": 0x00, "w": 0x0F, "e": 0x0A, "d": 0x05,
    "y": 0x1E, "o": 0x2A, "n": 0x26, "m": 0x36, "i": 0x4C,
    "p": 0x5A, "b": 0x75, "l": 0x9C, "a": 0x9C, ".": FLOOR,
}
# the seats' light and dark shade, the visor, black
SEAT = [(0x0F, 0x07), (0x4C, 0x45), (0x9C, 0x85), (0xCC, 0x26)]
VISOR, BLACK = 0x1E, 0x00


def rgb(i):
    return NTSC[i]


def widen(rows):
    """7 -> 8 wide: the middle column twice."""
    return [r[:4] + r[3:] for r in rows]


def tile_sets():
    return {k: [widen(f) for f in v] for k, v in D.tile_frames().items()}


def sprite_sets():
    return {k: widen(v) for k, v in D.sprite_frames().items()}


def used_tile_colours():
    s = set()
    for fs in tile_sets().values():
        for f in fs:
            for r in f:
                s.update(TILE_COL[c] for c in r)
    s.discard(FLOOR)
    return sorted(s)


# ---- mock screens: 160 x 192 palette indices --------------------------------

W, H = 160, 192


def blank(c):
    return [[c] * W for _ in range(H)]


def put(scr, x0, y0, rows, colour):
    for y, row in enumerate(rows):
        for x, p in enumerate(row):
            c = colour(p)
            if c is not None and 0 <= x0 + x < W and 0 <= y0 + y < H:
                scr[y0 + y][x0 + x] = c


def text5(scr, x, y, s, col):
    f5 = D.font5()
    for ch in s:
        g = f5[ord(ch.upper()) - 32]
        for r, row in enumerate(g):
            for c, p in enumerate(row):
                if p == "#" and 0 <= x + c < W:
                    scr[y + r][x + c] = col
        x += 8


HUD320 = None


def textn(scr, x, y, s, col):
    """The Namco arcade font (gfx/namco.bin, 8x8 a character)."""
    import drawart_cga as C
    f = C.namco()
    for ch in s:
        for r, row in enumerate(C.glyph(f, ch)):
            for c, p in enumerate(row):
                if p == "#" and 0 <= x + c < len(scr[0]):
                    scr[y + r][x + c] = col
        x += 8


def text3(scr, x, y, s, col):
    f3 = D.font3()
    for ch in s:
        g = f3[ord(ch.upper()) - 32]
        for r, row in enumerate(g):
            for c, p in enumerate(row):
                if p == "#" and 0 <= x + c < W:
                    scr[y + r][x + c] = col
        x += 4


def bomber(scr, x, y, rows, seat):
    body, shade = SEAT[seat]
    m = {"#": body, "+": shade, "v": VISOR, "k": BLACK}
    put(scr, x, y, rows, lambda p: m.get(p))


def mock_arena():
    scr = blank(FLOOR)
    tiles = tile_sets()
    # HUD strip
    for y in range(8):
        scr[y] = [0x06] * W
    global HUD320
    HUD320 = [[0x06] * 320 for _ in range(8)]
    x = 0
    for s, c, gap in [("ANN 1", 0, 6), ("BOB 0", 1, 6), ("2:41", None, 6), ("CAT 2", 2, 6), ("DAN 1", 3, 0)]:
        textn(HUD320, x, 0, s, SEAT[c][0] if c is not None else 0x0F)
        x += len(s) * 8 + gap * 4 // 1

    # frame
    for y in range(8, H):
        for x in range(24, 136):
            inside = 28 <= x < 132 and 12 <= y < 188
            if not inside:
                scr[y][x] = TILE_COL["e" if (x // 4 + y // 4) % 2 == 0 else "d"]
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
    grid[4][6] = "flameC"
    for c in (4, 5, 7, 8):
        grid[4][c] = "flameH"
    grid[4][3], grid[4][9] = "flameL", "flameR"
    grid[2][6], grid[6][6] = "flameU", "flameD"
    grid[3][6] = grid[5][6] = "flameV"
    grid[4][2] = "burn"
    grid[8][2], grid[8][4] = "bomb", "dud"
    pw = ("fire", "bombup", "speed", "kick", "glove")
    for i, p in enumerate(pw):
        grid[0][4 + i] = p
    grid[10][6] = "bomb"
    for r in range(11):
        for c in range(13):
            n = grid[r][c]
            fr = tiles[n][1 if (n in pw and c % 2) else 0]
            put(scr, 28 + c * 8, 12 + r * 16, fr, lambda p: TILE_COL[p])
    for i, n in enumerate(pw):
        y0 = 16 + i * 32
        put(scr, 2, y0, tiles[n][1], lambda p: TILE_COL[p])
        if i < 3:
            text3(scr, 12, y0 + 8, "X" + str([3, 2, 1][i]), 0x0F)
    text3(scr, 138, 20, "RND", 0x0F)
    text3(scr, 142, 30, "3", 0x1E)
    fr = sprite_sets()
    for seat, (n, px, py) in enumerate([("down0", 28, 12), ("right1", 28 + 12 * 8 - 3, 12 + 160),
                                        ("left0", 28 + 6 * 8 + 4, 12 + 128), ("up2", 28 + 12 * 8, 12 + 6)]):
        bomber(scr, px, py, fr[n], seat)
    return scr


def mock_title():
    bg = 0x75
    scr = blank(bg)
    lg = D.logo()
    put(scr, 10, 16, [r[:140] for r in lg], lambda p: TILE_COL[p] if p != "b" else bg)
    text5(scr, (W - 11 * 8) // 2, 56, "FOR FUJINET", 0x1E)
    fr = sprite_sets()
    for seat in range(4):
        bomber(scr, 24 + seat * 32, 88, fr[["right0", "right1", "right0", "right2"][seat]], seat)
    text3(scr, (W - 10 * 4) // 2, 136, "PRESS FIRE", 0x0F)
    text3(scr, (W - 31 * 4) // 2, 160, "BOMBS, BLOCKS AND BLASTS FOR 2-4", 0x0F)
    return scr


def mock_sprites():
    scr = [[0x95] * (18 * 9 + 2) for _ in range(4 * 20 + 4)]
    fr = sprite_sets()
    for seat in range(4):
        for i, n in enumerate(D.SPRITES):
            body, shade = SEAT[seat]
            m = {"#": body, "+": shade, "v": VISOR, "k": BLACK}
            for y, row in enumerate(fr[n]):
                for x, p in enumerate(row):
                    if p in m:
                        scr[2 + seat * 20 + y][1 + i * 9 + x] = m[p]
    return scr


def save(scr, path, sx=8, sy=5, hud=None):
    h, w = len(scr), len(scr[0])
    img = Image.new("RGB", (w * sx, h * sy))
    px = img.load()
    if hud:
        for y in range(8):
            for x in range(320):
                c = rgb(hud[y][x])
                for dy in range(sy):
                    for dx in range(sx // 2):
                        px[x * (sx // 2) + dx, y * sy + dy] = c
    for y in range(8 if hud else 0, h):
        for x in range(w):
            c = rgb(scr[y][x])
            for dy in range(sy):
                for dx in range(sx):
                    px[x * sx + dx, y * sy + dy] = c
    img.save(path)


def main():
    used = used_tile_colours()
    print("tile colours (%d, bg floor):" % len(used), " ".join("%02X" % c for c in used))
    assert len(used) <= 12, "tiles need more than 12 colours"
    if len(sys.argv) > 2 and sys.argv[1] == "--preview":
        d = sys.argv[2]
        os.makedirs(d, exist_ok=True)
        a = mock_arena()
        save(a, os.path.join(d, "arena.png"), hud=HUD320)
        save(mock_title(), os.path.join(d, "title.png"))
        save(mock_sprites(), os.path.join(d, "sprites.png"), 12, 8)
        print("previews in", d)


if __name__ == "__main__":
    main()

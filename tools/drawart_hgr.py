#!/usr/bin/env python3
"""drawart_hgr.py -- the Apple II's art in high-res colour (gfx/hgr/), from
the double hi-res art (gfx/dhgr/, tools/drawart_dhgr.py): the same shapes at
the same 140-pixel width, each of the 16 colours mapped to one of HGR's 6,
tile by tile (tools/hgr.py says why a byte can't mix violet/green with
blue/orange).

    python3 tools/drawart_hgr.py [--preview DIR]

writes gfx/hgr/{tiles,sprites,logo,wall,font3,font5}.png, and with
--preview pictures of the sheets and mock screens as the game would draw
them (tools/hgr.py's rough NTSC rendering, fringes and all).

The look (user, 2026-11-12): a green floor like the DHGR one; bombers white,
orange, blue and violet (green would vanish on the floor); menus on black.
"""
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from png import Image as PngImage  # noqa: E402
from artlayout import TILES, SPRITES  # noqa: E402
from hgr import BLACK, WHITE, VIOLET, GREEN, BLUE, ORANGE, RGB, Screen  # noqa: E402

ROOT = os.path.join(HERE, "..")
DH = os.path.join(ROOT, "gfx", "dhgr")
OUT = os.path.join(ROOT, "gfx", "hgr")

PAL16 = [
    (0x00, 0x00, 0x00), (0xDD, 0x00, 0x33), (0x00, 0x00, 0x99), (0xAA, 0x00, 0xFF),
    (0x00, 0x77, 0x22), (0x55, 0x55, 0x55), (0x22, 0x22, 0xFF), (0x66, 0xAA, 0xFF),
    (0x88, 0x55, 0x00), (0xFF, 0x66, 0x00), (0xAA, 0xAA, 0xAA), (0xFF, 0x99, 0x88),
    (0x00, 0xFF, 0x44), (0xFF, 0xFF, 0x00), (0x44, 0xFF, 0xCC), (0xFF, 0xFF, 0xFF),
]
DHGR = {c: i for i, c in enumerate(PAL16)}

# DHGR colour -> HGR, by tile (default first)
BASE = {0: BLACK, 4: GREEN, 15: WHITE}
POWER = {2: BLUE, 3: WHITE, 13: WHITE, 9: ORANGE, 1: ORANGE, 8: ORANGE, 11: ORANGE,
         14: WHITE, 7: WHITE, 5: BLACK, 10: WHITE}
FLAME = {9: ORANGE, 1: ORANGE, 13: WHITE}
TILE_MAP = {
    "floor": {},
    "pillar": {10: WHITE, 5: VIOLET},
    "block": {9: ORANGE, 8: BLACK, 13: WHITE},
    "burn": {9: ORANGE, 8: BLACK, 13: WHITE, 1: ORANGE},
    "bomb": {5: VIOLET, 13: WHITE, 8: BLACK, 9: WHITE},
    "dud": {5: VIOLET, 10: WHITE, 8: BLACK},
    "trophy": {4: BLACK, 13: ORANGE, 9: ORANGE, 8: WHITE, 15: BLACK},   # (menus only: on black)
}
for n in TILES:
    if n.startswith("flame"):
        TILE_MAP[n] = FLAME
    elif n in ("fire", "bombup", "speed", "kick", "glove"):
        TILE_MAP[n] = POWER
LOGO_MAP = {2: BLACK, 13: WHITE, 9: ORANGE, 1: ORANGE}
WALL_MAP = {10: WHITE, 5: VIOLET}

# Bombers: the sheet's roles (# body, + shade, v visor, k black) by seat; one
# palette a seat, so a bomber never clashes with itself.
ROLES = {(255, 255, 255): "#", (128, 128, 128): "+", (255, 255, 0): "v", (0, 0, 0): "k"}
SEAT_COLOURS = [  # body, shade, visor
    (WHITE, VIOLET, GREEN),
    (ORANGE, BLACK, WHITE),
    (BLUE, BLACK, ORANGE),
    (VIOLET, BLACK, WHITE),
]
# text colours, as gfx.c's palettes will use them
TEXT, HILITE = WHITE, ORANGE
SEAT_TEXT = [WHITE, ORANGE, BLUE, VIOLET]


def load(name):
    return PngImage(os.path.join(DH, name))


def remap(img, x0, y0, w, h, table):
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            i = DHGR[img.getpixel((x0 + x, y0 + y))[:3]]
            c = table.get(i, BASE.get(i))
            if c is None:
                sys.exit(f"no HGR colour for DHGR {i} at {x0 + x},{y0 + y}")
            row.append(c)
        rows.append(row)
    return rows


def tile_art():
    img = load("tiles.png")
    return {n: [remap(img, f * 7, i * 16, 7, 16, TILE_MAP[n]) for f in range(4)] for i, n in enumerate(TILES)}


def used(frames):
    """The frames a tile has: trailing all-floor ones are unused (as in mkgfx)."""
    n = 4
    while n > 1 and all(c == GREEN for r in frames[n - 1] for c in r):
        n -= 1
    return frames[:n]


def sprite_roles():
    img = load("sprites.png")
    out = {}
    for i, n in enumerate(SPRITES):
        rows = []
        for y in range(16):
            row = []
            for x in range(7):
                p = img.getpixel((i * 7 + x, y))
                row.append(ROLES[p[:3]] if p[3] else ".")
            rows.append("".join(row))
        out[n] = rows
    return out


def bomber(roles, seat):
    body, shade, visor = SEAT_COLOURS[seat]
    m = {"#": body, "+": shade, "v": visor, "k": BLACK, ".": None}
    return [[m[p] for p in row] for row in roles]


def font(name, gw):
    img = load(name)
    return [[[img.getpixel((g * gw + x, y))[:3] == (255, 255, 255) for x in range(gw)] for y in range(8)]
            for g in range(64)]


def write_png(path, rows):
    from PIL import Image
    h, w = len(rows), len(rows[0])
    im = Image.new("RGBA", (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            c = rows[y][x]
            px[x, y] = (0, 0, 0, 0) if c is None else RGB[c] + (255,)
    im.save(path)


def write_sheets():
    os.makedirs(OUT, exist_ok=True)
    tiles = tile_art()
    sheet = []
    for n in TILES:
        for y in range(16):
            sheet.append([c for f in range(4) for c in tiles[n][f][y]])
    write_png(os.path.join(OUT, "tiles.png"), sheet)
    # bombers keep their roles (gfx/dhgr/sprites.png): mkgfx_hgr.py colours them by seat
    from shutil import copyfile
    for f in ("sprites.png", "font3.png", "font5.png"):
        copyfile(os.path.join(DH, f), os.path.join(OUT, f))
    lg = load("logo.png")
    write_png(os.path.join(OUT, "logo.png"), remap(lg, 0, 0, lg.width, lg.height, LOGO_MAP))
    write_png(os.path.join(OUT, "wall.png"), remap(load("wall.png"), 0, 0, 4, 4, WALL_MAP))
    return tiles


# ---- previews: screens as the game draws them --------------------------------

UNDER = None            # the bombers' palette vote: None (theirs alone) or GREEN (floor-aware)


def put_rows(scr, x, y, rows, under=None):
    for dy, row in enumerate(rows):
        scr.put(y + dy, x, row, under)


def text(scr, x, y, s, colour, glyphs, gw, bg=None):
    for ch in s:
        g = glyphs[(ord(ch.upper()) - 32) & 63]
        for r in range(8):
            scr.put(y + r, x, [colour if g[r][c] else bg for c in range(gw)])
        x += gw


def mock_arena(tiles, roles, f3, f5):
    scr = Screen()
    hud = [("ANN1", 0), ("BOB0", 1), ("2:41", -1), ("CAT2", 2), ("DOT1", 3)]
    x = 0
    for s, c in hud:
        text(scr, x + (2 if len(s) < 4 else 0), 0, s, SEAT_TEXT[c] if c >= 0 else WHITE, f5, 7, BLACK)
        x += 28
    wall = remap(load("wall.png"), 0, 0, 4, 4, WALL_MAP)
    for y in range(8, 192):
        scr.put(y, 24, [wall[y % 4][x % 4] for x in range(24, 123)])
    rnd = random.Random(7)
    grid = [["floor"] * 13 for _ in range(11)]
    for r in range(11):
        for c in range(13):
            if r % 2 and c % 2:
                grid[r][c] = "pillar"
            elif rnd.random() < 0.55 and not (r < 2 and c < 2) and not (r > 8 and c > 10):
                grid[r][c] = "block"
    for (c, r) in [(0, 0), (1, 0), (0, 1), (12, 10), (11, 10), (12, 9), (12, 0), (0, 10), (6, 8), (7, 8), (5, 8)]:
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
            n = grid[r][c]
            fr = used(tiles[n])[-1] if n in ("fire", "bombup", "speed", "kick", "glove") else tiles[n][0]
            put_rows(scr, 28 + c * 7, 12 + r * 16, fr)
    for i, n in enumerate(("fire", "bombup", "speed", "kick", "glove")):
        y0 = 14 + i * 36
        put_rows(scr, 7, y0, used(tiles[n])[-1])
        if i < 3:
            text(scr, 14, y0 + 8, "X" + str([3, 2, 1][i]), WHITE, f3, 4, BLACK)
    text(scr, 126, 20, "RND", WHITE, f3, 4, BLACK)
    text(scr, 130, 30, "3", HILITE, f3, 4, BLACK)
    # four bombers at odd shifts (the fringes show), one bomb sliding
    spots = [("down0", 28 + 0 * 7 + 1, 12), ("right1", 28 + 12 * 7 - 3, 12 + 10 * 16),
             ("left0", 28 + 6 * 7 + 4, 12 + 8 * 16), ("up2", 28 + 12 * 7, 12 + 6)]
    for seat, (n, px, py) in enumerate(spots):
        put_rows(scr, px, py, bomber(roles[n], seat), UNDER)
    return scr


def mock_waiting(roles, f5):
    scr = Screen()
    text(scr, 14, 0, "ROOM BLAST1", WHITE, f5, 7)
    for s, name in enumerate(("ANN", "BOB", "CAT", "DOT")):
        y = 16 + s * 32
        put_rows(scr, 7, y, bomber(roles["down0"], s))
        text(scr, 28, y + 4, name, SEAT_TEXT[s], f5, 7)
        if s == 0:
            text(scr, 98, y + 4, "HOST", WHITE, f5, 7)
    text(scr, 14, 152, "ROUNDS: 5", HILITE, f5, 7)
    text(scr, 7, 168, "PRESS FIRE", WHITE, f5, 7)
    return scr


def mock_title(roles, f3, f5):
    scr = Screen()
    lg = remap(load("logo.png"), 0, 0, 140, 24, LOGO_MAP)
    put_rows(scr, 0, 16, lg)
    s = "FOR FUJINET"
    text(scr, (140 - len(s) * 7) // 2, 56, s, HILITE, f5, 7)
    for seat in range(4):
        put_rows(scr, 20 + seat * 28 + seat, 88, bomber(roles[["right0", "right1", "right0", "right2"][seat]], seat))
    s = "PRESS FIRE"
    text(scr, (35 - len(s)) // 2 * 4, 136, s, WHITE, f3, 4)
    s = "BOMBS, BLOCKS AND BLASTS FOR 2-4"
    text(scr, (35 - len(s)) // 2 * 4, 160, s, WHITE, f3, 4)
    return scr


def mock_bombers(roles):
    """Every frame of every seat on the floor, walking frames at all 7 shifts."""
    scr = Screen()
    floor = [[GREEN] * 140 for _ in range(16)]
    walk = SPRITES[:12]
    for seat in range(4):
        y = 4 + seat * 46
        put_rows(scr, 0, y, floor)
        put_rows(scr, 0, y + 20, floor)
        for i, n in enumerate(walk):
            put_rows(scr, i * 11 + i % 7, y, bomber(roles[n], seat), UNDER)
        for i, n in enumerate(SPRITES[12:]):
            put_rows(scr, 4 + i * 22 + i % 7, y + 20, bomber(roles[n], seat), UNDER)
    return scr


def mock_tiles(tiles):
    scr = Screen()
    for i, n in enumerate(TILES):
        for f in range(4):
            put_rows(scr, (i % 5) * 28 + f * 7, (i // 5) * 20 + 4, tiles[n][f])
    return scr


def main():
    tiles = write_sheets()
    print("wrote gfx/hgr/tiles.png sprites.png logo.png wall.png font3.png font5.png")
    if len(sys.argv) > 2 and sys.argv[1] == "--preview":
        d = sys.argv[2]
        os.makedirs(d, exist_ok=True)
        roles = sprite_roles()
        f3, f5 = font("font3.png", 4), font("font5.png", 7)
        mock_arena(tiles, roles, f3, f5).render(os.path.join(d, "arena.png"))
        mock_title(roles, f3, f5).render(os.path.join(d, "title.png"))
        mock_waiting(roles, f5).render(os.path.join(d, "waiting.png"))
        mock_bombers(roles).render(os.path.join(d, "bombers.png"))
        global UNDER
        UNDER = GREEN
        mock_bombers(roles).render(os.path.join(d, "bombers-floor.png"))
        mock_arena(tiles, roles, f3, f5).render(os.path.join(d, "arena-floor.png"))
        UNDER = None
        mock_tiles(tiles).render(os.path.join(d, "tiles.png"))
        print("previews in", d)


if __name__ == "__main__":
    main()

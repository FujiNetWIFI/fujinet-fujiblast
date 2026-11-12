"""preview_intv.py -- mock Intellivision screens from the art (for
tools/drawart_intv.py --preview): the arena on test.c's canned state, the
title, the waiting room, a game's end, and the sprite sheet. Each is the
screen as the port lays it out (20x12 cards, foreground/background mode,
MOBs), drawn 4 pixels a pixel with the border round it."""
import os

from PIL import Image

from artlayout_intv import (PAL, TILES, SEAT_BODY, VISOR, FLOOR, PANEL, DUD_FG, LOGO_TEXT, LOGO_TOP,
                            LOGO_BOTTOM, BLACK, WHITE, YELLOW, GREY, TAN)

JZ = os.environ.get("JZINTV_DIR", os.path.expanduser("~/Workspace/jzintv-20200712-src"))
SCALE = 4
NAMES = ["ANN", "BOB", "CAT", "DOT"]
COLOURS = {name: (fg, bg) for name, fg, bg in TILES}


class Screen:
    def __init__(self, border=BLACK):
        self.grom = open(os.path.join(JZ, "rom", "grom.bin"), "rb").read()
        self.cards = [[(["........"] * 8, WHITE, PANEL) for _ in range(20)] for _ in range(12)]
        self.mobs = []
        self.border = border

    def card(self, col, row, rows, fg, bg):
        self.cards[row][col] = (rows, fg, bg)

    def text(self, col, row, s, fg=WHITE, bg=PANEL):
        for i, ch in enumerate(s):
            n = ord(ch) - 32
            bits = self.grom[n * 8:n * 8 + 8]
            self.card(col + i, row, ["".join("#" if b >> (7 - x) & 1 else "." for x in range(8)) for b in bits],
                      fg, bg)

    def centre(self, row, s, fg=WHITE, bg=PANEL):
        self.text((20 - len(s)) // 2, row, s, fg, bg)

    def mob(self, x, y, rows, colour, flip=False, zoom=1):
        """x, y in screen pixels (0-159, 0-95); rows of 8, each half a pixel tall (zoom 1)."""
        self.mobs.append((x, y, rows, colour, flip, zoom))

    def save(self, path):
        w, h = 176, 112
        img = Image.new("RGB", (w * SCALE, h * SCALE), PAL[self.border])
        px = img.load()

        def dot(x, y, col, hw=SCALE, hh=SCALE):
            for dy in range(hh):
                for dx in range(hw):
                    px[x + dx, y + dy] = col
        for r in range(12):
            for c in range(20):
                rows, fg, bg = self.cards[r][c]
                for y in range(8):
                    for x in range(8):
                        dot((8 + c * 8 + x) * SCALE, (8 + r * 8 + y) * SCALE,
                            PAL[fg] if rows[y][x] == "#" else PAL[bg])
        for x0, y0, rows, colour, flip, zoom in reversed(self.mobs):
            for ry, row in enumerate(rows):
                for rx, p in enumerate(row):
                    if p != "#":
                        continue
                    xx = 7 - rx if flip else rx
                    dot((8 + x0 + xx * zoom) * SCALE, (8 + y0) * SCALE + ry * zoom * SCALE // 2,
                        PAL[colour], SCALE * zoom, SCALE * zoom // 2)
        img.save(path)


def bomber(scr, seat, x, y, body, visor, frame, flip=False, zoom=1, vdy=2):
    """A bomber: the visor MOB in front of the body's (2 rows down: vdy pixels)."""
    if visor:
        scr.mob(x, y + vdy * zoom, visor, VISOR, flip, zoom)
    scr.mob(x, y, body[frame], SEAT_BODY[seat], flip, zoom)


def hud(scr, timer, wins, alive):
    for s in range(4):
        col = [0, 4, 12, 16][s]
        name = NAMES[s] if alive[s] else "---"
        scr.text(col, 0, name + str(wins[s]), BLACK, SEAT_BODY[s])
    scr.text(8, 0, "%d:%02d" % (timer // 60, timer % 60), WHITE, PANEL)


def arena(d, tiles, body, visor):
    T = ["floor", "pillar", "block", "burn", "bomb", "dud", "flameC", "flameH", "flameV", "flameL", "flameR",
         "flameU", "flameD", "fire", "bombup", "speed", "kick", "glove"]
    m = [[0] * 13 for _ in range(11)]
    for r in range(11):
        for c in range(13):
            if c & 1 and r & 1:
                m[r][c] = 1
            elif 2 < c < 10 and (r == 0 or r == 10 or c == 4 or c == 8):
                m[r][c] = 2
    for (r, c), t in {(2, 0): 4, (4, 0): 5, (2, 12): 13, (4, 12): 14, (6, 12): 15, (8, 12): 16, (6, 0): 17,
                      (8, 0): 3, (5, 6): 6, (5, 5): 7, (5, 4): 2, (5, 7): 7, (5, 8): 10, (4, 6): 8,
                      (3, 6): 11, (6, 6): 12}.items():
        m[r][c] = t
    for page, border in ((0, BLACK), (2, BLACK), (0, GREY)):
        scr = Screen(border)
        hud(scr, 154, [0, 1, 2, 3], [1, 1, 1, 0])
        for r in range(11):
            scr.card(3, 1 + r, tiles["wall"][0], *COLOURS["wall"])
            scr.card(17, 1 + r, tiles["wall"][0], *COLOURS["wall"])
            for c in range(13):
                t = T[m[r][c]]
                if t == "floor":
                    scr.card(4 + c, 1 + r, ["........"] * 8, BLACK, FLOOR)
                else:
                    fr = tiles[t]
                    scr.card(4 + c, 1 + r, fr[page % len(fr)], *COLOURS[t])
        # the mover: a bomb half a tile right of (2, 2): halves over the tiles' backgrounds
        for c, h in ((2, "halfL"), (3, "halfR")):
            under = T[m[2][c]]
            bg = FLOOR if under == "floor" else COLOURS[under][1]
            scr.card(4 + c, 3, tiles[h][0], BLACK, bg)
        # the stats: fire 2, bombs 1, speed 1, kick and glove
        for i, (t, n) in enumerate((("fire", "2"), ("bombup", "1"), ("speed", "1"), ("kick", ""), ("glove", ""))):
            scr.card(0, 2 + i * 2, tiles[t][page], *COLOURS[t])
            scr.text(1, 2 + i * 2, n.rjust(2) if n else "  ", WHITE)
        scr.text(18, 2, "RD", WHITE)
        scr.text(18, 3, " 2", YELLOW)
        # the bombers at their corners; seat 3 dying (die2)
        pos = [(0, 0), (192, 160), (192, 0), (0, 160)]
        for s, (x, y) in enumerate(pos):
            sx, sy = 32 + x // 2, 8 + y // 2
            if s == 0:
                bomber(scr, s, sx + 6, sy, body, visor["side"], "right1")
            elif s == 1:
                bomber(scr, s, sx, sy - 4, body, None, "down1")      # walking up: no visor
            elif s == 2:
                bomber(scr, s, sx, sy, body, visor["front"], "down0")
            else:
                bomber(scr, s, sx, sy, body, visor["front"], "die2", vdy=3)
        scr.save(os.path.join(d, "arena%d%s.png" % (page // 2, "_border" if border == GREY else "")))


def title(d, body, visor, logo):
    scr = Screen()
    for i, rows in enumerate(logo):
        for cy in range(2):
            for cx in range(2):
                part = [r[cx * 8:cx * 8 + 8] for r in rows[cy * 8:cy * 8 + 8]]
                # the letter is the card's background; the rest its (black) foreground
                inv = ["".join("." if p == "#" else "#" for p in r) for r in part]
                scr.card(1 + i * 2 + cx, 1 + cy, inv, BLACK, LOGO_TOP if cy == 0 else LOGO_BOTTOM)
    scr.centre(4, "FOR FUJINET", YELLOW)
    for s in range(4):
        bomber(scr, s, 20 + s * 34, 48, body, visor["side"], ["right0", "right1", "right0", "right2"][s])
    scr.centre(8, "PRESS FIRE", WHITE)
    scr.centre(10, "BOMBS, BLOCKS AND", GREY if False else TAN)
    scr.centre(11, "BLASTS FOR 2-4", TAN)
    scr.save(os.path.join(d, "title.png"))


def waiting(d, body, visor):
    scr = Screen()
    scr.centre(0, "ROOM BLAST1", YELLOW)
    for s in range(4):
        r = 2 + s * 2
        bomber(scr, s, 8, r * 8, body, visor["front"], "down0")
        scr.text(3, r, NAMES[s] if s < 3 else "--------", SEAT_BODY[s] if s < 3 else WHITE)
        if s == 0:
            scr.text(13, r, "HOST", WHITE)
    scr.text(3, 10, "ROUNDS:5", WHITE)
    scr.text(12, 10, "<DISC>", TAN)
    scr.centre(11, "FIRE: START", YELLOW)
    scr.save(os.path.join(d, "waiting.png"))


def results(d, tiles, body, visor):
    scr = Screen()
    scr.centre(0, "BOB WINS!", BLACK, SEAT_BODY[1])
    wins = [1, 3, 0, 1]
    for s in range(4):
        r = 2 + s * 2
        bomber(scr, s, 8, r * 8, body, visor["front"], "down0")
        scr.text(3, r, NAMES[s], WHITE)
        scr.text(8, r, str(wins[s]), YELLOW)
        for k in range(wins[s]):
            scr.card(3 + k, r + 1, tiles["trophy"][0], YELLOW, PANEL)
    bomber(scr, 1, 120, 24, body, visor["front"], "win0", zoom=2)
    scr.centre(11, "BACK TO THE ROOM 9", WHITE)
    scr.save(os.path.join(d, "results.png"))


def sheet(d, tiles, body, visor):
    """Every card at 8x: the tiles' frames by row, the bombers in each seat."""
    z = 8
    names = [t[0] for t in TILES]
    img = Image.new("RGB", (60 * z, (len(names) * 10 + 22) * z), (40, 40, 40))
    px = img.load()

    def blit(x0, y0, rows, fg, bg, hh=z):
        for y, row in enumerate(rows):
            for x, p in enumerate(row):
                col = PAL[fg] if p == "#" else (PAL[bg] if bg is not None else None)
                if col is None:
                    continue
                for dy in range(hh):
                    for dx in range(z):
                        px[x0 + x * z + dx, y0 + y * hh + dy] = col
    for i, n in enumerate(names):
        fg, bg = COLOURS[n]
        for f in range(4):
            fr = tiles[n][f % len(tiles[n])]
            blit(f * 10 * z, i * 10 * z, fr, fg, bg)
    y0 = len(names) * 10 * z
    for i, k in enumerate(body):
        blit(i * 10 * z // 2 * 1, y0, body[k], SEAT_BODY[i % 4], FLOOR, z // 2)
    for i, k in enumerate(visor):
        blit(i * 10 * z // 2, y0 + 10 * z, visor[k], VISOR, FLOOR, z // 2)
    img.save(os.path.join(d, "sheet.png"))


def all_previews(d, tiles, body, visor, logo):
    arena(d, tiles, body, visor)
    title(d, body, visor, logo)
    waiting(d, body, visor)
    results(d, tiles, body, visor)
    sheet(d, tiles, body, visor)

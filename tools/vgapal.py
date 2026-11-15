"""vgapal.py -- the MS-DOS VGA build's 256 colours: where each part of the
art lives in the DAC, the cycling and the backdrops. tools/drawart_vga.py
draws with these entries; tools/mkgfx_vga.py writes them out for
src/msdos/vga/gfx.c. No Pillow here (the converter runs in defoogi).

The DAC is 6 bits a channel; colours are written here in 8 bits and
rounded (to6). Entries:

    0        black (also the border)
    1-15     greys, dark to white
    16-79    the four seats, 16 each: a sprite pixel is SEAT + 16*seat + code
    80-95    fire: 4 levels (white-hot core .. red rim) x 4 groups, cycled
    96-103   grass          104-115  pillar stone     116-127  brick
    128-135  bomb steel     136-139  fuse, cap        140-143  dud stone
    144-175  powerup panels and icons                 176-183  trophy gold
    184-191  the arena's wall                         192-195  side panels
    196-199  the HUD's bar
    200-231  the backdrop: 32 bands, reloaded per screen (title, menus)
    232-238  silver text    239-245  gold text        246-255  the logo
"""

import math

BLACK, GREY, SEAT, FIRE = 0, 1, 16, 80
GRASS, PILLAR, BRICK, BOMB, FUSE, DUD = 96, 104, 116, 128, 136, 140
PWR, TROPHY, WALL, PANEL, HUDBAR, BACK = 144, 176, 184, 192, 196, 200
SILVER, GOLD, LOGO = 232, 239, 246

# a seat's codes (sprite pixels before the seat is added)
C_OUT = 1                       # the outline
C_BODY = 2                      # 2-8: the body, dark to light
C_VIS = 9                       # 9-11: the visor, dark, mid, its shine
C_EYE = 12
C_BOOT = 13                     # 13-14: boots and belt, dark, light
C_BUCKLE = 15


def to6(c):
    return tuple((v * 63 + 127) // 255 for v in c)


def to8(c6):
    return tuple((v << 2) | (v >> 4) for v in c6)


def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def ramp(stops, n):
    """n colours along a path of (position 0-1, colour) stops."""
    out = []
    for i in range(n):
        t = i / (n - 1) if n > 1 else 0
        for k in range(len(stops) - 1):
            (t0, c0), (t1, c1) = stops[k], stops[k + 1]
            if t0 <= t <= t1:
                out.append(lerp(c0, c1, (t - t0) / (t1 - t0) if t1 > t0 else 0))
                break
    return out


# ---- the seats: outline, body ramp (7), then the shared visor, eyes, boots,
# buckle --------------------------------------------------------------------

SEAT_BODY = [
    # white
    ((28, 30, 54), [(70, 78, 112), (102, 110, 144), (138, 146, 174), (174, 180, 204),
                    (208, 213, 230), (234, 238, 248), (255, 255, 255)]),
    # red
    ((54, 6, 14), [(104, 14, 24), (142, 22, 30), (184, 34, 38), (220, 54, 50),
                   (242, 92, 80), (255, 140, 124), (255, 200, 188)]),
    # blue
    ((8, 14, 60), [(16, 34, 106), (24, 58, 156), (36, 90, 206), (58, 128, 238),
                   (98, 164, 252), (148, 200, 255), (208, 234, 255)]),
    # orange
    ((64, 26, 4), [(118, 50, 6), (166, 78, 10), (212, 112, 16), (244, 148, 30),
                   (255, 184, 58), (255, 216, 112), (255, 242, 190)]),
]
SEAT_SHARED = [(14, 18, 34), (38, 54, 96), (156, 206, 255),     # visor
               (244, 252, 255),                                 # eyes
               (34, 34, 44), (96, 96, 114),                     # boots, belt
               (255, 208, 64)]                                  # buckle

# ---- fire: level L (0 core .. 3 rim), group g; entry FIRE + L*4 + g. Each
# phase p of the cycle lights the groups by turns: d = (g - p) & 3 says how
# far towards the brighter (d 0) or the darker (d 3) the level is pushed.
FIRE_BASE = [(255, 255, 226), (255, 230, 92), (255, 148, 32), (214, 54, 18)]
FIRE_UP = [(255, 255, 255), (255, 252, 190), (255, 204, 76), (252, 112, 30)]
FIRE_DOWN = [(255, 232, 150), (255, 184, 46), (226, 96, 18), (128, 22, 10)]
FIRE_PUSH = [0.75, 0.2, -0.2, -0.7]


def fire_phase(p):
    out = []
    for lv in range(4):
        for g in range(4):
            k = FIRE_PUSH[(g - p) & 3]
            out.append(lerp(FIRE_BASE[lv], FIRE_UP[lv], k) if k >= 0 else
                       lerp(FIRE_BASE[lv], FIRE_DOWN[lv], -k))
    return out


# ---- the backdrops: line y is band BAND[y] (entry BACK + band), one map for
# every screen (B's backdrop stays put; gfx_show loads the screen's 32
# colours). It breaks at line 112, the title's horizon: 18 bands above, 14
# below, each side cut where the title's colours change most (equal steps
# along its path), so the steps are as small as 32 colours allow.


def _path(stops):
    def at(y):
        for k in range(len(stops) - 1):
            (y0, c0), (y1, c1) = stops[k], stops[k + 1]
            if y0 <= y <= y1:
                return lerp(c0, c1, (y - y0) / (y1 - y0) if y1 > y0 else 0)
        return stops[-1][1]
    return at


# the title: a night sky warming to a glow where the bombers walk (lines
# 96-111), then dark ground for the text
_TITLE = _path([(0, (4, 4, 26)), (40, (14, 12, 56)), (72, (42, 24, 98)),
                (96, (104, 40, 128)), (111, (200, 84, 138)),
                (112, (46, 16, 50)), (140, (26, 10, 36)), (199, (6, 4, 14))])
# the other screens: deep blue, lightest a little above the middle
_MENU = _path([(0, (8, 10, 40)), (90, (24, 32, 88)), (150, (14, 18, 58)), (199, (4, 6, 22))])


def _split(y0, y1, n, first):
    d = [0.0]
    for y in range(y0 + 1, y1 + 1):
        a, b = _TITLE(y), _TITLE(y - 1)
        d.append(d[-1] + math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(3))) + 1.5)
    return [first + min(n - 1, int(d[y - y0] / d[-1] * n)) for y in range(y0, y1 + 1)]


BAND = _split(0, 111, 18, 0) + _split(112, 199, 14, 18)


def _bands(colour_at):
    """32 colours: each band's lines' average of colour_at(y) (a band no
    line falls in takes the one before's)."""
    out = []
    for b in range(32):
        cs = [colour_at(y) for y in range(200) if BAND[y] == b]
        out.append(tuple(round(sum(c[i] for c in cs) / len(cs)) for i in range(3)) if cs else out[-1])
    return out


TITLE_BACK = _bands(_TITLE)
MENU_BACK = _bands(_MENU)

# the arena: no backdrop of its own; text sits on the HUD's bar (row 0) and
# the side panels (8 lines a cell, a light seam on top, a dark one below)
HUD_LINES = [3, 2, 1, 1, 1, 1, 0, 0]
PANEL_LINES = [3, 1, 1, 1, 1, 2, 2, 0]
ARENA_LINE = [HUDBAR + HUD_LINES[y] if y < 8 else PANEL + PANEL_LINES[y & 7] for y in range(200)]
MENU_LINE = [BACK + BAND[y] for y in range(200)]

# ---- text: a colour a glyph line ---------------------------------------------


def seat_text(s):
    b = SEAT + 16 * s + C_BODY
    return [b + 6, b + 6, b + 5, b + 5, b + 4, b + 3, b + 2, b + 2]


TEXT_SILVER = [SILVER, SILVER, SILVER + 1, SILVER + 2, SILVER + 3, SILVER + 4, SILVER + 5, SILVER + 6]
TEXT_GOLD = [GOLD, GOLD, GOLD + 1, GOLD + 2, GOLD + 3, GOLD + 4, GOLD + 5, GOLD + 6]
TEXT_INV = [LOGO + 7] * 8       # dark red on a bright gold bar (INV_BAR)
INV_BAR = [GOLD, GOLD + 1, GOLD + 1, GOLD + 2, GOLD + 2, GOLD + 3, GOLD + 3, GOLD + 4]
# styles, as src/msdos/vga/gfx.c numbers them
STYLES = [TEXT_SILVER, TEXT_GOLD, seat_text(0), seat_text(1), seat_text(2), seat_text(3), TEXT_INV]
ST_SILVER, ST_GOLD, ST_SEAT, ST_INV = 0, 1, 2, 6


def palette():
    """The 256 colours (8 bits a channel) as loaded at start: the menus'
    backdrop, the fire's phase 0."""
    p = [(0, 0, 0)] * 256
    for i in range(1, 16):
        v = 14 + (i - 1) * (255 - 14) // 14
        p[GREY - 1 + i] = (v, v, min(255, v + 6))
    for s, (out, body) in enumerate(SEAT_BODY):
        b = SEAT + 16 * s
        p[b] = (0, 0, 0)
        p[b + C_OUT] = out
        for i, c in enumerate(body):
            p[b + C_BODY + i] = c
        for i, c in enumerate(SEAT_SHARED):
            p[b + C_VIS + i] = c
    for i, c in enumerate(fire_phase(0)):
        p[FIRE + i] = c
    for i, c in enumerate(ramp([(0, (22, 70, 24)), (0.5, (54, 132, 46)), (1, (150, 210, 104))], 8)):
        p[GRASS + i] = c
    for i, c in enumerate(ramp([(0, (18, 20, 30)), (0.45, (98, 108, 132)), (0.85, (196, 204, 220)),
                                (1, (250, 252, 255))], 12)):
        p[PILLAR + i] = c
    p[BRICK + 0], p[BRICK + 1] = (44, 30, 30), (78, 56, 52)
    for i, c in enumerate(ramp([(0, (96, 22, 20)), (0.55, (196, 64, 46)), (0.85, (240, 126, 92)),
                                (1, (255, 196, 160))], 10)):
        p[BRICK + 2 + i] = c
    for i, c in enumerate(ramp([(0, (4, 6, 14)), (0.55, (40, 48, 76)), (0.85, (96, 110, 146)),
                                (1, (236, 244, 255))], 8)):
        p[BOMB + i] = c
    p[FUSE:FUSE + 4] = [(110, 84, 54), (184, 150, 100), (90, 94, 108), (176, 182, 196)]
    p[DUD:DUD + 4] = [(58, 54, 52), (92, 88, 82), (128, 124, 116), (172, 168, 158)]
    pw = [(80, 8, 8), (142, 20, 16), (194, 42, 28), (240, 90, 64),          # fire's panel
          (36, 40, 52), (72, 78, 98), (110, 118, 140), (160, 168, 188),     # bomb up's
          (10, 20, 88), (20, 44, 150), (36, 80, 210), (92, 142, 250),       # speed's
          (58, 10, 32), (106, 20, 52), (156, 36, 74), (208, 82, 114),       # kick's
          (6, 58, 70), (10, 98, 110), (22, 138, 148), (76, 190, 196),       # glove's
          (110, 200, 255), (190, 236, 255), (250, 255, 255),                # chevrons
          (180, 54, 66), (238, 128, 140), (255, 214, 214),                  # boot
          (172, 100, 62), (226, 160, 110), (255, 216, 174),                 # hand
          (196, 136, 18), (255, 206, 60), (255, 248, 176)]                  # a lit rim
    p[PWR:PWR + 32] = pw
    p[TROPHY:TROPHY + 8] = ramp([(0, (74, 42, 8)), (0.6, (242, 182, 48)), (1, (255, 250, 214))], 7) + [(48, 28, 22)]
    p[WALL:WALL + 8] = ramp([(0, (26, 26, 34)), (0.6, (130, 132, 150)), (1, (232, 232, 240))], 8)
    p[PANEL:PANEL + 4] = [(12, 16, 32), (22, 30, 56), (28, 38, 68), (54, 68, 108)]
    p[HUDBAR:HUDBAR + 4] = [(8, 8, 14), (18, 20, 32), (32, 36, 56), (60, 68, 98)]
    for i, c in enumerate(MENU_BACK):
        p[BACK + i] = c
    p[SILVER:SILVER + 7] = ramp([(0, (255, 255, 255)), (0.5, (204, 214, 236)), (1, (120, 138, 186))], 7)
    p[GOLD:GOLD + 7] = ramp([(0, (255, 252, 210)), (0.35, (255, 220, 96)), (1, (200, 100, 16))], 7)
    p[LOGO:LOGO + 10] = [(255, 255, 240), (255, 244, 150), (255, 214, 64), (255, 160, 32),
                         (240, 104, 24), (200, 50, 20), (140, 20, 16), (44, 6, 12), (12, 4, 22),
                         (255, 255, 255)]
    assert len(p) == 256 and all(len(c) == 3 for c in p)
    return p

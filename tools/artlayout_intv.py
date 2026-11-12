"""artlayout_intv.py -- where things are in gfx/intv/*.png and the colours
each card shows in (tools/drawart_intv.py writes them, tools/mkgfx_intv.py
reads them back).

The Intellivision's screen is in foreground/background mode: each card one
of 8 foreground colours (0-7) on any of 16 backgrounds, so each tile is
two colours; a pixel of the foreground colour is a 1 in the card."""

# The STIC's 16 colours as jzIntv shows them.
PAL = [(0x00, 0x00, 0x00), (0x00, 0x2D, 0xFF), (0xFF, 0x3D, 0x10), (0xC9, 0xCF, 0xAB),
       (0x38, 0x6B, 0x3F), (0x00, 0xA7, 0x56), (0xFA, 0xEA, 0x50), (0xFF, 0xFC, 0xFF),
       (0xBD, 0xAC, 0xC8), (0x24, 0xB8, 0xFF), (0xFF, 0xB4, 0x1F), (0x54, 0x6E, 0x00),
       (0xFF, 0x4E, 0x57), (0xA4, 0x96, 0xFF), (0x75, 0xCC, 0x80), (0xB5, 0x1A, 0x58)]
BLACK, BLUE, RED, TAN, DKGREEN, GREEN, YELLOW, WHITE = range(8)
GREY, CYAN, ORANGE, BROWN, PINK, LTBLUE, YELGREEN, PURPLE = range(8, 16)

FLOOR = DKGREEN         # the arena's floor (GROM's space on it)
PANEL = BLACK           # the screen round the arena

# gfx/intv/tiles.png: one row of 8 lines per tile, frames left to right (up
# to 4; fewer repeat over the four animation pages). (fg, bg) each.
TILES = [
    ("pillar", BLACK, GREY),
    ("block", BLACK, ORANGE),
    ("bomb", BLACK, FLOOR),
    ("dud", TAN, FLOOR),
    ("burn", YELLOW, RED),
    ("flameC", YELLOW, RED),
    ("flameH", YELLOW, RED),
    ("flameV", YELLOW, RED),
    ("flameL", YELLOW, RED),
    ("flameR", YELLOW, RED),
    ("flameU", YELLOW, RED),
    ("flameD", YELLOW, RED),
    ("fire", YELLOW, RED),
    ("bombup", WHITE, BLUE),
    ("speed", BLACK, CYAN),
    ("kick", WHITE, BROWN),
    ("glove", WHITE, PURPLE),
    ("halfL", BLACK, FLOOR),    # a bomb half a tile right: its left half, in the card's right half
    ("halfR", BLACK, FLOOR),    # ... its right half, in the left of the next card
    ("halfU", BLACK, FLOOR),    # half a tile down: its top half, in the card's bottom half
    ("halfD", BLACK, FLOOR),    # ... its bottom half, in the top of the next card
    ("wall", BLACK, GREY),
    ("trophy", YELLOW, PANEL),
]
DUD_FG = TAN            # a dud's halves are the bomb's, in the dud's colour

# The animated tiles, by GRAM card (gfx.bas): their pages, a group a frame
ANIM = ["bomb", "dud", "burn", "flameC", "flameH", "flameV", "flameL", "flameR", "flameU", "flameD",
        "fire", "bombup", "speed", "kick", "glove"]
G_TILE0 = 22            # the arena's GRAM cards from here: pillar, block, then ANIM, the halves, wall, trophy

# gfx/intv/sprites.png: 8x16 bodies (one colour: the seat's), then 8x8 visors.
BODIES = ["down0", "down1", "right0", "right1", "right2", "win0", "win1", "die2", "die3"]
VISORS = ["front", "side", "frontlo"]

# gfx/intv/logo.png: FUJIBLAST, a letter 16x16 (2x2 cards); its pixels are
# the cards' background (the letter's colour), the rest foreground black.
LOGO_TEXT = "FUJIBLAST"
LOGO_TOP, LOGO_BOTTOM = YELLOW, ORANGE      # the letters' upper and lower cards

# Seats: the body's colour (any of 16) and the HUD's (a background).
SEAT_BODY = [WHITE, RED, CYAN, YELGREEN]
VISOR = YELLOW

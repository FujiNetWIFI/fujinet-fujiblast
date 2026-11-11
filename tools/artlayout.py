"""artlayout.py -- where things are in gfx/*.png (drawart.py writes them,
the converters read them)."""

# Tile roles -> RGB (an approximation of the Atari colours they become).
ROLE_RGB = {
    ".": (64, 136, 48),     # floor: the background
    "w": (200, 200, 200),   # light: PF0
    "k": (40, 40, 40),      # dark: PF1
    "o": (200, 104, 40),    # orange: PF2
    "y": (240, 224, 64),    # yellow: PF3 (inverse cells)
}

# Sprite roles -> RGBA.
SPRITE_RGB = {
    ".": (0, 0, 0, 0),
    "#": (255, 255, 255, 255),  # the player's colour
    "v": (240, 224, 64, 255),   # the visor: missiles, PF3
}

# gfx/tiles.png: one row of 16 lines per tile, frames left to right (up to
# 4, 8 pixels each). A tile with fewer frames repeats them over the four
# animation charsets.
TILES = [
    "floor", "pillar", "block", "burn", "bomb", "dud",
    "flameC", "flameH", "flameV", "flameL", "flameR", "flameU", "flameD",
    "fire", "bombup", "speed", "kick", "glove",
    "trophy",
]

# gfx/glyphs.png: single 4x8 cells, left to right.
GLYPHS = [
    "wall", "panel",
    "font0", "font1", "font2", "font3", "font4", "font5", "font6", "font7", "font8", "font9",
    "fontx", "font/", "fontR", "fontN", "fontD",
]

# gfx/sprites.png: 8x16 frames, left to right.
SPRITES = [
    "down0", "down1", "down2", "up0", "up1", "up2",
    "right0", "right1", "right2", "left0", "left1", "left2",
    "win0", "win1", "die0", "die1", "die2", "die3",
]

# gfx/logo.png: 24 lines, 12 pixels (3 cells) a letter.
LOGO_TEXT = "FUJIBLAST"

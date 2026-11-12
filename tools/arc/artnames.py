"""artnames.py -- what is where in gfx/arcadia/*.png (drawart_arc.py writes
them, mkgfx_arc.py reads them; this needs no Pillow)."""

# sprites.png: 8x8 frames, left to right
SPRITES = ["down0", "down1", "down2", "up0", "up1", "up2",
           "right0", "right1", "right2", "left0", "left1", "left2",
           "win0", "win1", "die0", "die1", "die2", "die3"]

# glyphs.png: 8x8 user glyphs, left to right
GLYPH_ORDER = ["brick", "bomb0", "bomb1", "flameh0", "flameh1", "flamev0", "flamev1",
               "cursor", "dash", "colon", "trophy"]

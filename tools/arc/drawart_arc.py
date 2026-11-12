#!/usr/bin/env python3
"""drawart_arc.py -- the Emerson Arcadia 2001's art, drawn for the 2637:
gfx/arcadia/sprites.png, glyphs.png, logo.png (tools/arc/mkgfx_arc.py
turns them into the build's tables).

    drawart_arc.py              (needs Pillow; the build itself doesn't)

The 2637 has 8 user glyphs of 8x8, each row shown twice in 13-row mode
(8x16 lines: about square, its pixels are 1.8 times as wide as tall), one
colour on the ground. Four of them are also the four sprites' pictures,
so the bombers are one colour each and the arena's tiles get the other
four. Everything is reduced from gfx/sprites.png and tiles.png (the
master sheet, 8x16): the visor becomes holes, two master rows one row.
"""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from artnames import SPRITES, GLYPH_ORDER  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
OUT = os.path.join(ROOT, "gfx", "arcadia")

# ---- the bombers: 8x8, rows doubled on screen -------------------------------
DOWN = [
    "..####..",
    ".######.",
    "##....##",
    ".######.",
    "#.####.#",
    "..####..",
    "..#..#..",
    ".##..##.",
]
UP = DOWN[:2] + ["########"] + DOWN[3:]
RIGHT = [
    "..####..",
    ".######.",
    "####...#",
    ".######.",
    "..####..",
    ".#####..",
    "..#..#..",
    "..#..##.",
]


def legs(f, a, b):
    return f[:6] + [a, b]


def mirror(f):
    return [r[::-1] for r in f]


def sprite_frames():
    fr = {}
    fr["down0"] = DOWN
    fr["down1"] = legs(DOWN, "..#..#..", ".##...#.")
    fr["down2"] = legs(DOWN, "..#..#..", ".#...##.")
    fr["up0"] = UP
    fr["up1"] = legs(UP, "..#..#..", ".##...#.")
    fr["up2"] = legs(UP, "..#..#..", ".#...##.")
    fr["right0"] = RIGHT
    fr["right1"] = legs(RIGHT, ".#....#.", "##....##")
    fr["right2"] = legs(RIGHT, "...##...", "..###...")
    for k in "012":
        fr["left" + k] = mirror(fr["right" + k])
    fr["win0"] = [
        "#.####.#",
        "########",
        "##....##",
        ".######.",
        "..####..",
        "..####..",
        "..#..#..",
        ".##..##.",
    ]
    fr["win1"] = ["........"] + fr["win0"][:5] + ["..#..#..", ".##..##."]
    fr["die0"] = fr["up0"]
    fr["die1"] = fr["left0"]
    fr["die2"] = [
        "........",
        "........",
        "..####..",
        ".######.",
        "##....##",
        "########",
        ".##..##.",
        "........",
    ]
    fr["die3"] = [
        "........",
        "#..#...#",
        ".#...#..",
        "..#.#.#.",
        "#.......",
        "...#..#.",
        ".#.....#",
        "........",
    ]
    return fr



# ---- the glyphs: the arena's four (two frames where they animate), the text
# screens' -----------------------------------------------------------------
GLYPHS = {
    # a brick block: two courses, the joints offset; the mortar is the floor
    "brick": [
        "###.####",
        "###.####",
        "###.####",
        "........",
        "#######.",
        "#######.",
        "#######.",
        "........",
    ],
    # a bomb, its fuse sparking (two frames), a highlight hole
    "bomb0": [
        "......#.",
        ".....#..",
        "..####..",
        ".#..###.",
        "#.######",
        "########",
        ".######.",
        "..####..",
    ],
    "bomb1": [
        ".....#.#",
        "......#.",
        "..####..",
        ".#..###.",
        "#.######",
        "########",
        ".######.",
        "..####..",
    ],
    # the blast's arms: a band across (two frames: thick, thin) ...
    "flameh0": [
        "........",
        "########",
        "########",
        "########",
        "########",
        "########",
        "########",
        "........",
    ],
    "flameh1": [
        "........",
        "........",
        "########",
        "########",
        "########",
        "########",
        "........",
        "........",
    ],
    # ... and a band down
    "flamev0": [
        ".######.",
        ".######.",
        ".######.",
        ".######.",
        ".######.",
        ".######.",
        ".######.",
        ".######.",
    ],
    "flamev1": [
        "..####..",
        "..####..",
        "..####..",
        "..####..",
        "..####..",
        "..####..",
        "..####..",
        "..####..",
    ],
    # text screens
    "cursor": [
        "........",
        ".#......",
        ".##.....",
        ".###....",
        ".####...",
        ".###....",
        ".##.....",
        ".#......",
    ],
    "dash": [
        "........",
        "........",
        "........",
        "........",
        ".#####..",
        "........",
        "........",
        "........",
    ],
    "colon": [
        "........",
        "........",
        "...##...",
        "...##...",
        "........",
        "...##...",
        "...##...",
        "........",
    ],
    "trophy": [
        "........",
        "#.####.#",
        "#.####.#",
        ".######.",
        "..####..",
        "...##...",
        "..####..",
        ".######.",
    ],
}

# ---- the title's logo, in block graphics: 48 blocks across (3 to a cell:
# 3, 3 and 2 pixels wide), 2 block rows to a cell row; the first cell is
# the row's switch into block graphics (blank), so blocks 0-2 stay empty ---
LOGO = [
    "...####.#..#....#.###.###..#.....##...###.####..",
    "...#....#..#....#..#..#..#.#....#..#.#.....##...",
    "...###..#..#....#..#..###..#....####..##...##...",
    "...#....#..#....#..#..#..#.#....#..#....#..##...",
    "...#....#..#.#..#..#..#..#.#....#..#....#..##...",
    "...#.....##...##..###.###..####.#..#.###...##...",
]


def png(path, rows, scale=1):
    h, w = len(rows), len(rows[0])
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    for y, r in enumerate(rows):
        assert len(r) == w, (path, y, r)
        for x, p in enumerate(r):
            if p == "#":
                img.putpixel((x, y), (255, 255, 255, 255))
    img.save(path)


def main():
    os.makedirs(OUT, exist_ok=True)
    fr = sprite_frames()
    rows = ["".join(fr[n][y] for n in SPRITES) for y in range(8)]
    png(os.path.join(OUT, "sprites.png"), rows)
    rows = ["".join(GLYPHS[n][y] for n in GLYPH_ORDER) for y in range(8)]
    png(os.path.join(OUT, "glyphs.png"), rows)
    png(os.path.join(OUT, "logo.png"), [r.ljust(48, ".") for r in LOGO])
    print("wrote gfx/arcadia/sprites.png glyphs.png logo.png")


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Reuse the author's indexed artwork; quantize dirty regions to OCS planes.
Backdrop index zero is changed per band by the OS-managed copper list.
The virtual VGA palette keeps sprite colouring and the original asset layout.
"""

from pathlib import Path
import sys
import mkgfx_vga
import vgapal

# RGB4: outline, steel, white, red, gold, grass, blue and teal shades.
PALETTE = [
    0x000,
    0x001,
    0x112,
    0x223,
    0x345,
    0x567,
    0x789,
    0xABC,
    0xDEF,
    0xFFF,
    0x501,
    0x922,
    0xD43,
    0xF98,
    0x620,
    0xB60,
    0xF91,
    0xFD4,
    0xFFB,
    0x142,
    0x373,
    0x7B5,
    0x126,
    0x259,
    0x48E,
    0x9CF,
    0x167,
    0x5BC,
    0x956,
    0xA77,
    0x432,
    0x765,
]


def rgb(value):
    return tuple(((value >> shift) & 15) * 17 for shift in (8, 4, 0))


def nearest(colour):
    return min(
        range(1, 32),
        key=lambda i: sum((a - b) ** 2 for a, b in zip(colour, rgb(PALETTE[i]))),
    )


def main():
    mkgfx_vga.main()
    out = Path(sys.argv[1])
    for name in ("gfxdata.c", "gfxcodes.h"):
        path = out / name
        path.write_text(
            path.read_text()
            .replace("gfx/vga/*.png", "VGA PNG assets")
            .replace("__far ", "")
        )
    table = [nearest(c) for c in vgapal.palette()]
    table[vgapal.BACK : vgapal.BACK + 32] = [0] * 32
    (out / "palette.c").write_text(
        "#include <stdint.h>\n"
        + "const uint16_t amiga_palette[32] = {"
        + ",".join(hex(c) for c in PALETTE)
        + "};\n"
        + "const uint8_t amiga_pens[256] = {"
        + ",".join(map(str, table))
        + "};\n"
    )


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""preview.py -- MAME Arcadia snapshots (192x262) scaled to how a TV shows
them (4:3: a pixel 1.82 times as wide as a line is tall), for looking at.

    preview.py OUT.png IN.png [IN.png ...]   (side by side; needs Pillow)
"""
import sys

from PIL import Image

H = 2                                   # lines -> pixels
W = H * 262 * 4 / 3 / 192               # pixels -> pixels (about 3.64)


def main(out, *ins):
    ims = []
    for p in ins:
        im = Image.open(p).convert("RGB")
        ims.append(im.resize((round(im.width * W), im.height * H), Image.NEAREST))
    w = sum(i.width for i in ims) + 16 * (len(ims) - 1)
    h = max(i.height for i in ims)
    sheet = Image.new("RGB", (w, h), (32, 32, 32))
    x = 0
    for i in ims:
        sheet.paste(i, (x, 0))
        x += i.width + 16
    sheet.save(out)


if __name__ == "__main__":
    main(*sys.argv[1:])

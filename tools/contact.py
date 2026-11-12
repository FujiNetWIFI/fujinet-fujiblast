#!/usr/bin/env python3
"""contact.py OUT.png IN.png... -- a contact sheet of screenshots, 4 across, half size."""
import sys
from PIL import Image
files = sys.argv[2:]
ims = [Image.open(f).convert("RGB") for f in files]
w, h = ims[0].size
w2, h2 = w // 2, h // 2
cols = 4
rows = (len(ims) + cols - 1) // cols
sheet = Image.new("RGB", (cols * w2, rows * h2))
for i, im in enumerate(ims):
    sheet.paste(im.resize((w2, h2)), ((i % cols) * w2, (i // cols) * h2))
sheet.save(sys.argv[1])

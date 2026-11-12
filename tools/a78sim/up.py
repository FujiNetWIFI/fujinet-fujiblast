#!/usr/bin/env python3
"""up.py IN OUT [SCALE] [x0 y0 x1 y1] -- enlarge (and crop) a snapshot."""
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
s = int(sys.argv[3]) if len(sys.argv) > 3 else 3
if len(sys.argv) > 7:
    im = im.crop(tuple(int(v) for v in sys.argv[4:8]))
im.resize((im.width * s, im.height * s), Image.NEAREST).save(sys.argv[2])

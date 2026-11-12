#!/usr/bin/env python3
"""check.py -- read the 160B probe's snapshot: per zone, which objects MARIA
drew on how many of the zone's 8 lines.

    check.py probe.json snap.png SYSTEM
"""
import json
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "a78"))
from palette import NTSC  # noqa: E402

RUNC = [0x34, 0x84]
LEFTC, RIGHTC = 0xC4, 0x1C
BOMBC = [0x5A, 0x2A, 0x9E, 0x46]


def main():
    lay = json.load(open(sys.argv[1]))
    im = Image.open(sys.argv[2]).convert("RGB")
    px = im.load()
    w, h = im.size
    sx = w / 160.0
    # zone 0's first line: the first non-black line at the run's first cell
    x = int((lay["run_x"] + 2) * sx)
    y0 = next(y for y in range(h) if px[x, y] != (0, 0, 0))
    print("%s: %dx%d, zone 0 at y %d" % (sys.argv[3], w, h, y0))
    print("zone  dma  run L R b1 b2 b3 b4   (lines of 8 drawn; run = cells drawn of n)")
    bad = 0
    for z, zd in enumerate(lay["zones"]):
        def seen(hx, colour):
            n = 0
            for l in range(8):
                if px[int(hx * sx), y0 + z * 8 + l] == NTSC[colour]:
                    n += 1
            return n
        n = zd["run"]
        cells = 0
        covered = set()
        for k in range(zd["bombers"]):
            for i in range(28):
                cx = lay["run_x"] + 4 * i
                if cx < lay["bomb_x"][k] + 8 and cx + 4 > lay["bomb_x"][k]:
                    covered.add(i)
        want = 0
        for i in range(n):
            if i in covered:
                continue
            want += 1
            if seen(lay["run_x"] + 4 * i + 2, RUNC[i % 2]) >= 4:
                cells += 1
        left = seen(lay["left_x"] + 8, LEFTC) if zd["left"] else "-"
        right = seen(lay["right_x"] + 6, RIGHTC) if zd["right"] else "-"
        b = [seen(lay["bomb_x"][k] + 4, BOMBC[k]) if k < zd["bombers"] else "-" for k in range(4)]
        ok = cells == want and left in (8, "-") and right in (8, "-") and all(v in (8, "-") for v in b)
        bad += not ok
        print("%3d %5d  %2d/%-2d %s %s %s %s %s %s  %s" % (z, zd["dma"], cells, want, left, right, b[0], b[1], b[2], b[3],
                                                          "ok" if ok else "SHORT"))
    print("verdict:", "ALL FIT" if not bad else "%d zones short" % bad)


if __name__ == "__main__":
    main()

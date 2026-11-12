"""astropal.py -- the Astrocade's colours as MAME shows them.

A colour register holds hue<<3 | luma (hue 0-31, luma 0-7). MAME's
astrocade_palette (src/mame/bally/astrocde_v.cpp) puts hue on a circle in
YUV (blue at the top: hue 30-31 is blue, 7AH is olive, not blue) and the
register's luma at twice its value in a 16-step scale. The same float maths
as the ISS tracker's tools/mkmap.py astro_rgb.
"""
import math
import struct


def _f32(x):
    return struct.unpack("f", struct.pack("f", x))[0]


def astro_rgb(byte):
    hue, lum = (byte >> 3) & 31, byte & 7
    angle = (hue / 32.0) * (2.0 * math.pi)
    ry = _f32(0.75 * math.sin(angle)) if hue else 0.0
    by = _f32(1.15 * math.cos(angle)) if hue else 0.0
    y = _f32((2 * lum) / 15.0)
    r = int(_f32((ry + y) * 255))
    g = int(_f32((_f32(y - _f32(0.299 * _f32(ry + y)) - _f32(0.114 * _f32(by + y))) / 0.587) * 255))
    b = int(_f32((by + y) * 255))
    clamp = lambda v: max(0, min(255, v))
    return clamp(r), clamp(g), clamp(b)


if __name__ == "__main__":
    import sys
    for b in (int(x, 16) for x in sys.argv[1:]) if len(sys.argv) > 1 else range(256):
        print("%02XH %3d %3d %3d" % ((b,) + astro_rgb(b)))

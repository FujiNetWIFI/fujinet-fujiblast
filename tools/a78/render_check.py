#!/usr/bin/env python3
"""render_check.py -- decode gen/atari7800's binaries the way MARIA does
(maria.cpp write_line_ram) and compare with tools/drawart_a78.py's art.

    python3 tools/a78/render_check.py [GENDIR]      # PNG in GENDIR/check/
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
sys.path.insert(0, HERE)
import drawart_a78 as A     # noqa: E402
import drawart_cga as C     # noqa: E402
import drawart_dhgr as D    # noqa: E402
from palette import NTSC    # noqa: E402
from artlayout import SPRITES, TILES  # noqa: E402

G = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "gen", "atari7800")
rd = lambda n: open(os.path.join(G, n), "rb").read()      # noqa: E731
REPL, TXT, BIG, GFX, ANIM = rd("repl.bin"), rd("art_txt.bin"), rd("art_big.bin"), rd("art_gfx.bin"), rd("art_anim.bin")

# the palette registers (DESIGN.md): index = palette*4 + colour
PAL = {}
for p, row in enumerate([(0x05, 0x0F, 0x0A), (0x26, 0x2A, 0x36), (0x4C, 0x5A, 0x75), (0x00, 0x1E, 0x9C)]):
    for i, c in enumerate(row):
        PAL[p * 4 + i + 1] = c
for s, (light, shade) in enumerate(A.SEAT):
    PAL[(4 + s) * 4 + 1] = shade
    PAL[(4 + s) * 4 + 2] = light
    PAL[(4 + s) * 4 + 3] = light


def dec160b(byte, grp):
    """MARIA 160B: two pixels (colour index into the 5-bit palette number or None)."""
    out = []
    for c, p in (((byte >> 6) & 3, (byte >> 2) & 3), ((byte >> 4) & 3, byte & 3)):
        out.append(PAL[(grp * 4 + p) * 4 + c] if c else None)
    return out


def dec160a(byte, pal):
    return [PAL[pal * 4 + c] if c else None for c in ((byte >> 6) & 3, (byte >> 4) & 3, (byte >> 2) & 3, byte & 3)]


def cell160b(block, code, grp=0):
    rows = []
    for k in range(8):
        a = (7 - k) * 256 + code
        rows.append(dec160b(block[a], grp) + dec160b(block[a + 1], grp))
    return rows


def ref_tile(frame):
    return [[None if c == "." else A.TILE_COL[c] for c in r] for r in frame]


def codes():
    """tile_chars etc. parsed from the generated C."""
    import re
    t = open(os.path.join(G, "gfxdata.c")).read()
    tc = [[int(x, 16) for x in m.groups()] for m in re.finditer(r"\{ 0x(..), 0x(..), 0x(..), 0x(..) \},\s+/\*", t)]
    tr = [int(x, 16) for x in re.search(r"trophy_chars\[4\] = \{ (.*?) \}", t).group(1).replace("0x", "").split(", ")]
    logo = []
    body = t[t.index("logo_chars"):t.index("trophy_chars")]
    for blk in re.findall(r"\{\n(.*?)\n  \}", body, re.S):
        logo.append([int(x, 16) for x in re.findall(r"0x(..)", blk)])
    h = open(os.path.join(G, "gfxcodes.h")).read()
    ch = {m.group(1): int(m.group(2), 16) for m in re.finditer(r"#define (CH_\w+)\s+0x(..)", h)}
    return tc, tr, logo, ch


def main():
    tc, tr, logo, ch = codes()
    tiles = A.tile_sets()
    names = TILES[:18]
    bad = 0
    sheet_w = 18 * 9 + 2
    img = Image.new("RGB", (900, 700), (0x20, 0x20, 0x20))
    px = img.load()

    def put(x0, y0, rows, sc=2):
        for y, r in enumerate(rows):
            for x, c in enumerate(r):
                if c is None:
                    continue
                for dy in range(sc):
                    for dx in range(sc):
                        px[x0 + x * sc + dx, y0 + y * sc + dy] = NTSC[c]

    # (a) arena tiles, every set
    y0 = 4
    for s in range(4):
        x0 = 4
        for i, n in enumerate(names):
            fr = tiles[n]
            ref = ref_tile(fr[s % len(fr)])
            blk = ANIM[s * 2048:(s + 1) * 2048]
            got = [[None] * 8 for _ in range(16)]
            for k, code in enumerate(tc[i]):
                cell = cell160b(blk, code)
                for y in range(8):
                    for x in range(4):
                        got[(k // 2) * 8 + y][(k % 2) * 4 + x] = cell[y][x]
            if got != ref:
                bad += 1
                print("tile mismatch", n, "set", s)
            put(x0, y0, got)
            x0 += 18 if i < 17 else 0
            if x0 > 880:
                break
        y0 += 36
    # (b) stat font + wall (set 0)
    x0 = 4
    for nm in ["WALL"] + ["FONT_" + c for c in "0123456789X"] + ["FONT_SLASH", "FONT_R", "FONT_N", "FONT_D"]:
        cell = cell160b(ANIM, ch["CH_" + nm])
        if nm.startswith("FONT_"):
            c = nm[5:]
            c = "/" if c == "SLASH" else c
            g = D.font3()[ord(c) - 32]
            ref = [[0x0F if (y < 7 and x < 3 and g[y][x] == "#") else None for x in range(4)] for y in range(8)]
        else:
            ref = [[A.TILE_COL[c] for c in D.WALL[y % 4]] for y in range(8)]
        if cell != ref:
            bad += 1
            print("glyph mismatch", nm)
        put(x0, y0, cell)
        x0 += 12
    y0 += 24
    # logo + trophy (GFX block)
    lg = D.logo()
    for r in range(3):
        for c, code in enumerate(logo[r]):
            cell = cell160b(GFX, code)
            for y in range(8):
                for x in range(4):
                    ch_ = lg[r * 8 + y][c * 4 + x] if c * 4 + x < len(lg[0]) else "b"
                    ref = None if ch_ == "b" else A.TILE_COL[ch_]
                    if cell[y][x] != ref:
                        bad += 1
                        print("logo mismatch", r, c, y, x)
                        break
            put(4 + c * 8, y0 + r * 16, cell)
    y0 += 52
    tfr = tiles["trophy"][0]
    got = [[None] * 8 for _ in range(16)]
    for k, code in enumerate(tr):
        cell = cell160b(GFX, code)
        for y in range(8):
            for x in range(4):
                got[(k // 2) * 8 + y][(k % 2) * 4 + x] = cell[y][x]
    if got != ref_tile(tfr):
        bad += 1
        print("trophy mismatch")
    put(300, y0 - 52, got)
    # TXT and BIG samples
    msg = "FUJIBLAST 0123 AZ:-"
    for n, text in enumerate([msg, msg]):
        pass
    for i, chh in enumerate(msg):
        idx = ord(chh) - 32
        for inv, base in ((0, 0), (1, 64)):
            for k in range(8):
                b = TXT[(7 - k) * 256 + base + idx]
                ref = C.glyph(C.namco(), chh)[k]
                refb = int(ref.replace(".", "0").replace("#", "1"), 2) ^ (0xFF if inv else 0)
                if b != refb:
                    bad += 1
                    print("txt mismatch", chh, inv, k)
                for x in range(8):
                    if b & (0x80 >> x):
                        px[4 + i * 9 + x, y0 + inv * 10 + k] = NTSC[0x0F]
        for k in range(8):
            a = (7 - k) * 256 + 2 * idx
            v = BIG[a] << 8 | BIG[a + 1]
            ref = C.glyph(C.namco(), chh)[k]
            for x in range(8):
                got_ = (v >> (14 - 2 * x)) & 3
                if got_ != (2 if ref[x] == "#" else 0):
                    bad += 1
                    print("big mismatch", chh, k, x)
                if got_:
                    px[4 + i * 9 + x, y0 + 24 + k] = NTSC[PAL[3 * 4 + got_]]
    # (c) the bombers, shifted 3 lines, through the zones as MARIA walks them
    y0 += 40
    for seat in range(4):
        for f, name in enumerate(SPRITES):
            ref = A.sprite_sets()[name]
            ysp = 3
            scr = [[None] * 8 for _ in range(32)]
            for zone in range(4):
                t = zone * 8
                d = t - ysp
                if not (-8 < d < 16):
                    continue
                hi = 0x8F - d
                for layer, pal in ((0, 4 + seat), (1, 3)):
                    col = 2 * (f * 2 + layer)
                    for k in range(8):
                        page = hi + 7 - k
                        assert 0x80 <= page <= 0x9D, (name, zone, k, hex(page))
                        for b in range(2):
                            byte = REPL[(page - 0x80) * 256 + col + b]
                            for x, c in enumerate(dec160a(byte, pal)):
                                if c is not None:
                                    scr[t + k][b * 4 + x] = c
            for y in range(16):
                for x in range(8):
                    p = ref[y][x]
                    body, shade = A.SEAT[seat]
                    want = {"#": body, "+": shade, "v": A.VISOR, "k": A.BLACK}.get(p)
                    if scr[y + ysp][x] != want:
                        bad += 1
                        print("sprite mismatch", name, seat, y, x)
            put(4 + f * 18, y0 + seat * 36, [r for r in scr[ysp:ysp + 16]])
    os.makedirs(os.path.join(G, "check"), exist_ok=True)
    out = os.path.join(G, "check", "art.png")
    img.save(out)
    print("check:", out, "mismatches:", bad)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()

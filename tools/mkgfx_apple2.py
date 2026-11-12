#!/usr/bin/env python3
"""mkgfx_apple2.py -- gfx/dhgr/*.png -> the Apple II's double hi-res data.

    python3 tools/mkgfx_apple2.py OUTDIR    # writes OUTDIR/gfxdata.s, gfxcodes.h

Double hi-res: a line is 80 bytes, aux and main in turn (aux 0, main 0,
aux 1, ...), 7 dots a byte, bit 0 first. A colour pixel is 4 dots; dot j of
a pixel carries bit (j+1)%4 of its colour. 7 pixels (28 dots, 4 bytes: aux,
main, aux, main) make a group, the unit everything here is aligned to.

The art goes to auxiliary RAM (segment AUXGFX, the file FUJIBLST.GFX), which
the blitter (src/apple2/dhgr.s) reads with RAMRD on:
  ax_exp      fragment (7 pixel bits, bit 0 left) -> byte k's bits, 4 x 128
  ax_nexp     the same, inverted (the bits to keep)
  ax_shl      by shift s: the fragment shifted s pixels, low 7 / high 7, 7 x 256
  ax_ctab     per sprite colour: exp & the colour, 4 x 128 each
  ax_tiles    tile frames: 16 lines of aux0, main0, aux1, main1
  ax_movers   bomb/dud mover images on floor-less masks, 2 groups wide:
              16 lines of (data, keep) for 8 bytes; offset 0 and 3 pixels
  ax_spr      sprite frames: 16 lines of mask, body, shade, visor (7 px)
  ax_sprbig   the same, twice as wide (two 7-pixel halves)
  ax_logo     24 lines: 40 aux bytes, then 40 main
  ax_frame    the arena's frame: 4 wall-band lines and 4 side-wall lines,
              each 40 aux + 40 main
Main memory (RODATA): the fonts, for the text packer (src/apple2/gfx.c), and
the tiles' frame counts and addresses.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from artlayout import TILES, SPRITES  # noqa: E402
from png import Image  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
DH = os.path.join(ROOT, "gfx", "dhgr")
AUX_BASE = 0x6000           # src/apple2/fujiblst.cfg: AUXGFX
SPR_BASE = 0x0800           # and AUXSPR (page aligned: dh_bomber's pointers)

# drawart_dhgr.py's palette and roles
PAL16 = [
    (0x00, 0x00, 0x00), (0xDD, 0x00, 0x33), (0x00, 0x00, 0x99), (0xAA, 0x00, 0xFF),
    (0x00, 0x77, 0x22), (0x55, 0x55, 0x55), (0x22, 0x22, 0xFF), (0x66, 0xAA, 0xFF),
    (0x88, 0x55, 0x00), (0xFF, 0x66, 0x00), (0xAA, 0xAA, 0xAA), (0xFF, 0x99, 0x88),
    (0x00, 0xFF, 0x44), (0xFF, 0xFF, 0x00), (0x44, 0xFF, 0xCC), (0xFF, 0xFF, 0xFF),
]
RGB_COL = {c: i for i, c in enumerate(PAL16)}
ROLE = {(255, 255, 255): "#", (128, 128, 128): "+", PAL16[13]: "v", (0, 0, 0): "k"}
SEAT_COLOURS = [(15, 10), (11, 1), (7, 6), (12, 8)]
VISOR = 13
FLOOR = 4
MAP_TILES = TILES[:18]


def colour_bit(dot):
    """The colour bit a dot (0..27 within a group) carries."""
    return ((dot % 4) + 1) % 4


def colpat(c):
    """A group all in colour c: its 4 bytes."""
    out = []
    for k in range(4):
        b = 0
        for bit in range(7):
            if (c >> colour_bit(7 * k + bit)) & 1:
                b |= 1 << bit
        out.append(b)
    return out


def exp_byte(frag, k):
    """The bits of byte k (of a group) that the pixels in frag cover."""
    b = 0
    for p in range(7):
        if frag >> p & 1:
            for j in range(4):
                d = 4 * p + j
                if d // 7 == k:
                    b |= 1 << (d % 7)
    return b


def pack_pixels(px):
    """7*n colour pixels -> 4*n bytes (aux, main, aux, main per group)."""
    assert len(px) % 7 == 0
    out = []
    for g in range(len(px) // 7):
        bs = [0, 0, 0, 0]
        for p in range(7):
            c = px[g * 7 + p]
            for j in range(4):
                d = 4 * p + j
                if (c >> colour_bit(d)) & 1:
                    bs[d // 7] |= 1 << (d % 7)
        out += bs
    return out


def pack_mask(keep):
    """7*n flags (1: the screen shows through) -> 4*n keep-bytes."""
    out = []
    for g in range(len(keep) // 7):
        bs = [0, 0, 0, 0]
        for p in range(7):
            if keep[g * 7 + p]:
                for j in range(4):
                    d = 4 * p + j
                    bs[d // 7] |= 1 << (d % 7)
        out += bs
    return out


def halves(stream):
    """Interleaved bytes of a line -> (aux bytes, main bytes)."""
    return stream[0::2], stream[1::2]


def colours(img, x0, y0, w, h):
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            p = img.getpixel((x0 + x, y0 + y))[:3]
            if p not in RGB_COL:
                sys.exit(f"stray colour {p} at {x0 + x},{y0 + y}")
            row.append(RGB_COL[p])
        rows.append(row)
    return rows


def tile_frames(img, i):
    fr = [colours(img, f * 7, i * 16, 7, 16) for f in range(4)]
    n = 4
    while n > 1 and all(all(c == FLOOR for c in r) for r in fr[n - 1]):
        n -= 1
    return fr[:n]


def sprite_planes(img, i):
    """mask, body, shade, visor: 16 rows of 7 bits (bit 0 left)."""
    out = []
    for y in range(16):
        m = b = s = v = 0
        for x in range(7):
            p = img.getpixel((i * 7 + x, y))
            if p[3] == 0:
                continue
            r = ROLE.get(p[:3])
            if r is None:
                sys.exit(f"sprite {SPRITES[i]}: stray colour {p}")
            m |= 1 << x
            if r == "#":
                b |= 1 << x
            elif r == "+":
                s |= 1 << x
            elif r == "v":
                v |= 1 << x
        out.append((m, b, s, v))
    return out


def font(path, gw):
    img = Image(path)
    glyphs = []
    for i in range(64):
        rows = []
        for y in range(7):
            b = 0
            for x in range(gw):
                if img.getpixel((i * gw + x, y))[:3] == (255, 255, 255):
                    b |= 1 << x
            rows.append(b)
        glyphs.append(rows + [0])
    return glyphs


def asm_bytes(data, per=16):
    lines = []
    for i in range(0, len(data), per):
        lines.append("        .byte   " + ",".join(f"${b:02X}" for b in data[i:i + per]))
    return "\n".join(lines)


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "gen", "apple2")
    os.makedirs(out_dir, exist_ok=True)
    tiles_img = Image(os.path.join(DH, "tiles.png"))
    spr_img = Image(os.path.join(DH, "sprites.png"))
    logo_img = Image(os.path.join(DH, "logo.png"))
    wall_img = Image(os.path.join(DH, "wall.png"))
    font3 = font(os.path.join(DH, "font3.png"), 4)
    font5 = font(os.path.join(DH, "font5.png"), 7)

    aux = []                    # (label, bytes, comment)

    exp = [exp_byte(f, k) for k in range(4) for f in range(128)]
    nexp = [(~e) & 0x7F for e in exp]
    aux.append(("ax_exp", exp, "fragment -> byte k's bits"))
    aux.append(("ax_nexp", nexp, "the bits to keep"))
    shl = []
    for s in range(7):
        shl += [(f << s) & 0x7F for f in range(128)]
        shl += [(f << s) >> 7 for f in range(128)]
    aux.append(("ax_shl", shl, "by shift: low 128, high 128"))
    # a byte's 7 dots moved r dots up (dh_bomber's shift: 4 dots a pixel, r = 4s mod 7)
    dsh = []
    for r in range(7):
        dsh += [(v << r) & 0x7F for v in range(128)]
        dsh += [v >> (7 - r) for v in range(128)]
    aux.append(("ax_dsh", dsh, "by r: low 128, high 128"))
    ctab_cols = []
    for body, shade in SEAT_COLOURS:
        for c in (body, shade):
            if c not in ctab_cols:
                ctab_cols.append(c)
    ctab_cols.append(VISOR)
    ctab = []
    for c in ctab_cols:
        cp = colpat(c)
        ctab += [exp[k * 128 + f] & cp[k] for k in range(4) for f in range(128)]
    aux.append(("ax_ctab", ctab, "per colour: exp & colour, 4 x 128"))

    # tiles
    tile_first, tile_n, tbytes = [], [], []
    for i, name in enumerate(TILES):
        frs = tile_frames(tiles_img, i)
        if name in ("fire", "bombup", "speed", "kick", "glove"):
            frs = frs[-1:]      # lit, and still: redrawing them costs the Apple dear
        elif name == "burn" or name.startswith("flame"):
            frs = frs[:1]       # still too (they last half a second): only bombs and duds move
        tile_first.append(len(tbytes) // 64)
        tile_n.append(len(frs))
        for fr in frs:
            for row in fr:
                tbytes += pack_pixels(row)
    aux.append(("ax_tiles", tbytes, f"{len(tbytes) // 64} tile frames"))

    # movers: the bomb's and the dud's frames, at offsets 0 and 3 pixels
    mov = []
    for name in ("bomb", "dud"):
        i = TILES.index(name)
        frs = tile_frames(tiles_img, i)
        frs = (frs * 4)[:4]
        for off in (0, 3):
            for fr in frs:
                for row in fr:
                    px = [FLOOR] * 14
                    keep = [1] * 14
                    for x, c in enumerate(row):
                        if c != FLOOR:
                            px[off + x] = c
                            keep[off + x] = 0
                    d, k = pack_pixels(px), pack_mask(keep)
                    for j in range(8):
                        mov += [d[j] & ~k[j] & 0x7F, k[j]]
    aux.append(("ax_movers", mov, "bomb/dud x offset 0,3 x 4 frames: 16 lines of 8 (data, keep)"))

    # sprites
    spr, big = [], []
    for i in range(len(SPRITES)):
        for planes in sprite_planes(spr_img, i):
            spr += list(planes)
            for p in planes:
                w = 0
                for x in range(7):
                    if p >> x & 1:
                        w |= 3 << (2 * x)
                big += [w & 0x7F, w >> 7]
    aux.append(("ax_spr", spr, "16 lines of mask, body, shade, visor"))
    aux.append(("ax_sprbig", big, "16 lines of 4 planes, each 2 halves (14 px)"))

    # bombers, coloured: by frame, then seat, 16 lines of 4 data bytes; and by
    # frame the coverage, 16 lines of 4 (dh_bomber shifts both)
    bspr, bmask = [], []
    for i in range(len(SPRITES)):
        planes = sprite_planes(spr_img, i)
        for body, shade in SEAT_COLOURS:
            for m, b, sh, v in planes:
                px = []
                for x in range(7):
                    c = 0
                    if m >> x & 1:
                        c = body if b >> x & 1 else shade if sh >> x & 1 else VISOR if v >> x & 1 else 0
                    px.append(c)
                bspr += pack_pixels(px)
        for m, b, sh, v in planes:
            bmask += pack_mask([m >> x & 1 for x in range(7)])
    assert len(bspr) == len(SPRITES) * 256
    spraux = [("ax_bspr", bspr, "frame, seat, 16 lines of 4 bytes, in colour"),
              ("ax_bmask", bmask, "frame, 16 lines of 4 bytes: the covered bits")]

    # logo: 140x24
    lg = []
    lc = colours(logo_img, 0, 0, logo_img.width, 24)
    for row in lc:
        a, m = halves(pack_pixels(row))
        lg += a + m
    aux.append(("ax_logo", lg, "24 lines: 40 aux, 40 main"))

    # the arena's frame: walls 4 px wide at px 24-27 and 119-122, 4 lines
    # tall across px 24-122; panels black
    wc = colours(wall_img, 0, 0, 4, 4)
    frame = []
    for kind in ("band", "side"):
        for y in range(4):
            row = [0] * 140
            for x in range(24, 123):
                if kind == "band" or x < 28 or x > 118:
                    row[x] = wc[y][x % 4]
                else:
                    row[x] = FLOOR
            a, m = halves(pack_pixels(row))
            frame += a + m
    aux.append(("ax_frame", frame, "4 band lines, 4 side lines: 40 aux, 40 main"))

    # ---- write ---------------------------------------------------------------
    s = ["; gfxdata.s -- made by tools/mkgfx_apple2.py from gfx/dhgr/*.png; do not edit.",
         "        .export " + ", ".join("_" + lab for lab, _, _ in aux),
         "        .export _font3, _font5, _tile_first, _tile_n, _ctab_of, _colpat",
         "",
         '        .segment "AUXGFX"']
    addr = AUX_BASE
    sizes = []
    for lab, data, note in aux:
        s.append(f"; {note} (${addr:04X}, {len(data)} bytes)")
        s.append(f"_{lab}:")
        s.append(asm_bytes(data))
        sizes.append((lab, addr, len(data)))
        addr += len(data)
    assert dict((l, a) for l, a, _ in sizes)["ax_dsh"] % 256 == 0     # dh_bomber's pages
    s += ["", '        .segment "AUXSPR"', "        .export " + ", ".join("_" + lab for lab, _, _ in spraux)]
    saddr = SPR_BASE
    for lab, data, note in spraux:
        s.append(f"; {note} (${saddr:04X}, {len(data)} bytes)")
        s.append(f"_{lab}:")
        s.append(asm_bytes(data))
        sizes.append((lab, saddr, len(data)))
        saddr += len(data)
    assert saddr <= 0x2000
    s += ["", '        .segment "LC"', "; a group all in colour c: its 4 bytes (dh_frags, dh_fill)", "_colpat:"]
    s.append(asm_bytes([b for c in range(16) for b in colpat(c)]))
    s += ["", "        .rodata", "; by line, then character (ASCII 32-95): font3[line * 64 + c]", "_font3:"]
    s.append(asm_bytes([font3[c][l] for l in range(8) for c in range(64)]))
    s.append("_font5:")
    s.append(asm_bytes([font5[c][l] for l in range(8) for c in range(64)]))
    s.append("_tile_first:")
    s.append(asm_bytes(tile_first))
    s.append("_tile_n:")
    s.append(asm_bytes(tile_n))
    s.append("; per seat: the ctab index of body, shade; then the visor's")
    s.append("_ctab_of:")
    s.append(asm_bytes([v for body, shade in SEAT_COLOURS for v in (ctab_cols.index(body), ctab_cols.index(shade))]
                       + [ctab_cols.index(VISOR)]))
    open(os.path.join(out_dir, "gfxdata.s"), "w").write("\n".join(s) + "\n")

    h = ["/* gfxcodes.h -- made by tools/mkgfx_apple2.py; do not edit. */",
         "#ifndef GFXCODES_H", "#define GFXCODES_H", "#include <stdint.h>"]
    for i, name in enumerate(SPRITES):
        h.append(f"#define FR_{name.upper():<8} {i}")
    h.append(f"#define N_FRAMES {len(SPRITES)}")
    h.append(f"#define T_TROPHY {TILES.index('trophy')}")
    h.append(f"#define AUXGFX_SIZE {addr - AUX_BASE}")
    for lab, a, n in sizes:
        h.append(f"#define {lab.upper():<12} 0x{a:04X}u   /* {n} bytes */")
    h += ["extern const uint8_t font3[8][64];         /* by line: 3x7, bit 0 left */",
          "extern const uint8_t font5[8][64];         /* 5x7 */",
          "extern const uint8_t tile_first[];         /* by tile: its first frame */",
          "extern const uint8_t tile_n[];             /* and how many */",
          "extern const uint8_t ctab_of[];",
          "#endif"]
    open(os.path.join(out_dir, "gfxcodes.h"), "w").write("\n".join(h) + "\n")
    print(f"aux: {addr - AUX_BASE} bytes (${AUX_BASE:04X}-${addr - 1:04X}); "
          f"{len(tbytes) // 64} tile frames, {len(SPRITES)} sprite frames")



if __name__ == "__main__":
    main()

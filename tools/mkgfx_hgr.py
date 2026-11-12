#!/usr/bin/env python3
"""mkgfx_hgr.py -- gfx/hgr/*.png -> the Apple II's high-res data.

    python3 tools/mkgfx_hgr.py OUTDIR    # writes OUTDIR/gfxdata.s, gfxcodes.h

High-res (tools/hgr.py): 40 bytes a line, a colour pixel two dots, so a
group of 7 colour pixels -- a tile's width, the unit gfx.c works in -- is two
bytes, 2g and 2g+1. The art goes to auxiliary RAM (segment AUXGFX, the file
FUJIBLST.GFX), which the blitter (src/apple2/hgr.s) reads with RAMRD on
while it writes the screen in main RAM:
  ax_tiles    tile frames: 16 lines of 2 bytes
  ax_bspr     the bombers, coloured, by seat, frame and shift s (0-6 colour
              pixels into the group): 16 lines of 3 bytes, from byte 2g for
              s < 4 and 2g+1 after (screen = screen and keep, or data)
  ax_bkeep    their keep bytes, by frame and shift (the same for every seat)
  ax_movd     bomb and dud movers, by (dud, half-tile offset, phase): data
  ax_movk     and keep, 16 lines of 3 bytes from byte 2g
  ax_big      the winner's bombers, twice the size, by seat and frame (win0,
              win1): 32 lines of 4 bytes from 2g
  ax_bigk     and keep, by frame
  ax_logo     24 lines of 40 bytes
  ax_frame    the arena's frame: 4 wall-band lines and 4 side-wall lines of 40
  ax_font3/5  the fonts (gfxa.s reads them with RAMRD on)
A bomber's edge bytes take the floor's palette when the floor is most of
them (hgr.encode's under): green floor, not orange bars beside an orange
bomber (user, 2026-11-12). Its bytes' bit 7 always comes from the data.
In main memory (RODATA, read with RAMRD off): the colours' byte pairs
(colpat), the fragment tables (fexp0, fexp1: 7 colour pixels -> the dots of
byte 0 and 1) and the tiles' frames.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from artlayout import TILES, SPRITES  # noqa: E402
from png import Image  # noqa: E402
import hgr  # noqa: E402
from hgr import GREEN, BLACK, FROM_RGB  # noqa: E402
from drawart_hgr import SEAT_COLOURS, ROLES  # noqa: E402

ROOT = os.path.join(HERE, "..")
GFX = os.path.join(ROOT, "gfx", "hgr")
AUX_BASE = 0x0800           # src/apple2/fujiblst.cfg: AUXGFX
FLOOR = GREEN
SHIFTS = 7
WIN = ("win0", "win1")


def colours(img, x0, y0, w, h):
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            p = img.getpixel((x0 + x, y0 + y))[:3]
            if p not in FROM_RGB:
                sys.exit(f"{img}: not an HGR colour {p} at {x0 + x},{y0 + y}")
            row.append(FROM_RGB[p])
        rows.append(row)
    return rows


def tile_frames(img, i):
    fr = [colours(img, f * 7, i * 16, 7, 16) for f in range(4)]
    n = 4
    while n > 1 and all(all(c == FLOOR for c in r) for r in fr[n - 1]):
        n -= 1
    return fr[:n]


def tile_bytes(rows):
    out = []
    for row in rows:
        b = hgr.solid(row, 0)
        out += [b.get(0, 0), b.get(1, 0)]
    return out


def masked(row, x0, start, n, under):
    """A row of colour pixels (None: clear) at colour pixel x0 of a group:
    n (data, keep) bytes from byte start. Touched bytes always take bit 7
    from the data; untouched ones are kept whole."""
    enc = hgr.encode(row, x0, under)
    out = []
    for b in range(start, start + n):
        if b in enc:
            d, k = enc[b]
            out.append((d, k & 0x7F))
        else:
            out.append((0, 0xFF))
    for b in enc:
        if not start <= b < start + n:
            sys.exit(f"pixels outside bytes {start}..{start + n - 1} (x0 {x0})")
    return out


def sprite_roles(img, i):
    rows = []
    for y in range(16):
        row = []
        for x in range(7):
            p = img.getpixel((i * 7 + x, y))
            row.append(ROLES[p[:3]] if p[3] else ".")
        rows.append(row)
    return rows


def coloured(roles, seat):
    body, shade, visor = SEAT_COLOURS[seat]
    m = {"#": body, "+": shade, "v": visor, "k": BLACK, ".": None}
    return [[m[p] for p in row] for row in roles]


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
    return "\n".join("        .byte   " + ",".join(f"${b:02X}" for b in data[i:i + per])
                     for i in range(0, len(data), per))


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "gen", "apple2")
    os.makedirs(out_dir, exist_ok=True)
    tiles_img = Image(os.path.join(GFX, "tiles.png"))
    spr_img = Image(os.path.join(GFX, "sprites.png"))
    logo_img = Image(os.path.join(GFX, "logo.png"))
    wall_img = Image(os.path.join(GFX, "wall.png"))
    font3 = font(os.path.join(GFX, "font3.png"), 4)
    font5 = font(os.path.join(GFX, "font5.png"), 7)

    aux = []                    # (label, bytes, comment)

    # tiles
    tile_first, tile_n, tbytes = [], [], []
    for i, name in enumerate(TILES):
        frs = tile_frames(tiles_img, i)
        if name in ("fire", "bombup", "speed", "kick", "glove"):
            frs = frs[-1:]      # lit, and still: redrawing them costs the Apple dear
        elif name == "burn" or name.startswith("flame") or name == "trophy":
            frs = frs[:1]       # still too: only bombs and duds move
        tile_first.append(len(tbytes) // 32)
        tile_n.append(len(frs))
        for fr in frs:
            tbytes += tile_bytes(fr)
    aux.append(("ax_tiles", tbytes, f"{len(tbytes) // 32} tile frames, 16 lines of 2 bytes"))

    # bombers: by seat, frame, shift; keep by frame, shift
    roles = [sprite_roles(spr_img, i) for i in range(len(SPRITES))]
    bspr, bkeep = [], []
    for seat in range(4):
        for i in range(len(SPRITES)):
            rows = coloured(roles[i], seat)
            for s in range(SHIFTS):
                start = 1 if s >= 4 else 0
                for row in rows:
                    bspr += [d for d, k in masked(row, s, start, 3, FLOOR)]
    for i in range(len(SPRITES)):
        rows = coloured(roles[i], 0)
        for s in range(SHIFTS):
            start = 1 if s >= 4 else 0
            for row in rows:
                bkeep += [k for d, k in masked(row, s, start, 3, FLOOR)]
    aux.append(("ax_bspr", bspr, "bombers: seat, frame, shift: 16 lines of 3 bytes"))
    aux.append(("ax_bkeep", bkeep, "their keep bytes: frame, shift"))

    # movers: the bomb's and the dud's frames, at offsets 0 and 3 pixels
    movd, movk = [], []
    for name in ("bomb", "dud"):
        frs = tile_frames(tiles_img, TILES.index(name))
        frs = (frs * 4)[:4]
        for off in (0, 3):
            for fr in frs:
                for row in fr:
                    px = [None if c == FLOOR else c for c in row]
                    for d, k in masked(px, off, 0, 3, FLOOR):
                        movd.append(d)
                        movk.append(k)
    aux.append(("ax_movd", movd, "movers (dud, offset, phase): 16 lines of 3 bytes"))
    aux.append(("ax_movk", movk, "and their keep bytes"))

    # the winner, twice the size (results screen; whole groups only)
    big, bigk = [], []
    for seat in range(4):
        for name in WIN:
            for row in coloured(roles[SPRITES.index(name)], seat):
                row2 = [c for c in row for _ in (0, 1)]
                line = [d for d, k in masked(row2, 0, 0, 4, None)]
                big += line + line
    for name in WIN:
        for row in coloured(roles[SPRITES.index(name)], 0):
            row2 = [c for c in row for _ in (0, 1)]
            line = [k for d, k in masked(row2, 0, 0, 4, None)]
            bigk += line + line
    aux.append(("ax_big", big, "big bombers (seat, win0/win1): 32 lines of 4 bytes"))
    aux.append(("ax_bigk", bigk, "and their keep bytes, by frame"))

    # logo: 140 x 24
    lg = []
    for row in colours(logo_img, 0, 0, logo_img.width, 24):
        b = hgr.solid(row, 0)
        lg += [b.get(i, 0) for i in range(40)]
    aux.append(("ax_logo", lg, "24 lines of 40 bytes"))

    # the arena's frame: walls 4 px wide at px 24-27 and 119-122, 4 lines
    # tall across px 24-122; panels black
    wc = colours(wall_img, 0, 0, 4, 4)
    frame = []
    for kind in ("band", "side"):
        for y in range(4):
            row = [BLACK] * 140
            for x in range(24, 123):
                if kind == "band" or x < 28 or x > 118:
                    row[x] = wc[y][x % 4]
                else:
                    row[x] = FLOOR
            b = hgr.solid(row, 0)
            frame += [b.get(i, 0) for i in range(40)]
    aux.append(("ax_frame", frame, "4 band lines, 4 side lines of 40 bytes"))
    # the fonts, by line, then character (ASCII 32-95): gfxa.s's pack_text, pack_big
    aux.append(("ax_font3", [font3[c][l] for l in range(8) for c in range(64)], "3x7 font: line * 64 + c"))
    aux.append(("ax_font5", [font5[c][l] for l in range(8) for c in range(64)], "5x7 font"))

    # language card: colours as byte pairs, fragments to dots
    colpat = []
    for c in range(6):
        b = hgr.solid([c] * 7, 0)
        colpat += [b.get(0, 0), b.get(1, 0)]
    fexp0, fexp1 = [], []
    for f in range(128):
        d = 0
        for p in range(7):
            if f >> p & 1:
                d |= 3 << (2 * p)
        fexp0.append(d & 0x7F)
        fexp1.append(d >> 7)

    # ---- write ---------------------------------------------------------------
    s = ["; gfxdata.s -- made by tools/mkgfx_hgr.py from gfx/hgr/*.png; do not edit.",
         "        .export " + ", ".join("_" + lab for lab, _, _ in aux),
         "        .export _tile_first, _tile_n, _colpat, _fexp0, _fexp1",
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
    s += ["", '        .rodata',
          "; a group all in colour c: its 2 bytes (hg_fill, hg_frags)", "_colpat:",
          asm_bytes(colpat),
          "; 7 colour pixels (bit 0 left) -> the dots they light in a group's byte 0, byte 1",
          "_fexp0:", asm_bytes(fexp0), "_fexp1:", asm_bytes(fexp1)]
    s += ["", "        .rodata", "_tile_first:"]
    s.append(asm_bytes(tile_first))
    s.append("_tile_n:")
    s.append(asm_bytes(tile_n))
    open(os.path.join(out_dir, "gfxdata.s"), "w").write("\n".join(s) + "\n")

    h = ["/* gfxcodes.h -- made by tools/mkgfx_hgr.py; do not edit. */",
         "#ifndef GFXCODES_H", "#define GFXCODES_H", "#include <stdint.h>"]
    for i, name in enumerate(SPRITES):
        h.append(f"#define FR_{name.upper():<8} {i}")
    h.append(f"#define N_FRAMES {len(SPRITES)}")
    h.append(f"#define T_TROPHY {TILES.index('trophy')}")
    h.append(f"#define AUXGFX_SIZE {addr - AUX_BASE}")
    for lab, a, n in sizes:
        h.append(f"#define {lab.upper():<12} 0x{a:04X}u   /* {n} bytes */")
    h += ["extern const uint8_t tile_first[];         /* by tile: its first frame */",
          "extern const uint8_t tile_n[];             /* and how many */",
          "#endif"]
    open(os.path.join(out_dir, "gfxcodes.h"), "w").write("\n".join(h) + "\n")
    if addr > 0xC000:
        sys.exit(f"aux art runs to ${addr:04X}: past $BFFF")
    print(f"aux: {addr - AUX_BASE} bytes (${AUX_BASE:04X}-${addr - 1:04X}); "
          f"{len(tbytes) // 32} tile frames, {len(SPRITES)} sprite frames")


if __name__ == "__main__":
    main()

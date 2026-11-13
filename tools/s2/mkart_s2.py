#!/usr/bin/env python3
"""mkart_s2.py -- FujiBlast's Studio II pictures (gfx/studio2/art.txt) as
1802 data for the cart's protocol-2 graphics layer, and previews.

    mkart_s2.py gfx/studio2/art.txt OUTDIR [--preview DIR]

OUTDIR gets:
  art.inc      the image ids (IM_*), the frame numbers (FR_*): equates
  artdata.inc  ART_BOOT, every fixed picture as an IMAGE upload record
               (id, w, h, rows), $FF at the end; TILE_IMG / TILE_ANIM, the
               image of each T_* tile and of its other frame
  artfr.inc    SEAT_FR, four pages: seat s's frame f at page s, byte 16 f
               (15 rows: a blank one, then the 14-row figure); WIN_FR, the
               two cheering frames a seat, 16 bytes each
--preview DIR: sheet.png (every seat's frames) and arena.png (src/test.c's
canned state laid out as the arena draws it), a pixel 10 wide, a line 3
tall, as a TV shows it.
"""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
sys.path.insert(0, os.path.join(ROOT, "tools", "o2"))
from mkcanned import canned, proto  # noqa: E402

VIEWS = ("front", "back", "side")
DIRS = ("up", "right", "down", "left")          # DIR_UP .. DIR_LEFT
TILE_ORDER = [  # T_* -> (frame 0, frame 1)
    ("floor", None), ("pillar", None), ("block", None), ("burn0", "burn1"),
    ("bomb0", "bomb1"), ("dud", None), ("flameC", None), ("flameH", None),
    ("flameV", None), ("flameL", None), ("flameR", None), ("flameU", None),
    ("flameD", None), ("fire0", "fire1"), ("bombup0", "bombup1"), ("speed0", "speed1"),
    ("kick0", "kick1"), ("glove0", "glove1"),
]
ICONS = ["head0", "head1", "head2", "head3", "trophy"]
FR_NAMES = ["UP0", "UP1", "UP2", "RIGHT0", "RIGHT1", "RIGHT2", "DOWN0", "DOWN1", "DOWN2",
            "LEFT0", "LEFT1", "LEFT2", "DIE0", "DIE1", "DIE2", "DIE3"]


def parse(path):
    blocks, cur = [], None
    for raw in open(path):
        line = raw.rstrip("\n")
        if line.startswith("#") or not line.strip():
            continue
        if line.startswith("@"):
            cur = (line[1:].split(), [])
            blocks.append(cur)
        else:
            cur[1].append(line.strip())
    return blocks


def rows_of(lines, w):
    out = []
    for l in lines:
        if len(l) != w or set(l) - set("X."):
            raise SystemExit("mkart_s2: bad row %r (want %d of X/.)" % (l, w))
        out.append(l)
    return out


def bits(row):
    v = 0
    for i, ch in enumerate(row):
        if ch == "X":
            v |= 0x80 >> i
    return v


def mirror(rows):
    return [r[::-1] for r in rows]


def orrows(a, b):
    return ["".join("X" if (x == "X" or y == "X") else "." for x, y in zip(ra, rb)) for ra, rb in zip(a, b)]


def build(path):
    tiles, icons, gear, head, body, legs, frames = {}, {}, {}, {}, {}, {}, {}
    for hdr, lines in parse(path):
        kind = hdr[0]
        if kind == "tile":
            r = rows_of(lines, 4)
            if len(r) != 10:
                raise SystemExit("mkart_s2: tile %s has %d rows" % (hdr[1], len(r)))
            tiles[hdr[1]] = r
        elif kind == "icon":
            icons[hdr[1]] = rows_of(lines, int(hdr[2]))
        elif kind == "gear":
            gear[(int(hdr[1]), hdr[2])] = rows_of(lines, 4)
        elif kind == "head":
            head[hdr[1]] = rows_of(lines, 4)
        elif kind == "body":
            body[(int(hdr[1]), hdr[2])] = rows_of(lines, 4)
        elif kind == "legs":
            legs[(hdr[1], int(hdr[2]))] = rows_of(lines, 4)
        elif kind == "frame":
            g = None
            if lines and lines[0].startswith("gear"):
                g = int(lines[0].split()[1])
                lines = lines[1:]
            r = rows_of(lines, 4)
            if len(r) != 14:
                raise SystemExit("mkart_s2: frame %s has %d rows" % (hdr[2], len(r)))
            frames[hdr[2]] = (g, r)
        else:
            raise SystemExit("mkart_s2: no such block @%s" % kind)

    def figure(s, view, walk):
        """14 rows: gear over the head (rows 0-3), the body, the legs."""
        f = ["...."] * 2 + head[view] + body[(s, view)] + legs[(view, walk)]
        f[0:4] = orrows(f[0:4], gear[(s, view)])
        return f

    seats = []
    for s in range(4):
        fr = []
        for d in DIRS:
            for w in range(3):
                if d == "up":
                    f = figure(s, "back", w)
                elif d == "down":
                    f = figure(s, "front", w)
                else:
                    f = figure(s, "side", w)
                    if d == "left":
                        f = mirror(f)
                fr.append(f)
        for n in ("die0", "die1", "die2", "die3", "win0", "win1"):
            g, r = frames[n]
            r = list(r)
            if g is not None:
                r[g:g + 4] = orrows(r[g:g + 4], gear[(s, "front")])
            fr.append(r)
        seats.append(fr)
    return tiles, icons, seats


def logo_rows():
    """gfx/logo.png (9 letters, 12 px each) resampled to 6-px letters and a
    1-px gap: 63 x 24, then cut into 8 images 8 wide."""
    from PIL import Image
    im = Image.open(os.path.join(ROOT, "gfx", "logo.png")).convert("RGB")
    px = im.load()
    bg = px[0, 0]
    W, H = im.size
    src = [[px[x, y] != bg for x in range(W)] for y in range(H)]
    rows = []
    for y in range(H):
        row = ""
        for L in range(9):
            x0 = L * 12 + 1
            for j in range(6):
                a, b = x0 + j * 11 / 6, x0 + (j + 1) * 11 / 6
                cov = on = 0
                x = int(a)
                while x < b:
                    w = min(b, x + 1) - max(a, x)
                    cov += w
                    on += w * src[y][x]
                    x += 1
                row += "X" if on / cov >= 0.5 else "."
            row += "."
        rows.append(row + ".")
    return [[r[8 * k:8 * k + 8] for r in rows] for k in range(8)]


def main():
    args = sys.argv[1:]
    prev = None
    if "--preview" in args:
        i = args.index("--preview")
        prev = args[i + 1]
        del args[i:i + 2]
    src, outdir = args
    tiles, icons, seats = build(src)

    # image ids: the tiles, the seats' slots, the icons
    ids, uploads = {}, []
    n = 0
    for a, b in TILE_ORDER:
        for name in (a, b):
            if name and name not in ids:
                ids[name] = n
                uploads.append((n, 4, tiles[name]))
                n += 1
    seat_slot = n
    n += 4
    for name in ICONS:
        ids[name] = n
        uploads.append((n, len(icons[name][0]), icons[name]))
        n += 1
    for k, rows in enumerate(logo_rows()):
        ids["logo%d" % k] = n
        uploads.append((n, 8, rows))
        n += 1
    if n > 96:
        raise SystemExit("mkart_s2: %d images, the cart has 96" % n)

    inc = ["; GENERATED by tools/s2/mkart_s2.py from gfx/studio2/art.txt -- do not edit."]
    for name, i in ids.items():
        inc.append("IM_%-10s EQU %d" % (name.upper(), i))
    inc.append("IM_SEAT    EQU %d            ; 4: each seat's frame, uploaded as it changes" % seat_slot)
    inc.append("NIMAGES    EQU %d" % n)
    for i, f in enumerate(FR_NAMES):
        inc.append("FR_%-8s EQU %d" % (f, i))
    open(os.path.join(outdir, "art.inc"), "w").write("\n".join(inc) + "\n")

    dat = ["; GENERATED by tools/s2/mkart_s2.py -- do not edit.",
           "; ART_BOOT: IMAGE records (id, w, h, rows), $FF at the end"]
    size = 1
    for i, w, rows in uploads:
        size += 3 + len(rows)
    dat.append("        DFIT    %d" % size)
    dat.append("ART_BOOT:")
    for i, w, rows in uploads:
        dat.append("        DB      %d, %d, %d, %s" % (i, w, len(rows), ", ".join("%03XH" % bits(r) for r in rows)))
    dat.append("        DB      0FFH")
    dat.append("; TILE_IMG / TILE_ANIM: T_* -> its image, and its other frame's")
    dat.append("        PFIT    %d" % (2 * len(TILE_ORDER)))
    dat.append("TILE_IMG:")
    dat.append("        DB      " + ", ".join(str(ids[a]) for a, b in TILE_ORDER))
    dat.append("TILE_ANIM:")
    dat.append("        DB      " + ", ".join(str(ids[b or a]) for a, b in TILE_ORDER))
    open(os.path.join(outdir, "artdata.inc"), "w").write("\n".join(dat) + "\n")

    fr = ["; GENERATED by tools/s2/mkart_s2.py -- do not edit.",
          "; SEAT_FR: page s = seat s, byte 16 f = frame f (FR_*): a blank row, then 14",
          "SEAT_FR:"]
    for s in range(4):
        for f in range(16):
            rows = ["...."] + seats[s][f]
            fr.append("        DB      " + ", ".join("%03XH" % bits(r) for r in rows) + ", 0")
    fr.append("; WIN_FR: seat s's cheering frame k at 32 s + 16 k")
    fr.append("WIN_FR:")
    for s in range(4):
        for f in (16, 17):
            rows = ["...."] + seats[s][f]
            fr.append("        DB      " + ", ".join("%03XH" % bits(r) for r in rows) + ", 0")
    open(os.path.join(outdir, "artfr.inc"), "w").write("\n".join(fr) + "\n")

    if prev:
        os.makedirs(prev, exist_ok=True)
        preview(prev, tiles, icons, seats)
    print("mkart_s2: %d images (%d fixed, 4 seat slots), 4 x 18 frames" % (n, len(uploads)))


# ---- previews ------------------------------------------------------------------
class Canvas:
    def __init__(self, w=64, h=128):
        self.w, self.h = w, h
        self.px = [[0] * w for _ in range(h)]

    def put(self, x, y, rows, mode="set"):
        w = len(rows[0])
        for dy, r in enumerate(rows):
            yy = y + dy
            if not 0 <= yy < self.h:
                continue
            for dx in range(w):
                xx = x + dx
                if not 0 <= xx < self.w:
                    continue
                on = r[dx] == "X"
                if mode == "set":
                    self.px[yy][xx] = 1 if on else 0
                elif on:
                    self.px[yy][xx] = 1

    def save(self, path, sx=10, sy=3):
        from PIL import Image
        im = Image.new("L", (self.w * sx, self.h * sy))
        p = im.load()
        for y in range(self.h):
            for x in range(self.w):
                if self.px[y][x]:
                    for i in range(sx):
                        for j in range(sy):
                            p[x * sx + i, y * sy + j] = 255
        im.save(path)


FONT5 = {  # the cart's 3x5 font, enough for previews
    "0": ["XXX", "X.X", "X.X", "X.X", "XXX"], "1": [".X.", "XX.", ".X.", ".X.", "XXX"],
    "2": ["XXX", "..X", "XXX", "X..", "XXX"], "3": ["XXX", "..X", ".XX", "..X", "XXX"],
    "4": ["X.X", "X.X", "XXX", "..X", "..X"], "5": ["XXX", "X..", "XXX", "..X", "XXX"],
    "6": ["XXX", "X..", "XXX", "X.X", "XXX"], "7": ["XXX", "..X", "..X", "..X", "..X"],
    "8": ["XXX", "X.X", "XXX", "X.X", "XXX"], "9": ["XXX", "X.X", "XXX", "..X", "XXX"],
    ":": ["...", ".X.", "...", ".X.", "..."], " ": ["..."] * 5,
}


def text5(c, x, y, s):
    for i, ch in enumerate(s):
        c.put(x + 4 * i, y, [r + "." for r in FONT5[ch]])


def preview(d, tiles, icons, seats):
    # the sheet: each seat's 18 frames in a row, a dark line between
    c = Canvas(18 * 5, 4 * 17)
    for s in range(4):
        for f in range(18):
            c.put(f * 5, s * 17 + 1, seats[s][f])
    c.save(os.path.join(d, "sheet.png"))
    # the arena: the canned state, as play would draw it
    p = proto(os.path.join(ROOT, "src", "proto.h"))
    st = canned(p)
    names = [a for a, b in TILE_ORDER]
    c = Canvas()
    for y in range(10, 120):
        c.px[y][0] = c.px[y][53] = 1
    for t in range(p["TILES"]):
        r, col = divmod(t, p["ARENA_W"])
        c.put(1 + 4 * col, 10 + 10 * r, tiles[names[st[p["OFF_MAP"] + t]]])
    # bombers: the canned four, and one walking between tiles, and one
    # under a pillar's row (its head over the wall)
    spots = [(0, 0, 6), (48, 100, 6), (48, 0, 3), (0, 100, 9)]
    spots = [(4, 50, 6), (24, 30, 3), (40, 70, 9), (8, 100, 0)]
    for s, (x, y, f) in enumerate(spots):
        fr = ["...."] + seats[s][f]
        c.put(1 + x, 10 + y - 5, fr)
    # the score band: heads, wins, the clock; ours inverted
    xs = [0, 12, 44, 56]
    for s in range(4):
        c.put(xs[s], 0, icons["head%d" % s])
        text5(c, xs[s] + 4, 0, str(s))
    text5(c, 24, 0, "2:34")
    for y in range(5):
        for x in range(12, 20):
            c.px[y][x] ^= 1
    # the panel: power-ups and counts
    for k, (n, v) in enumerate((("fire0", "2"), ("bombup0", "1"), ("speed0", "1"),
                                ("kick0", None), ("glove0", None))):
        y = 12 + 22 * k
        c.put(55, y, tiles[n])
        if v:
            text5(c, 60, y + 3, v)
    c.save(os.path.join(d, "arena.png"))


if __name__ == "__main__":
    main()

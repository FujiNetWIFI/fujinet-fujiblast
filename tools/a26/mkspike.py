#!/usr/bin/env python3
"""mkspike.py -- the Atari 2600 arena spike's generated include files.

    mkspike.py OUTDIR [LOOK]

Writes OUTDIR/rows.inc (the unrolled pair code, cycle-annotated and checked)
and OUTDIR/gen.inc (the six PF tables, the row colours, the bomber jobs and
shapes), from a canned map shaped like src/test.c's scene 1.

The kernel (spike.asm) is M.U.L.E.'s map loop: an ENTRY is two scanlines (line
A updates player 0, line B player 1), the six playfield registers are
rewritten from six tables every line, and a tile row is 7 entries (14 lines)
plus the seam lines.  What differs is the tile: 13 columns of 3 playfield bits
(39 of the 40), and the colour of an entry comes from its row's two colours
CA (even entries) and CB (odd), loaded at the seam.

LOOK 1: static kinds (pillar, block) take the even entries' colour, dynamic
        kinds (bomb, flame, power-up) the odd entries' colour.  A row with no
        dynamic kind draws its static art on every entry.
LOOK 2: one colour per row -- the highest-priority kind present, every kind
        drawn on every entry (the cheap look; mixed rows mis-colour).
"""
import os
import sys

W, H = 13, 11
PAIRS = 7                      # entries a tile row
UNIT = 16

# tile kinds (server/internal/proto)
FLOOR, PILLAR, BLOCK, BURN, BOMB, DUD, FC, FH, FV, FL, FR, FU, FD, FIRE, BUP, SPD, KICK, GLOVE = range(18)
STATIC = {PILLAR, BLOCK}
FLAMES = {FC, FH, FV, FL, FR, FU, FD, BURN}
POWER = {FIRE, BUP, SPD, KICK, GLOVE}
BOMBS = {BOMB, DUD}

# colours (NTSC)
C_FLOOR = 0xC2
C_STATIC = 0x2A
C_FLAME = 0x1E
C_BOMB = 0x00
C_POWER = 0x9A


def canned():
    m = [[FLOOR] * W for _ in range(H)]
    for r in range(H):
        for c in range(W):
            if c & 1 and r & 1:
                m[r][c] = PILLAR
            elif 2 < c < 10 and (r in (0, 10) or c in (4, 8)):
                m[r][c] = BLOCK
    put = {(0, 2): BOMB, (0, 4): DUD, (12, 2): FIRE, (12, 4): BUP, (12, 6): SPD, (12, 8): KICK,
           (0, 6): GLOVE, (0, 8): BURN, (6, 5): FC, (5, 5): FH, (4, 5): BLOCK, (7, 5): FH,
           (8, 5): FR, (6, 4): FV, (6, 3): FU, (6, 6): FD}
    # (col,row) pairs above are (row, col) in mkcanned; keep that orientation
    for (r, c), k in put.items():
        if r < H and c < W:
            m[r][c] = k
    return m


# 3 bits wide, 7 entries tall, bit 2 = left.  STATIC art has a full version and
# an "even entries only" version (used when the row has a dynamic kind).
ART_FULL = {
    PILLAR: [0b000, 0b011, 0b111, 0b111, 0b111, 0b110, 0b000],
    BLOCK:  [0b000, 0b110, 0b101, 0b111, 0b101, 0b110, 0b000],
    BOMB:   [0b000, 0b010, 0b111, 0b111, 0b111, 0b010, 0b000],
    DUD:    [0b000, 0b010, 0b101, 0b111, 0b101, 0b010, 0b000],
    FC:     [0b000, 0b111, 0b111, 0b111, 0b111, 0b111, 0b000],
    FH:     [0b000, 0b000, 0b111, 0b111, 0b111, 0b000, 0b000],
    FV:     [0b000, 0b010, 0b010, 0b010, 0b010, 0b010, 0b000],
    FL:     [0b000, 0b000, 0b011, 0b111, 0b111, 0b000, 0b000],
    FR:     [0b000, 0b000, 0b110, 0b111, 0b111, 0b000, 0b000],
    FU:     [0b000, 0b010, 0b010, 0b111, 0b111, 0b010, 0b000],
    FD:     [0b000, 0b010, 0b111, 0b111, 0b010, 0b010, 0b000],
    BURN:   [0b000, 0b101, 0b010, 0b101, 0b010, 0b101, 0b000],
    FIRE:   [0b000, 0b010, 0b111, 0b010, 0b111, 0b010, 0b000],
    BUP:    [0b000, 0b111, 0b101, 0b111, 0b101, 0b111, 0b000],
    SPD:    [0b000, 0b001, 0b011, 0b111, 0b110, 0b100, 0b000],
    KICK:   [0b000, 0b100, 0b110, 0b111, 0b110, 0b100, 0b000],
    GLOVE:  [0b000, 0b111, 0b101, 0b111, 0b111, 0b111, 0b000],
}


def tile_bits(kind, p, look, dyn_row):
    """3 bits for kind at entry p, or 0 when it is not drawn there."""
    a = ART_FULL.get(kind)
    if a is None:
        return 0
    if look == 1 and dyn_row:
        if kind in STATIC:
            return a[p] if p % 2 == 0 else 0
        return a[p] if p % 2 == 1 else 0
    return a[p]


def row_colours(look, row):
    kinds = set(row)
    if look == 1:
        dyn = kinds & (FLAMES | BOMBS | POWER)
        cb = C_STATIC
        if dyn:
            cb = C_FLAME if kinds & FLAMES else (C_BOMB if kinds & BOMBS else C_POWER)
        return C_STATIC, cb, bool(dyn)
    if kinds & FLAMES:
        c = C_FLAME
    elif kinds & BOMBS:
        c = C_BOMB
    elif kinds & POWER:
        c = C_POWER
    else:
        c = C_STATIC
    return c, c, False


def build_tables(m, look):
    t = [bytearray(PAIRS * H) for _ in range(6)]
    ca, cb = bytearray(H), bytearray(H)
    for r in range(H):
        ca[r], cb[r], dyn = row_colours(look, m[r])
        for p in range(PAIRS):
            bits = 0                       # 40 bits, bit n = playfield pixel n
            for c in range(W):
                v = tile_bits(m[r][c], p, look, dyn)
                for k in range(3):         # k=0 leftmost of the three
                    if v & (4 >> k):
                        bits |= 1 << (c * 3 + k)
            # left half: pixels 0-19; PF0 draws bits 4-7 low-first (pixel 0 = bit 4),
            # PF1 bits 7-0 high-first, PF2 bits 0-7 low-first
            def pf0(px):  # 4 pixels
                return sum(((px >> i) & 1) << (4 + i) for i in range(4))
            def pf1(px):  # 8 pixels, first pixel = bit 7
                return sum(((px >> i) & 1) << (7 - i) for i in range(8))
            def pf2(px):
                return sum(((px >> i) & 1) << i for i in range(8))
            for half in range(2):
                b = (bits >> (20 * half)) & 0xFFFFF
                e = r * PAIRS + p
                t[half * 3 + 0][e] = pf0(b & 0xF)
                t[half * 3 + 1][e] = pf1((b >> 4) & 0xFF)
                t[half * 3 + 2][e] = pf2((b >> 12) & 0xFF)
    return t, ca, cb


# 8 wide x 7 tall bomber
SHAPE = [
    0b00111100,
    0b01111110,
    0b01011010,
    0b00111100,
    0b01111110,
    0b00100100,
    0b01100110,
]


def jobs_for(bombers):
    """bombers: [(x, y, colour)] world units. -> [(seamrow, xpix, col, shapetop, cnt)]."""
    out = []
    for x, y, col in bombers:
        etop = (y >> 4) * PAIRS + (((y & 15) * PAIRS) >> 4)
        row = etop // PAIRS
        xpix = (x * 12) // 16 + 2
        out.append((row, xpix, col, etop, etop + PAIRS - row * PAIRS))
    out.sort()
    return out


# ------------------------------------------------------------------ kernel
CYC = {"lda zp": 3, "lda abs,y": 4, "sta zp": 3, "ldx abs,y": 4, "stx zp": 3,
       "lda #": 2, "dcp zp": 5, "iny": 2, "lda (zp),y": 5, "jmp": 3}


def pair_code(p, nxt):
    """One pair: line A (player 0), line B (player 1)."""
    L = []
    # the even entries are always the static colour; the odd ones the row's CB
    a = [("sta WSYNC", None), ("lda #C_STATIC" if p % 2 == 0 else "lda CB", 2 if p % 2 == 0 else 3),
         ("sta COLUPF", 3)]
    for reg, tab in (("PF0", 0), ("PF1", 1), ("PF2", 2), ("PF0", 3), ("PF1", 4)):
        a += [("lda MT%d,y" % tab, 4), ("sta %s" % reg, 3)]
    a += [("ldx MT5,y", 4), ("lda #P0H-1", 2), ("dcp P0CNT", 5), ("stx PF2", 3),
          ("bcs P%dA1" % p, 2)]
    L += ["        IF (*&$FF) > $BC", "        ALIGN   256,$EA", "        ENDIF", "P%dA:" % p]
    cyc = 0
    for ins, c in a:
        L.append("        %-16s; %d" % (ins, cyc))
        cyc += c or 0
    L += ["        lda     #0              ; %d" % cyc,
          "        db      $2C             ; bit abs: skips the next lda",
          "P%dA1:   lda     (P0PTR),y" % p,
          "        IF (P%dA1>>8) <> ((P%dA1-3)>>8)" % (p, p),
          "        ERROR   \"P%dA's branch crosses a page\"" % p,
          "        ENDIF",
          "        sta     GRP0"]
    assert cyc + 2 + 4 + 3 <= 71, ("line A too long", p, cyc)
    b = [("sta WSYNC", None)]
    for reg, tab in (("PF0", 0), ("PF1", 1), ("PF2", 2)):
        b += [("lda MT%d,y" % tab, 4), ("sta %s" % reg, 3)]
    L += ["        IF (*&$FF) > $BC", "        jmp     P%dB" % p, "        ALIGN   256,$EA", "        ENDIF", "P%dB:" % p]
    cyc = 0
    for ins, c in b:
        L.append("        %-16s; %d" % (ins, cyc))
        cyc += c or 0
    L += ["        lda     #P1H-1          ; %d" % cyc,
          "        dcp     P1CNT",
          "        bcs     P%dB1" % p,
          "        lda     #0",
          "        db      $2C",
          "P%dB1:   lda     (P1PTR),y" % p,
          "        IF (P%dB1>>8) <> ((P%dB1-3)>>8)" % (p, p),
          "        ERROR   \"P%dB's branch crosses a page\"" % p,
          "        ENDIF",
          "        sta     GRP1",
          "        lda     MT3,y",
          "        sta     PF0",
          "        lda     MT4,y",
          "        sta     PF1",
          "        lda     MT5,y",
          "        sta     PF2",
          "        iny"]
    L.append("        jmp     %s" % nxt if nxt else "        jmp     SEAM0")
    return L


def kernel_asm():
    L = ["; GENERATED by tools/a26/mkspike.py -- do not edit."]
    for p in range(PAIRS):
        L += pair_code(p, "P%dA" % (p + 1) if p + 1 < PAIRS else None)
    return "\n".join(L) + "\n"


def data_asm(look):
    m = canned()
    t, ca, cb = build_tables(m, look)
    own = (4 * UNIT + 8, 5 * UNIT + 4)              # world units (x, y)
    others = [(10 * UNIT, 1 * UNIT), (2 * UNIT + 4, 6 * UNIT + 8), (8 * UNIT, 9 * UNIT)]
    cols = [0x4C, 0x9C, 0xCC]
    jobs = jobs_for([(x, y, c) for (x, y), c in zip(others, cols)])
    etop0 = (own[1] >> 4) * PAIRS + (((own[1] & 15) * PAIRS) >> 4)
    x0 = (own[0] * 12) // 16 + 2
    L = ["; GENERATED by tools/a26/mkspike.py (look %d) -- do not edit." % look]
    L += ["LOOK    EQU     %d" % look, "P0XPIX  EQU     %d" % x0, "P0TOPE  EQU     %d" % etop0]
    L += ["        ORG     $F800"]
    for n in range(6):
        L.append("        ORG     $F800+%d" % (n * 128))
        L.append("MT%d:" % n)
        for i in range(0, len(t[n]), 16):
            L.append("        db      " + ",".join("$%02X" % v for v in t[n][i:i + 16]))
    L += ["        ORG     $FB00", "RCA:"]
    L.append("        db      " + ",".join("$%02X" % v for v in ca))
    L += ["RCB:", "        db      " + ",".join("$%02X" % v for v in cb)]
    L += ["JROW:", "        db      " + ",".join(str(j[0]) for j in jobs) + ",255",
          "JX:", "        db      " + ",".join(str(j[1]) for j in jobs),
          "JCOL:", "        db      " + ",".join("$%02X" % j[2] for j in jobs),
          "JCNT:", "        db      " + ",".join(str(j[4]) for j in jobs),
          "JTOP:", "        db      " + ",".join(str(j[3]) for j in jobs)]
    L += ["        ORG     $FB80", "SHAPE:"]
    L.append("        db      " + ",".join("$%02X" % v for v in SHAPE))
    L += ["C_FLOOR  EQU     $%02X" % C_FLOOR]
    return "\n".join(L) + "\n"


def main():
    out = sys.argv[1]
    look = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, "rows.inc"), "w").write(kernel_asm())
    open(os.path.join(out, "gen.inc"), "w").write(data_asm(look))


if __name__ == "__main__":
    main()

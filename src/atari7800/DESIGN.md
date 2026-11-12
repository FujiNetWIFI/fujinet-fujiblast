# FujiBlast on the Atari 7800 (FujiNet cartridge) -- design

cc65 C + ca65, built from M.U.L.E.'s 7800 layer
(`~/Workspace/fujinet-multiplayer-mule/clients/mekkogx/src/atari7800`, read-only
reference; the notes in `~/.claude/memory/fujinet/fujinet-multiplayer-mule/atari7800-client.md`).
The neutral game (`src/*.c`) is unchanged. A `PLAT_` define is `__ATARI7800__`.
Look: `gfx/a78/preview/*.png` (approved), colours in `tools/drawart_a78.py`.
MARIA probe results: `tools/a78sim/probe160b` (all fits but 28-cell run + 2 panels + 4 bombers).

## Memory
- Cart image: SuperGame + RAM, 128K = 8 banks of 16K (kind 5, flags 1 claim as M.U.L.E.).
  Banks 0-6 appear at $8000-$BFFF: `$8000-$9FFF` REPL = the same 8K in every bank;
  `$A000-$BFFF` the bank's own. Bank 7 fixed at `$C000-$FF6F`. A store to `$8000` picks the bank.
- REPL = sprite pages (MARIA reads it in any bank), see below. It holds NO code.
- Cart RAM `$4000-$7FFF`:
  - `$4000-$5FFF`  region R (8K), by screen:
    - arena: four animation blocks A0 `$4000` A1 `$4800` A2 `$5000` A3 `$5800` (2K each)
    - menus/results: GFXB `$4000` (logo, trophies, menu wall), BIGB `$4800` (big text)
  - `$6000-$67FF` TXTB (the 320A text font, always resident)
  - `$6800-$6BE7` SCREEN: 25 rows x 40 bytes (the neutral code's screen, gfx.c)
  - `$6BE8-$7CFF` cc65 DATA/BSS
  - `$7D00-$7FFF` C stack (`$0300`)
- Console RAM (MARIA reads display lists in 2 cycles there): `$1800-$203F`, `$2200-$27FF`
  (`$2040-$20FF` and `$2140-$21FF` are zero-page/stack mirrors: never use). Zone display-list
  slots (2 x 64 B a zone), the DLL, the DLI tables.
- Zero page `$40-$FF`. Nothing ever stores to `$00-$1F` (INPTCTRL).

## Character blocks
A block is 8 pages (2K); CHARBASE = its first page; glyph line k (0 = top) of code c is at
`block + (7-k)*256 + c`. A "char" in a 1-byte-char zone (CW=0) is that one byte per line; in a
CW=1 zone it is two bytes (c, c+1) per line, so codes are EVEN (<=128 chars a block).
- TXTB (320A, 1 byte/char, 1 bit/pixel, MSB left): code 0-63 = ASCII 32-95 (Namco font,
  `gfx/namco.bin`); code 64-127 = the same inverted (byte ^ $FF). Pixel 1 = the object's colour 2.
- BIGB (160A, CW=1): code 2*i = ASCII 32+i (i 0-63), 8 px = 2 bytes a line, pixel value 2 where
  the Namco glyph has a bit (colour 2 of the object's palette).
- GFXB (160B, CW=1): menu cells: logo, trophy, menu wall; even codes, blank = code 0.
- A0-A3 (160B, CW=1): arena cells (4x8 px, 2 bytes a line). Code 0 = blank (floor, panel bg).
  Same code, same meaning in every set; animated cells differ by set (a tile with n frames
  shows frame `set mod n`). The frame NMI advances the set every 4 jiffies.
160B byte = pixel0: bits 7-6 colour, bits 3-2 palette in the group; pixel1: bits 5-4 colour,
bits 1-0 palette. Colour 0 = transparent (BACKGRND shows). Header palette 0-3 = group P0-P3.
160A byte = 4 pixels, 2 bits each, MSB left; colour n of palette p = P(p) C(n).

## Palettes (static per screen)
P0 = (d 05, w 0F, e 0A)   P1 = (n 26, o 2A, m 36)   P2 = (i 4C, p 5A, b 75)   P3 = (k 00, y 1E, l 9C)
(the 12 arena/logo colours; C2 of P0 is white and of P3 yellow: the text colours)
P4-P7 = seats 0-3: (C1 shade, C2 light, C3 light): 07/0F, 45/4C, 85/9C, 26/CC. C2 is the
text colour too (HUD names). BACKGRND per screen/zone: floor green C5 (arena), HUD grey 06,
menu blue 90, set by the DLI. PAL: hue +1 above $20 where the Atari port does (`hue()`).

## Sprites (the bombers)
Two 160A layers an 8x16 bomber, REPL pages: layer 0 = body (value 2 = light, 1 = shade; header
palette 4+seat), layer 1 = visor + black (1 = black, 2 = yellow; header palette 3). Layer 1 is
drawn after layer 0 (later objects overwrite). Object `o = frame*2 + layer` (frame 0-17 =
`FR_*`), 2 bytes at REPL column `2*o` (0-71), line L (0-15) of it at page index `22 - L`
(address `$8000 + 256*idx + col`); pages 0-6 and 23-29 are zero (the shift's pad, columns 0-71).
For a zone with top line t and a sprite at screen line y: `d = t - y`, show if `-8 < d < 16`,
`graph_adr = ((0x8F - d) << 8) | col`; zone line k reads page `hi + 7 - k` (OFFSET counts down).
Header (5 bytes, 160A): `lo, $40, hi, (pal<<5)|(32-2 &31), hpos`; width 2. Last objects in a
zone's list: after all tiles/text.

## Screen model (the neutral gfx.h, 25 rows of 8 lines = 200 lines, 160 px wide)
`SCREEN[row*40 + col]` = the char code (the pointer MARIA reads) for TEXT (TXT code), BIG
(`2*idx` for char idx 0-63, in cols 0-19; colour 0-3 of each in cols 20-39), GFX/ARENA (even cell codes,
cols 0-39 = 4-px cells). Row 0 of the arena screen is the HUD: SCREEN row 0 = TXT codes (40 cols,
packed by gfx_big_line from the 20 logical chars, see gfx.c), colours in `hud_pal[40]`
(0-3 = seat, 4 = white) -> object palette 4+seat or 0.
Row kinds: TEXT (320A CW0, CHARBASE `>TXTB`, CTRL `$4B`), BIG (160A CW1, `>BIGB`, `$50`),
GFX (160B, `>GFXB`, `$50`), ARENA (160B, `>A0 + 8*anim`, `$50`), HUD (as TEXT).
A DLI starts every zone whose kind differs from the zone above (`mt_tables`): CHARBASE, CTRL,
BACKGRND (and the late 160->320 switch as M.U.L.E. nmi.s).
Arena geometry follows the Atari: rows 1/24 wall, rows 2-23 the 13x11 tiles from cell col 7,
stats cols 1-5, RND cols 35-37; x px = `AR_COL*4 + (x>>1)`, y = `AR_ROW*8 + y`.
DL builder (maria.s `mt_compose`): per zone, back slot: text/cell runs of contiguous nonzero
(floor gaps >= 2 cells split a run), split at colour/palette changes, then sprite objects.
Cycle budget 426: trim the last run's right-hand cells before ever dropping a bomber.

## As built (2026-11-12)
- Files: `gfx.c` (the per-frame screen calls, fixed bank), `gfxsetup.c` (start-up, the DLI tables, the
  character blocks, screens made of rows: OVERLAY1, bank 0), `compose.s` (a zone's display list),
  `nmi.s` (the DLIs, the frame's DLI, the clock, the zone flips), `bank.c` (the only stores to
  `$8000`; `bank_copy` runs from the fixed bank), `art.s` (the art images, REPL), `nstrans.c`,
  `input.c`, `sound.c` + `snd.s`, `crt0.s`, `fujiblst-a78.cfg`.
- The DLIs' ordinals must stay in step with the zones: `gfxsetup.c`'s `wait_raw()` waits for a real frame
  before installing tables (plat.c's `frame_wait` may return at once when the game is behind).
- A blank TEXT row becomes a 160-mode row (`tables_build`): a bomber on 320A would show as bits.
- Bank 0 is full: a test build (`TEST_SCENE=n`) leaves out the menus (`NO_MENUS`) and the lib's
  appkeys (`NO_APPKEYS`); a production build leaves out `test.c` (`PLAT_NO_TEST`).
- The results screen draws the big bomber at the normal size; the title's fourth bomber walks off
  screen at times (x up to 167, as on the Atari).

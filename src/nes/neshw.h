/*
 * neshw.h -- the NES as FujiBlast drives it (src/nes/).
 *
 * The cart is the FujiNet NES cartridge (fujinet-firmware pico/nes)
 * running the image as an MMC3 board (mapper 4): 128K PRG, 16K CHR-RAM, 8K
 * WRAM at $6000 (src/nes/fujiblst-nes.cfg).
 *
 *   PRG  $8000-$9FFF  R6: the art (bank 0, left there)
 *        $A000-$BFFF  R7: more of the program, never switched
 *        $C000-$FFFF  the program (fixed)
 *   CHR  $0000-$0FFF  the background's 256 cells (a screen's set, loaded
 *                     with the display off)
 *        $1000-$1FFF  the sprites' 8 x 16 pairs
 *
 * The screen is a name table of 8 x 8 cells; a 16 x 16 tile is 4 cells
 * and one attribute block (one of 4 palettes). Drawing goes through a queue
 * the NMI drains in the vertical blank (src/nes/crt0.s).
 */
#ifndef NESHW_H
#define NESHW_H

#include <stdint.h>

#define PPU_CTRL    (*(volatile uint8_t *)0x2000)
#define PPU_MASK    (*(volatile uint8_t *)0x2001)
#define PPU_STATUS  (*(volatile uint8_t *)0x2002)
#define PPU_ADDR    (*(volatile uint8_t *)0x2006)
#define PPU_DATA    (*(volatile uint8_t *)0x2007)
#define JOY1        (*(volatile uint8_t *)0x4016)

/* MMC3 */
#define MMC3_SEL    (*(volatile uint8_t *)0x8000)
#define MMC3_BANK   (*(volatile uint8_t *)0x8001)

extern uint8_t mmc3_sel;
#pragma zpsym ("mmc3_sel")
void __fastcall__ prg_r6(uint8_t bank);

/* ---- the PPU queue (crt0.s drains it in the blank) -------------------
 * A command: n (1-127; or 0x80 | n for a fill), the address (high, low),
 * then n bytes (a fill: one byte, written n times). */
extern uint8_t ppuq[256];
extern uint8_t qhead, qtail;
#pragma zpsym ("qhead")
#pragma zpsym ("qtail")
extern volatile uint8_t ppu_direct;     /* 1: rendering off, main drains */
#pragma zpsym ("ppu_direct")
extern volatile uint8_t ppu_off;        /* set by the NMI that turned it off */
#pragma zpsym ("ppu_off")
extern volatile uint8_t oam_lock;       /* 1: no OAM DMA (the game is writing it) */
#pragma zpsym ("oam_lock")
extern uint8_t bd_top, bd_bot, split_on;    /* the backdrop split (crt0.s) */
#pragma zpsym ("bd_top")
#pragma zpsym ("bd_bot")
#pragma zpsym ("split_on")
void __fastcall__ ppu_drain(uint8_t budget);    /* 255 = all */
extern uint8_t oam[256];

#endif

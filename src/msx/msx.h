/*
 * msx.h -- the MSX (MSX1 and up) under a FujiNet cartridge: the BIOS calls
 * the game makes (msxbios.asm), the frame clock (msxint.asm) and the
 * cartridge's registers.
 *
 * The clock is the BIOS's interrupt, through the H.TIMI hook: it counts
 * 60ths of a second (a 50 Hz machine counts every fifth twice) and steps
 * the sound. The VDP routines are src/tms9918's; the interrupt can be held
 * off, so there is nothing like the Adam's vdp_busy.
 */
#ifndef MSX_H
#define MSX_H

#include <stdint.h>
#include "vdp.h"

/* msxint.asm */
void msx_hook_init(void);
void msx_quit(void);            /* back to the FujiNet's CONFIG */

/* msxbios.asm (one argument in L: __z88dk_fastcall) */
uint8_t msx_snsmat(uint8_t row) __z88dk_fastcall;
uint8_t msx_gtstck(uint8_t stick) __z88dk_fastcall;
uint8_t msx_gttrig(uint8_t trig) __z88dk_fastcall;
uint8_t msx_chsns(void);
uint8_t msx_chget(void);

/* sound.c: a frame of the sound (from the interrupt) */
void snd_nmi(void);

#define FB_GETC   (*(volatile uint8_t *)0xBFFC)
#define FB_STATUS (*(volatile uint8_t *)0xBFFD)
#define FB_PUTC   (*(volatile uint8_t *)0xBFFE)

uint8_t plat_net_probe(void);

#endif

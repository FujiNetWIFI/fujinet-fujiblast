/*
 * vdp.h -- the TMS9918A, as the Adam and the MSX both have it (the shared
 * gfx.c and vdp.asm).
 *
 *   Adam  ports BEh (data) and BFh (control). The VDP's interrupt is the
 *         Z80's NMI, the frame clock (src/adam/nmi.asm). It reads the
 *         VDP's status, which also resets the address latch, so it must not
 *         land between the two bytes of an address or inside a burst of
 *         data: while the game is at the VDP (vdp_busy) it leaves the
 *         status alone and says so in vdp_pend, and the unlock takes the
 *         frame then.
 *   MSX   ports 98h and 99h. The BIOS's interrupt (H.TIMI, the frame
 *         clock: src/msx/msxint.asm) reads the status too, but can be
 *         masked: the lock is DI, and the writers keep interrupts off for
 *         a whole burst (with them on, bytes were lost).
 *
 * In Graphics II the CPU may touch the VDP only every 8 us while the
 * screen is drawn (29 T-states): the writers here are slower than that,
 * except va_writef, which is for the frame's blank and a blanked display.
 */
#ifndef VDP_H
#define VDP_H

#include <stdint.h>

#ifdef __ADAM__
__sfr __at 0xBE VDP_DATA;
__sfr __at 0xBF VDP_CTRL;
#define VDP_PORT_DATA  0xBE
#define VDP_PORT_CTRL  0xBF
extern volatile uint8_t vdp_busy, vdp_pend;
void vdp_catch_up(void);
#define VDP_LOCK()   (vdp_busy = 1)
#define VDP_UNLOCK() do { vdp_busy = 0; if (vdp_pend) vdp_catch_up(); } while (0)
#else
__sfr __at 0x98 VDP_DATA;
__sfr __at 0x99 VDP_CTRL;
#define VDP_LOCK()   __asm__("di")
#define VDP_UNLOCK() __asm__("ei")
#endif

/* The MSX ROM keeps the art in two banks of an ASCII8 mapper's window
 * (A000h; the bank register is at 7800h): ART_BANK(ART_A) shows the arena's
 * cells, ART_BANK(ART_B) the logo, trophies, sprites and font. The Adam
 * has them all in memory. */
#ifdef __ADAM__
#define ART_BANK(n) ((void)0)
#else
#define ART_BANK(n) (*(volatile uint8_t *)0x7800 = (n))
#endif
#define ART_A 3
#define ART_B 4

extern uint8_t vdp_seen;        /* status bits seen by the frame clock */

/* VRAM (Graphics II: three thirds, each with its own 256 patterns and colours) */
#define VR_PAT   0x0000         /* thirds at +0, +800h, +1000h; 8 bytes a character */
#define VR_NAME  0x1800         /* 32 x 24 */
#define VR_SAT   0x1B00
#define VR_COL   0x2000         /* laid out as the patterns */
#define VR_SPAT  0x3800         /* sprite patterns: 16 x 16, 32 bytes each */

/* vdp.asm: the arguments are these variables */
extern uint16_t va_addr, va_len;
extern const uint8_t *va_src;
extern uint8_t va_val;
void va_write(void);            /* va_len bytes from va_src to VRAM va_addr */
void va_writef(void);           /* the same, flat out: blank or blanked display only */
void va_fill(void);             /* va_len bytes of va_val */

/* gfx.c: set by frame_wait: this frame is just past the VDP interrupt, so
 * the blank is ours to write in flat out */
extern uint8_t gfx_fast;
/* The animated tiles redrawn a frame when the frame's blank isn't ours to write
 * flat out (the MSX never has it: its interrupt's latency is too long to tell) */
#ifdef __MSX__
#define ANIM_SLOW 1
#else
#define ANIM_SLOW 1
#endif

#endif

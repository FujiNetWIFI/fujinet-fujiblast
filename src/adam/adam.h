/*
 * adam.h -- the Coleco Adam: the TMS9918A, the NMI that counts frames, and
 * the few routines written in assembler (vdp.asm, eoscall.asm).
 *
 * The VDP's interrupt is the Z80's NMI (nmi.asm) and can't be masked. It
 * reads the VDP's status, which also resets the address latch, so it must
 * not land between the two bytes of an address or inside a burst of data:
 * while the game is at the VDP (vdp_busy) it leaves the status alone and
 * says so in vdp_pend, and the unlock takes the frame then.
 *
 * In Graphics II the CPU may touch the VDP only every 8 us while the
 * screen is drawn (29 T-states): the writers here are slower than that,
 * except va_writef, which is for the frame's blank and a blanked display.
 */
#ifndef ADAM_H
#define ADAM_H

#include <stdint.h>

__sfr __at 0xBE VDP_DATA;
__sfr __at 0xBF VDP_CTRL;

extern volatile uint8_t vdp_busy, vdp_pend;
extern uint8_t vdp_seen;        /* status bits seen by the frame clock */
void vdp_catch_up(void);
#define VDP_LOCK()   (vdp_busy = 1)
#define VDP_UNLOCK() do { vdp_busy = 0; if (vdp_pend) vdp_catch_up(); } while (0)

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

/* eoscall.asm */
extern uint8_t joy_ctl[4];      /* controller 1: stick, left fire, right fire, keypad code */
void eos_kbd_start(void);       /* ask the keyboard for a character */
uint8_t eos_kbd_end(void);      /* 0 none yet, 1 failed (ask again), else the character */
void joy_read(void);            /* controller 1 into joy_ctl, read from its port */

/* gfx.c: set by frame_wait: this frame is just past the VDP interrupt, so
 * the blank is ours to write in flat out */
extern uint8_t gfx_fast;

/* sound.c: a frame of the sound (from the NMI) */
void snd_nmi(void);

uint8_t plat_net_probe(void);

#endif

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

#include "vdp.h"

/* eoscall.asm */
extern uint8_t joy_ctl[4];      /* controller 1: stick, left fire, right fire, keypad code */
void eos_kbd_start(void);       /* ask the keyboard for a character */
uint8_t eos_kbd_end(void);      /* 0 none yet, 1 failed (ask again), else the character */
void joy_read(void);            /* controller 1 into joy_ctl, read from its port */

/* sound.c: a frame of the sound (from the NMI) */
void snd_nmi(void);

uint8_t plat_net_probe(void);

#endif

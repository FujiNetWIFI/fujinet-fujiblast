/*
 * plat.c -- the MSX (MSX1 and up, a FujiNet cartridge): start-up, the
 * 60 Hz clock and quitting (the Adam's src/adam/plat.c, with the BIOS's
 * interrupt for the clock: msxint.asm).
 */
#ifdef __MSX__

#include "plat.h"
#include "gfx.h"
#include "input.h"
#include "sound.h"
#include "msx.h"

uint16_t frame_count;
uint8_t vdp_seen, gfx_fast;
uint16_t msx_passes, msx_late, msx_skip;        /* for a profile: frame_wait calls; drawn late; made up */
static uint16_t seed = 0x1234;

void plat_init(void)
{
    msx_hook_init();
    snd_init();
    input_init();
    gfx_init();
}

/* Back to the FujiNet's CONFIG. */
void plat_quit(void)
{
    msx_quit();
}

/* The FujiNet's status register shows it has a user ROM ready (us); where
 * there is no FujiNet the ROM itself is there, and its last bytes are 0. */
uint8_t plat_net_probe(void)
{
    return (FB_STATUS & 0x40) != 0;
}

/* One pass of the game is a jiffy of logic (the walking, the timers), so a
 * pass that comes after the next frame has already come is made up by one
 * that doesn't wait or draw (frame_count - seen says how far behind): the
 * game keeps real time when the drawing is slower than 60 a second.
 * The writes always go slowly (50 T-states a byte, safe in the display
 * too): the Adam's NMI comes the moment the blank starts, so a pass that
 * waited for it may write flat out, but here the BIOS's interrupt has its
 * own work to do first and the same writes lost bytes (a shifted sprite
 * table) when the blank was over before they were done. The pass is
 * called right after the interrupt all the same, and slow writes are
 * done in the blank, which is 15000 T-states long. */
#define CATCH_UP 20             /* frames behind we make up; further, we let go */

static uint16_t now(void)
{
    uint16_t n;
    __asm__("di");              /* the interrupt changes it too */
    n = frame_count;
    __asm__("ei");
    return n;
}

void frame_wait(void)
{
    static uint16_t seen;
    uint16_t d = now() - seen;
    ++msx_passes;
    if (d > CATCH_UP) {
        seen = now() - 1;
        d = 1;
    }
    if (d >= 2) {
        ++seen;                 /* behind: a pass with no waiting, no drawing */
        ++msx_skip;
        return;
    }
    gfx_fast = 0;               /* (never: see below) */
    msx_late += d;
    while (*(volatile uint16_t *)&frame_count == seen)
        ;
    seen = now();
    seed += seen;
    spr_flush();
}

uint8_t plat_rand(void)
{
    /* xorshift, stirred by the frame count */
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return (uint8_t)seed;
}

#endif

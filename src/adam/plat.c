/*
 * plat.c -- the Coleco Adam: start-up, the 60 Hz clock and quitting (from
 * M.U.L.E.'s src/adam/plat.c).
 *
 * The clock is the VDP's frame interrupt, the Z80's NMI (nmi.asm): it
 * counts on while EOS waits for AdamNet, so a network call loses no time.
 */
#ifdef __ADAM__

#include "plat.h"
#include "gfx.h"
#include "input.h"
#include "sound.h"
#include "adam.h"
#include "fujinet-fuji.h"

uint16_t frame_count;
volatile uint8_t vdp_busy, vdp_pend;
uint8_t vdp_seen, gfx_fast;
uint16_t adam_passes, adam_late, adam_skip;     /* for a profile: frame_wait calls; drawn late; made up */
static uint16_t seed = 0x1234;

/* The NMI came while the VDP was busy: take it now (with the NMI held off
 * again meanwhile: it steps the sound, which nothing may interrupt). */
void vdp_catch_up(void)
{
    uint8_t s;
    vdp_busy = 1;
    do {
        vdp_pend = 0;
        s = VDP_CTRL;               /* (the NMI may have beaten us to it) */
        if (s & 0x80) {
            vdp_seen |= s;
            ++frame_count;
            snd_nmi();
        }
    } while (vdp_pend);
    vdp_busy = 0;
}

void plat_init(void)
{
    snd_init();
    input_init();
    gfx_init();
}

/* Back to the FujiNet's CONFIG, as the Adam's reset switch would. */
void plat_quit(void)
{
    fuji_set_boot_config(1);
    __asm
        di
        xor     a
        out     (0x7F),a        ; SmartWriter ROM and the lower 32K of RAM
        jp      0
    __endasm;
}

/* The DCB table EOS keeps (FEC3h: how many; FEC4h: 21 bytes each). The
 * FujiNet answers as device 0Fh. */
uint8_t plat_net_probe(void)
{
    uint8_t n = *(uint8_t *)0xFEC3, i;
    const uint8_t *d = (const uint8_t *)0xFEC4;
    for (i = 0; i < n; ++i, d += 21)
        if ((d[16] & 0x0F) == 0x0F)
            return 1;
    return 0;
}

/* One pass of the game is a jiffy of logic (the walking, the timers), so a
 * pass that comes after the next frame has already come is made up by one
 * that doesn't wait or draw (frame_count - seen says how far behind): the
 * game keeps real time when the drawing is slower than 60 a second.
 * Called as the frame comes, the blank is ours to draw in (sprites,
 * animation) flat out; called late in the frame, the writes go slowly, in
 * the display. */
#define CATCH_UP 20             /* frames behind we make up; further, we let go */

void frame_wait(void)
{
    static uint16_t seen;
    uint16_t d = *(volatile uint16_t *)&frame_count - seen;
    ++adam_passes;
    if (d > CATCH_UP) {
        seen = frame_count - 1;
        d = 1;
    }
    if (d >= 2) {
        ++seen;                 /* behind: a pass with no waiting, no drawing */
        ++adam_skip;
        return;
    }
    gfx_fast = d == 0;
    adam_late += d;
    while (*(volatile uint16_t *)&frame_count == seen)
        ;
    seen = frame_count;
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

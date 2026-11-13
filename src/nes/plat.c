/*
 * plat.c -- the NES: start-up, the 60 Hz clock and quitting (from the
 * Adam's src/adam/plat.c and M.U.L.E.'s src/nes/plat.c).
 *
 * The clock is the NMI's frame count (crt0.s): it counts on while a network
 * call waits. One pass of the game is a jiffy of logic, so a pass that
 * comes after the next frame has already come is made up by one that
 * neither waits nor draws: the game keeps real time when the drawing is
 * slower than 60 a second.
 */
#ifdef __NES__

#include "plat.h"
#include "gfx.h"
#include "input.h"
#include "sound.h"
#include "nstrans.h"
#include "neshw.h"

#define CATCH_UP 20             /* frames behind we make up; further, we let go */

void snd_step(void);            /* sound.c: a frame of the tunes */

uint16_t nes_passes, nes_skip;      /* for a profile: frame_wait calls; made up without drawing */
static uint16_t seed = 0x1234;

void plat_init(void)
{
    snd_init();
    input_init();
    gfx_init();
}

/* A console has nowhere to go back to (Reset doesn't reset the cartridge:
 * CONFIG comes back at power-on): say goodbye to the server, then stop. */
void plat_quit(void)
{
    nst_close();
    snd_stop();
    gfx_begin();
    gfx_row(10, ROW_BIG, PAL_MENU);
    gfx_text(10, 0, "GOODBYE!", 0);
    gfx_text(13, 6, "YOU CAN TURN THE NES OFF", 0);
    gfx_show();
    for (;;)
        frame_wait();
}

void frame_wait(void)
{
    static uint16_t seen;
    uint16_t d = *(volatile uint16_t *)&frame_count - seen;
    ++nes_passes;
    if (d > CATCH_UP) {
        seen = frame_count - 1;
        d = 1;
    }
    if (d >= 2) {
        ++seen;                 /* behind: a pass with no waiting, no drawing */
        ++nes_skip;
        snd_step();
        return;
    }
    while (*(volatile uint16_t *)&frame_count == seen)
        ;
    seen = frame_count;
    seed += seen;
    snd_step();
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

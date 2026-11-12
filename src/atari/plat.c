/*
 * plat.c -- start-up, the 60 Hz clock and quitting (from M.U.L.E.).
 *
 * The clock is RTCLOK, which the OS counts in the vertical blank even while
 * SIO is busy. On a PAL machine (50 Hz) every fifth blank counts twice, so
 * the game's 60 jiffies a second hold.
 *
 * The game's loop is one pass a jiffy, and a pass with four players and the
 * link can take two or three. When frame_wait finds the game behind the
 * clock it returns at once: that pass is logic only (walking, timers), so
 * our player walks at 60 steps a second as the server's bots do. Every
 * MAX_SKIP passes it draws all the same.
 */
#ifdef __ATARI__

#include "plat.h"
#include "gfx.h"
#include "input.h"
#include "sound.h"
#include "nstrans.h"

#define RTCLOK (*(volatile uint8_t *)0x14)
#define PAL    (*(volatile uint8_t *)0xD014)
#define RANDOM (*(volatile uint8_t *)0xD20A)
#define SOUNDR (*(volatile uint8_t *)0x41)

#define CATCH_UP 30             /* jiffies behind: past this (a stall), let them go */
#define MAX_SKIP 4              /* passes without drawing, at most, when behind */

uint16_t frame_count;
static uint8_t last_rt, pal_n, is_pal;

void plat_init(void)
{
    is_pal = (PAL & 0x0E) == 0;         /* GTIA: 1 on NTSC, 0 on PAL */
    SOUNDR = 0;                         /* SIO quiet: no beeps over the game */
    last_rt = RTCLOK;
    snd_init();
    input_init();
    gfx_init();
}

void plat_quit(void)
{
    nst_close();
    asm("jmp $E477");                   /* COLDSV */
}

static void catch_up(void)
{
    uint8_t now = RTCLOK, d = now - last_rt;
    last_rt = now;
    while (d--) {
        ++frame_count;
        if (is_pal && ++pal_n == 5) {
            pal_n = 0;
            ++frame_count;
        }
    }
}

void frame_wait(void)
{
    static uint16_t seen;
    static uint8_t skipped;     /* passes since the last drawn */
    uint16_t d;
    catch_up();
    d = frame_count - seen;
    if (d > CATCH_UP) {         /* a stall (SIO, an appkey): let it go */
        seen = frame_count - 1;
        d = 1;
    }
    if (d >= 2 && skipped < MAX_SKIP) {
        ++skipped;
        ++seen;                 /* behind: a pass with no drawing */
        return;
    }
    skipped = 0;
    spr_flush();
    while (frame_count == seen)
        catch_up();
    ++seen;
}

uint8_t plat_rand(void)
{
    return RANDOM;
}

#endif

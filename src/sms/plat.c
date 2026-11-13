/*
 * plat.c -- the SMS (a FujiNet cartridge): start-up, the 60 Hz clock and
 * quitting (M.U.L.E.'s src/sms/plat.c and the Adam's frame_wait).
 *
 * The clock is the VDP's frame interrupt (smsint.asm), counting 60ths of a
 * second on a 50 Hz console too. One pass of the game is a jiffy of logic
 * (the walking, the timers), so a pass that comes after the next frame has
 * already come is made up by one that neither waits nor draws: the game
 * keeps real time when the drawing is slower than 60 a second.
 */
#ifdef __SMS__

#include "plat.h"
#include "gfx.h"
#include "input.h"
#include "sound.h"
#include "nstrans.h"
#include "fujinet-sms.h"

#define CATCH_UP 20             /* frames behind we make up; further, we let go */

uint16_t frame_count;
uint8_t vdp_seen;
uint16_t sms_passes, sms_skip;  /* for a profile: frame_wait calls; made up without drawing */
extern uint8_t sms_pal;
static uint16_t seed = 0x1234;

void sms_int_init(void);
uint8_t sms_pal_probe(void);

void plat_init(void)
{
    sms_int_init();
    snd_init();
    input_init();
    gfx_init();
    sms_pal = sms_pal_probe();
}

/* Back to the cartridge's CONFIG (its loader page, out of the way of
 * slot 2's bank), having said goodbye to the server. Without a cartridge
 * there is nowhere to go. */
void plat_quit(void)
{
    nst_close();
    snd_stop();
    if (fuji_sms_present())
        fuji_sms_exit_to_config();
    gfx_begin();
    gfx_row(10, ROW_BIG, PAL_MENU);
    gfx_text(10, 0, "GOODBYE!", 0);
    gfx_text(13, 6, "YOU CAN TURN THE SMS OFF", 0);
    gfx_show();
    for (;;)
        frame_wait();
}

void frame_wait(void)
{
    static uint16_t seen;
    uint16_t d = *(volatile uint16_t *)&frame_count - seen;
    ++sms_passes;
    if (d > CATCH_UP) {
        seen = frame_count - 1;
        d = 1;
    }
    if (d >= 2) {
        ++seen;                 /* behind: a pass with no waiting, no drawing */
        ++sms_skip;
        return;
    }
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

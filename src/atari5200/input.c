/*
 * input.c -- controller 1 on the 5200: the analog stick, its two fire
 * buttons and the keypad (from M.U.L.E.'s src/atari5200/input.c).
 *
 *   stick          walk / choose
 *   either fire    the button
 *   START          RETURN: start, finish
 *   *              ESC: leave the table (at the title, quit to CONFIG)
 *   # or PAUSE     M: sound off and on
 *
 * The stick is the crt0's VBI's copy of the pots (PADDL0/1, 114 at rest),
 * read through the game ports' dead zone; the top button is GTIA's TRIG0,
 * the lower one SKSTAT bit 3 (clear while held, for the controller CONSOL
 * picks). The keypad comes from the crt0's keypad IRQ, which latches one
 * press (its code + 1) in fuji_a5200_key.
 */
#ifdef __ATARI5200__

#include <stdint.h>
#include "input.h"

#define PADDL0  (*(volatile uint8_t *)0x11)
#define PADDL1  (*(volatile uint8_t *)0x12)
#define TRIG0   (*(volatile uint8_t *)0xC010)
#define SKSTAT  (*(volatile uint8_t *)0xE80F)

#define STICK_LO 0x40
#define STICK_HI 0xA4

/* fuji_a5200_key: 0-9 + 1, then '*', '#', START, PAUSE, RESET */
#define K_STAR   11
#define K_HASH   12
#define K_START  13
#define K_PAUSE  14

extern volatile uint8_t fuji_a5200_key;     /* crt0.s */

uint8_t inp_dir, inp_btn, ev_btn, ev_key;
uint8_t inp_hook, inp_hook_dir, inp_hook_btn, inp_hook_key;

static uint8_t btn_prev, btn_latch, key_latch;

void input_init(void)
{
    fuji_a5200_key = 0;
}

void input_poll(void)
{
    uint8_t dir = 0, btn, k, x = PADDL0, y = PADDL1;

    if (y < STICK_LO)
        dir |= IN_UP;
    else if (y > STICK_HI)
        dir |= IN_DOWN;
    if (x < STICK_LO)
        dir |= IN_LEFT;
    else if (x > STICK_HI)
        dir |= IN_RIGHT;
    btn = TRIG0 == 0 || (SKSTAT & 0x08) == 0;

    k = fuji_a5200_key;
    if (k) {
        fuji_a5200_key = 0;
        if (k == K_START)
            key_latch = KEY_RETURN;
        else if (k == K_STAR)
            key_latch = KEY_ESC;
        else if (k == K_HASH || k == K_PAUSE)
            key_latch = 'M';
    }

    if (inp_hook) {
        dir = inp_hook_dir;
        btn = inp_hook_btn;
        if (inp_hook_key) {
            key_latch = inp_hook_key;
            inp_hook_key = 0;
        }
    }
    inp_dir = dir;
    inp_btn = btn;
    if (btn && !btn_prev)
        btn_latch = 1;
    btn_prev = btn;
}

void input_take(void)
{
    ev_btn = btn_latch;
    btn_latch = 0;
    ev_key = key_latch;
    key_latch = KEY_NONE;
}

#endif

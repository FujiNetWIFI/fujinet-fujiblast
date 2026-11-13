/*
 * input.c -- the NES controller (port 1), from M.U.L.E.'s src/nes/input.c.
 *
 *   pad        walk / choose
 *   A          the button (fire; again on your own bomb: throw)
 *   B          rub out (name entry)
 *   START      RETURN: start, finish
 *   SELECT     ESC: leave the table (at the title: quit)
 *   START + SELECT   M: sound off and on
 *
 * The pad is read a bit at a time after a strobe: A, B, SELECT, START, up,
 * down, left, right.
 */
#ifdef __NES__

#include <stdint.h>
#include "input.h"
#include "neshw.h"

uint8_t inp_dir, inp_btn, ev_btn, ev_key;
uint8_t inp_hook, inp_hook_dir, inp_hook_btn, inp_hook_key;

#define J_A      0x80
#define J_B      0x40
#define J_SELECT 0x20
#define J_START  0x10
#define J_UP     0x08
#define J_DOWN   0x04
#define J_LEFT   0x02
#define J_RIGHT  0x01

static uint8_t prev_j, btn_prev, btn_latch, key_latch;

static uint8_t read_pad(void)
{
    uint8_t i, j = 0;
    JOY1 = 1;
    JOY1 = 0;
    for (i = 0; i < 8; ++i)
        j = (j << 1) | (JOY1 & 1);
    return j;
}

void input_init(void)
{
    prev_j = read_pad();
}

void input_poll(void)
{
    uint8_t j = read_pad(), dir = 0, btn, down;

    down = j & ~prev_j;                 /* newly pressed */
    prev_j = j;
    if (j & J_UP)    dir |= IN_UP;
    if (j & J_DOWN)  dir |= IN_DOWN;
    if (j & J_LEFT)  dir |= IN_LEFT;
    if (j & J_RIGHT) dir |= IN_RIGHT;
    btn = (j & J_A) != 0;
    if (down & J_B)
        key_latch = KEY_BACK;
    else if ((down & (J_START | J_SELECT)) && (j & (J_START | J_SELECT)) == (J_START | J_SELECT))
        key_latch = 'M';
    else if (down & J_START)
        key_latch = KEY_RETURN;
    else if (down & J_SELECT)
        key_latch = KEY_ESC;

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

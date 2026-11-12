/*
 * input.c -- the Adam's keyboard and controller 1 (from M.U.L.E.).
 *
 * Keys: the arrows walk, SPACE or a fire button is the button, RETURN, ESCAPE
 * and BACKSPACE as they say; M turns the sound off and on. The keyboard is
 * an AdamNet device that sends characters, not keys held: an arrow counts
 * as held for a while after each one it sends (its repeat keeps it held).
 */
#ifdef __ADAM__

#include "input.h"
#include "adam.h"

uint8_t inp_dir, inp_btn, ev_btn, ev_key;
uint8_t inp_hook, inp_hook_dir, inp_hook_btn, inp_hook_key;

static uint8_t btn_prev, btn_latch, key_latch;
static uint8_t held_dir, held_t, space_t;

#define K_UP     0xA0
#define K_RIGHT  0xA1
#define K_DOWN   0xA2
#define K_LEFT   0xA3
#define HOLD     20             /* frames an arrow stays held: its first
                                 * repeat comes in about a third of a second */

void input_init(void)
{
    eos_kbd_start();
}

/* A character from the keyboard, if one came. */
static void typed(void)
{
    uint8_t c = eos_kbd_end();
    if (c == 0)
        return;                 /* still waiting */
    eos_kbd_start();
    if (c == 1)
        return;                 /* it failed: asked again */
    switch (c) {
    case K_UP:    held_dir = IN_UP;    held_t = HOLD; return;
    case K_RIGHT: held_dir = IN_RIGHT; held_t = HOLD; return;
    case K_DOWN:  held_dir = IN_DOWN;  held_t = HOLD; return;
    case K_LEFT:  held_dir = IN_LEFT;  held_t = HOLD; return;
    case ' ':
        btn_latch = 1;
        space_t = 6;
        return;
    }
    if (c >= 'a' && c <= 'z')
        c -= 32;
    if (key_latch == KEY_NONE && (c == 13 || c == 27 || c == 8 || (c > 32 && c < 96)))
        key_latch = c;
}

void input_poll(void)
{
    uint8_t dir = 0, btn = 0, j;

    typed();
    if (held_t) {
        --held_t;
        dir = held_dir;
    }
    if (space_t) {
        --space_t;
        btn = 1;
    }
    joy_read();
    j = joy_ctl[0];             /* 1 north, 2 east, 4 south, 8 west */
    if (j & 1) dir |= IN_UP;
    if (j & 2) dir |= IN_RIGHT;
    if (j & 4) dir |= IN_DOWN;
    if (j & 8) dir |= IN_LEFT;
    if (joy_ctl[1] || joy_ctl[2])   /* left, right fire */
        btn = 1;

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

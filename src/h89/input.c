/*
 * input.c -- the H19's keyboard (bytes from h89io.asm's ring), from
 * M.U.L.E.'s client.
 *
 * The keypad is shifted (h19_init: ESC t): 8 2 4 6 are the arrows (ESC A
 * B D C), 7 9 1 3 the diagonals (ESC @ N L M), 5 is ESC H. SPACE is the
 * button (a bomb), RETURN, ESC, BACKSPACE / DELETE and the letters are
 * keys; f1 turns the bell off and on, f5 or ERASE draws the screen again.
 *
 * A terminal sends keys, not keys held, and the H19 repeats only with its
 * REPEAT key. So in the arena an arrow keeps the bomber going until keypad
 * 5 or another arrow (a diagonal turns at the next opening); SPACE drops a
 * bomb and keeps going. Elsewhere an arrow counts as held for a few frames
 * (H89_HOLD), so the menus see one press.
 *
 * A lone ESC is the ESC key; one followed within two frames by a letter is
 * a key's sequence.
 */
#include "input.h"
#include "plat.h"
#include "sound.h"
#include "h19.h"
#include "h89.h"

uint8_t inp_dir, inp_btn, ev_btn, ev_key;
uint8_t inp_hook, inp_hook_dir, inp_hook_btn, inp_hook_key;

static uint8_t btn_prev, btn_latch, key_latch;
static uint8_t held_dir, held_t, btn_t, latch, esc_on;
static uint16_t esc_at;

#ifndef H89_HOLD
#define H89_HOLD 10             /* frames an arrow counts as held (menus) */
#endif

void input_init(void)
{
}

void input_stop(void)
{
    latch = 0;
    held_t = 0;
}

static void arrow(uint8_t d)
{
    if (gfx_in_arena)
        latch = d;
    held_dir = d;
    held_t = H89_HOLD;
}

static void button(void)
{
    btn_latch = 1;
    btn_t = 6;
}

static void key(uint8_t c)
{
    if (key_latch == KEY_NONE)
        key_latch = c;
}

/* The letter after an ESC. */
static void sequence(uint8_t c)
{
    switch (c) {
    case 'A': arrow(IN_UP); break;
    case 'B': arrow(IN_DOWN); break;
    case 'C': arrow(IN_RIGHT); break;
    case 'D': arrow(IN_LEFT); break;
    case '@': arrow(IN_UP | IN_LEFT); break;            /* keypad 7 */
    case 'N': arrow(IN_UP | IN_RIGHT); break;           /* 9 */
    case 'L': arrow(IN_DOWN | IN_LEFT); break;          /* 1 */
    case 'M': arrow(IN_DOWN | IN_RIGHT); break;         /* 3 */
    case 'H': input_stop(); break;                      /* 5 */
    case 'S': snd_toggle_mute(); break;                 /* f1 */
    case 'W':                                           /* f5 */
    case 'J':                                           /* ERASE */
    case 'E': h19_repaint(); break;                     /* SHIFT ERASE */
    default: break;                                     /* BLUE, RED, GRAY... */
    }
}

static void typed(uint8_t c)
{
    if (c == ' ') {
        button();
        return;
    }
    if (c >= 'a' && c <= 'z')
        c -= 32;
    if (c == 127)
        c = KEY_BACK;
    if (c == KEY_RETURN || c == KEY_BACK || (c > 32 && c < 96))
        key(c);
}

void input_poll(void)
{
    uint8_t dir = 0, btn = 0;
    int k;

    /* (one key a frame is all the game takes: the rest wait in the ring) */
    while (key_latch == KEY_NONE && (k = kbd_get()) >= 0) {
        if (esc_on) {
            esc_on = 0;
            if (k != 27) {
                sequence((uint8_t)k);
                continue;
            }
            key(KEY_ESC);               /* ESC, ESC */
        }
#ifdef H89_RIG_KEYS
        if (k == '`') {                 /* (the rig can't type an ESC) */
            key(KEY_ESC);
            continue;
        }
#endif
        if (k == 27) {
            esc_on = 1;
            esc_at = frame_count;
            continue;
        }
        typed((uint8_t)k);
    }
    if (esc_on && (uint16_t)(frame_count - esc_at) >= 2) {
        esc_on = 0;
        key(KEY_ESC);
    }
    if (!gfx_in_arena)
        latch = 0;
    if (latch)
        dir = latch;
    else if (held_t) {
        --held_t;
        dir = held_dir;
    }
    if (btn_t) {
        --btn_t;
        btn = 1;
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

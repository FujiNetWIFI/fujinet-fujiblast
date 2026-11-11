/*
 * input.c -- joystick 0 and the keyboard (from M.U.L.E.'s src/atari).
 * The space bar is the button too.
 */
#ifdef __ATARI__

#include <stdint.h>
#include "input.h"
#include "sound.h"

/* the hardware, not the OS's shadows: gfxa.s skips the VBI stage that copies them */
#define PORTA  (*(volatile uint8_t *)0xD300)
#define TRIG0  (*(volatile uint8_t *)0xD010)
#define CH     (*(volatile uint8_t *)0x02FC)
#define SKSTAT (*(volatile uint8_t *)0xD20F)

uint8_t inp_dir, inp_btn, ev_btn, ev_key;
uint8_t inp_hook, inp_hook_dir, inp_hook_btn, inp_hook_key;

static uint8_t key_btn, btn_prev, btn_latch, key_latch;

/* keyboard code (bits 0-5) -> ASCII, 0 none */
static const char kmap[64] = {
    'L', 'J', ';', 0,   0,   'K', '+', '*', 'O', 0,   'P', 'U', 13,  'I', '-', '=',
    'V', 0,   'C', 0,   0,   'B', 'X', 'Z', '4', 0,   '3', '6', 27,  '5', '2', '1',
    ',', ' ', '.', 'N', 0,   'M', '/', 0,   'R', 0,   'E', 'Y', 9,   'T', 'W', 'Q',
    '9', 0,   '0', '7', 8,   '8', '<', '>', 'F', 'H', 'D', 0,   0,   'G', 'S', 'A',
};

void input_init(void)
{
    CH = 0xFF;
}

void input_poll(void)
{
    uint8_t k, c, btn;

    k = CH;
    if (k != 0xFF) {
        CH = 0xFF;
        c = (k & 0xC0) ? 0 : kmap[k & 0x3F];
        key_btn = c == ' ';
        if (c && c != ' ')
            key_latch = c;
    } else if (SKSTAT & 0x04) {
        key_btn = 0;                    /* let go */
    }
    btn = key_btn || !TRIG0;
    inp_dir = ~PORTA & 15;              /* 1 up, 2 down, 4 left, 8 right */
    if (inp_hook) {
        inp_dir = inp_hook_dir;
        btn = inp_hook_btn;
        if (inp_hook_key) {
            key_latch = inp_hook_key;
            inp_hook_key = 0;
        }
    }
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

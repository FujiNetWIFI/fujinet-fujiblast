/*
 * input.c -- the keyboard and joystick A on the PC and PCjr. From M.U.L.E.'s
 * src/msdos/input.c.
 *
 * Keys: the arrows (or the keypad) walk, SPACE is fire, ESC leaves, M turns
 * the sound off and on; letters, digits, BACKSPACE and ENTER for the name.
 *
 * Walking needs to know which arrows are held, which the BIOS doesn't say:
 * the keyboard interrupt (INT 9) is hooked to keep a table of the keys that
 * are down, then passed on to the BIOS, which still queues the typed
 * characters (read with INT 16h). On a PC the scan code is at port 60h; on
 * the PCjr the BIOS's NMI handler gathers it from the keyboard's serial
 * line and calls INT 9 with it in AL.
 *
 * The walk keys, SPACE and every key's repeats are kept from the BIOS: a
 * held key would fill its 15-key buffer, and then each repeat beeps with
 * interrupts off, long enough to lose serial bytes. The PCjr's BIOS makes
 * the repeats itself; they are turned off there while the game runs.
 *
 * The joystick: port 201h's one-shots, timed with interrupts off. It is
 * used if it answers at start, when it should be centred. The axes are
 * timed only while no reply is on its way from the FujiNet (fb_busy), so
 * no byte is lost to them; a loop stops once both are past their mark.
 */
#ifdef __MSDOS__

#include <dos.h>
#include <i86.h>
#include <conio.h>
#include <bios.h>
#include "input.h"
#include "msdos.h"

uint8_t inp_dir, inp_btn, ev_btn, ev_key;
uint8_t inp_hook, inp_hook_dir, inp_hook_btn, inp_hook_key;

static uint8_t btn_prev, btn_latch, key_latch;

/* Scan codes (set 1). */
#define SC_SPACE 0x39
#define SC_UP    0x48
#define SC_LEFT  0x4B
#define SC_RIGHT 0x4D
#define SC_DOWN  0x50
#define SC_KP7   0x47           /* the keypad's diagonals */
#define SC_KP9   0x49
#define SC_KP1   0x4F
#define SC_KP3   0x51

volatile uint8_t kb_down[0x60];         /* 1: the key is held (tools/dossim peeks it) */
static volatile uint8_t space_hit;      /* SPACE went down since the last poll */

static void (__interrupt __far *old_int9)(void);

static void __interrupt __far kb_isr(union INTPACK r)
{
    uint8_t sc = plat_pcjr ? r.h.al : inp(0x60);
    uint8_t k = sc & 0x7F, keep = 0, b;
    if (sc == 0xE0) {
        keep = 1;               /* the grey keys' prefix: the BIOS would put */
                                /* it on the next key it was given */
    } else if (k < sizeof(kb_down)) {
        switch (k) {
        case SC_SPACE: case SC_UP: case SC_DOWN: case SC_LEFT: case SC_RIGHT:
        case SC_KP7: case SC_KP9: case SC_KP1: case SC_KP3:
            keep = 1;
        }
        if (sc & 0x80) {
            kb_down[k] = 0;
        } else {
            if (kb_down[k])
                keep = 1;       /* a repeat */
            kb_down[k] = 1;
            if (k == SC_SPACE)
                space_hit = 1;
        }
    }
    if (!keep)
        _chain_intr(old_int9);
    if (!plat_pcjr) {           /* what the BIOS would do (the PCjr's INT 9 */
        b = inp(0x61);          /* is a call from its NMI handler: no EOI) */
        outp(0x61, b | 0x80);
        outp(0x61, b);
        outp(0x20, 0x20);
    }
}

/* ---- joystick A ----------------------------------------------------------- */
static uint8_t joy_on;
static uint16_t joy_cx, joy_cy, joy_end;

/* Time both axes' one-shots (loop counts; 0xFFFF: still high at the end). */
static void joy_read(uint16_t *x, uint16_t *y, uint16_t end)
{
    uint16_t n = 0, ax = 0xFFFF, ay = 0xFFFF;
    uint8_t v;
    _disable();
    outp(0x201, 0);
    do {
        v = inp(0x201);
        if (!(v & 1) && ax == 0xFFFF)
            ax = n;
        if (!(v & 2) && ay == 0xFFFF)
            ay = n;
    } while ((v & 3) && ++n < end);
    _enable();
    *x = ax;
    *y = ay;
}

/* The PCjr BIOS's typematic: AL=4 off, AL=0 back to its defaults. */
static void typematic(uint8_t al)
{
    union REGS r;
    if (!plat_pcjr)
        return;
    r.h.ah = 0x03;
    r.h.al = al;
    int86(0x16, &r, &r);
}

void input_init(void)
{
    old_int9 = _dos_getvect(0x09);
    _dos_setvect(0x09, kb_isr);
    typematic(4);
    joy_read(&joy_cx, &joy_cy, 2000);
    joy_on = joy_cx != 0xFFFF && joy_cy != 0xFFFF && joy_cx && joy_cy;
    /* past 1.5x the centre on both axes is all a read needs to know */
    joy_end = (joy_cx > joy_cy ? joy_cx : joy_cy);
    joy_end += joy_end / 2 + 1;
}

void input_restore(void)
{
    if (old_int9) {
        _dos_setvect(0x09, old_int9);
        typematic(0);
        old_int9 = 0;
    }
}

/* The typed keys the BIOS has queued: one is latched a frame, the ones
 * that aren't for the game are thrown away. */
static void typed(void)
{
    uint16_t k;
    uint8_t c;
    while (key_latch == KEY_NONE &&
           *(volatile uint16_t __far *)MK_FP(0x40, 0x1A) != *(volatile uint16_t __far *)MK_FP(0x40, 0x1C)) {
        /* (the BIOS's buffer, head and tail: not INT 16h every pass) */
        k = _bios_keybrd(_KEYBRD_READ);
        c = (uint8_t)k;
        if (c >= 'a' && c <= 'z')
            c -= 32;
        if (c == KEY_RETURN || c == KEY_ESC || c == KEY_BACK || (c > 32 && c < 96))
            key_latch = c;      /* (SPACE is the button: INT 9 saw it) */
    }
}

#ifdef STATS
extern uint32_t t_input;
uint32_t prof_ticks(void);
static void poll(void);

void input_poll(void)
{
    uint32_t t = prof_ticks();
    poll();
    t_input += prof_ticks() - t;
}

static void poll(void)
#else
void input_poll(void)
#endif
{
    static uint16_t jx = 0x8000, jy = 0x8000;
    static uint8_t jbtn;
    uint8_t dir = 0, btn;

    typed();
    if (kb_down[SC_UP] || kb_down[SC_KP7] || kb_down[SC_KP9]) dir |= IN_UP;
    if (kb_down[SC_DOWN] || kb_down[SC_KP1] || kb_down[SC_KP3]) dir |= IN_DOWN;
    if (kb_down[SC_LEFT] || kb_down[SC_KP7] || kb_down[SC_KP1]) dir |= IN_LEFT;
    if (kb_down[SC_RIGHT] || kb_down[SC_KP9] || kb_down[SC_KP3]) dir |= IN_RIGHT;
    btn = kb_down[SC_SPACE];
    if (space_hit) {
        space_hit = 0;
        btn_latch = 1;          /* however short the press */
    }
    if (joy_on) {
        if (!fb_busy())
            joy_read(&jx, &jy, joy_end);
        if (jx < joy_cx / 2) dir |= IN_LEFT;
        else if (jx > joy_cx + joy_cx / 2) dir |= IN_RIGHT;
        if (jy < joy_cy / 2) dir |= IN_UP;
        else if (jy > joy_cy + joy_cy / 2) dir |= IN_DOWN;
        jbtn = (inp(0x201) & 0x30) != 0x30;     /* either of its buttons */
        btn |= jbtn;
    }

#ifdef STATS
    {   /* plays by itself: leaves the title, walks about, drops bombs */
        static uint16_t t;
        static uint8_t d = IN_RIGHT;
        extern uint8_t plat_rand(void);
        if (++t == 240)
            key_latch = KEY_RETURN;
        if (!(t & 31))
            d = 1 << (plat_rand() & 3);
        dir = d;
        if ((t & 127) == 64)
            btn = 1;
    }
#endif
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

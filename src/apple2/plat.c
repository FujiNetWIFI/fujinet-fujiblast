/*
 * plat.c -- Apple II start-up, the 60 Hz clock and quitting.
 *
 * A 128K //e, //c or IIgs: double hi-res needs the aux RAM, and the clock
 * is the vertical blank (frame_tick in dhgr.s: polled on the //e and IIgs).
 *
 * The game's loop is one pass a jiffy (src/main.c), but drawing four
 * sprites takes the Apple more than a jiffy. So frame_wait draws only when
 * the game has caught up with the clock: when it is behind, it returns at
 * once and the pass is logic only (walking, the link, sound). A long stall
 * (a SmartPort call, an appkey) is let go rather than raced after; jiffies
 * the polling missed are put back from the server's timer (clock_check).
 */
#ifdef __APPLE2__

#include <apple2.h>
#include <stdlib.h>
#include <string.h>
#include "plat.h"
#include "gfx.h"
#include "input.h"
#include "sound.h"
#include "gfxcodes.h"
#include "game.h"

#define CATCH_UP 30             /* jiffies behind: past this (a stall), let them go */

uint16_t frame_count;
uint8_t plat_drew;                      /* this pass drew (input.c reads the stick then) */
static uint16_t seed = 0x1234;

/* lowlevel.s */
void vbl_init(uint8_t mode);
void dhgr_on(void);
void dhgr_off(void);
uint8_t __fastcall__ mli(uint8_t cmd, void *parms);
void aux_put(void *dst, const void *src, uint16_t n);
/* dhgr.s, gfx.c */
void frame_tick(void);
void gfx_render(void);
void gfx_clear_pages(void);
void snd_tick(void);

extern uint8_t _AUXGFX__[], _AUXG_LAST__[], _AUXSPR_LOAD__[], _AUXSPR_RUN__[];

/* kept in DATA: the constructor runs before BSS is cleared */
static uint8_t boot_mode = 0xFF;

#define MACHID   (*(volatile uint8_t *)0xBF98)
#define DEVCNT   (*(volatile uint8_t *)0xBF31)
#define DEVLST   ((volatile uint8_t *)0xBF32)
#define DEVADR32 (*(volatile uint16_t *)0xBF26)      /* slot 3, drive 2 */
#define NODEV    (*(volatile uint16_t *)0xBF10)

/* ---- start-up, once: a constructor (cc65 runs it before main, before the
 * language card is copied and BSS cleared; it keeps nothing in BSS), in
 * the ONCE segment, which BSS takes over after. Nothing here may write to
 * BSS: until the constructors are done, BSS holds the language card's code
 * waiting to be copied. -------------------------------------------------- */
#pragma code-name (push, "ONCE")
#pragma rodata-name (push, "ONCE")
#pragma static-locals (push, off)       /* BSS is the language card's image yet */

/* A message on the text screen, then back to ProDOS. */
static void fail(const char *s)
{
    uint8_t *p = (uint8_t *)0x0400, i;
    for (i = 0; i < 40; ++i)
        p[i] = ' ' | 0x80;
    for (i = 0; s[i] && i < 40; ++i)
        p[i] = s[i] | 0x80;
    while (!(*(volatile uint8_t *)0xC000 & 0x80))
        ;
    *(volatile uint8_t *)0xC010 = 0;
    exit(1);
}

/* /RAM lives in aux RAM, which the game takes: unplug it (ProDOS TN #8). */
static void ram_off(void)
{
    uint8_t i, j;
    if (DEVADR32 == NODEV)
        return;
    for (i = DEVCNT + 1; i--;) {
        if ((DEVLST[i] & 0xF0) == 0xB0) {
            for (j = i; j < DEVCNT; ++j)
                DEVLST[j] = DEVLST[j + 1];
            DEVLST[DEVCNT] = 0;
            --DEVCNT;
            DEVADR32 = NODEV;
            return;
        }
    }
}

/* FUJIBLST.GFX into aux RAM, 8K at a time through the main half of page 1
 * (not shown yet). ProDOS's MLI itself: cc65's open() would take its
 * buffer from the heap. */
#define TEMP  ((uint8_t *)0x2000)
#define IOBUF ((uint8_t *)0x4000)       /* in page 2, also not shown */

static uint8_t load_gfx(void)
{
    static char path[] = "\014FUJIBLST.GFX";
    static struct { uint8_t n; char *path; uint8_t *buf; uint8_t ref; } p_open = { 3, path, IOBUF, 0 };
    static struct { uint8_t n, ref; uint8_t *buf; uint16_t req, got; } p_read = { 4, 0, TEMP, 0x2000, 0 };
    static struct { uint8_t n, ref; } p_close = { 1, 0 };
    uint8_t err = 0;
    uint16_t total = 0, n, m;
    uint16_t split = _AUXSPR_LOAD__ - _AUXGFX__;       /* the file's last part goes to $0800 */
    if (mli(0xC8, &p_open))             /* OPEN */
        return 0;
    p_read.ref = p_close.ref = p_open.ref;
    do {
        err = mli(0xCA, &p_read);       /* READ */
        if (err || !p_read.got)
            break;
        n = p_read.got;
        m = 0;
        if (total < split) {
            m = split - total;
            if (m > n)
                m = n;
            aux_put(_AUXGFX__ + total, TEMP, m);
        }
        if (m < n)
            aux_put(_AUXSPR_RUN__ + (total + m - split), TEMP + m, n - m);
        total += n;
    } while (p_read.got == 0x2000);
    mli(0xCC, &p_close);                /* CLOSE */
    return total == (uint16_t)(_AUXG_LAST__ - _AUXGFX__);
}

/* The machine, the memory, the art. */
void plat_boot(void)
{
    uint8_t t = get_ostype();
    if (t < APPLE_IIE)
        fail("FUJIBLAST NEEDS AN APPLE //E, //C OR IIGS");
    if ((MACHID & 0x30) != 0x30)
        fail("FUJIBLAST NEEDS 128K");
    *(volatile uint8_t *)0xC000 = 0;    /* 80STORE off: AUXMOVE, RAMWRT */
    ram_off();
    if (!load_gfx())
        fail("CAN'T LOAD FUJIBLST.GFX");
    boot_mode = t >= APPLE_IIGS ? 1 : (t >= APPLE_IIC && t < APPLE_IIGS) ? 2 : 0;
}

#pragma static-locals (pop)
#pragma rodata-name (pop)
#pragma code-name (pop)

void plat_init(void)
{
    vbl_init(boot_mode);
    gfx_clear_pages();
    dhgr_on();
    snd_init();
    input_init();
    gfx_init();
}

void plat_quit(void)
{
    dhgr_off();
    exit(0);
}

/* The //e's blank is polled, and the shared code has stretches longer than
 * the blank, so jiffies can go uncounted. The server's round timer says
 * what real time did: each second it ticks, the jiffies counted since are
 * set against 60, and what is missing goes back on the clock, one a pass
 * (a count that came late is made up by the next one coming early). */
static uint8_t tm_last = 0xFF;
static uint16_t tm_at;
static int8_t drift;

static void clock_check(void)
{
    uint8_t t = st[OFF_TIMER];
    int16_t d;
    if (t != tm_last) {
        if ((uint8_t)(tm_last - t) == 1) {
            d = drift + 60 - (int16_t)(frame_count - tm_at);
            drift = d > 90 ? 90 : d < -90 ? -90 : (int8_t)d;
        }
        tm_last = t;
        tm_at = frame_count;
    }
    if (drift > 0) {
        ++frame_count;
        --drift;
    }
}

void frame_wait(void)
{
    static uint16_t seen;
    uint16_t d;
    frame_tick();
    clock_check();
    d = frame_count - seen;
    if (d > CATCH_UP) {
        seen = frame_count - 1;
        d = 1;
    }
    plat_drew = d < 2;
    if (!plat_drew) {
        ++seen;                         /* behind: a pass with no drawing */
    } else {
        gfx_render();
        while (frame_count == seen)
            frame_tick();
        ++seen;
    }
    seed += frame_count;
    snd_tick();
}

uint8_t plat_rand(void)
{
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return (uint8_t)seed;
}

#endif

/*
 * plat.c -- an IBM PC, XT or PCjr (or compatible): start-up, the 60 Hz
 * clock and quitting. From M.U.L.E.'s src/msdos/plat.c.
 *
 * The clock is the PIT's channel 0 sped up from 18.2 to 60 Hz. Its
 * interrupt counts frame_count, steps the speaker (sound.c), and passes
 * every 3.3rd tick on to the BIOS, so the time of day and the floppy motor
 * still work.
 *
 * The game's loop is one pass a jiffy (src/main.c), and a 4.77 MHz 8088
 * can't always draw in one. So frame_wait draws only when the game has
 * caught up with the clock: when it is behind, it returns at once and the
 * pass is logic only (walking, the link, the timers), as on the Apple. A
 * long stall (an appkey, a slow network call) is let go, not raced after.
 * Every MAX_SKIP passes it draws all the same: when the link keeps the
 * passes over a jiffy, the game runs a little slow rather than never
 * drawing.
 *
 * Quitting goes back to the FujiNet lobby (lobby.c), the way the FujiNet's
 * other DOS games do.
 */
#ifdef __MSDOS__

#include <string.h>
#include <stdlib.h>
#include <dos.h>
#include <i86.h>
#include <conio.h>
#include "plat.h"
#include "gfx.h"
#include "input.h"
#include "sound.h"
#include "msdos.h"

#define CATCH_UP 30             /* jiffies behind: past this (a stall), let them go */
#define MAX_SKIP 4              /* passes without drawing, at most, when behind */
#define PIT_60HZ 19886u         /* 1193182 / 60 */

uint16_t frame_count;
uint8_t plat_pcjr, plat_pal;
uint16_t plat_mem[4];
uint16_t plat_late;             /* jiffies passed with no drawing (tools/dossim) */
uint16_t plat_drawn;            /* frames drawn (tools/dossim) */
uint8_t plat_drew;              /* this pass drew */
static uint16_t seed = 0x1234;
static uint8_t old_mode;

static void (__interrupt __far *old_int8)(void);
static void (__interrupt __far *old_int1b)(void);
static void (__interrupt __far *old_int23)(void);
static void (__interrupt __far *old_int24)(void);
static void (__interrupt __far *old_intf5)(void);
static uint16_t bios_acc;
static uint8_t no_fn;

#ifdef STATS
/* A profile on a real PC's timing (86Box: tools/dosrig/86box.sh): where
 * the timer's interrupt finds the CPU, by 64 bytes of code; and the PIT
 * in mode 2 (a count down by one each tick) as a clock to time drawing,
 * waiting and the link with (ticks of 0.838 us). */
#define PIT_MODE 0x34
#define PIT_TICK PIT_60HZ
uint16_t prof_hist[512], prof_other, prof_uart;
static uint16_t code_seg, uart_seg;
static uint8_t sub;
extern void __far fb_isr(void);
#pragma aux fb_isr "_*"
static volatile uint8_t prof_off;       /* not while writing the files */
uint32_t t_render, t_wait, t_net, t_input, t_logic;    /* PIT ticks */
uint16_t getcs(void);
#pragma aux getcs = "mov ax, cs" value [ax];

static volatile uint32_t tick_base;     /* PIT ticks at the last interrupt */

uint32_t prof_ticks(void)
{
    uint16_t c;
    uint32_t b;
    _disable();
    outp(0x43, 0x00);           /* latch channel 0 */
    c = inp(0x40);
    c |= inp(0x40) << 8;
    b = tick_base;
    outp(0x20, 0x0A);           /* a tick due, not yet counted */
    if (inp(0x20) & 1)
        b += PIT_TICK;
    _enable();
    return b + (PIT_TICK - c);
}

static void __interrupt __far timer_isr(union INTPACK r)
{
    uint16_t a = bios_acc;
    tick_base += PIT_TICK;
    if (prof_off)
        ;
    else if (r.w.cs == code_seg)
        ++prof_hist[(r.w.ip >> 6) & 511];
    else if (r.w.cs == uart_seg)
        ++prof_uart;
    else
        ++prof_other;
    bios_acc += PIT_TICK;
    ++frame_count;
    snd_isr();
    if (bios_acc < a)           /* 65536 counts: the BIOS's tick */
        _chain_intr(old_int8);
    outp(0x20, 0x20);
}
#else
#define PIT_MODE 0x36
#define PIT_TICK PIT_60HZ
static void __interrupt __far timer_isr(void)
{
    uint16_t a = bios_acc;
    ++frame_count;
    snd_isr();
    bios_acc += PIT_60HZ;
    if (bios_acc < a)           /* 65536 counts: the BIOS's tick */
        _chain_intr(old_int8);
    outp(0x20, 0x20);
}
#endif

static void __interrupt __far no_break(void)
{
}

/* DOS's critical error (INT 24h): retry, rather than DOS's "Abort, Retry,
 * Ignore?" over the game. */
static void __interrupt __far crit_err(union INTPACK r)
{
    r.h.al = 1;
}

/* Without FUJINET.SYS nothing answers INT F5h, and its vector may point
 * anywhere (on the PCjr it is left over from BASIC): every call gets an
 * error instead, so the game says so rather than crashing (and the test
 * scenes, which need no network, still run). FUJINET.SYS's header is at
 * the start of INT F5's segment: a block device with a FAT BPB (attribute
 * 2002h, 6020h or 6002h by release), its own entry points, 1-26 units.
 * Loaded high, the segment can be past A000h. */
static uint8_t have_fujinet(void)
{
    void __far *v = _dos_getvect(0xF5);
    uint16_t seg = FP_SEG(v);
    uint16_t attr;
    uint8_t units;
    if (!v || seg < 0x60)
        return 0;
    attr = *(uint16_t __far *)MK_FP(seg, 4);
    units = *(uint8_t __far *)MK_FP(seg, 10);
    return (attr & 0xA000) == 0x2000
        && *(uint16_t __far *)MK_FP(seg, 6) && *(uint16_t __far *)MK_FP(seg, 8)
        && units && units <= 26;
}

static void __interrupt __far no_fujinet(union INTPACK r)
{
    r.w.ax = 'E';
    r.w.dx = 0;
}

uint8_t plat_net_probe(void)
{
    return !no_fn;
}

static void pit(uint16_t divisor)
{
    _disable();
    outp(0x43, divisor ? PIT_MODE : 0x36);      /* channel 0, lo/hi, mode 3 */
    outp(0x40, divisor & 0xFF);
    outp(0x40, divisor >> 8);
    _enable();
}

/* EGA or VGA: INT 10h AH=12h BL=10h returns BL changed. */
static uint8_t ega_or_vga(void)
{
    union REGS r;
    r.h.ah = 0x12;
    r.h.bl = 0x10;
    int86(0x10, &r, &r);
    return r.h.bl != 0x10;
}

/* A Tandy 1000: F000:C000 holds 21h and FFFF:000E FFh. */
static uint8_t tandy(void)
{
    return *(uint8_t __far *)MK_FP(0xF000, 0xC000) == 0x21
        && *(uint8_t __far *)MK_FP(0xFFFF, 0x000E) == 0xFF;
}

/* The stack's low-water mark: what is unused now is filled with a pattern
 * (tools/dossim finds how much stays untouched: plat_stack_free). */
extern unsigned _STACKLOW;

static void stack_fill(void)
{
    uint8_t here;
    uint8_t *p = (uint8_t *)_STACKLOW;
    while (p < &here - 64)
        *p++ = 0xA5;
}

uint16_t plat_stack_free(void)
{
    uint8_t *p = (uint8_t *)_STACKLOW;
    while (*p == 0xA5)
        ++p;
    return (uint16_t)(p - (uint8_t *)_STACKLOW);
}

void plat_init(void)
{
    union REGS r;
    extern uint8_t test_hook[];         /* test.c */

    stack_fill();
    test_hook[0] = 0x3C;        /* with [1]'s C3: tools/dossim finds DGROUP */
    r.h.ah = 0x0F;
    int86(0x10, &r, &r);
    old_mode = r.h.al;
    plat_pcjr = *(uint8_t __far *)MK_FP(0xF000, 0xFFFE) == 0xFD;
    plat_pal = plat_pcjr ? PAL_PCJR : ega_or_vga() ? PAL_EGA : tandy() ? PAL_TANDY : PAL_CGA;

    seed ^= *(uint16_t __far *)MK_FP(0x40, 0x6C);   /* the BIOS tick count */
    old_int8 = _dos_getvect(0x08);
    old_int1b = _dos_getvect(0x1B);
    old_int23 = _dos_getvect(0x23);
    old_int24 = _dos_getvect(0x24);
    if (!have_fujinet()) {
        no_fn = 1;
#ifdef STATS
        test_hook[0] = 0x5A;    /* offline: the arena's test scene, by itself */
        test_hook[2] = 1;
#endif
        old_intf5 = _dos_getvect(0xF5);
        _dos_setvect(0xF5, no_fujinet);
    }
    _dos_setvect(0x1B, no_break);
    _dos_setvect(0x23, no_break);
    _dos_setvect(0x24, crit_err);
    snd_init();
    _disable();
#ifdef STATS
    code_seg = getcs();
    uart_seg = FP_SEG((void __far *)fb_isr);
#endif
    _dos_setvect(0x08, timer_isr);
    pit(PIT_TICK);
    input_init();
    plat_mem[0] = _psp;
    plat_mem[1] = *(uint16_t __far *)MK_FP(0x40, 0x13) * 64u;
    gfx_init();
    {
        union REGS q;
        q.h.ah = 0x48;          /* (fails: says how much is left) */
        q.w.bx = 0xFFFF;
        intdos(&q, &q);
        plat_mem[3] = q.w.bx;
    }
}

/* Everything back as DOS had it. */
void plat_restore(void)
{
    union REGS r;
    snd_off();
    fb_off();
    input_restore();
    pit(0);
    _dos_setvect(0x08, old_int8);
    _dos_setvect(0x1B, old_int1b);
    _dos_setvect(0x23, old_int23);
    _dos_setvect(0x24, old_int24);
    if (no_fn)
        _dos_setvect(0xF5, old_intf5);
    r.h.ah = 0;
    r.h.al = old_mode;
    int86(0x10, &r, &r);
}

void plat_quit(void)
{
    nst_close_quiet();
    plat_restore();
    lobby_return();
    exit(0);
}

#ifdef STATS
/* STATS=1 builds (tools/dosrig/86box.sh): every 10 s a line appended to
 * A:\STATS.TXT: jiffies, passes drawn, passes late, the link's counters. */
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
extern uint8_t fb_cnt[4];       /* fbnet.c */
extern volatile uint8_t fb_ovr; /* uartisr.asm */
#pragma aux fb_ovr "_*"

static char *num(char *p, uint16_t n)
{
    char b[6], *q = b + 5;
    *q = 0;
    do {
        *--q = '0' + n % 10;
        n /= 10;
    } while (n);
    while (*q)
        *p++ = *q++;
    *p++ = ' ';
    return p;
}

extern uint16_t fb_nrx;         /* uartisr.asm: bytes received */
#pragma aux fb_nrx "_*"

static uint32_t t_stats;        /* writing the files: left out */
static char bline[48];          /* the benchmark's line, written first */

static uint16_t pct(uint32_t part, uint32_t all)        /* in tenths */
{
    return (uint16_t)((part >> 8) * 1000 / ((all >> 8) + 1));
}

static void stats(void)
{
    static char line[160];
    static uint32_t t0;
    uint32_t t = prof_ticks(), all = t - t0 - t_stats;
    char *p = line;
    int fd;
    unsigned n;
    uint8_t i;
    p = num(p, frame_count);
    p = num(p, plat_drawn);
    p = num(p, plat_late);
    for (i = 0; i < 4; ++i)
        p = num(p, fb_cnt[i]);
    p = num(p, fb_ovr);
    *p++ = '|';
    *p++ = ' ';
    /* tenths of a per cent of the time: drawing, waiting, the passes'
     * logic (of which: the link's decoding, input) */
    p = num(p, pct(t_render, all));
    p = num(p, pct(t_wait, all));
    p = num(p, pct(t_logic, all));
    p = num(p, pct(t_net, all));
    p = num(p, pct(t_input, all));
    {
        extern uint32_t fb_isrt;        /* uartisr.asm: its own time */
        #pragma aux fb_isrt "_*"
        p = num(p, pct(fb_isrt, all));
        fb_isrt = 0;
    }
    {
        extern uint16_t g_cnt[8];       /* gfx.c */
        extern uint32_t t_g[3];
        *p++ = '#';
        *p++ = ' ';
        for (i = 0; i < 3; ++i) {
            p = num(p, pct(t_g[i], all));
            t_g[i] = 0;
        }
        for (i = 0; i < 8; ++i) {
            p = num(p, g_cnt[i]);
            g_cnt[i] = 0;
        }
    }
    p = num(p, fb_nrx);
    t_render = t_wait = t_net = t_input = t_logic = 0;
    fb_nrx = 0;
    prof_off = 1;
    *p++ = '\r';
    *p++ = '\n';
    if (!_dos_creat("A:\\PROF.BIN", _A_NORMAL, &fd)) {
        _dos_write(fd, prof_hist, sizeof(prof_hist), &n);
        _dos_write(fd, &prof_other, 2, &n);
        _dos_write(fd, &prof_uart, 2, &n);
        _dos_close(fd);
    }
    if (_dos_open("A:\\STATS.TXT", O_WRONLY, &fd) && _dos_creat("A:\\STATS.TXT", _A_NORMAL, &fd)) {
        prof_off = 0;
        t0 = prof_ticks();
        return;
    }
    lseek(fd, 0, SEEK_END);
    if (bline[0]) {
        _dos_write(fd, bline, strlen(bline), &n);
        bline[0] = 0;
    }
    _dos_write(fd, line, (unsigned)(p - line), &n);
    _dos_close(fd);
    prof_off = 0;
    t0 = prof_ticks();
    t_stats = 0;
}
#endif

void frame_wait(void)
{
    static uint16_t seen;
    static uint8_t skipped;     /* passes since the last drawn */
    uint16_t d;
#ifdef STATS
    static uint16_t told;
    static uint32_t out;        /* when the last call returned */
    uint32_t in = prof_ticks();
    if (out)
        t_logic += in - out;
    if ((uint16_t)(frame_count - told) >= 600) {
        told = frame_count;
        {
            static uint8_t benched;
            if (!benched) {     /* once, the first time: what things cost */
                extern void gfx_bench(void);
                extern uint16_t bench[6];
                char *q = bline;
                uint8_t k;
                benched = 1;
                gfx_bench();
                *q++ = 'B';
                *q++ = ' ';
                for (k = 0; k < 6; ++k)
                    q = num(q, bench[k]);
                *q++ = '\r';
                *q++ = '\n';
                *q = 0;
            }
        }
        stats();
        in = prof_ticks();
    }
#endif
    d = frame_count - seen;
    if (d > CATCH_UP) {
        seen = frame_count - 1;
        d = 1;
    }
    plat_drew = d < 2 || skipped >= MAX_SKIP;
    if (!plat_drew) {
        ++skipped;
        ++seen;                 /* behind: a pass with no drawing */
        ++plat_late;
    } else {
#ifdef STATS
        uint32_t t1 = prof_ticks(), t2;
        gfx_render();
        t2 = prof_ticks();
        t_render += t2 - t1;
#else
        gfx_render();
#endif
        ++plat_drawn;
        skipped = 0;
        while (*(volatile uint16_t *)&frame_count == seen)
            ;                   /* (the timer interrupt counts it) */
#ifdef STATS
        t_wait += prof_ticks() - t2;
#endif
        ++seen;
    }
    seed += frame_count;
#ifdef STATS
    out = prof_ticks();
#endif
}

uint8_t plat_rand(void)
{
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return (uint8_t)seed;
}

#endif

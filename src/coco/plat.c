/*
 * plat.c -- CoCo 1/2 and CoCo 3 start-up, the 60 Hz clock and quitting
 * (from M.U.L.E.'s src/coco/plat.c).
 *
 * The clock is the VDG's field sync interrupt (PIA0 CB1), counted by
 * irq_isr. The DriveWire ROM masks interrupts while it talks to the
 * FujiNet, so jiffies can go uncounted: the server's round timer says what
 * real time did (clock_check, as on the Apple II).
 *
 * The game's loop is one pass a jiffy (src/main.c), but drawing four
 * sprites takes a CoCo more than a jiffy. So frame_wait draws only when
 * the game has caught up with the clock: when it is behind, it returns at
 * once and the pass is logic only (walking, the link, sound).
 *
 * By the time plat_init has run bank_init, the machine is in all-RAM mode
 * (see bank.c): the BASIC ROMs are gone, so the IRQ handler must not chain
 * to them.
 *
 * The CoCo 3 (COCO3) uses the same interrupt, vector ($010C) and keyboard;
 * it asks for the monitor type instead of the artifact colours, and runs at
 * 1.79 MHz.
 */
#include <string.h>
#include "fbcoco.h"
#include "game.h"
#include "gfx.h"
#include "input.h"
#include "plat.h"
#include "sound.h"

#define CATCH_UP 30             /* jiffies behind: past this (a stall), let them go */

uint16_t frame_count;
static uint16_t seed = 0x1234;

/* The keyboard, scanned here 60 times a second so that no key press is
 * missed however long the game's own frame takes (input.c reads these):
 * kb_rows[c] is column c's rows (1 = down, joystick buttons masked out),
 * and each key that went down is queued as column * 8 + row. */
uint8_t kb_rows[8], kb_prev[8];
uint8_t kb_q[8], kb_head, kb_tail;

/* The IRQ: on the field sync count a frame and scan the keyboard; clear
 * whichever PIA0 flag brought us here. */
interrupt asm void irq_isr(void)
{
    asm {
        lda     $FF03
        bpl     @hsync
        lda     $FF02           ; clears the field sync flag
        ldd     _frame_count
        addd    #1
        std     _frame_count
        ; the keyboard: rows low with no column driven are joystick buttons
        lda     #$FF
        sta     $FF02
        lda     $FF00
        coma
        anda    #$7F
        pshs    a               ; 0,s: the buttons
        leax    _kb_rows,pcr
        leay    _kb_prev,pcr
        ldb     #$FE            ; column 0 low
        clr     ,-s             ; 0,s: column; 1,s: buttons
@col
        stb     $FF02
        lda     $FF00
        coma
        anda    #$7F
        pshs    b
        ldb     2,s
        comb
        pshs    b
        anda    ,s+             ; not the buttons
        puls    b
        sta     ,x
        eora    ,y              ; changed ...
        anda    ,x              ; ... and down now: pressed
        beq     @same
        pshs    b,x,y
        ldb     5,s             ; the column
        aslb
        aslb
        aslb
@bit
        lsra
        bcc     @nobit
        pshs    a,b
        lda     _kb_head
        leax    _kb_q,pcr
        stb     a,x             ; queue column * 8 + row
        inca
        anda    #7
        cmpa    _kb_tail
        beq     @full
        sta     _kb_head
@full
        puls    a,b
@nobit
        incb
        tsta
        bne     @bit
        puls    b,x,y
@same
        lda     ,x+
        sta     ,y+
        inc     ,s
        orcc    #$01            ; next column low
        rolb
        bcs     @col
        leas    2,s
        lda     #$FF
        sta     $FF02
        bra     @out
@hsync
        lda     $FF00           ; (not enabled; clear it anyway)
@out
    }
}

#ifdef COCO3
void irq3(void);                /* src/coco3/sound3a.asm: the sound's timer, else irq_isr */
#define IRQ_TARGET _irq3
#else
#define IRQ_TARGET _irq_isr
#endif

void plat_irq_init(void)
{
    asm {
        orcc    #$50
        lda     #$7E            ; JMP irq_isr at the RAM vector ($010C)
        sta     $010C
        leax    IRQ_TARGET,pcr
        stx     $010D
        lda     $FF01           ; PIA0 CA1 (hsync) interrupt off
        anda    #$FE
        sta     $FF01
        lda     $FF03           ; CB1 (field sync) on, falling edge
        ora     #$01
        anda    #$FD
        sta     $FF03
        lda     $FF02
        andcc   #$EF            ; IRQ on (FIRQ stays off)
    }
}

#ifndef COCO3
/* Which artifact colour is which: show a bar and let the player say. */
static void colour_check(void)
{
    uint8_t i;
    gfx_begin();
    gfx_bar();
    gfx_row(6, ROW_BIG, PAL_MENU);
    centre(6, "IS IT BLUE?", 0, 20);
    gfx_row(14, ROW_TEXT, PAL_MENU);
    centre(14, "ENTER: YES   CLEAR: NO", 0, 40);
    gfx_show();
    for (i = 0; i < 240; ++i) {         /* 4 s, then on with it */
        frame_wait();
        input_poll();
        input_take();
        if (ev_btn || ev_key == KEY_RETURN)
            break;
    }
}
#endif

#ifdef COCO3
extern uint8_t gfx3_cmp;

/* Which monitor: R, C, or ENTER (or the button) for the palette showing;
 * CLEAR switches it here too (input.c). */
static void monitor_check(void)
{
    uint16_t i;
    gfx_begin();
    gfx_row(5, ROW_BIG, PAL_MENU);
    centre(5, "MONITOR?", 0, 20);
    gfx_row(9, ROW_TEXT, PAL_MENU);
    centre(9, "R: RGB    C: COMPOSITE", 0, 40);
    centre(11, "CLEAR SWITCHES IT LATER", 0, 40);
    centre(13, "ENTER: GO ON", 0, 40);
    gfx_show();
    for (i = 0; i < 360; ++i) {         /* 6 s, then on with it (RGB) */
        frame_wait();
        input_poll();
        input_take();
        if ((ev_key == 'R' || ev_key == 'C') && (ev_key == 'C') != gfx3_cmp)
            gfx_swap_colours();
        if (ev_key == 'R' || ev_key == 'C' || ev_key == KEY_RETURN || ev_btn)
            break;
    }
}
#endif

void plat_init(void)
{
    const char *err = bank_init();
    if (err) {
        gfx_boot_text(err);
        for (;;)
            ;
    }
#ifdef COCO3
    *(volatile uint8_t *)0xFFD9 = 0;    /* 1.79 MHz (HDB-DOS has it on already) */
#endif
    gfx_init();
    snd_init();
    input_init();
#ifdef COCO3
    monitor_check();
#else
    colour_check();
#endif
}

void plat_quit(void)
{
    asm {
        orcc    #$50
        clr     $0071           ; cold start
#ifndef COCO3
        sta     $FFDE           ; ROM back in
#endif
        jmp     [$FFFE]
    }
}

/* The server's round timer says what real time did: each second it ticks,
 * the jiffies counted since are set against 60, and what is missing goes
 * back on the clock, one a pass (a count that came late is made up by the
 * next one coming early). */
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
        asm { orcc #$10 }           /* the IRQ changes it too */
        ++frame_count;
        asm { andcc #$EF }
        --drift;
    }
}

void frame_wait(void)
{
    static uint16_t seen;
    uint16_t d;
    clock_check();
    d = frame_count - seen;
    if (d > CATCH_UP) {
        seen = frame_count - 1;
        d = 1;
    }
    if (d >= 2) {
        ++seen;                         /* behind: a pass with no drawing */
    } else {
        spr_flush();
        while (frame_count == seen)
            ;
        ++seen;
    }
    seed += frame_count;
    snd_tick();
}

uint8_t plat_rand(void)
{
    /* xorshift, stirred by the frame count */
    seed ^= seed << 7;
    seed ^= seed >> 9;
    seed ^= seed << 8;
    return (uint8_t)seed;
}

/*
 * sound3.c -- FujiBlast's sounds on the CoCo 3: the same effects and jingles
 * as the CoCo 1/2 (src/coco/sndtab.h), played by the GIME's timer: it
 * interrupts (IRQ) once a half wave of the tone, and src/coco3/sound3a.asm
 * flips the 6-bit DAC between the volume and 0 (noise: a step of an LFSR at
 * a limited rate). One voice, as on the CoCo 2: an effect cuts the jingle.
 * The envelopes and the notes are stepped here, once a pass for each jiffy
 * that passed (snd_tick, from frame_wait), so a late pass costs next to
 * nothing. The timer's interrupt is on only while something sounds.
 *
 * The joystick reads (input.c) change the DAC and the mux that picks the
 * sound source too, so they bracket themselves with snd_gate(), which holds
 * the timer's interrupt source off.
 */
#ifdef COCO3
#include "fbcoco.h"
#include "plat.h"
#include "sound.h"
#include "sndtab.h"

#define DAC    (*(volatile uint8_t *)0xFF20)
#define PIA1CB (*(volatile uint8_t *)0xFF23)
#define PIA0CA (*(volatile uint8_t *)0xFF01)
#define PIA0CB (*(volatile uint8_t *)0xFF03)
#define GIME_INIT0 (*(volatile uint8_t *)0xFF90)
#define GIME_INIT1 (*(volatile uint8_t *)0xFF91)
#define GIME_IRQEN (*(volatile uint8_t *)0xFF92)
#define GIME_TIMER_HI (*(volatile uint8_t *)0xFF94)
#define GIME_TIMER_LO (*(volatile uint8_t *)0xFF95)

/* sound3a.asm's voice */
uint8_t ph, hf, lev, lf = 1;

#ifndef NOISE_MIN
#define NOISE_MIN 1600          /* timer counts a noise step at least (~2.2 kHz): it is the IRQ rate that costs */
#endif

static uint8_t muted, on, running;
static uint8_t mode;            /* the vector's target now: 0 tone, 1 half-rate tone, 2 noise */
static uint16_t t_now;          /* the timer's count now */
static uint16_t last;           /* frame_count at the last snd_tick */
static const Step *fx_at;
static uint8_t fx_left, fx_c, fx_v, fx_kind;
static int8_t fx_dc, fx_dv;
static const Note *mu_at;
static uint8_t mu_left, mu_c, mu_v;
static uint16_t mu_v16, mu_dv;

void irq3(void), irq3h(void), irq3n(void);      /* sound3a.asm */

static void set_mode(uint8_t m)
{
    void (*f)(void) = m == 2 ? irq3n : m ? irq3h : irq3;
    mode = m;
    asm { orcc #$10 }                   /* (the vector's address is two bytes) */
    *(void (**)(void))0x010D = f;
    asm { andcc #$EF }
}

static void snd_irq(uint8_t go)
{
    if (go == running)
        return;
    running = go;
    if (go) {
        GIME_INIT0 = 0x6C;              /* the GIME's IRQ on, only while something sounds */
        GIME_IRQEN = 0x20;
    } else {
        GIME_IRQEN = 0;
        GIME_INIT0 = 0x4C;
        DAC = 0;
    }
}

/* The joystick reads need the DAC and mux: the timer's IRQ source is off
 * for them (not the CPU's masks: the keyboard IRQ goes on). */
void snd_gate(uint8_t go)
{
    if (go && on && !muted) {
        PIA0CA &= ~0x08;                /* the mux on the DAC */
        PIA0CB &= ~0x08;
        DAC = 0;
        PIA1CB |= 0x08;                 /* sound on */
        if (running)
            GIME_IRQEN = 0x20;
    } else {
        if (running)
            GIME_IRQEN = 0;
        PIA1CB &= ~0x08;
    }
}

void snd_init(void)
{
    lev = 0;
    GIME_INIT1 = 0x20;                  /* the timer counts the fast clock (279 ns) */
    on = 1;
    last = frame_count;
    snd_gate(1);
}

/* a half-wave delay c (the CoCo 1/2's: 8c+11 cycles of 0.895 MHz) is 32c+44
 * timer counts; the timer is 12 bits, so from 127 up the IRQ comes every
 * second half wave (half the count) */
static void sound_at(uint8_t m, uint16_t count)
{
    if (m != mode)
        set_mode(m);
    if (count != t_now) {
        t_now = count;
        GIME_TIMER_HI = count >> 8;
        GIME_TIMER_LO = (uint8_t)count;
    }
}

static void fx_step(void)
{
    if (!fx_at->frames) {
        fx_at = 0;
        return;
    }
    fx_kind = fx_at->kind;
    fx_left = fx_at->frames;
    fx_c = fx_at->c0;
    fx_dc = fx_at->dc;
    fx_v = fx_at->v0;
    fx_dv = fx_at->dv;
    ++fx_at;
}

void snd_play(uint8_t id)
{
    if (muted)
        return;
    fx_at = fxs[id];
    fx_step();
}

void snd_music(uint8_t id)
{
    if (muted)
        return;
    mu_at = mus[id];
    mu_left = 0;
}

void snd_stop(void)
{
    mu_at = 0;
    fx_at = 0;
}

void snd_toggle_mute(void)
{
    muted ^= 1;
    snd_stop();
    lev = 0;
    snd_irq(0);
    snd_gate(1);
}

/* once a pass: the envelopes and the notes a step on for each jiffy that
 * passed since (a late pass costs only this), and the timer set to what
 * sounds now. */
void snd_tick(void)
{
    uint8_t n = (uint8_t)(frame_count - last) > 8 ? 8 : (uint8_t)(frame_count - last);
    uint8_t v;
    uint16_t c;
    last = frame_count;
    if (muted)
        return;
    while (n--) {
        if (fx_at) {
            fx_c += fx_dc;
            fx_v += fx_dv;
            if (!--fx_left)
                fx_step();
        }
        if (mu_at) {                    /* its time goes on under an effect */
            if (!mu_left) {
                if (!mu_at->len) {
                    mu_at = 0;
                    continue;
                }
                mu_c = mu_at->c;
                mu_left = mu_at->len;
                mu_v16 = 56u << 8;
                mu_dv = (56u << 8) / mu_left;
                ++mu_at;
            }
            mu_v = mu_v16 >> 8;
            mu_v16 -= mu_dv;
            --mu_left;
        }
    }
    if (fx_at) {                        /* an effect cuts the jingle, as on the CoCo 2 */
        v = fx_v > 63 ? 63 : fx_v;
        lev = v << 2;
        c = fx_c ? fx_c : 1;
        if (fx_kind) {
            c = 32u * (c + 8);
            sound_at(2, c < NOISE_MIN ? NOISE_MIN : c);
        } else if (c > 126) {
            sound_at(1, 16u * c + 22);
        } else {
            sound_at(0, 32u * c + 44);
        }
        snd_irq(1);
    } else if (mu_at && mu_c && mu_left) {
        lev = mu_v << 2;
        c = mu_c;
        if (c > 126)
            sound_at(1, 16u * c + 22);
        else
            sound_at(0, 32u * c + 44);
        snd_irq(1);
    } else {
        snd_irq(0);
    }
}

/* the wait for the end of jiffy `seen` (the timer's IRQ does the sound) */
void snd_idle(uint8_t seen)
{
    while ((uint8_t)frame_count == seen)
        ;
}
#endif

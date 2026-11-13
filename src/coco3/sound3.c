/*
 * sound3.c -- FujiBlast's sounds on the CoCo 3: the same effects and jingles
 * as the CoCo 1/2 (src/coco/sndtab.h), but played by the GIME's timer: it
 * interrupts (IRQ) about 4400 times a second, and src/coco3/sound3a.asm adds
 * two voices into the 6-bit DAC (effects, jingles), so a jingle goes on under
 * an effect and the waves are steady. The envelopes and the notes are
 * stepped here, once a jiffy (snd_tick, from frame_wait). The timer's
 * interrupt is on only while something sounds.
 *
 * The joystick reads (input.c) change the DAC and the mux that picks the
 * sound source too, so they bracket themselves with snd_gate(), which also
 * holds the interrupt off.
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

uint16_t snd_slice;             /* (the CoCo 1/2's: not used here) */

/* sound3a.asm's voices */
uint16_t v1p, v1s, v2p, v2s;
uint8_t v1v, v1n, v2v, lf = 1;

static uint8_t muted, on, running;
static const Step *fx_at;
static uint8_t fx_left, fx_c, fx_v, fx_kind;
static int8_t fx_dc, fx_dv;
static const Note *mu_at;
static uint8_t mu_left, mu_len, mu_c;

/* a half-wave delay c (the CoCo 1/2's, tone ~ 55900 / c Hz) as the phase's step a sample (4380 Hz) */
static uint16_t step_of(uint8_t c)
{
    uint16_t d = c < 13 ? 13 : c;
    return (uint16_t)(836000UL / d);
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

void snd_gate(uint8_t go)
{
    if (go && on && !muted) {
        PIA0CA &= ~0x08;                /* the mux on the DAC */
        PIA0CB &= ~0x08;
        DAC = 0;
        PIA1CB |= 0x08;                 /* sound on */
        if (running) {
            asm { andcc #$BF }
        }
    } else {
        asm { orcc #$40 }               /* (the DAC is the joystick's now) */
        PIA1CB &= ~0x08;
    }
}

void snd_init(void)
{
    v1v = v2v = 0;
    GIME_INIT1 = 0x20;                  /* the timer counts the fast clock (279 ns) */
    GIME_TIMER_HI = 0x03;               /* 816 of them: 4.4 kHz */
    GIME_TIMER_LO = 0x30;
    on = 1;
    snd_gate(1);
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
    v1v = v2v = 0;
    snd_irq(0);
    snd_gate(1);
}

/* once a jiffy: the envelopes and the notes a step on */
void snd_tick(void)
{
    uint8_t v;
    if (muted)
        return;
    if (fx_at) {
        v = fx_v > 63 ? 63 : fx_v;
        v1n = fx_kind;
        v1v = v >> 1;
        v1s = step_of(fx_c);
        fx_c += fx_dc;
        fx_v += fx_dv;
        if (!--fx_left)
            fx_step();
    } else {
        v1v = 0;
    }
    v2v = 0;
    if (mu_at) {
        if (!mu_left) {
            if (!mu_at->len) {
                mu_at = 0;
                goto out;
            }
            mu_c = mu_at->c;
            mu_len = mu_left = mu_at->len;
            ++mu_at;
        }
        if (mu_c) {
            v = 56 - (uint8_t)((uint16_t)56 * (mu_len - mu_left) / mu_len);
            v2v = v >> 1;
            v2s = step_of(mu_c);
        }
        --mu_left;
    }
out:
    snd_irq(v1v || v2v);
}
#endif

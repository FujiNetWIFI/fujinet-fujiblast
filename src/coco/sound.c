/*
 * sound.c -- FujiBlast's sounds on the CoCo 1/2 and 3's 6-bit DAC (PIA1
 * port A, bits 7-2; $FF20).
 *
 * The CoCo 1/2 has no timer to play a sample from, so the CPU makes the
 * wave itself, a slice of a jiffy at a time (snd_tick, from frame_wait): a
 * tone is the DAC jumping between a volume and zero, each half a counted
 * delay; noise is the same with a delay that wanders. The volume
 * (the DAC's level) falls a step each jiffy, so the effects have a
 * decay, which the 1-bit speaker's don't. The slice is short in the arena
 * (the game needs the CPU: about 2400 cycles a jiffy) and long on the menus,
 * where the jingles are all but continuous.
 *
 * Two voices as on the Atari: effects (one at a time) and jingles; an effect
 * takes over from the jingle while it plays, which goes on under it.
 *
 * The joystick reads (input.c) change the DAC and the mux that picks the
 * sound source too, so they bracket themselves with snd_gate().
 *
 * The CoCo 3 (COCO3) plays the same data from a timer interrupt instead
 * (src/coco3/sound3.c): more voices, a steady wave.
 */
#ifndef COCO3
#include <string.h>
#include "fbcoco.h"
#include "plat.h"
#include "sound.h"

#define DAC    (*(volatile uint8_t *)0xFF20)
#define PIA1CB (*(volatile uint8_t *)0xFF23)
#define PIA0CA (*(volatile uint8_t *)0xFF01)
#define PIA0CB (*(volatile uint8_t *)0xFF03)

uint16_t snd_slice = 9000;      /* cycles a jiffy the sound may take (gfx.c: less in the arena) */

uint8_t ntab[256];
uint8_t snd_dbg[3];             /* effects started, tones, noises (the rigs peek them) */       /* noise: delays, 0-15 */
static uint8_t muted, on;

/* n waves of a tone: the DAC at hi for `count` loops (8 cycles each), then 0
 * for as long. (The arguments are read before anything is pushed, and U, the
 * frame pointer's, kept in the code.) */
static void tone(uint16_t count, uint8_t n, uint8_t hi)
{
    asm {
        stu     @su
        ldu     :count
        ldb     :n
        lda     :hi
@wave
        sta     $FF20
        tfr     u,x
@h      leax    -1,x
        bne     @h
        clr     $FF20
        tfr     u,x
@l      leax    -1,x
        bne     @l
        decb
        bne     @wave
        ldu     @su
        bra     @out
@su     fdb     0
@out
    }
}

/* n halves of noise: each a delay of base + ntab[i] loops, high then low */
static void noise(uint16_t base, uint8_t n, uint8_t hi)
{
    asm {
        stu     @su
        sty     @sy
        ldd     :base
        std     @tmp
        lda     :n
        sta     @cnt
        lda     :hi
        sta     @hi
        leay    _ntab,pcr
@half
        lda     @hi
        sta     $FF20
        ldb     ,y+
        clra
        addd    @tmp
        tfr     d,x
@h      leax    -1,x
        bne     @h
        clr     $FF20
        ldb     ,y+
        clra
        addd    @tmp
        tfr     d,x
@l      leax    -1,x
        bne     @l
        dec     @cnt
        bne     @half
        ldu     @su
        ldy     @sy
        bra     @out
@su     fdb     0
@sy     fdb     0
@tmp    fdb     0
@cnt    fcb     0
@hi     fcb     0
@out
    }
}

#include "sndtab.h"

static const Step *fx_at;       /* the effect: its step, */
static uint8_t fx_left, fx_c, fx_v;
static int8_t fx_dc, fx_dv;
static uint8_t fx_kind;
static const Note *mu_at;       /* the jingle: its note, */
static uint8_t mu_left, mu_len, mu_c;

void snd_gate(uint8_t go)
{
    if (go && on && !muted) {
        PIA0CA &= ~0x08;                /* the mux on the DAC */
        PIA0CB &= ~0x08;
        DAC = 0;
        PIA1CB |= 0x08;                 /* sound on */
    } else {
        PIA1CB &= ~0x08;
    }
}

void snd_init(void)
{
    uint16_t i, x = 0xACE1;
    for (i = 0; i < 256; ++i) {
        x = (x >> 1) ^ (x & 1 ? 0xB400 : 0);
        ntab[i] = x & 15;
    }
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
    ++snd_dbg[0];
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
    snd_gate(1);
}

/* once a jiffy: a slice of whatever is playing */
void snd_tick(void)
{
    uint8_t n, v;
    uint16_t c;
    if (muted)
        return;
    if (fx_at) {
        v = fx_v > 63 ? 63 : fx_v;
        if (fx_kind) {
            c = fx_c;
            n = (uint8_t)(snd_slice / (28u + 8u * (c + 8)) * 2);
            if (n > 250)
                n = 250;
            noise(c, n ? n : 2, v << 2);
            ++snd_dbg[2];
        } else {
            c = fx_c;
            n = (uint8_t)(snd_slice / (16u * c + 29u));
            tone(c, n ? n : 1, v << 2);
            ++snd_dbg[1];
        }
        fx_c += fx_dc;
        fx_v += fx_dv;
        if (!--fx_left)
            fx_step();
    }
    if (mu_at) {                        /* its time goes on under an effect */
        if (!mu_left) {
            if (!mu_at->len) {
                mu_at = 0;
                return;
            }
            mu_c = mu_at->c;
            mu_len = mu_left = mu_at->len;
            ++mu_at;
        }
        if (mu_c && !fx_at) {
            v = 56 - (uint8_t)((uint16_t)56 * (mu_len - mu_left) / mu_len);
            c = mu_c;
            n = (uint8_t)(snd_slice / (16u * c + 29u));
            tone(c, n ? n : 1, v << 2);
        }
        --mu_left;
    }
}
#endif

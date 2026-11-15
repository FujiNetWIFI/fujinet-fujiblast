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

uint8_t ntab[256];
uint8_t nidx;                   /* where the noise is in ntab */
uint8_t snd_dbg[3];             /* effects started, tones, noises (the rigs peek them) */
static uint8_t muted, on;

#ifndef SND_BURST
#define SND_BURST 600           /* cycles the sound plays on a drawn pass even with no idle time */
#endif

/* Play half-waves (the DAC jumping between hi and 0, each half `c` loops of
 * 8 cycles; noise: c + ntab[i]) until the jiffy `seen` is over (frame_count's
 * low byte moves on) and `budget` loops were played. The check is at each
 * edge, so the sound ends within a half wave of the jiffy. c is not 0. */
static void fill(uint8_t seen, uint8_t kind, uint16_t c, uint8_t hi, uint16_t budget)
{
    asm {
        stu     @su
        sty     @sy
        lda     :seen
        sta     @sn
        lda     :hi
        sta     @hv
        lda     :kind
        sta     @kd
        ldd     :c
        std     @c
        ldd     :budget
        std     @bud
        clr     @lvl
        leay    _ntab,pcr
@half
        lda     @lvl
        sta     $FF20
        eora    @hv
        sta     @lvl
        ldx     @c
        tst     @kd
        beq     @go
        ldb     _nidx
        inc     _nidx
        clra
        ldb     b,y
        addd    @c
        tfr     d,x
@go     stx     @d
@dl     leax    -1,x
        bne     @dl
        ldd     @d              ; budget -= the half (loops) + 4
        addd    #4
        std     @t
        ldd     @bud
        subd    @t
        bhs     @has
        clra
        clrb
@has    std     @bud
        bne     @half
        lda     _frame_count+1  ; played enough: out when the jiffy is over
        cmpa    @sn
        beq     @half
        clr     $FF20
        ldu     @su
        ldy     @sy
        bra     @out
@su     fdb     0
@sy     fdb     0
@c      fdb     0
@d      fdb     0
@t      fdb     0
@bud    fdb     0
@sn     fcb     0
@hv     fcb     0
@kd     fcb     0
@lvl    fcb     0
@out
    }
}

#include "sndtab.h"

static const Step *fx_at;       /* the effect: its step, */
static uint8_t fx_left, fx_c, fx_v;
static int8_t fx_dc, fx_dv;
static uint8_t fx_kind;
static const Note *mu_at;       /* the jingle: its note, */
static uint8_t mu_left, mu_c, mu_v;
static uint16_t mu_v16, mu_dv;  /* its volume (8.8), falling over the note */
static uint16_t last;           /* frame_count at the last snd_tick */

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
    last = frame_count;
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

/* once a pass: the envelopes and the notes a step on for each jiffy that
 * passed since (so a pass that is late or has no drawing costs only this, no
 * sound, and the effects keep their length). snd_idle plays them. */
void snd_tick(void)
{
    uint8_t n = (uint8_t)(frame_count - last) > 8 ? 8 : (uint8_t)(frame_count - last);
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
}

/* the wait for the end of jiffy `seen`, which the sound plays in */
void snd_idle(uint8_t seen)
{
    uint8_t v;
    if (!muted && fx_at) {
        v = fx_v > 63 ? 63 : fx_v;
        if (fx_kind) {
            fill(seen, 1, fx_c ? fx_c : 1, v << 2, SND_BURST / 8);
            ++snd_dbg[2];
        } else {
            fill(seen, 0, fx_c ? fx_c : 1, v << 2, SND_BURST / 8);
            ++snd_dbg[1];
        }
        return;
    }
    if (!muted && mu_at && mu_c && mu_left) {
        fill(seen, 0, mu_c, mu_v << 2, SND_BURST / 8);
        return;
    }
    while ((uint8_t)frame_count == seen)
        ;
}
#endif

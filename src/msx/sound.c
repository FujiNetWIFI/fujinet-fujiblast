/*
 * sound.c -- the effects (voice 1: tone channel B, or the noise) and
 * jingles (voice 0: tone channel A) on the MSX's PSG, an AY-3-8910 (ports
 * A0h and A1h), stepped a frame at a time by the interrupt (msxint.asm,
 * snd_nmi), so a note ends on time however long the game is busy. The
 * tunes are the Adam's (src/tms9918/tunes.h): its divisor is the AY's
 * period, and its attenuation (2 dB a step) a volume 15 - a.
 *
 * The interrupt is the only thing that writes the PSG while a tune goes;
 * the BIOS's own PSG routines (the joystick) hold the interrupt off.
 */
#ifdef __MSX__

#include "sound.h"
#include "msx.h"
#include "tunes.h"

__sfr __at 0xA0 PSG_ADDR;
__sfr __at 0xA1 PSG_DATA;

#define MIX_OFF 0xBF            /* tones and noise off; port B out, port A in */

static const Note *vp[2];       /* the tune going on each voice */
static uint8_t vf[2];           /* frames left of its note */
static uint8_t muted;
static uint8_t mix = MIX_OFF;

static void psg(uint8_t r, uint8_t v)
{
    PSG_ADDR = r;
    PSG_DATA = v;
}

static void quiet(uint8_t v)
{
    psg(8 + v, 0);
    mix |= 0x09 << v;           /* tone v and noise v off */
    psg(7, mix);
}

static void put(uint8_t v, const Note *n)
{
    uint8_t rate;
    if (n->d & 0x8000) {
        rate = 8 << (n->d & 3);                 /* 8, 16, 32, 64: the AY has 5 bits */
        psg(6, rate > 31 ? 31 : rate);
        mix = (mix | (1 << v)) & ~(8 << v);     /* noise on, tone off */
    } else if (!n->d) {
        quiet(v);
        return;
    } else {
        psg(v * 2, (uint8_t)n->d);
        psg(v * 2 + 1, (uint8_t)(n->d >> 8));
        mix = (mix | (8 << v)) & ~(1 << v);     /* tone on, noise off */
    }
    psg(7, mix);
    psg(8 + v, 15 - n->a);
}

/* One frame of both voices (called from the interrupt). */
void snd_nmi(void)
{
    uint8_t v;
    const Note *n;
    for (v = 0; v < 2; ++v) {
        if (!vp[v])
            continue;
        if (vf[v] > 1) {
            --vf[v];
            continue;
        }
        n = vp[v]++;
        if (!n->f) {
            vp[v] = 0;
            quiet(v);
            continue;
        }
        put(v, n);
        vf[v] = n->f;
    }
}

static void start(uint8_t v, const Note *t)
{
    __asm__("di");              /* (the interrupt would step it half set) */
    vp[v] = t;
    vf[v] = 1;
    __asm__("ei");
}

void snd_init(void)
{
    uint8_t v;
    mix = MIX_OFF;
    for (v = 0; v < 2; ++v)
        psg(8 + v, 0);
    psg(7, mix);
}

void snd_play(uint8_t id)
{
    if (!muted)
        start(1, fx[id]);
}

void snd_music(uint8_t id)
{
    if (!muted)
        start(0, mus[id]);
}

void snd_stop(void)
{
    __asm__("di");
    vp[0] = vp[1] = 0;
    quiet(0);
    quiet(1);
    __asm__("ei");
}

void snd_toggle_mute(void)
{
    muted ^= 1;
    snd_stop();
}

#endif

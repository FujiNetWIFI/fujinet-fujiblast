/*
 * sound.c -- the effects (voice 1: pulse 1, or the noise channel) and
 * jingles (voice 0: pulse 2) on the NES's APU, stepped a frame at a time by
 * frame_wait (snd_step).
 *
 * The tunes are the Adam's and the MSX's (src/tms9918/tunes.h): a note is a
 * divisor (111861 / Hz), frames and an attenuation (0 loud, 15 off). The
 * pulse channel's timer for f Hz is 1789773 / 16 / f - 1, which is the
 * divisor less one. A frame count of 0 ends the tune.
 */
#ifdef __NES__

#include "sound.h"
#include "tunes.h"

#define APU(n)     (*(volatile uint8_t *)(0x4000 + (n)))
#define APU_STATUS (*(volatile uint8_t *)0x4015)

static const Note *vp[2];       /* the tune going on each voice */
static uint8_t vf[2];           /* frames left of its note */
static uint8_t muted;
uint8_t snd_calls, snd_notes;   /* for a check: tunes started, notes sent */

static void quiet(uint8_t v)
{
    if (v) {
        APU(0) = 0x30;          /* pulse 1 and noise off */
        APU(12) = 0x30;
    } else {
        APU(4) = 0x30;
    }
}

static void put(uint8_t v, const Note *n)
{
    uint8_t vol = 15 - n->a, base = v ? 0 : 4;
    uint16_t t;
    if (n->d & 0x8000) {
        APU(0) = 0x30;
        APU(12) = 0x30 | vol;                   /* noise: halt, constant volume */
        APU(14) = ((n->d & 4) ? 0 : 0x80) | (6 + (n->d & 3) * 3);
        APU(15) = 0x08;
    } else if (!n->d) {
        quiet(v);
    } else {
        t = n->d - 1;
        if (t > 0x7FF)
            t = 0x7FF;
        APU(base) = 0xB0 | vol;                 /* 50%, halt, constant volume */
        APU(base + 2) = (uint8_t)t;
        APU(base + 3) = (uint8_t)(t >> 8) | 0x08;
        if (v)
            APU(12) = 0x30;
    }
}

/* One frame of both voices. */
void snd_step(void)
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
        ++snd_notes;
        vf[v] = n->f;
    }
}

void snd_init(void)
{
    APU_STATUS = 0x0B;          /* pulse 1 and 2, noise */
    APU(0) = 0x30;
    APU(1) = 0x08;              /* no sweep */
    APU(4) = 0x30;
    APU(5) = 0x08;
    APU(12) = 0x30;
}

void snd_play(uint8_t id)
{
    ++snd_calls;
    if (muted)
        return;
    vp[1] = fx[id];
    vf[1] = 1;
}

void snd_music(uint8_t id)
{
    if (muted)
        return;
    vp[0] = mus[id];
    vf[0] = 1;
}

void snd_stop(void)
{
    vp[0] = vp[1] = 0;
    quiet(0);
    quiet(1);
}

void snd_toggle_mute(void)
{
    muted ^= 1;
    snd_stop();
}

#endif

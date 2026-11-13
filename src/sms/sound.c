/*
 * sound.c -- the effects (voice 1: tone channel 1, or the noise channel)
 * and jingles (voice 0: tone channel 0) on the SMS's SN76489 (port 7Fh, the
 * Adam's at the same clock: src/adam/sound.c), stepped a frame at a time by
 * the frame interrupt (smsint.asm, snd_nmi), so a note ends
 * on time however long the game is busy.
 *
 * A note is a divisor (3579545 / 32 / Hz; 0 a rest; bit 15 the noise
 * channel, its control in the low bits), frames and an attenuation (0
 * loud, 15 off). A frame count of 0 ends the tune.
 */
#ifdef __SMS__

#include "sound.h"

__sfr __at 0x7F SN_PORT;

#include "tunes.h"

static const Note *vp[2];       /* the tune going on each voice */
static uint8_t vf[2];           /* frames left of its note */
static uint8_t muted;

static void quiet(uint8_t v)
{
    if (v) {
        SN_PORT = 0xBF;         /* tone 1 and noise off */
        SN_PORT = 0xFF;
    } else {
        SN_PORT = 0x9F;
    }
}

static void put(uint8_t v, const Note *n)
{
    uint8_t ch = v ? 0x20 : 0;
    if (n->d & 0x8000) {
        SN_PORT = 0xBF;
        SN_PORT = 0xE0 | (n->d & 7);
        SN_PORT = 0xF0 | n->a;
    } else if (!n->d) {
        quiet(v);
    } else {
        SN_PORT = 0x80 | ch | (n->d & 15);
        SN_PORT = (n->d >> 4) & 0x3F;
        SN_PORT = 0x90 | ch | n->a;
        if (v)
            SN_PORT = 0xFF;
    }
}

/* One frame of both voices (called from the NMI). */
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
    __asm
    di
__endasm;                  /* (the interrupt would step it half set) */
    vp[v] = t;
    vf[v] = 1;
    __asm
    ei
__endasm;
}

void snd_init(void)
{
    uint8_t v;
    for (v = 0; v < 4; ++v)
        SN_PORT = 0x9F | (v << 5);
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
    __asm
    di
__endasm;
    vp[0] = vp[1] = 0;
    quiet(0);
    quiet(1);
    __asm
    ei
__endasm;
}

void snd_toggle_mute(void)
{
    muted ^= 1;
    snd_stop();
}

#endif

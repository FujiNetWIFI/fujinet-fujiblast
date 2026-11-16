/*
 * sound.c -- the effects (voice 1: tone channel 1, or the noise channel)
 * and jingles (voice 0: tone channel 0) on the Adam's SN76489 (port FFh), stepped
 * a frame at a time by the NMI (nmi.asm, snd_nmi), so a note ends
 * on time however long the game is busy.
 *
 * A note is a divisor (3579545 / 32 / Hz; 0 a rest; bit 15 the noise
 * channel, its control in the low bits), frames and an attenuation (0
 * loud, 15 off). A frame count of 0 ends the tune.
 */
#ifdef __ADAM__

#include "sound.h"
#include "adam.h"

__sfr __at 0xFF SN_PORT;

#include "tunes.h"

static const Note *vp[2];       /* the tune going on each voice */
static uint8_t vf[2];           /* frames left of its note */
static uint8_t muted;           /* 0; 2 no loop; 3 nothing (PLAT_MUSIC) */
static uint8_t fxnoise;         /* an effect has the noise channel: the drums keep out */

static void quiet(uint8_t v)
{
    if (v) {
        SN_PORT = 0xBF;         /* tone 1 off */
        if (fxnoise) {
            SN_PORT = 0xFF;     /* and the noise, if an effect had it */
            fxnoise = 0;
        }
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
        fxnoise = 1;
    } else if (!n->d) {
        quiet(v);
    } else {
        SN_PORT = 0x80 | ch | (n->d & 15);
        SN_PORT = (n->d >> 4) & 0x3F;
        SN_PORT = 0x90 | ch | n->a;
        if (v && fxnoise) {
            SN_PORT = 0xFF;     /* (not a drum's) */
            fxnoise = 0;
        }
    }
}

#ifdef PLAT_MUSIC
/*
 * The round's music (music/play.txt, tools/mkmus_sms.py): two slots of
 * steps A, B, frames. Slot 0 (ch1) is tone 2 (the bass and kick) or the
 * noise channel (snare, hat; skipped while an effect has the noise); slot 1
 * (ch2, the chatter) is tone 0. Codes in the frames byte: $FE call, $FD
 * return, $FF jump.
 */
extern const uint8_t m_loop1[], m_loop2[];

static const uint8_t *mp[2];    /* the next step; 0 = the slot is off */
static const uint8_t *mr[2];    /* where a call returns */
static uint8_t ml[2];           /* frames left of the step */

static void mus_step(void)
{
    uint8_t s, a, b, n;
    const uint8_t *p;
    for (s = 0; s < 2; ++s) {
        p = mp[s];
        if (!p)
            continue;
        if (ml[s] > 1) {
            --ml[s];
            continue;
        }
        for (;;) {
            a = p[0];
            b = p[1];
            n = p[2];
            p += 3;
            if (n == 0xFE) {
                mr[s] = p;
                p = (const uint8_t *)(a | (uint16_t)b << 8);
            } else if (n == 0xFD) {
                p = mr[s];
            } else if (n == 0xFF) {
                p = (const uint8_t *)(a | (uint16_t)b << 8);
            } else {
                break;
            }
        }
        mp[s] = p;
        ml[s] = n;
        if (s) {                                /* tone 0 */
            if (b) {
                SN_PORT = 0x80 | (a & 15);
                SN_PORT = b & 0x3F;
                SN_PORT = 0x90 | (a >> 4);
            } else {
                SN_PORT = 0x9F;
            }
        } else if (b & 0x80) {                  /* a drum */
            SN_PORT = 0xDF;
            if (!fxnoise) {
                SN_PORT = 0xE0 | (a & 7);
                SN_PORT = 0xF0 | (a >> 4);
            }
        } else {
            if (!fxnoise)
                SN_PORT = 0xFF;
            if (b) {                            /* the bass */
                SN_PORT = 0xC0 | (a & 15);
                SN_PORT = b & 0x3F;
                SN_PORT = 0xD0 | (a >> 4);
            } else {
                SN_PORT = 0xDF;
            }
        }
    }
}
#endif

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
#ifdef PLAT_MUSIC
    mus_step();
#endif
}

static void start(uint8_t v, const Note *t)
{
    VDP_LOCK();                 /* (the NMI would step it half set) */
    vp[v] = t;
    vf[v] = 1;
    VDP_UNLOCK();
}

void snd_init(void)
{
    uint8_t v;
    for (v = 0; v < 4; ++v)
        SN_PORT = 0x9F | (v << 5);
}

void snd_play(uint8_t id)
{
    if (!(muted & 1))
        start(1, fx[id]);
}

#ifdef PLAT_MUSIC
/* The loop's slots off and quiet (NMI locked out). */
static void music_off(void)
{
    mp[0] = mp[1] = 0;
    SN_PORT = 0xDF;             /* tone 2 off, and tone 0 */
    SN_PORT = 0x9F;
    if (!fxnoise)
        SN_PORT = 0xFF;
}

/* The round's music, both channels from the top on the same frame; or off.
 * Already playing, it plays on. A jingle ends. */
void snd_loop(uint8_t on)
{
    if (on && mp[0] && !vp[0])
        return;
    if (muted)
        on = 0;
    VDP_LOCK();
    vp[0] = 0;
    music_off();
    if (on) {
        mr[0] = mr[1] = 0;
        ml[0] = ml[1] = 1;
        mp[0] = m_loop1;
        mp[1] = m_loop2;
    }
    VDP_UNLOCK();
}
#endif

void snd_music(uint8_t id)
{
    if (muted & 1)
        return;
    VDP_LOCK();
#ifdef PLAT_MUSIC
    music_off();                /* the jingle has tone 0 */
#endif
    vp[0] = mus[id];
    vf[0] = 1;
    VDP_UNLOCK();
}

void snd_stop(void)
{
    VDP_LOCK();
    vp[0] = vp[1] = 0;
#ifdef PLAT_MUSIC
    music_off();
#endif
    quiet(0);
    quiet(1);
    VDP_UNLOCK();
}

/* all, the effects (no music), none */
void snd_toggle_mute(void)
{
#ifdef PLAT_MUSIC
    muted = muted == 0 ? 2 : muted == 2 ? 3 : 0;
    if (muted == 3)
        snd_stop();
    else if (muted)
        snd_loop(0);
#else
    muted ^= 1;
    snd_stop();
#endif
}

#endif

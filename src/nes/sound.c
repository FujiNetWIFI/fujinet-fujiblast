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
extern uint8_t snd_ptr[4];      /* snd.s: the NMI's player of the loop */
#pragma zpsym("snd_ptr")
extern uint8_t snd_left[4];
extern uint8_t snd_mute;        /* bit 0: all off; bit 1: no loop */
extern uint8_t snd_hold;
extern uint8_t snd_loopon;
extern uint8_t snd_fxnoise;     /* an effect has the noise channel: the drums keep out */
extern uint8_t snd_hi2;
extern uint8_t snd_p2on;
uint8_t snd_calls, snd_notes;   /* for a check: tunes started, notes sent */

static void quiet(uint8_t v)
{
    if (v) {
        APU(0) = 0x30;          /* pulse 1 off, and the noise unless a drum has it */
    } else {
        APU(4) = 0x30;
    }
    if (snd_fxnoise) {
        APU(12) = 0x30;
        snd_fxnoise = 0;
    }
}

static void put(uint8_t v, const Note *n)
{
    uint8_t vol = 15 - n->a, base = v ? 0 : 4;
    uint16_t t;
    if (n->d & 0x8000) {
        APU(0) = 0x30;
        snd_fxnoise = 1;
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
        if (snd_fxnoise) {
            APU(12) = 0x30;                     /* (not a drum's) */
            snd_fxnoise = 0;
        }
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
    APU_STATUS = 0x0F;          /* pulse 1 and 2, triangle, noise */
    APU(8) = 0x80;              /* triangle off */
    snd_ptr[1] = snd_ptr[3] = 0;
    APU(0) = 0x30;
    APU(1) = 0x08;              /* no sweep */
    APU(4) = 0x30;
    APU(5) = 0x08;
    APU(12) = 0x30;
}

/* The loop's slots, quiet (the NMI's hold keeps it off them meanwhile). */
static void music_off(void)
{
    snd_hold = 1;
    snd_ptr[1] = snd_ptr[3] = 0;
    snd_left[0] = snd_left[2] = 0;
    snd_loopon = 0;
}

/* The NMI reads a slot's pointer only while its high byte is set: that
 * goes last. */
static void start(uint8_t v, const uint8_t *s)
{
    snd_ptr[v + 1] = 0;
    snd_left[v] = 0;
    snd_ptr[v] = (uint8_t)(uint16_t)s;
    snd_ptr[v + 1] = (uint16_t)s >> 8;
}

void snd_play(uint8_t id)
{
    ++snd_calls;
    if (snd_mute & 1)
        return;
    vp[1] = fx[id];
    vf[1] = 1;
}

void snd_music(uint8_t id)
{
    if (snd_mute & 1)
        return;
    music_off();
    snd_hold = 0;
    snd_p2on = 0;               /* pulse 2 is the jingle's now */
    vp[0] = mus[id];
    vf[0] = 1;
}

#ifdef PLAT_MUSIC
extern const uint8_t m_loop1[], m_loop2[];

/* The round's music, both channels from the top on the same frame; or off.
 * Already playing, it plays on. A jingle ends. */
void snd_loop(uint8_t on)
{
    if (on && snd_loopon)
        return;
    music_off();
    if (on) {
        vp[0] = 0;
        APU(4) = 0x30;
        snd_hi2 = 0xFF;         /* (the first note writes the wave's restart) */
        start(0, m_loop1);
        start(2, m_loop2);
        snd_loopon = 2;
    }
    snd_hold = 0;
}
#endif

void snd_stop(void)
{
    music_off();
    snd_hold = 0;
    vp[0] = vp[1] = 0;
    quiet(0);
    quiet(1);
}

/* all, the effects (no music), none */
void snd_toggle_mute(void)
{
    snd_mute = snd_mute == 0 ? 2 : snd_mute == 2 ? 3 : 0;
    if (snd_mute == 3)
        snd_stop();
}

#endif

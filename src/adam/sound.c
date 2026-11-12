/*
 * sound.c -- the effects (voice 1: tone channel 1, or the noise channel)
 * and jingles (voice 0: tone channel 0) on the Adam's SN76489 (port FFh),
 * stepped a frame at a time by the NMI (nmi.asm, snd_nmi), so a note ends
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

typedef struct {
    uint16_t d;
    uint8_t f, a;
} Note;

#define HZ(f)  ((uint16_t)(111861UL / (f)))
#define NOISE(c) (0x8000 | (c))
#define END    { 0, 0, 0 }

/* the Atari's notes (src/atari/sound.c), as divisors */
#define G4 HZ(392)
#define A4 HZ(440)
#define B4 HZ(494)
#define C5 HZ(523)
#define D5 HZ(587)
#define E5 HZ(659)
#define G5 HZ(784)
#define A5 HZ(880)
#define C6 HZ(1047)

#define NT(n, l) { n, (l) - 3, 2 }, { n, 3, 8 }     /* a note, and its fall */
#define RS(l)    { 0, l, 15 }

static const Note s_drop[]   = { { 300, 2, 2 }, { 220, 2, 4 }, { 160, 3, 6 }, END };
static const Note s_boom[]   = { { NOISE(6), 3, 0 }, { NOISE(6), 3, 2 }, { NOISE(6), 4, 4 }, { NOISE(6), 4, 6 },
                                 { NOISE(6), 5, 8 }, { NOISE(6), 6, 10 }, { NOISE(6), 6, 12 }, { NOISE(6), 8, 14 }, END };
static const Note s_pickup[] = { { G4, 3, 2 }, { B4, 3, 2 }, { D5, 3, 2 }, { G5, 5, 2 }, END };
static const Note s_kick[]   = { { 600, 2, 2 }, { 420, 2, 4 }, END };
static const Note s_throw[]  = { { 140, 2, 4 }, { 180, 2, 3 }, { 240, 2, 3 }, { 360, 3, 4 }, END };
static const Note s_fizzle[] = { { NOISE(5), 6, 4 }, { NOISE(5), 8, 7 }, { NOISE(5), 10, 10 }, { NOISE(5), 10, 13 }, END };
static const Note s_die[]    = { { 190, 4, 2 }, { 230, 4, 2 }, { 270, 4, 3 }, { 330, 5, 4 }, { 400, 6, 4 },
                                 { 470, 6, 5 }, { 560, 8, 6 }, { 700, 10, 8 }, END };
static const Note s_beep[]   = { { A4, 6, 3 }, END };
static const Note s_go[]     = { { A5, 16, 2 }, END };
static const Note s_select[] = { { C5, 2, 4 }, END };

static const Note m_title[] = { NT(G4, 8), NT(C5, 8), NT(E5, 8), NT(G5, 16), NT(E5, 8), NT(G5, 24),
                                RS(8), NT(A5, 8), NT(G5, 8), NT(E5, 8), NT(C6, 32), END };
static const Note m_round[] = { NT(C5, 6), NT(C5, 6), NT(G5, 18), END };
static const Note m_win[]   = { NT(E5, 8), NT(G5, 8), NT(C6, 24), RS(4), NT(G5, 8), NT(C6, 32), END };
static const Note m_game[]  = { NT(C5, 8), NT(E5, 8), NT(G5, 8), NT(C6, 16), NT(A5, 8), NT(C6, 16),
                                NT(G5, 8), NT(E5, 8), NT(G5, 8), NT(C6, 40), END };

static const Note *const fx[SND_COUNT] = {
    s_drop, s_boom, s_pickup, s_kick, s_throw, s_fizzle, s_die, s_beep, s_go, s_select,
};
static const Note *const mus[MUS_COUNT] = { m_title, m_round, m_win, m_game };

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
    VDP_LOCK();
    vp[0] = vp[1] = 0;
    quiet(0);
    quiet(1);
    VDP_UNLOCK();
}

void snd_toggle_mute(void)
{
    muted ^= 1;
    snd_stop();
}

#endif

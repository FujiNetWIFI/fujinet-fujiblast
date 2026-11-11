/*
 * sound.c -- the PC speaker: the Atari's effects and jingles (src/atari/
 * sound.c's steps: AUDF, AUDC, frames), stepped by the 60 Hz timer
 * interrupt (plat.c) and played on the PIT's channel 2, which makes the
 * square wave by itself.
 *
 * The speaker has one voice: an effect takes it, and a jingle goes on in
 * time underneath and is heard again when the effect ends. A pure tone at
 * POKEY's AUDF N is 63921 / (2 (N + 1)) Hz, a divisor of 37.33 (N + 1) at
 * the PIT's 1193182 Hz. POKEY's noise ($8x) becomes a tone whose divisor
 * wanders at random each jiffy; its buzz ($Cx) is a plain tone. Volume 0
 * is silence, any other volume full: the speaker has no other.
 *
 * On a PCjr, port 61h bits 5-6 pick what the speaker plays: 00 is the
 * PIT's channel 2.
 */
#ifdef __MSDOS__

#include <conio.h>
#include <i86.h>
#include "sound.h"
#include "msdos.h"

/* notes (POKEY AUDF values) */
#define G4 81
#define A4 72
#define B4 64
#define C5 60
#define D5 53
#define E5 47
#define F5 45
#define G5 40
#define A5 35
#define B5 31
#define C6 29

#define N(f, l) f, 0xA8, (l) - 3, f, 0xA4, 3       /* a note, and its fall */
#define R(l)    0, 0xA0, l                          /* a rest */
#define END     0, 0, 0

static const uint8_t s_drop[]   = { 0xC0, 0xA8, 2, 0xE0, 0xA6, 2, 0xF0, 0xA3, 3, END };
static const uint8_t s_boom[]   = { 40, 0x8F, 3, 60, 0x8E, 3, 80, 0x8C, 4, 100, 0x8A, 4, 140, 0x88, 5,
                                    180, 0x86, 6, 220, 0x84, 6, 255, 0x82, 8, END };
static const uint8_t s_pickup[] = { G4, 0xAA, 3, B4, 0xAA, 3, D5, 0xAA, 3, G5, 0xAA, 5, END };
static const uint8_t s_kick[]   = { 30, 0xC8, 2, 60, 0xC6, 2, END };
static const uint8_t s_throw[]  = { 120, 0x84, 2, 90, 0x86, 2, 60, 0x86, 2, 40, 0x84, 3, END };
static const uint8_t s_fizzle[] = { 20, 0x84, 6, 25, 0x83, 8, 30, 0x82, 10, 35, 0x81, 10, END };
static const uint8_t s_die[]    = { 60, 0xAA, 4, 72, 0xAA, 4, 85, 0xA9, 4, 100, 0xA8, 5, 121, 0xA7, 6,
                                    144, 0xA6, 6, 170, 0xA5, 8, 200, 0xA4, 10, END };
static const uint8_t s_beep[]   = { A4, 0xA8, 6, END };
static const uint8_t s_go[]     = { A5, 0xAA, 16, END };
static const uint8_t s_select[] = { C5, 0xA6, 2, END };

static const uint8_t m_title[] = { N(G4, 8), N(C5, 8), N(E5, 8), N(G5, 16), N(E5, 8), N(G5, 24),
                                   R(8), N(A5, 8), N(G5, 8), N(E5, 8), N(C6, 32), END };
static const uint8_t m_round[] = { N(C5, 6), N(C5, 6), N(G5, 18), END };
static const uint8_t m_win[]   = { N(E5, 8), N(G5, 8), N(C6, 24), R(4), N(G5, 8), N(C6, 32), END };
static const uint8_t m_game[]  = { N(C5, 8), N(E5, 8), N(G5, 8), N(C6, 16), N(A5, 8), N(C6, 16),
                                   N(G5, 8), N(E5, 8), N(G5, 8), N(C6, 40), END };

static const uint8_t *const fx[SND_COUNT] = {
    s_drop, s_boom, s_pickup, s_kick, s_throw, s_fizzle, s_die, s_beep, s_go, s_select,
};
static const uint8_t *const mus[MUS_COUNT] = { m_title, m_round, m_win, m_game };

/* Two voices, as the Atari's: 0 the jingle, 1 the effect. The interrupt
 * owns them once ptr is set. */
static const uint8_t *volatile ptr[2];
static uint8_t left[2], cur_f[2], cur_c[2];
static uint8_t mute, sounding;
static uint16_t sounded;        /* the divisor last given the PIT */
static uint16_t rnd = 0xACE1;

static void spk_off(void)
{
    if (sounding) {
        outp(0x61, inp(0x61) & 0xFC);
        sounding = 0;
    }
}

static void spk_on(uint16_t div)
{
    if (div != sounded) {
        outp(0x43, 0xB6);       /* channel 2, lo/hi, mode 3 */
        outp(0x42, div & 0xFF);
        outp(0x42, div >> 8);
        sounded = div;
    }
    if (!sounding) {
        outp(0x61, inp(0x61) | 3);
        sounding = 1;
    }
}

/* One voice's jiffy: 1 if it has a step sounding now. */
static uint8_t step(uint8_t v)
{
    const uint8_t *p;
    if (!left[v]) {
        p = ptr[v];
        if (!p)
            return 0;
        if (!p[2]) {
            ptr[v] = 0;
            return 0;
        }
        cur_f[v] = p[0];
        cur_c[v] = p[1];
        left[v] = p[2];
        ptr[v] = p + 3;
    }
    --left[v];
    return (cur_c[v] & 0x0F) != 0;
}

/* From the timer interrupt, 60 times a second. */
void snd_isr(void)
{
    uint8_t m = step(0), e = step(1), v;
    uint16_t div;
    if (mute || !(m | e)) {
        spk_off();
        return;
    }
    v = e ? 1 : 0;
    div = (uint16_t)(cur_f[v] + 1) * 37;
    if ((cur_c[v] & 0xF0) == 0x80) {    /* noise: wander */
        rnd = rnd >> 1 ^ (-(rnd & 1) & 0xB400);
        div = (div >> 1) + (uint16_t)(((uint32_t)rnd * div) >> 16);    /* 0.5-1.5x */
    }
    spk_on(div);
}

static void start(uint8_t v, const uint8_t *s)
{
    _disable();
    ptr[v] = s;
    left[v] = 0;
    _enable();
}

void snd_init(void)
{
    if (plat_pcjr)
        outp(0x61, inp(0x61) & 0x9F);   /* the speaker plays the PIT */
    sounding = 1;
    spk_off();
}

void snd_play(uint8_t id)
{
    start(1, fx[id]);
}

void snd_music(uint8_t id)
{
    start(0, mus[id]);
}

void snd_stop(void)
{
    _disable();
    ptr[0] = ptr[1] = 0;
    left[0] = left[1] = 0;
    _enable();
}

void snd_toggle_mute(void)
{
    mute ^= 1;
}

void snd_off(void)
{
    snd_stop();
    _disable();
    sounding = 1;
    spk_off();
    _enable();
}

#endif

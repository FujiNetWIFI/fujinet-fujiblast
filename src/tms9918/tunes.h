/*
 * tunes.h -- FujiBlast's effects and jingles as notes, for the Adam's
 * SN76489 and the MSX's AY-3-8910 (both clocked from the colour carrier at
 * half rate, so one divisor is one period: 111861 / Hz).
 *
 * A note is a divisor (0 a rest; bit 15 the noise channel, its control in
 * the low bits: bit 2 white, bits 0-1 the rate), frames and an attenuation
 * (0 loud, 15 off). A frame count of 0 ends the tune.
 */
#ifndef TUNES_H
#define TUNES_H

#include <stdint.h>
#include "sound.h"

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

#endif

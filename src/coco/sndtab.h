/* sndtab.h -- FujiBlast's sounds on the CoCo (sound.c, src/coco3/sound3.c). A
 * delay c is a half wave of 8 c + 11 cycles of the CoCo 1/2's 0.895 MHz, a
 * tone of about 55900 / c Hz. */
#ifndef SNDTAB_H
#define SNDTAB_H

/* a sound: steps of a kind, frames long, the delay (loops of 8 cycles) and
 * volume (0-63) at the start and how they change a jiffy */
typedef struct {
    uint8_t kind, frames;       /* 0 tone, 1 noise */
    uint8_t c0;                 /* delay */
    int8_t dc;
    uint8_t v0;
    int8_t dv;
} Step;

#define T 0
#define N 1
#define END { 0, 0, 0, 0, 0, 0 }

static const Step s_drop[]   = { { T, 5, 60, 2, 46, -8 }, END };
static const Step s_boom[]   = { { N, 16, 3, 1, 63, -3 }, END };
static const Step s_pickup[] = { { T, 2, 70, 0, 46, 0 }, { T, 2, 52, 0, 50, 0 }, { T, 4, 38, 0, 56, -9 }, END };
static const Step s_kick[]   = { { T, 4, 150, -12, 60, -12 }, END };
static const Step s_throw[]  = { { T, 6, 30, 6, 44, -5 }, END };
static const Step s_fizzle[] = { { N, 6, 2, 0, 36, -5 }, END };
static const Step s_die[]    = { { T, 18, 50, 6, 56, -3 }, END };
static const Step s_beep[]   = { { T, 3, 66, 0, 40, 0 }, END };
static const Step s_go[]     = { { T, 3, 70, 0, 50, 0 }, { T, 6, 40, 0, 60, -8 }, END };
static const Step s_select[] = { { T, 1, 55, 0, 36, 0 }, END };
static const Step *const fxs[SND_COUNT] = {
    s_drop, s_boom, s_pickup, s_kick, s_throw, s_fizzle, s_die, s_beep, s_go, s_select,
};

/* the jingles: a note is its delay (50 C6 .. 140 G4) and how long; 0 delay a rest.
 * Each note's volume falls from 56 over its length. */
typedef struct { uint8_t c, len; } Note;
#define G4 140
#define A4 124
#define B4 110
#define C5 104
#define D5 92
#define E5 82
#define G5 68
#define A5 60
#define C6 50
#define R 0
static const Note m_title[] = {
    { G4, 8 }, { C5, 8 }, { E5, 8 }, { G5, 16 }, { E5, 8 }, { G5, 24 }, { R, 8 },
    { A5, 8 }, { G5, 8 }, { E5, 8 }, { C6, 32 }, { 0, 0 } };
static const Note m_round[] = { { C5, 6 }, { C5, 6 }, { G5, 18 }, { 0, 0 } };
static const Note m_win[] = { { E5, 8 }, { G5, 8 }, { C6, 24 }, { R, 4 }, { G5, 8 }, { C6, 32 }, { 0, 0 } };
static const Note m_game[] = {
    { C5, 8 }, { E5, 8 }, { G5, 8 }, { C6, 16 }, { A5, 8 }, { C6, 16 }, { G5, 8 }, { E5, 8 }, { G5, 8 },
    { C6, 40 }, { 0, 0 } };
static const Note *const mus[MUS_COUNT] = { m_title, m_round, m_win, m_game };


#endif

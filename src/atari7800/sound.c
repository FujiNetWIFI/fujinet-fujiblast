/*
 * sound.c (Atari 7800: the cart's POKEY, src/atari7800/snd.s) -- the effects
 * (voice 2), the jingles (voice 1) and the in-play music (gen/atari7800/
 * music.s from music/play.txt: ch1 on voice 1, ch2 on voice 3) as POKEY steps
 * (snd.s). AUDF at POKEY's 64 kHz clock: 63921 / (2 (N + 1)) Hz. AUDC: $A0
 * a pure tone, $80 noise, $C0 a buzz; the low nibble the volume.
 * The step data is in bank 1 (SNDDATA) with the player: only the frame NMI
 * reads it, so the pointer tables below stay in the fixed bank.
 */
#ifdef __ATARI7800__

#include "sound.h"

extern uint8_t snd_ptr[6];
#pragma zpsym("snd_ptr")
extern uint8_t snd_left[6];
extern uint8_t snd_mute;
extern uint8_t snd_hold;
extern uint8_t snd_loopon;

/* notes */
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

#pragma rodata-name(push, "SNDDATA")
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
#pragma rodata-name(pop)

static const uint8_t *const fx[SND_COUNT] = {
    s_drop, s_boom, s_pickup, s_kick, s_throw, s_fizzle, s_die, s_beep, s_go, s_select,
};
static const uint8_t *const mus[MUS_COUNT] = { m_title, m_round, m_win, m_game };

/* The NMI reads a slot's pointer only while its high byte is set: that
 * goes last. */
static void start(uint8_t v, const uint8_t *s)
{
    snd_ptr[v + 1] = 0;
    snd_left[v] = 0;
    snd_ptr[v] = (uint8_t)(uint16_t)s;
    snd_ptr[v + 1] = (uint16_t)s >> 8;
}

/* slots 0 and 4 quiet (the NMI's hold keeps it off them meanwhile) */
static void music_off(void)
{
    snd_hold = 1;
    snd_ptr[1] = snd_ptr[5] = 0;
    snd_left[0] = snd_left[4] = 0;
    snd_loopon = 0;
}

void snd_init(void)
{
    snd_ptr[1] = snd_ptr[3] = snd_ptr[5] = 0;
    *(volatile uint8_t *)0x045F = 0;    /* SKCTL: reset, then the clocks on */
    *(volatile uint8_t *)0x045F = 3;
    *(volatile uint8_t *)0x0458 = 0;    /* AUDCTL: the 64 kHz clock */
}

void snd_play(uint8_t id)
{
    start(2, fx[id]);
}

void snd_music(uint8_t id)
{
    music_off();
    start(0, mus[id]);
    snd_hold = 0;
}

#ifdef PLAT_MUSIC
extern const uint8_t m_loop1[], m_loop2[];

/* The round's music, both channels from the top on the same frame; or off.
 * Already playing, it plays on. */
void snd_loop(uint8_t on)
{
    if (on && snd_loopon)
        return;
    music_off();
    if (on) {
        start(0, m_loop1);
        start(4, m_loop2);
        snd_loopon = 2;
    }
    snd_hold = 0;
}
#endif

void snd_stop(void)
{
    music_off();
    snd_ptr[3] = 0;
    snd_left[2] = 0;
    snd_hold = 0;
}

/* all, the effects (no music), none */
void snd_toggle_mute(void)
{
    snd_mute = snd_mute == 0 ? 2 : snd_mute == 2 ? 3 : 0;
}

#endif

/*
 * sound.c -- FujiBlast's sounds on the Apple II.
 *
 * With a Mockingboard (mock.s), the Atari's own effects and jingles. Without
 * one, the speaker: it only clicks while the CPU times it, so an effect is
 * a short blip, a slice of a jiffy at a time (snd_tick, from frame_wait),
 * and there are no jingles. A delay d is a tone of 1020000 / (28 * d) Hz.
 */
#ifdef __APPLE2__

#include <stdint.h>
#include "sound.h"
#include "plat.h"

void snd_tone(uint8_t delay, uint8_t n);   /* lowlevel.s */

/* mock.s, sndtab.s (in aux RAM: addresses only) */
uint8_t mb_detect(void);
void __fastcall__ mb_start(uint8_t voice, const void *steps);
void mb_tick(void);
void mb_quiet(void);
extern uint8_t mb_slot;
#pragma zpsym("mb_slot")
extern const uint8_t s_drop[], s_boom[], s_pickup[], s_kick[], s_throw[], s_fizzle[];
extern const uint8_t s_die[], s_beep[], s_go[], s_select[];
extern const uint8_t m_title[], m_round[], m_win[], m_game[];
static const uint8_t *const mb_fx[SND_COUNT] = {
    s_drop, s_boom, s_pickup, s_kick, s_throw, s_fizzle, s_die, s_beep, s_go, s_select,
};
static const uint8_t *const mb_mus[MUS_COUNT] = { m_title, m_round, m_win, m_game };

/* ~2 ms of a jiffy: a slice's clicks = 2000 / (14 * delay) */
#define SLICE_CYCLES 2000u

typedef struct {
    uint8_t d1, f1;             /* delay, jiffies */
    uint8_t d2, f2;             /* then a second tone (f2 0: none) */
} Effect;

static const Effect fx[SND_COUNT] = {
    {  60,  2,   0,  0 },       /* drop */
    { 200,  3, 255,  4 },       /* boom: low, lower */
    {  30,  2,  22,  2 },       /* pickup: up */
    {  90,  1,   0,  0 },       /* kick */
    {  45,  2,   0,  0 },       /* throw */
    { 160,  3,   0,  0 },       /* fizzle */
    {  80,  3, 140,  5 },       /* die: down */
    {  35,  1,   0,  0 },       /* beep */
    {  25,  4,   0,  0 },       /* go */
    {  40,  1,   0,  0 },       /* select */
};

static uint8_t cur_d, cur_f, cur_n, next_d, next_f, muted;

/* clicks in a slice of a tone (a division: once a tone, not each tick) */
static void tone(uint8_t d, uint8_t f)
{
    uint8_t n = (uint8_t)(SLICE_CYCLES / (14u * d));
    cur_d = d;
    cur_f = f;
    cur_n = n ? n : 1;
}

void snd_init(void)
{
    cur_f = next_f = 0;
    mb_detect();
}

void snd_play(uint8_t id)
{
    const Effect *e = &fx[id];
    if (muted || id >= SND_COUNT)
        return;
    if (mb_slot) {
        mb_start(1, mb_fx[id]);
        return;
    }
    tone(e->d1, e->f1);
    next_d = e->d2;
    next_f = e->f2;
}

void snd_music(uint8_t id)
{
    if (mb_slot && !muted)
        mb_start(0, mb_mus[id]);
}

void snd_stop(void)
{
    cur_f = next_f = 0;
    if (mb_slot)
        mb_quiet();
}

void snd_toggle_mute(void)
{
    muted ^= 1;
    cur_f = next_f = 0;
    if (mb_slot)
        mb_quiet();
}

void snd_tick(void)
{
    if (mb_slot) {
        mb_tick();
        return;
    }
    if (!cur_f)
        return;
    snd_tone(cur_d, cur_n);
    if (--cur_f == 0 && next_f) {
        tone(next_d, next_f);
        next_f = 0;
    }
}

#endif

/*
 * sound.c -- the H89's one sound: the H19's bell (BEL), a fixed beep. It
 * rings for a blast, a bomber lost, the countdown and the go, at most
 * every RING_GAP jiffies; the other effects and the jingles are silent.
 * M (screens.c) or f1 turns it off and on.
 */
#include "sound.h"
#include "plat.h"
#include "h19.h"

#define RING_GAP 20

static uint8_t muted;
static uint16_t rung;

void snd_init(void)
{
}

void snd_play(uint8_t id)
{
    if (muted)
        return;
    if (id != SND_BOOM && id != SND_DIE && id != SND_BEEP && id != SND_GO)
        return;
    if ((uint16_t)(frame_count - rung) < RING_GAP)
        return;
    rung = frame_count;
    h19_bell();
}

void snd_music(uint8_t id)
{
    (void)id;
}

void snd_stop(void)
{
}

void snd_toggle_mute(void)
{
    muted ^= 1;
}

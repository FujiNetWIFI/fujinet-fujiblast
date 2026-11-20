/* Pure, bounded sequencer: one or two 60 Hz ticks per video frame. */
#include <string.h>
#include "sequencer.h"
#include "music.h"
#include "../tms9918/tunes.h"

typedef struct { const Note *next; uint8_t left; Note note; } Jingle;
typedef struct { const FbMusicStep *next; uint8_t left, order; FbMusicStep note; } Loop;
static Jingle jingle, effect;
static Loop loop[2];
static uint8_t looping, muted, fraction, refresh_hz;
static uint32_t clock_hz;

void fb_audio_stop(void)
{
    memset(&jingle, 0, sizeof jingle);
    memset(&effect, 0, sizeof effect);
    memset(loop, 0, sizeof loop);
    looping = 0;
}
void fb_audio_init(unsigned refresh)
{
    fb_audio_stop();
    muted = fraction = 0;
    refresh_hz = refresh == 50 ? 50 : 60;
    clock_hz = refresh_hz == 50 ? 3546895UL : 3579545UL;
}
void fb_audio_effect(uint8_t id)
{
    if (id >= SND_COUNT || muted) return;
    memset(&effect, 0, sizeof effect);
    effect.next = fx[id];
}
void fb_audio_jingle(uint8_t id)
{
    if (id >= MUS_COUNT || muted) return;
    looping = 0;
    memset(&jingle, 0, sizeof jingle);
    jingle.next = mus[id];
}
void fb_audio_loop(uint8_t on)
{
    unsigned i;
    if (on && looping) return;
    looping = !!on;
    if (on) {
        memset(&jingle, 0, sizeof jingle);
        memset(loop, 0, sizeof loop);
        for (i = 0; i < 2; ++i) loop[i].next = fb_music_order[0][i];
    }
}
void fb_audio_toggle(void) { muted ^= 1; }
uint8_t fb_audio_muted(void) { return muted; }

static void jiffy(Jingle *v)
{
    if (v->left) --v->left;
    if (v->left || !v->next) return;
    v->note = *v->next++;
    v->left = v->note.f;
    if (!v->left) v->next = 0;
}
static void loop_jiffy(unsigned channel)
{
    Loop *v = &loop[channel];
    if (v->left) --v->left;
    if (v->left) return;
    if (!v->next->frames) {
        if (++v->order == fb_music_count) v->order = fb_music_loopat;
        v->next = fb_music_order[v->order][channel];
    }
    v->note = *v->next++;
    v->left = v->note.frames;
}
static void put_note(const Jingle *v, FbAudioVoice *tone, FbAudioVoice *noise)
{
    static const uint8_t volume[16] = {64,51,40,32,25,20,16,13,10,8,6,5,4,3,2,0};
    uint16_t divisor = v->note.d;
    if (!v->left || !divisor) return;
    if (divisor & 0x8000) {
        /* Shared PSG noise controls: successive rates halve the sample rate. */
        noise->period = (uint16_t)(clock_hz / (14000UL >> (divisor & 3)));
        noise->volume = volume[v->note.a & 15];
    } else {
        tone->period = (uint32_t)divisor * clock_hz / (111861UL * 8);
        if (tone->period < 124) tone->period = 124;
        tone->volume = volume[v->note.a & 15];
    }
}
void fb_audio_frame(FbAudioVoice output[4])
{
    unsigned i;
    fraction += 60;
    while (fraction >= refresh_hz) {
        fraction -= refresh_hz;
        jiffy(&jingle); jiffy(&effect);
        if (looping) for (i = 0; i < 2; ++i) loop_jiffy(i);
    }
    for (i = 0; i < 4; ++i) { output[i].period = 124; output[i].volume = 0; }
    if (muted) return;
    if (looping) {
        for (i = 0; i < 2; ++i) {
            const FbMusicStep *n = &loop[i].note;
            FbAudioVoice *v = &output[n->noise ? 3 : i];
            v->period = refresh_hz == 50 ? n->pal : n->ntsc;
            v->volume = n->volume;
        }
    } else put_note(&jingle, &output[0], &output[3]);
    /* Effects have their own tone voice and priority over percussion noise. */
    put_note(&effect, &output[2], &output[3]);
}

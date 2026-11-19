#ifndef FB_AMIGA_SEQUENCER_H
#define FB_AMIGA_SEQUENCER_H
#include <stdint.h>
typedef struct { uint16_t period; uint8_t volume; } FbAudioVoice;
/* Task-side commands must exclude the vblank caller. No OS calls here. */
void fb_audio_init(unsigned refresh);
void fb_audio_effect(uint8_t id);
void fb_audio_jingle(uint8_t id);
void fb_audio_loop(uint8_t on);
void fb_audio_stop(void);
void fb_audio_toggle(void);
uint8_t fb_audio_muted(void);
void fb_audio_frame(FbAudioVoice output[4]);
#endif

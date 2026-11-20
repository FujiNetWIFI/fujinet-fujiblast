#ifndef FB_AMIGA_MUSIC_H
#define FB_AMIGA_MUSIC_H
#include <stdint.h>
/* Generated periods for an eight-byte cycle, duration in 60 Hz jiffies. */
typedef struct { uint16_t pal, ntsc; uint8_t frames, volume, noise; } FbMusicStep;
extern const FbMusicStep *const fb_music_order[][2];
extern const uint8_t fb_music_count, fb_music_loopat;
#endif

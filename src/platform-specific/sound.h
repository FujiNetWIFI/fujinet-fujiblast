/* sound.h -- two voices: effects on one, short jingles on the other (on the
 * Atari, netstream clocks the serial port with POKEY's other two). A
 * platform with PLAT_MUSIC also plays the round's music (music/play.txt):
 * its ch1 on the jingles' voice, its ch2 on the effects' between effects. */
#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

enum {
    SND_DROP, SND_BOOM, SND_PICKUP, SND_KICK, SND_THROW, SND_FIZZLE,
    SND_DIE, SND_BEEP, SND_GO, SND_SELECT,
    SND_COUNT
};
enum { MUS_TITLE, MUS_ROUND, MUS_WIN, MUS_GAME, MUS_COUNT };

void snd_init(void);
void snd_play(uint8_t id);      /* an effect */
void snd_music(uint8_t id);     /* a jingle */
void snd_stop(void);
void snd_toggle_mute(void);     /* PLAT_MUSIC: all, effects only, none */

#ifdef PLAT_MUSIC
#ifdef __MSDOS__
typedef struct { uint16_t d; uint8_t n; } MusStep;     /* gen/msdosmus/music.c */
#endif
void snd_loop(uint8_t on);      /* the round's music, from the top; or off */
#else
#define snd_loop(on) ((void)0)
#endif

#endif

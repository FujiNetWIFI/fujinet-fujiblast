/* sound.h -- two voices: effects on one, short jingles on the other (on the
 * Atari, netstream clocks the serial port with POKEY's other two). */
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
void snd_toggle_mute(void);

#endif

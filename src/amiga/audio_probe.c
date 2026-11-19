/* Focused guest acceptance. No foreground frame_wait while sound plays. */
#ifdef AMIGA_AUDIO
#include <stdio.h>
#include <stdlib.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "amiga.h"
#include "sound.h"
extern volatile unsigned long snd_probe_frames, snd_probe_changes, snd_probe_errors;
extern uint8_t snd_probe_ready(void);
static void check(int ok, const char *message)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", message); fflush(stdout);
    if (!ok) exit(20);
}
static void stage(const char *name, unsigned seconds)
{
    unsigned long before = snd_probe_frames;
    printf("AUDIO %s\n", name); fflush(stdout);
    Delay(seconds * 50UL); /* DOS ticks remain 50 Hz on PAL and NTSC. */
    check(snd_probe_frames - before >= seconds * (SysBase->VBlankFrequency - 1),
          "vblank audio advances while foreground sleeps");
    check(snd_probe_errors == 0, "audio.device commands succeed");
}
void amiga_audio_test(void)
{
    unsigned i;
    check(snd_probe_ready(), "four audio channels allocated");
    snd_music(MUS_TITLE); stage("title", 3);
    snd_music(MUS_ROUND); stage("round", 1);
    snd_music(MUS_WIN); stage("win", 2);
    snd_music(MUS_GAME); stage("game", 3);
    snd_loop(1); stage("loop including wrap", 28);
    for (i=0;i<SND_COUNT;++i) { snd_play(i); stage("effect over loop", 1); }
    snd_toggle_mute(); check(snd_is_muted(), "mute state"); stage("muted", 2);
    snd_toggle_mute(); check(!snd_is_muted(), "unmute state"); stage("resumed loop", 2);
    snd_stop(); stage("stopped", 2);
    check(snd_probe_changes > 200, "score produces changing periods and volumes");
    snd_close(); check(!snd_probe_ready(), "audio closes");
    snd_init(); check(snd_probe_ready(), "audio channels reusable after close");
    snd_music(MUS_TITLE); stage("reopened title", 3);
    puts("FUJIBLAST AUDIO OK"); fflush(stdout); exit(0);
}
#endif

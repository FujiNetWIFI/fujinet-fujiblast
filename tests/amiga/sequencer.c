#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sound.h"
#include "amiga/sequencer.h"
#include "amiga/music.h"
static FbAudioVoice out[4], baseline[120][4];
static void frames(unsigned n) { while(n--) fb_audio_frame(out); }
static int silent(void) { return !(out[0].volume|out[1].volume|out[2].volume|out[3].volume); }
int main(void)
{
    unsigned i, j, count, both=0, drums=0;
    static const unsigned durations[] = {136,30,84,128};
    for (i=0;i<MUS_COUNT;++i) {
        fb_audio_init(60); fb_audio_jingle(i);
        frames(1); assert(out[0].volume && !out[2].volume);
        frames(durations[i]); assert(silent());
    }
    fb_audio_init(60); fb_audio_jingle(MUS_TITLE); frames(1);
    assert(out[0].period > 1100 && out[0].period < 1160);
    frames(8); assert(out[0].period > 830 && out[0].period < 880);
    fb_audio_stop(); frames(1); assert(silent());
    for(i=0;i<SND_COUNT;++i) {
        fb_audio_effect(i); frames(1); assert(out[2].volume || out[3].volume);
        frames(80); assert(silent());
    }
    fb_audio_loop(1);
    for(i=0;i<120;++i) {
        fb_audio_frame(baseline[i]);
        both += baseline[i][0].volume && baseline[i][1].volume;
        drums += baseline[i][3].volume != 0;
    }
    assert(both && drums);
    fb_audio_stop(); fb_audio_loop(1);
    for(i=0;i<120;++i) {
        if(i==12) fb_audio_effect(SND_BOOM);
        if(i==24) fb_audio_effect(SND_PICKUP);
        fb_audio_frame(out);
        for(j=0;j<2;++j) assert(out[j].period==baseline[i][j].period && out[j].volume==baseline[i][j].volume);
    }
    /* Repeated loop(1) must not restart the song. Mute advances it silently. */
    fb_audio_stop(); fb_audio_loop(1);
    for(i=0;i<120;++i) {
        fb_audio_loop(1);
        if(i==5 || i==65) fb_audio_toggle();
        fb_audio_frame(out);
        if(i>=5 && i<65) assert(silent());
        else for(j=0;j<4;++j) assert(out[j].period==baseline[i][j].period && out[j].volume==baseline[i][j].volume);
    }
    /* Every pattern's two voices have equal duration; full score loops. */
    count=0;
    for(i=0;i<fb_music_count;++i) {
        unsigned lengths[2]={0};
        for(j=0;j<2;++j) {
            const FbMusicStep *p=fb_music_order[i][j];
            for(;p->frames;++p) {
                assert(p->pal>=124 && p->ntsc>=124 && p->volume<=64);
                lengths[j]+=p->frames;
            }
        }
        assert(lengths[0]==lengths[1]); count+=lengths[0];
    }
    assert(count==1536 && fb_music_loopat==0);
    fb_audio_init(60); fb_audio_loop(1); frames(count);
    fb_audio_frame(out);
    assert(out[0].period==baseline[0][0].period && out[0].volume==baseline[0][0].volume);
    /* PAL has the same 60 Hz musical timeline after one second. */
    fb_audio_init(50); fb_audio_loop(1); frames(50);
    assert(out[0].volume==baseline[59][0].volume && out[1].volume==baseline[59][1].volume);
    for(j=0;j<2;++j) if(out[j].volume) {
        unsigned scaled=(unsigned)baseline[59][j].period*3546895.0/3579545.0;
        assert(out[j].period+1>=scaled && out[j].period<=scaled+1);
    }
    fb_audio_loop(0); frames(1); assert(silent());
    fb_audio_effect(SND_GO); fb_audio_loop(0); frames(1); assert(out[2].volume);
    fb_audio_jingle(MUS_WIN); frames(1); assert(out[0].volume && out[2].volume);
    fb_audio_stop(); fb_audio_jingle(255); fb_audio_effect(255); frames(1); assert(silent());
    puts("audio tunes, duration, two-voice loop, effects, mute, PAL/NTSC and stop passed");
}

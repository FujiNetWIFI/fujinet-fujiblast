/* Four continuous Paula DMA voices owned by audio.device. Only synchronous,
 * IOF_QUICK PERVOL commands run in the level-3 vblank handler; allocation,
 * abort/reap and freeing remain in task context. No streaming or disk I/O. */
#include <stdint.h>
#include <stdio.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <devices/audio.h>
#include <proto/exec.h>
#include <clib/alib_protos.h>
#include "sound.h"
#include "sequencer.h"
#include "amiga.h"
#define VOICES 4
#define NOISE_BYTES 2048
#define WAVE_BYTES (8 + NOISE_BYTES)
static struct MsgPort *port;
static struct IOAudio *owner, *write_req[VOICES];
static struct IOAudio control;
static BYTE *wave;
static uint8_t opened, pending[VOICES];
static volatile uint8_t ready;
static FbAudioVoice previous[VOICES];
#ifdef AMIGA_AUDIO
volatile unsigned long snd_probe_frames, snd_probe_changes, snd_probe_errors;
uint8_t snd_probe_ready(void) { return ready; }
#endif

void snd_stop(void)
{
    Disable(); fb_audio_stop(); Enable();
}
void snd_close(void)
{
    unsigned i;
    Disable(); ready = 0; fb_audio_stop(); Enable();
    for (i = 0; i < VOICES; ++i) {
        if (pending[i]) {
            if (!CheckIO((struct IORequest *)write_req[i]))
                AbortIO((struct IORequest *)write_req[i]);
            WaitIO((struct IORequest *)write_req[i]);
            pending[i] = 0;
        }
    }
    if (opened) CloseDevice((struct IORequest *)owner);
    opened = 0;
    for (i = 0; i < VOICES; ++i) {
        if (write_req[i]) DeleteExtIO((struct IORequest *)write_req[i]);
        write_req[i] = 0;
    }
    if (owner) DeleteExtIO((struct IORequest *)owner);
    if (port) DeletePort(port);
    if (wave) FreeMem(wave, WAVE_BYTES);
    owner = 0; port = 0; wave = 0;
}
void snd_init(void)
{
    static UBYTE channels[] = {15};
    uint16_t random = 0xACE1;
    unsigned i;
    fb_audio_init(SysBase->VBlankFrequency);
    port = CreatePort(0, 0);
    wave = AllocMem(WAVE_BYTES, MEMF_CHIP);
    if (!port || !wave) goto failed;
    for (i = 0; i < 8; ++i) wave[i] = i < 4 ? 96 : -96;
    for (i = 8; i < WAVE_BYTES; ++i) {
        random = (random >> 1) ^ (-(random & 1) & 0xB400);
        wave[i] = random & 1 ? 96 : -96;
    }
    owner = (struct IOAudio *)CreateExtIO(port, sizeof(*owner));
    if (!owner) goto failed;
    for (i = 0; i < VOICES; ++i) {
        write_req[i] = (struct IOAudio *)CreateExtIO(port, sizeof(*owner));
        if (!write_req[i]) goto failed;
    }
    owner->ioa_Request.io_Message.mn_Node.ln_Pri = 20;
    owner->ioa_Request.io_Flags = ADIOF_NOWAIT;
    owner->ioa_Data = channels;
    owner->ioa_Length = sizeof channels;
    if (OpenDevice((UBYTE *)"audio.device", 0, (struct IORequest *)owner, 0)) goto failed;
    opened = 1;
    control = *owner;
    control.ioa_Request.io_Command = ADCMD_PERVOL;
    for (i = 0; i < VOICES; ++i) {
        struct IOAudio *r = write_req[i];
        r->ioa_Request.io_Device = owner->ioa_Request.io_Device;
        r->ioa_AllocKey = owner->ioa_AllocKey;
        r->ioa_Request.io_Unit = (struct Unit *)(1UL << i);
        r->ioa_Request.io_Command = CMD_WRITE;
        r->ioa_Request.io_Flags = ADIOF_PERVOL;
        r->ioa_Data = (UBYTE *)(wave + (i == 3 ? 8 : 0));
        r->ioa_Length = i == 3 ? NOISE_BYTES : 8;
        r->ioa_Period = 124;
        r->ioa_Volume = 0;
        r->ioa_Cycles = 0;
        BeginIO((struct IORequest *)r);
        pending[i] = 1;
        previous[i].period = 124;
        previous[i].volume = 0;
        if (CheckIO((struct IORequest *)r) && r->ioa_Request.io_Error) goto failed;
    }
    ready = 1;
    return;
failed:
    snd_close();
    puts("FujiBlast: audio unavailable (needs four Paula channels)");
}
/* ADCMD_PERVOL is synchronous, supports level 3, and with IOF_QUICK never
 * queues a reply. Reset io_Unit because it is also an output field. */
void snd_vblank(void)
{
    FbAudioVoice output[VOICES];
    unsigned i;
    if (!ready) return;
#ifdef AMIGA_AUDIO
    ++snd_probe_frames;
#endif
    fb_audio_frame(output);
    for (i = 0; i < VOICES; ++i) {
        if (output[i].period == previous[i].period && output[i].volume == previous[i].volume)
            continue;
        control.ioa_Request.io_Unit = (struct Unit *)(1UL << i);
        control.ioa_Request.io_Flags = IOF_QUICK;
        control.ioa_Period = output[i].period;
        control.ioa_Volume = output[i].volume;
        BeginIO((struct IORequest *)&control);
#ifdef AMIGA_AUDIO
        ++snd_probe_changes;
        if (control.ioa_Request.io_Error || !(control.ioa_Request.io_Flags & IOF_QUICK)) ++snd_probe_errors;
#endif
        previous[i] = output[i];
    }
}
void snd_play(uint8_t id) { Disable(); fb_audio_effect(id); Enable(); }
void snd_music(uint8_t id) { Disable(); fb_audio_jingle(id); Enable(); }
void snd_loop(uint8_t on) { Disable(); fb_audio_loop(on); Enable(); }
void snd_toggle_mute(void) { Disable(); fb_audio_toggle(); Enable(); }
uint8_t snd_is_muted(void) { return fb_audio_muted(); }

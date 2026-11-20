#include <stdio.h>
#include <stdlib.h>
#include <exec/execbase.h>
#include <exec/interrupts.h>
#include <hardware/intbits.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <fujinet-nio.h>
#include "amiga.h"
#include "plat.h"
#include "gfx.h"
#include "input.h"
#include "sound.h"
#include "nstrans.h"
#include "controls.h"
#include "perf.h"
#include "worker.h"
#include "game.h"
uint16_t frame_count;
static uint8_t fraction, clock_added;
static struct Interrupt vblank;
static volatile uint16_t ticks;
static uint16_t consumed_ticks;
static BYTE old_priority;
static uint8_t priority_changed;
#ifdef AMIGA_PERF
static unsigned long totals[PERF_COUNT], calls[PERF_COUNT];
static unsigned long updates, dropped, all_updates, all_dropped;
static uint16_t window_start, last_frame, longest;
static unsigned frames;
uint16_t amiga_perf_clock(void) { return ticks; }
void amiga_perf_add(unsigned kind, uint16_t start)
{
    Forbid();
    totals[kind] += (uint16_t)(ticks - start);
    ++calls[kind];
    Permit();
}
void amiga_perf_steps(unsigned requested, unsigned applied)
{
    updates += applied;
    dropped += requested - applied;
    all_updates += applied;
    all_dropped += requested - applied;
}
void amiga_perf_movement(unsigned long *applied, unsigned long *discarded)
{
    *applied = all_updates; *discarded = all_dropped;
}
static void perf_frame(void)
{
    uint16_t now = ticks, dt = now - last_frame, span = now - window_start;
    unsigned hz = SysBase->VBlankFrequency, i;
    FILE *f;
    unsigned long fps10, ups10, measured[PERF_COUNT], counted[PERF_COUNT];
    last_frame = now;
    if (dt > longest)
        longest = dt;
    ++frames;
    if (span < hz * 5)
        return;
    fps10 = (unsigned long)frames * hz * 10 / span;
    ups10 = updates * hz * 10 / span;
    /* Worker counters overlap foreground work. Snapshot without losing an
     * increment to a task switch; never hold Forbid over DOS logging. */
    Forbid();
    for (i = 0; i < PERF_COUNT; ++i) {
        measured[i] = totals[i]; counted[i] = calls[i];
        totals[i] = calls[i] = 0;
    }
    Permit();
    f = fopen("FB:timing.log", "a");
    if (f)
    {
        fprintf(f, "ms=%lu frames=%u fps=%lu.%lu ups=%lu.%lu dropped=%lu max_loop_ms=%lu",
                (unsigned long)span * 1000 / hz, frames, fps10 / 10, fps10 % 10, ups10 / 10,
                ups10 % 10, dropped, (unsigned long)longest * 1000 / hz);
        for (i = 0; i < PERF_COUNT; ++i)
            fprintf(f, " %s_ms=%lu %s_calls=%lu",
                    i == 0   ? "render"
                    : i == 1 ? "read"
                             : "write",
                    measured[i] * 1000 / hz,
                    i == 0   ? "render"
                    : i == 1 ? "read"
                             : "write",
                    counted[i]);
        fputc('\n', f);
        fclose(f);
    }
    window_start = now;
    frames = longest = 0;
    updates = dropped = 0;

}
#endif
static ULONG clock_interrupt(void)
{
    ++ticks;
    snd_vblank();
    return 0;
}
static void cleanup(void)
{
    if (clock_added) {
        RemIntServer(INTB_VERTB, &vblank);
        clock_added = 0;
    }
    snd_close(); /* Stop continuous DMA before network shutdown can wait. */
    nst_close();
    amiga_worker_shutdown();
    if (priority_changed) SetTaskPri(FindTask(0), old_priority);
    input_close();
    gfx_close();
}
void amiga_fail(const char *why)
{
    printf("FujiBlast: %s\n", why);
    fflush(stdout);
    exit(20);
}
void plat_init(void)
{
#if !defined(TEST_SCENE) && !defined(AMIGA_AUDIO)
    int ready = amiga_worker_init();
    /* Register after fn_init's own atexit handler: shutdown must run in the
     * worker before the library's no-op fallback runs in this process. */
    atexit(cleanup);
    if (!ready)
        amiga_fail("Load fujinet-nio.device before starting the game");
#else
    atexit(cleanup);
#endif
#if !defined(TEST_SCENE) && !defined(AMIGA_AUDIO)
    /* Open the lazy broker backend before graphics/audio claim chip memory. */
    appkey_load();
#endif
    /* Wake the interactive loop promptly at vblank, ahead of CPU-bound
     * network parsing. Frame pacing yields to the workers when ahead of time. */
    old_priority = SetTaskPri(FindTask(0), 1);
    priority_changed = 1;
    gfx_init();
    input_init();
    snd_init();
    vblank.is_Node.ln_Type = NT_INTERRUPT;
    vblank.is_Node.ln_Name = (char *)"FujiBlast clock";
    vblank.is_Code = (void (*)())clock_interrupt;
    AddIntServer(INTB_VERTB, &vblank);
    clock_added = 1;
    consumed_ticks = ticks;
#ifdef AMIGA_AUDIO
    {
        extern void amiga_audio_test(void);
        amiga_audio_test();
    }
#endif
#ifdef AMIGA_LIVE
    {
        extern void amiga_live_test(void);
        amiga_live_test();
    }
#endif
}
void plat_quit(void)
{
#ifdef AMIGA_ACCEPT
    puts("FUJIBLAST QUIT OK");
    fflush(stdout);
#endif
    exit(0);
}
uint8_t plat_rand(void)
{
    return (uint8_t)rand();
}
void frame_wait(void)
{
#ifdef AMIGA_PERF
    perf_frame();
#endif
    /* Do not add another whole frame of sleep after a late render. */
    if (ticks == consumed_ticks) WaitTOF();
    {
        uint16_t now = ticks, elapsed = (uint16_t)(now - consumed_ticks);
        consumed_ticks = now;
        while (elapsed--)
            frame_count += amiga_jiffies(SysBase->VBlankFrequency, &fraction);
    }
    gfx_render();
#ifdef AMIGA_ACCEPT
    {
        static unsigned frames;
        ++frames;
        if (frames == 120 || frames == 240) {
            gfx_dump(frames == 120 ? "FB:screen120.bin" : "FB:screen240.bin");
        }
        if (frames == 300) {
            extern void gfx_verify(void);
            gfx_verify();
            puts("FUJIBLAST SCENE OK");
            exit(0);
        }
    }
#endif
}

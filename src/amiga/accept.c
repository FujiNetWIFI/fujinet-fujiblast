/* Test-only live acceptance; all game/session calls are production functions. */
#ifdef AMIGA_LIVE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "amiga.h"
#include "game.h"
#include "gfx.h"
#include "plat.h"
#include "sound.h"
#include "input.h"
#include "perf.h"
#include "worker.h"
#include "nio/servers.h"
#include <exec/execbase.h>
#include <proto/exec.h>
static void check(int ok, const char *why)
{
    if (!ok) {
        printf("FAIL %s\n", why);
        fflush(stdout);
        amiga_fail(why);
    }
    printf("PASS %s\n", why);
    fflush(stdout);
}
static uint8_t poll_state(void)
{
    uint8_t rc;
    frame_wait();
    rc = net_poll();
    if (rc == NET_LOST || rc == NET_ERROR)
        amiga_fail("Live network lost/error");
    return rc;
}
static void state(void)
{
    uint16_t start = frame_count;
    while ((uint16_t)(frame_count - start) < 600)
        if (poll_state() == NET_STATE)
            return;
    amiga_fail("State timeout");
}
#ifdef AMIGA_RESPONSIVE
static void responsive_test(void)
{
    uint16_t began, last, longest = 0, frames = 0;
    uint8_t origin = mv_x, fire_seen = 0, bomb_preview = 0;
    unsigned sums[5] = {0}, peak[5] = {0};
    uint16_t input_clock, max_input_gap = 0;
    unsigned long applied0, discarded0, applied1, discarded1;
    FILE *f;
    inp_hook = 0;
    /* Flush the screen transition before measuring steady-state work. */
    frame_wait();
    f = fopen("FB:keyboard", "w");
    check(f != 0, "keyboard request");
    fprintf(f, "%u", origin ? 0x4f : 0x4e);
    fclose(f);
    began = frame_count;
    do {
        frame_wait();
        input_poll();
        input_take();
    } while (!inp_dir && (uint16_t)(frame_count - began) < 180);
    check(inp_dir != 0, "real keyboard held");
    play_frame(); /* Consume time spent waiting for the injected key. */
    amiga_perf_movement(&applied0, &discarded0);
    began = last = frame_count;
    input_clock = amiga_perf_clock();
    while ((uint16_t)(frame_count - began) < 120) {
        uint16_t gap, t[6];
        unsigned j;
        uint8_t result;
        t[0] = amiga_perf_clock();
        frame_wait();
        t[1] = amiga_perf_clock();
        input_poll(); input_take();
        if (ev_btn) fire_seen = 1;
        t[2] = amiga_perf_clock();
        gap = (uint16_t)(t[2] - input_clock);
        if (gap > max_input_gap) max_input_gap = gap;
        input_clock = t[2];
        result = net_poll();
        t[3] = amiga_perf_clock();
        if (result == NET_STATE) play_state();
        t[4] = amiga_perf_clock();
        play_frame();
        if (pend_tile != NONE) bomb_preview = 1;
        t[5] = amiga_perf_clock();
        for (j = 0; j < 5; ++j) {
            unsigned d = (uint16_t)(t[j+1] - t[j]);
            sums[j] += d;
            if (d > peak[j]) peak[j] = d;
        }
        gap = frame_count - last;
        if (gap > longest) longest = gap;
        last = frame_count;
        ++frames;
    }
    amiga_perf_movement(&applied1, &discarded1);
    printf("INPUT max_poll_ms=%lu\n", (unsigned long)max_input_gap * 1000 / SysBase->VBlankFrequency);
    printf("MOVEMENT applied=%lu discarded=%lu\n", applied1-applied0, discarded1-discarded0);
    printf("STAGES wait_render=%u/%u input=%u/%u net=%u/%u state=%u/%u play=%u/%u (sum/max vblanks)\n",
           sums[0],peak[0],sums[1],peak[1],sums[2],peak[2],sums[3],peak[3],sums[4],peak[4]);
    printf("RESPONSIVE frames=%u ticks=%u max_gap=%u distance=%u\n",
           frames, (uint16_t)(frame_count - began), longest, abs((int)mv_x - origin));
    fflush(stdout);
    f = fopen("FB:keyboard-done", "w");
    if (f) { fputs("DONE", f); fclose(f); }
    check(applied1-applied0 >= 110 && discarded1-discarded0 <= 6, "movement maintains 60Hz logical cadence");
    check(fire_seen && bomb_preview, "real keyboard fire predicts bomb during stalled NIO");
    check(frames >= 40, "foreground at least 20 FPS during stalled NIO");
    check(longest <= 6 && max_input_gap <= SysBase->VBlankFrequency / 10, "foreground gap at most 100ms");
    check(abs((int)mv_x - origin) >= 16, "keyboard movement during stalled NIO");
    puts("FUJIBLAST RESPONSIVE OK");
    fflush(stdout);
    exit(0);
}
#endif
void amiga_live_test(void)
{
    unsigned i, seen_bomb = 0, moved = 0, rejoined = 0;
    uint8_t phase = 255, origin = 0, bomb_tile = 0, rc;
    uint16_t start;
    appkey_load();
    strcpy(my_name, "AMIGA");
    appkey_save_name();
    my_name[0] = 0;
    appkey_load();
    check(!strcmp(my_name, "AMIGA"), "NIO name persistence");
    {
        char original[FB_SERVER_TEXT], host[FB_HOST_MAX+1];
        struct fb_servers list = {0}, loaded;
        uint8_t data[FB_SERVERS_BYTES];
        uint16_t size;
        check(amiga_history_read(data, &size) && size == 0 &&
              fb_servers_decode(&loaded, data, size) && loaded.count == 0,
              "first setup without saved history");
        snprintf(original, sizeof original, "%s:%u", srv_host, (unsigned)srv_port);
        memset(host, 'a', sizeof host-1); host[sizeof host-1] = 0;
        check(fb_nio_save_setup("AMIGA", host) == 1, "save long native endpoint");
        strcpy(srv_table, "blast1");
        appkey_save_room();
        srv_host[0] = 0;
        appkey_load();
        check(!strcmp(srv_host, host) && !strcmp(srv_table, "blast1"), "native URL over 255 bytes");
        check(fb_servers_add(&list, host), "long history entry");
        fb_servers_encode(&list, data);
        check(amiga_history_write(data, sizeof data), "write chunked history");
        memset(data, 0, sizeof data);
        check(amiga_history_read(data, &size) && fb_servers_decode(&loaded, data, size) &&
              loaded.count == 1 && !strcmp(loaded.address[0], list.address[0]), "read chunked history");
        check(fb_nio_save_setup("AMIGA", original) == 1, "restore test endpoint");
    }
    check(net_tables() > 0, "room list");
    strcpy(srv_table, "blast1");
    appkey_save_room();
    net_hello();
    state();
    my_seat = NONE;
    for (i = 0; i < SEATS; ++i)
        if (!strncmp(NAME(i), "AMIGA", NAME_LEN))
            my_seat = i;
    check(my_seat != NONE, "join seat");
    check(st[OFF_HOST] == my_seat, "human host");
    wait_enter();
    frame_wait();
    gfx_dump("FB:live-room.bin");
    net_act(ACT_ROUNDS, 1, 0);
    net_act(ACT_START, 0, 0);
    inp_hook = 1;
    start = frame_count;
    while ((uint16_t)(frame_count - start) < 10800) {
        rc = poll_state();
        if (st[OFF_PHASE] != phase) {
            phase = st[OFF_PHASE];
            printf("PHASE %u\n", phase);
            fflush(stdout);
            if (phase == PH_COUNTDOWN || phase == PH_PLAY)
                play_enter();
            else if (phase == PH_ROUNDEND || phase == PH_GAMEOVER)
                results_enter(phase == PH_GAMEOVER);
        }
        if (phase == PH_PLAY) {
#ifdef AMIGA_RESPONSIVE
            responsive_test();
#endif
            if (rc == NET_STATE)
                play_state();
            inp_hook_btn = 0;
            if (!moved) {
                origin = mv_x;
                inp_hook_dir = origin ? IN_LEFT : IN_RIGHT;
                moved = 1;
            } else if (moved == 1 && abs((int)mv_x - origin) >= 16) {
                inp_hook_dir = 0;
                if (PL(my_seat, P_X) == mv_x) {
                    check(1, "server movement");
                    bomb_tile = centre_tile(mv_x, mv_y);
                    inp_hook_btn = 1;
                    moved = 2;
                }
            }
            input_poll();
            input_take();
            play_frame();
            if (moved == 2 && MAPT(bomb_tile) == T_BOMB && !seen_bomb) {
                seen_bomb = 1;
                check(1, "server bomb");
                frame_wait();
                gfx_dump("FB:live-arena.bin");
                /* The host relay cuts the TCP connection without SC_BYE. */
                {
                    FILE *cut = fopen("FB:cut", "w");
                    uint16_t began = frame_count;
                    uint8_t result = NET_NONE;
                    check(cut != 0, "request connection cut");
                    fputs("CUT", cut);
                    fclose(cut);
                    while ((uint16_t)(frame_count - began) < 600 && result != NET_LOST) {
                        frame_wait();
                        result = net_poll();
                    }
                    check(result == NET_LOST, "detect disconnected stream");
                }
                check(net_open(), "reconnect open");
                net_hello();
                state();
                check(SEATED(my_seat) && !strncmp(NAME(my_seat), "AMIGA", NAME_LEN), "rejoin seat");
                rejoined = 1;
            }
        }
        if (phase == PH_ROUNDEND || phase == PH_GAMEOVER)
            results_frame();
        if (phase == PH_GAMEOVER) {
            frame_wait();
            gfx_dump("FB:live-results.bin");
            break;
        }
    }
    check(seen_bomb && moved == 2 && rejoined, "movement bomb reconnect");
    check(phase == PH_GAMEOVER, "completed game");
    net_leave();
    net_close();
    check(net_open(), "open after leave");
    net_hello();
    state();
    check(net_heard, "server state after leave");
    net_leave();
    net_close();
    appkey_clear_room();
    my_name[0] = 0;
    appkey_load();
    check(!strcmp(my_name, "AMIGA") && !srv_table[0], "persist name and clear room");
    puts("FUJIBLAST LIVE OK");
    fflush(stdout);
    exit(0);
}
#endif

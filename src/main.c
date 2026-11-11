/*
 * main.c -- FujiBlast: a bomb-maze game for two to four players on FujiNet.
 *
 * The lobby's appkeys say who we are and where to play (appkey.c); else we
 * ask, and list the rooms. At a table, the server's state says which screen
 * to show: the waiting room, the arena (countdown and play), the trophies.
 */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "input.h"
#include "plat.h"
#include "sound.h"

#define JOIN_WAIT 600           /* frames for the first state before giving up */

enum { S_NONE, S_WAIT, S_ARENA, S_ROUND, S_GAME };

uint8_t my_seat = NONE;

static uint8_t find_seat(void)
{
    uint8_t s;
    for (s = 0; s < SEATS; ++s)
        if (SEATED(s) && !strncmp(NAME(s), my_name, NAME_LEN))
            return s;
    return NONE;
}

static uint8_t screen_for(void)
{
    switch (st[OFF_PHASE]) {
    case PH_COUNTDOWN:
    case PH_PLAY:
        return S_ARENA;
    case PH_ROUNDEND:
        return S_ROUND;
    case PH_GAMEOVER:
        return S_GAME;
    }
    return S_WAIT;
}

/* Play at srv_table. 1: come back to it (the link dropped); 0: we left, or
 * it would not have us. */
static uint8_t play_table(void)
{
    uint8_t r, shown = S_NONE, shown_seq = 0, want;
    uint16_t t = 0;

    appkey_save_room();
    message("JOINING", srv_table, 1);
    if (!net_open()) {
        message("NO ANSWER FROM", srv_host, 0);
        return 0;
    }
    net_hello();
    for (;;) {
        frame_wait();
        input_poll();
        input_take();
        if (ev_key == 'M')
            snd_toggle_mute();
        r = net_poll();
        if (r == NET_ERROR) {
            net_close();
            message("THE SERVER SAID NO", net_err == ERR_VERSION ? "UPDATE FUJIBLAST" : "NO SUCH ROOM", 0);
            return 0;
        }
        if (r == NET_LOST) {
            spr_hide_all();
            message("CONNECTION LOST", "TRYING AGAIN", 120);
            return 1;
        }
        if (ev_key == KEY_ESC) {
            net_leave();
            net_close();
            return 0;
        }
        if (!net_heard) {
            if (++t > JOIN_WAIT) {
                net_close();
                message("NO ANSWER FROM", srv_host, 0);
                return 0;
            }
            continue;
        }
        my_seat = find_seat();
        want = screen_for();
        if (want != shown || (want == S_ARENA && st[OFF_SEQ] != shown_seq)) {
            shown = want;
            shown_seq = st[OFF_SEQ];
            switch (want) {
            case S_WAIT:
                wait_enter();
                break;
            case S_ARENA:
                play_enter();
                break;
            default:
                results_enter(want == S_GAME);
            }
        } else if (r == NET_STATE) {
            if (shown == S_WAIT)
                wait_state();
            else if (shown == S_ARENA)
                play_state();
        }
        switch (shown) {
        case S_WAIT:
            wait_frame();
            break;
        case S_ARENA:
            play_frame();
            break;
        default:
            results_frame();
        }
    }
}

void test_check(void);

int main(void)
{
    plat_init();
    test_check();
    appkey_load();
    title();
    if (!my_name[0]) {
        name_entry();
        appkey_save_name();
    }
    for (;;) {
        if (!srv_table[0] && !table_list())
            continue;
        if (!play_table())
            appkey_clear_room();
    }
}

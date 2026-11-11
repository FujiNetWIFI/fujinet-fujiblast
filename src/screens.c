/*
 * screens.c -- the screens at a table besides the arena: the waiting room,
 * the trophies after a round and a game; and a message. (The title, the
 * name and the rooms are in menus.c, which the Apple II keeps in an
 * overlay; the helpers here are shared with it.)
 */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "gfxcodes.h"
#include "input.h"
#include "plat.h"
#include "sound.h"

uint8_t prev_dir;

void tick(void)
{
    frame_wait();
    input_poll();
    input_take();
    if (ev_key == 'M')
        snd_toggle_mute();
}

/* stick pressed this frame (not held) */
uint8_t pressed(uint8_t bit)
{
    return (inp_dir & bit) && !(prev_dir & bit);
}

void centre(uint8_t row, const char *s, uint8_t colour, uint8_t width)
{
    gfx_text(row, (width - strlen(s)) / 2, s, colour);
}

void message(const char *a, const char *b, uint8_t frames)
{
    gfx_begin();
    gfx_row(10, ROW_BIG, PAL_MENU);
    centre(10, a, 0, 20);
    centre(13, b, 0, 40);
    if (!frames)
        centre(18, "PRESS FIRE", 0, 40);
    gfx_show();
    for (;;) {
        tick();
        if (frames ? !--frames : ev_btn || ev_key == KEY_RETURN)
            break;
    }
}

/* ---- the waiting room ------------------------------------------------------ */

#define SEAT_ROW(s) (4 + (s) * 4)

void wait_enter(void)
{
    uint8_t s;
    gfx_begin();
    gfx_row(1, ROW_BIG, PAL_MENU);
    for (s = 0; s < SEATS; ++s)
        gfx_row(SEAT_ROW(s), ROW_BIG, PAL_SEAT0 + s);
    gfx_show();
    wait_state();
}

void wait_state(void)
{
    static char b[21];
    uint8_t s, n = 0, host = st[OFF_HOST];
    gfx_clear_row(1);
    strcpy(b, "ROOM ");
    strcat(b, srv_table);
    centre(1, b, 0, 20);
    for (s = 0; s < SEATS; ++s) {
        gfx_clear_row(SEAT_ROW(s));
        if (SEATED(s)) {
            ++n;
            memcpy(b, NAME(s), NAME_LEN);
            b[NAME_LEN] = 0;
            gfx_text(SEAT_ROW(s), 4, b, 0);
            if (s == host)
                gfx_text(SEAT_ROW(s), 14, "HOST", 0);
            spr_at(s, 8, SEAT_ROW(s) * 8, FR_DOWN0, s, 0);
        } else {
            gfx_text(SEAT_ROW(s), 4, "--------", 0);
            spr_hide(s);
        }
    }
    gfx_clear_row(20);
    gfx_text(20, 12, "ROUNDS:", 0);
    gfx_number(20, 20, st[OFF_ROUNDS], 0);
    if (my_seat != NONE && my_seat == host)
        gfx_text(20, 23, "(LEFT/RIGHT)", 0);
    gfx_clear_row(22);
    if (my_seat == NONE)
        centre(22, "WATCHING: THE ROOM IS FULL", 0, 40);
    else if (my_seat != host)
        centre(22, "WAITING FOR THE HOST TO START", 0, 40);
    else if (n < 2)
        centre(22, "WAITING FOR PLAYERS", 0, 40);
    else
        centre(22, "PRESS FIRE TO START", 1, 40);
    gfx_clear_row(24);
    centre(24, "ESC: LEAVE    M: SOUND", 0, 40);
}

void wait_frame(void)
{
    uint8_t r = st[OFF_ROUNDS];
    if (my_seat != NONE && my_seat == st[OFF_HOST]) {
        if (pressed(IN_LEFT) && r > 1)
            net_act(ACT_ROUNDS, r - 1, 0);
        if (pressed(IN_RIGHT) && r < MAX_ROUNDS)
            net_act(ACT_ROUNDS, r + 1, 0);
        if (ev_btn)
            net_act(ACT_START, 0, 0);
    }
    prev_dir = inp_dir;
}

/* ---- trophies ---------------------------------------------------------- */

static uint8_t res_final, res_seat, res_row[SEATS], res_timer;

void results_enter(uint8_t final)
{
    static char b[21];
    uint8_t s, k = 0, w, r;
    res_final = final;
    res_seat = st[OFF_RESULT];
    res_timer = NONE;
    gfx_begin();
    gfx_row(1, ROW_BIG, res_seat < SEATS ? PAL_SEAT0 + res_seat : PAL_MENU);
    if (res_seat < SEATS) {
        memcpy(b, NAME(res_seat), NAME_LEN);
        b[NAME_LEN] = 0;
        strcat(b, final ? " WINS!" : " WINS");
    } else {
        strcpy(b, final ? "NO WINNER" : "DRAW");
    }
    centre(1, b, 0, 20);
    if (!final) {
        strcpy(b, st[OFF_SUDDEN] ? "SUDDEN DEATH" : "ROUND ");
        if (!st[OFF_SUDDEN])
            gfx_number(2, 25, st[OFF_ROUND], 0);
        gfx_text(2, st[OFF_SUDDEN] ? 14 : 19, b, 0);
    }
    for (s = 0; s < SEATS; ++s) {
        res_row[s] = 0;
        if (!SEATED(s)) {
            spr_hide(s);
            continue;
        }
        r = 4 + k * 5;
        res_row[s] = r;
        ++k;
        gfx_row(r, ROW_BIG, PAL_SEAT0 + s);
        memcpy(b, NAME(s), NAME_LEN);
        b[NAME_LEN] = 0;
        gfx_text(r, 4, b, 0);
        w = PL(s, P_WINS);
        gfx_number(r, 13, w, 0);
        gfx_trophies(r + 1, 8, w > 15 ? 15 : w);
        spr_at(s, 8, r * 8, FR_DOWN0, s, 0);
    }
    if (final && res_seat < SEATS)
        spr_at(res_seat, 136, 12 * 8, FR_WIN0, res_seat, 1);
    gfx_show();
    if (final)
        snd_music(MUS_GAME);
    else if (res_seat < SEATS)
        snd_music(MUS_WIN);
}

void results_frame(void)
{
    static char b[41];
    uint8_t f = (frame_count >> 4) & 1;
    if (res_seat < SEATS) {
        if (res_final)
            spr_at(res_seat, 136, 12 * 8, FR_WIN0 + f, res_seat, 1);
        else if (res_row[res_seat])
            spr_at(res_seat, 8, res_row[res_seat] * 8, FR_WIN0 + f, res_seat, 0);
    }
    if (st[OFF_TIMER] != res_timer) {
        res_timer = st[OFF_TIMER];
        strcpy(b, res_final ? "BACK TO THE ROOM IN " : "NEXT ROUND IN ");
        gfx_clear_row(24);
        gfx_text(24, 8, b, 0);
        gfx_number(24, 8 + strlen(b), res_timer, 0);
    }
}

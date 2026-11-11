/*
 * screens.c -- everything but the arena: the title, the name, the rooms,
 * the waiting room, and the trophies after a round and a game.
 */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "gfxcodes.h"
#include "input.h"
#include "plat.h"
#include "sound.h"

static uint8_t prev_dir;

static void tick(void)
{
    frame_wait();
    input_poll();
    input_take();
    if (ev_key == 'M')
        snd_toggle_mute();
}

/* stick pressed this frame (not held) */
static uint8_t pressed(uint8_t bit)
{
    return (inp_dir & bit) && !(prev_dir & bit);
}

static void centre(uint8_t row, const char *s, uint8_t colour, uint8_t width)
{
    gfx_text(row, (width - strlen(s)) / 2, s, colour);
}

void title(void)
{
    uint8_t n, x;
    uint16_t t = 0;
    gfx_begin();
    gfx_logo(3);
    gfx_row(8, ROW_BIG, PAL_MENU);
    centre(8, "FOR FUJINET", 0, 20);
    centre(18, "PRESS FIRE", 0, 40);
    centre(21, "BOMBS, BLOCKS AND BLASTS FOR 2-4", 0, 40);
    gfx_show();
    snd_music(MUS_TITLE);
    for (;;) {
        tick();
        ++t;
        for (n = 0; n < SEATS; ++n) {
            x = (uint8_t)((t >> 1) + n * 40) % 168;
            spr_at(n, x, 96, FR_RIGHT0 + ((t >> 3) & 1 ? (n & 1) + 1 : 0), n, 0);
        }
        if (ev_btn || ev_key == KEY_RETURN)
            break;
    }
    snd_stop();
    spr_hide_all();
}

void name_entry(void)
{
    uint8_t n = 0;
    char c;
    gfx_begin();
    gfx_row(6, ROW_BIG, PAL_MENU);
    centre(6, "WHAT IS YOUR NAME?", 0, 20);
    gfx_row(10, ROW_BIG, PAL_MENU);
    centre(14, "LETTERS AND DIGITS, THEN RETURN", 0, 40);
    gfx_show();
    my_name[0] = 0;
    for (;;) {
        gfx_clear_row(10);
        gfx_text(10, 6, my_name, 1);
        if (n < NAME_LEN)
            gfx_text(10, 6 + n, "-", 0);
        tick();
        c = ev_key;
        if (((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) && n < NAME_LEN) {
            my_name[n++] = c;
            my_name[n] = 0;
            snd_play(SND_SELECT);
        } else if (c == KEY_BACK && n) {
            my_name[--n] = 0;
        } else if (c == KEY_RETURN && n >= 2) {
            break;
        }
    }
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

static void phase_name(char *b, uint8_t ph)
{
    strcpy(b, ph == PH_WAIT ? "WAITING" : "PLAYING");
}

uint8_t table_list(void)
{
    static char b[41];
    uint8_t n, i, sel = 0, *t;
    message("CONNECTING", srv_host, 1);
    n = net_tables();
    if (!n) {
        net_close();
        message("NO ANSWER FROM", srv_host, 0);
        return 0;
    }
    gfx_begin();
    gfx_row(1, ROW_BIG, PAL_MENU);
    centre(1, "PICK A ROOM", 0, 20);
    for (i = 0; i < n; ++i) {
        t = tables[i];
        memset(b, ' ', 40);
        memcpy(b + 4, t + 8, 12);
        for (sel = 4; sel < 16; ++sel)
            if (!b[sel])
                b[sel] = ' ';
        b[18] = '0' + t[20];
        b[19] = '/';
        b[20] = '0' + t[21];
        phase_name(b + 24, t[22]);
        gfx_text(5 + i * 2, 0, b, 0);
    }
    centre(22, "STICK: CHOOSE    FIRE: JOIN", 0, 40);
    gfx_show();
    sel = 0;
    for (;;) {
        for (i = 0; i < n; ++i)
            gfx_text(5 + i * 2, 2, i == sel ? ">" : " ", 0);
        prev_dir = inp_dir;
        tick();
        net_poll();
        if (pressed(IN_UP) && sel) {
            --sel;
            snd_play(SND_SELECT);
        }
        if (pressed(IN_DOWN) && sel + 1 < n) {
            ++sel;
            snd_play(SND_SELECT);
        }
        if (ev_btn || ev_key == KEY_RETURN)
            break;
    }
    memcpy(srv_table, tables[sel], TABLE_LEN);
    srv_table[TABLE_LEN] = 0;
    return 1;
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

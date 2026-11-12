/*
 * menus.c -- the screens before a table: the title, the name, the rooms.
 * (The Apple II keeps them in an overlay: src/apple2/overlay.c.)
 */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "gfxcodes.h"
#include "input.h"
#include "plat.h"
#include "sound.h"

#ifndef TAGLINE
#define TAGLINE "BOMBS, BLOCKS AND BLASTS FOR 2-4"
#endif
#ifndef NAME_HELP
#define NAME_HELP "LETTERS AND DIGITS, THEN " KEY_END_NAME
#endif
#ifndef TITLE_Y
#define TITLE_Y(n) 96           /* the title's bombers walk along one line */
#endif

#ifndef KEY_END_NAME
#define KEY_END_NAME "RETURN"   /* (the PC's key says ENTER) */
#endif

void title(void)
{
    uint8_t n, x;
    uint16_t t = 0;
    gfx_begin();
    gfx_logo(3);
    gfx_row(8, ROW_BIG, PAL_MENU);
    centre(8, "FOR FUJINET", 0, 20);
    centre(18, "PRESS FIRE", 0, 40);
    centre(21, TAGLINE, 0, 40);
#ifdef QUIT_HINT
    centre(23, QUIT_HINT, 0, 40);       /* (a platform that can quit) */
#endif
    gfx_show();
    snd_music(MUS_TITLE);
    for (;;) {
        tick();
        ++t;
        for (n = 0; n < SEATS; ++n) {
            x = (uint8_t)((t >> 1) + n * 40) % 168;
            spr_at(n, x, TITLE_Y(n), FR_RIGHT0 + ((t >> 3) & 1 ? (n & 1) + 1 : 0), n, 0);
        }
        if (ev_btn || ev_key == KEY_RETURN)
            break;
#ifdef QUIT_HINT
        if (ev_key == KEY_ESC)
            plat_quit();
#endif
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
    centre(14, NAME_HELP, 0, 40);
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

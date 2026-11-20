/* Live room browser. The published list stays usable during a refresh. */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "input.h"
#include "plat.h"
#include "sound.h"
#define REFRESH_TICKS (5 * 60)
#define TIMEOUT_TICKS (10 * 60)
static void draw_rooms(uint8_t n)
{
    uint8_t i;
    char b[41];
    for (i = 0; i < MAX_TABLES; ++i) {
        memset(b, ' ', 40); b[40] = 0;
        if (i < n) {
            const uint8_t *t = tables[i];
            unsigned j;
            for (j = 0; j < 12 && t[8+j]; ++j) b[4+j] = t[8+j];
            b[18] = '0' + t[20]; b[19] = '/'; b[20] = '0' + t[21];
            memcpy(b+24, t[22] == PH_WAIT ? "WAITING" : "PLAYING", 7);
        }
        gfx_text(5+i*2, 0, b, 0);
    }
    if (!n) centre(9, "NO ROOMS AVAILABLE", 0, 40);
}
uint8_t amiga_table_list(void)
{
    uint8_t n, i, sel = 0, pending = 0;
    uint16_t started;
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
    draw_rooms(n);
    centre(22, "STICK: CHOOSE    FIRE: JOIN", 0, 40);
    centre(24, "ESC: TITLE    AUTO REFRESH: 5S", 0, 40);
    gfx_show();
    started = frame_count;
    for (;;) {
        uint8_t result;
        for (i = 0; i < n; ++i) gfx_text(5+i*2, 2, i == sel ? ">" : " ", 0);
        prev_dir = inp_dir;
        frame_wait(); input_poll(); input_take();
        if (ev_key == KEY_ESC || ev_key == KEY_SETUP) {
            net_close();
            return 0;
        }
        if (ev_key == 'M') snd_toggle_mute();
        result = net_poll();
        if (result == NET_LOST || result == NET_ERROR ||
            (pending && (uint16_t)(frame_count - started) >= TIMEOUT_TICKS)) {
            net_close();
            message("ROOM LIST ERROR", "PRESS FIRE TO RETURN", 0);
            return 0;
        }
        if (pending) {
            char selected[TABLE_LEN];
            uint8_t count;
            memcpy(selected, tables[sel], TABLE_LEN);
            if (net_tables_ready(&count)) {
                uint8_t had_selection = n != 0;
                n = count;
                if (sel >= n) sel = n ? n-1 : 0;
                if (had_selection)
                    for (i = 0; i < n; ++i)
                        if (!memcmp(selected, tables[i], TABLE_LEN)) { sel = i; break; }
                pending = 0;
                started = frame_count;
                draw_rooms(n);
            }
        } else if ((uint16_t)(frame_count - started) >= REFRESH_TICKS) {
            net_tables_request();
            pending = 1;
            started = frame_count;
        }
        if (pressed(IN_UP) && sel) { --sel; snd_play(SND_SELECT); }
        if (pressed(IN_DOWN) && sel+1 < n) { ++sel; snd_play(SND_SELECT); }
        if (n && (ev_btn || ev_key == KEY_RETURN)) {
            memcpy(srv_table, tables[sel], TABLE_LEN);
            srv_table[TABLE_LEN] = 0;
            return 1;
        }
    }
}

/* Title-screen configuration. All edits are staged until Save setup. */
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "input.h"
#include "plat.h"
#include "config.h"
#include "nio/servers.h"
#include "worker.h"
#ifndef LOBBY_SERVER
#define LOBBY_SERVER "fujinet.online:9910"
#endif

/* PAL_HUD exposes the four seat gradients on narrow text rows. */
enum { TEXT_VALUE = 0, TEXT_ALERT = 1, TEXT_LABEL = 2, TEXT_ACTIVE = 3 };

static void poll(void)
{
    /* Text entry must not trigger M/mute or ESC/quit. */
    frame_wait(); input_poll(); input_take();
}

/* Keep the glyph cache in step with erased text while switching fields. */
static void clear_text(unsigned row, unsigned width)
{
    static const char spaces[] = "                                        ";
    gfx_text(row, 0, spaces + 40 - width, 0);
}

static uint8_t edit(char *value, unsigned limit, uint8_t address)
{
    char draft[FB_NIO_URL_BYTES], line[38];
    const char *error = "";
    unsigned row;
    strcpy(draft, value);
    gfx_begin();
    gfx_row(3, ROW_BIG, PAL_MENU);
    for (row = 7; row <= 23; ++row) gfx_row(row, ROW_TEXT, PAL_HUD);
    for (row = 0; row < 25; ++row) clear_text(row, row == 3 ? 20 : 40);
    gfx_show();
    centre(3, address ? "ADD SERVER" : "PLAYER NAME", 0, 20);
    centre(7, address ? "HOST OR IP:PORT" : "2-8 LETTERS / DIGITS", TEXT_LABEL, 40);
    if (address) centre(13, "PORT DEFAULTS TO 9910", TEXT_LABEL, 40);
    centre(20, "RETURN: ACCEPT   BACKSPACE: ERASE", 0, 40);
    centre(23, "ESC: CANCEL EDIT", 0, 40);
    for (;;) {
        unsigned n = strlen(draft), start = n > 34 ? n - 34 : 0;
        uint8_t k;
        clear_text(10, 40);
        clear_text(16, 40);
        snprintf(line, sizeof line, "%s_", draft + start);
        gfx_text(10, 2, line, TEXT_ACTIVE);
        centre(16, error, TEXT_ALERT, 40);
        for (;;) {
            poll(); k = ev_key;
            if (k) break;
        }
        if (k == KEY_ESC) return 0;
        if (k == KEY_RETURN) {
            char normal[FB_SERVER_TEXT];
            if (address ? fb_server_normalize(draft, normal) : n >= 2) {
                strcpy(value, address ? normal : draft);
                return 1;
            }
            error = address ? "CHECK HOST AND PORT (1-65535)" : "NAME NEEDS AT LEAST TWO CHARACTERS";
        } else if (k == KEY_BACK && n) {
            draft[n-1] = 0;
            error = "";
        } else if (n < limit && ((k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9') ||
                   (address && (k == '.' || k == '-' || k == ':' || k == '/')))) {
            draft[n] = k; draft[n+1] = 0;
            error = "";
        }
    }
}

static void endpoint(unsigned row, const char *address, uint8_t colour)
{
    char first[35];
    snprintf(first, sizeof first, "%.34s", address);
    gfx_text(row, 4, first, colour);
    if (strlen(address) > 34) {
        snprintf(first, sizeof first, "%.34s", address+34);
        if (strlen(address) > 68) memcpy(first+31, "...", 4);
        gfx_text(row+1, 4, first, colour);
    }
}

static void draw(const char *name, const struct fb_servers *list, const char *lobby,
                 unsigned custom, unsigned field, int selected, const char *error, int fresh)
{
    unsigned i;
    if (fresh) {
        gfx_begin();
        gfx_row(1, ROW_BIG, PAL_MENU);
        for (i = 4; i <= 22; ++i) gfx_row(i, ROW_TEXT, PAL_HUD);
        gfx_show();
    }
    clear_text(1, 20);
    centre(1, "SETUP", 0, 20);
    for (i = 4; i < 25; ++i) clear_text(i, 40);
    gfx_text(4, 4, "NAME:", TEXT_LABEL);
    gfx_text(4, 12, name[0] ? name : "ENTER NAME", field == 0 ? TEXT_ACTIVE : TEXT_VALUE);
    gfx_text(4, 0, field == 0 ? ">" : " ", TEXT_ACTIVE);
    gfx_text(6, 2, "SERVER:", TEXT_LABEL);
    gfx_text(6, 12, custom ? " LOBBY " : "[LOBBY]", custom ? TEXT_VALUE : TEXT_ACTIVE);
    gfx_text(6, 23, custom ? "[CUSTOM]" : " CUSTOM ", custom ? TEXT_ACTIVE : TEXT_VALUE);
    gfx_text(6, 0, field == 1 ? ">" : " ", TEXT_ACTIVE);
    if (custom) {
        for (i = 0; i < list->count; ++i) {
            gfx_text(8+i*2, 0, field == i+2 ? ">" : " ", TEXT_ACTIVE);
            gfx_text(8+i*2, 2, selected == (int)i ? "*" : " ", TEXT_ACTIVE);
            endpoint(8+i*2, list->address[i], selected == (int)i ? TEXT_ACTIVE : TEXT_VALUE);
        }
        if (!list->count) centre(10, "NO CUSTOM SERVERS SAVED", 0, 40);
        gfx_text(18, 0, field == list->count+2U ? ">" : " ", TEXT_ACTIVE);
        gfx_text(18, 2, "+ ADD SERVER", field == list->count+2U ? TEXT_ACTIVE : TEXT_LABEL);
    } else {
        endpoint(9, lobby[0] ? lobby : "LOBBY PRESET NOT CONFIGURED", TEXT_ACTIVE);
    }
    gfx_text(20, 0, field == (custom ? list->count+3U : 2) ? ">" : " ", TEXT_ACTIVE);
    gfx_text(20, 2, "[ SAVE SETUP ]", field == (custom ? list->count+3U : 2) ? TEXT_ACTIVE : TEXT_VALUE);
    centre(21, error, TEXT_ALERT, 40);
    centre(23, "UP/DOWN: MOVE  RETURN/FIRE: CHOOSE", 0, 40);
    centre(24, custom ? "BACKSPACE: DELETE SERVER   ESC: CANCEL" : "LEFT/RIGHT: SERVER TYPE   ESC: CANCEL", 0, 40);
}

void amiga_setup(void)
{
    /* This menu is not reentrant. Keep its large buffers off the 4 KB Shell stack. */
    static struct fb_servers list;
    static uint8_t data[FB_SERVERS_BYTES];
    uint16_t size;
    char name[NAME_LEN+1], current[FB_SERVER_TEXT], lobby[FB_SERVER_TEXT] = "";
    const char *error = "";
    unsigned field = 0, old_dir = 0, custom, i;
    int selected = -1, redraw = 2;
    memset(&list, 0, sizeof list);
    strcpy(name, my_name);
    snprintf(current, sizeof current, "%s:%u", srv_host, (unsigned)srv_port);
    fb_server_normalize(current, current);
    fb_server_normalize(LOBBY_SERVER, lobby);
    custom = strcmp(current, lobby) != 0;
    for (;;) {
        uint8_t loaded = amiga_history_read(data, &size);
        if (loaded && fb_servers_decode(&list, data, size)) break;
        gfx_begin();
        clear_text(8, 40); clear_text(12, 40);
        centre(8, loaded ? "INVALID SAVED SERVER LIST" : "CANNOT LOAD SAVED SERVERS", 0, 40);
        centre(12, "RETURN: RETRY   ESC: CANCEL", 0, 40);
        gfx_show();
        do { poll(); if (ev_key == KEY_ESC) return; } while (ev_key != KEY_RETURN);
    }
    /* Import the previous single-host setting on first use only. An explicitly
     * saved empty list must stay empty after its last entry is deleted. */
    if (!size && custom) fb_servers_add(&list, current);
    if (list.count) selected = 0;
    for (i = 0; i < list.count; ++i)
        if (!strcmp(current, list.address[i])) selected = i;
    for (;;) {
        unsigned count = custom ? list.count+4 : 3;
        uint8_t dir, k;
        if (redraw) { draw(name, &list, lobby, custom, field, selected, error, redraw == 2); redraw = 0; }
        poll(); k = ev_key; dir = inp_dir & ~old_dir; old_dir = inp_dir;
        if (k == KEY_ESC) return;
        if (k == 9 || (dir & IN_DOWN)) {
            field = (field+1) % count; redraw = 1;
        } else if (dir & IN_UP) {
            field = field ? field-1 : count-1; redraw = 1;
        } else if (field == 1 && ((dir & (IN_LEFT|IN_RIGHT)) || k == KEY_RETURN || ev_btn)) {
            custom ^= 1; error = ""; redraw = 1;
        } else if (custom && field >= 2 && field < list.count+2U && (k == KEY_BACK || k == 127)) {
            unsigned index = field-2;
            fb_servers_delete(&list, index);
            if (!list.count) selected = -1;
            else if (selected > (int)index) --selected;
            else if (selected >= (int)list.count) selected = list.count-1;
            if (field >= list.count+2U) field = list.count+2U;
            error = "DELETED - SAVE SETUP TO KEEP"; redraw = 1;
        } else if (k == KEY_RETURN || ev_btn) {
            redraw = 1;
            if (!field) {
                edit(name, NAME_LEN, 0);
                redraw = 2;
            } else if (custom && field < list.count+2U) {
                selected = field-2;
                field = list.count+3U;
                error = "";
            } else if (custom && field == list.count+2U) {
                if (list.count == FB_SERVER_COUNT) error = "LIST FULL - DELETE AN ENTRY FIRST";
                else {
                    char added[FB_NIO_URL_BYTES] = "";
                    redraw = 2;
                    if (edit(added, sizeof added-1, 1) && fb_servers_add(&list, added)) {
                        selected = 0; field = 2; error = "ADDED - SAVE SETUP TO KEEP";
                    }
                }
            } else {
                const char *address = custom && selected >= 0 ? list.address[selected] : lobby;
                if (custom && selected < 0) error = "ADD OR SELECT A CUSTOM SERVER";
                else if (!address[0]) error = "LOBBY PRESET NOT CONFIGURED";
                else if (strlen(name) < 2) error = "ENTER A PLAYER NAME FIRST";
                else {
                    uint8_t result;
                    fb_servers_encode(&list, data);
                    if (!amiga_history_write(data, sizeof data)) error = "SAVE FAILED - RETURN TO RETRY";
                    else {
                        result = fb_nio_save_setup(name, address);
                        if (result == 1) return;
                        error = result == 2 ? "SAVE FAILED - RETURN TO RETRY" : "CHECK NAME AND SERVER";
                    }
                }
            }
            old_dir = inp_dir;
        }
    }
}

/*
 * overlay.c -- the menus' overlay. The title, the name, the rooms
 * (src/menus.c) and the lobby's appkeys (src/appkey.c) wait in aux RAM
 * (the end of FUJIBLST.GFX) and run in page 2's main half ($4000-$5FFF):
 * none of them is needed at a table, and main RAM has no room for them.
 *
 * They are built under other names (ov_title ...: the Makefile); these are
 * the names the game calls. While their code is in page 2 the screens are
 * drawn on page 1 alone (gfx_single), shown as they are drawn; the next
 * screen of the game draws page 2 afresh, and the overlay is fetched again
 * the next time it is wanted.
 */
#ifdef __APPLE2__

#include <stdint.h>
#include "game.h"

void aux_get(void *dst, const void *src, uint16_t n);  /* lowlevel.s */
void gfx_single(uint8_t on);                           /* gfx.c */
extern uint8_t ov_here;
extern uint8_t _OVERLAY_LOAD__[], _OVERLAY_RUN__[], _OVERLAY_SIZE__[];

void ov_title(void);
void ov_name_entry(void);
uint8_t ov_table_list(void);
void ov_appkey_load(void);
void ov_appkey_save_name(void);
void ov_appkey_save_room(void);
void ov_appkey_clear_room(void);

static void in(void)
{
    gfx_single(1);
    if (!ov_here) {
        aux_get(_OVERLAY_RUN__, _OVERLAY_LOAD__, (uint16_t)_OVERLAY_SIZE__);
        ov_here = 1;
    }
}

void title(void)
{
    in();
    ov_title();
    gfx_single(0);
}

void name_entry(void)
{
    in();
    ov_name_entry();
    gfx_single(0);
}

uint8_t table_list(void)
{
    uint8_t r;
    in();
    r = ov_table_list();
    gfx_single(0);
    return r;
}

void appkey_load(void)
{
    in();
    ov_appkey_load();
    gfx_single(0);
}

void appkey_save_name(void)
{
    in();
    ov_appkey_save_name();
    gfx_single(0);
}

void appkey_save_room(void)
{
    in();
    ov_appkey_save_room();
    gfx_single(0);
}

void appkey_clear_room(void)
{
    in();
    ov_appkey_clear_room();
    gfx_single(0);
}

#endif

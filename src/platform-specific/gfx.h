/* gfx.h -- the screen: 25 rows of 40 cells (a BIG row: 20), each row one of
 * a few kinds, and four player sprites. */
#ifndef GFX_H
#define GFX_H

#include <stdint.h>

#define SCR_ROWS 25

/* row kinds */
#define ROW_TEXT  0     /* 40 small characters */
#define ROW_BIG   1     /* 20 big characters, each in one of the row's 4 colours */
#define ROW_GFX   2     /* menu graphics: the logo, trophies */
#define ROW_ARENA 3     /* the arena's tiles */

/* row palettes */
enum { PAL_MENU, PAL_GFX, PAL_HUD, PAL_ARENA, PAL_SEAT0, PAL_SEAT1, PAL_SEAT2, PAL_SEAT3, PAL_TITLE };

void gfx_init(void);
void gfx_begin(void);                           /* a new screen, off: text rows, cleared */
void gfx_row(uint8_t row, uint8_t kind, uint8_t pal);
void gfx_show(void);                            /* on, from the next frame */
void gfx_clear_row(uint8_t row);
/* A row's text all at once (a BIG row: 20 characters, colours 0-3 per
 * character), so it never shows half rewritten. */
void gfx_big_line(uint8_t row, const char *s, const uint8_t *colour);
/* Text: on a BIG row colour (0-3) picks the row's colour. */
void gfx_text(uint8_t row, uint8_t col, const char *s, uint8_t colour);
void gfx_number(uint8_t row, uint8_t col, uint8_t n, uint8_t colour);
void gfx_logo(uint8_t row);                     /* 3 GFX rows */
void gfx_trophies(uint8_t row, uint8_t col, uint8_t n);    /* 2 GFX rows */

/* The arena screen: the HUD on row 0, then the arena (13x11 tiles) in its
 * border, the player's stats beside it. */
void gfx_arena(void);
void gfx_tile(uint8_t c, uint8_t r, uint8_t tile);
void gfx_bomb_at(uint8_t hx, uint8_t hy, uint8_t dud);      /* half-tile steps */
void gfx_stat(uint8_t i, uint8_t n);            /* 0 fire, 1 bombs, 2 speed, 3 kick, 4 glove; n 0 hides 3, 4 */
void gfx_round(uint8_t round);
#ifdef PLAT_MAP_NEXT
/* The first tile from `from` on where map and drawn differ, or TILES. */
uint8_t map_next(const uint8_t *map, const uint8_t *drawn, uint8_t from);
#ifdef __WATCOMC__
#pragma aux map_next "*_" parm [si] [di] [dx] value [al] modify [ax cx dx si di es]
#endif
#endif

/* Sprites: n 0-3, in seat's colour. spr_arena takes arena units; spr_at
 * the screen's own (x 0-159, y 0-199). big: twice the size. */
void spr_arena(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat);
void spr_at(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat, uint8_t big);
void spr_hide(uint8_t n);
void spr_hide_all(void);
void spr_flush(void);          /* draw them as set (frame_wait does) */

#endif

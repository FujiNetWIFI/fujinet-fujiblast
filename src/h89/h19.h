/*
 * h19.h -- the H19 terminal's screen, drawn in cells and sent by difference
 * (from M.U.L.E.'s client).
 *
 * A cell is one byte: the low seven bits are the byte the terminal is sent
 * for it in graphics mode (which is on all the time: 20h-5Dh print as
 * themselves, upper case; `, a-z, {, |, }, ^ and _ are the graphics
 * characters), bit 7 is reverse video. The game draws into bg; sprites lie
 * over it; h19_flush() sends what differs from what the terminal shows, a
 * little at a time so the console line never backs up. Rows 0-23 are the
 * screen, row 24 the 25th line.
 */
#ifndef H19_H
#define H19_H

#include <stdint.h>

#define SCR_W  80
#define SCR_H  25
#define ROW25  24
#define REV    0x80
#define SPR_SLOTS 4             /* the bombers */

/* the graphics characters: the byte sent (graphics mode) */
#define G_VLINE   '`'       /* vertical line */
#define G_HLINE   'a'       /* horizontal line */
#define G_CROSS   'b'
#define G_UR      'c'       /* corners: upper right */
#define G_LR      'd'       /* lower right */
#define G_LL      'e'       /* lower left */
#define G_UL      'f'       /* upper left */
#define G_CHECK   'i'       /* 50% checkerboard */
#define G_TOP     'p'       /* upper half (reversed: lower half) */
#define G_RIGHT   'q'       /* right half (reversed: left half) */
#define G_TRI_UL  'r'       /* filled upper-left triangle (reversed: lower-right) */
#define G_X       'w'       /* diagonal cross */
#define G_BAR_B   '{'       /* line along the bottom */
#define G_TRI_UR  '_'       /* filled upper-right triangle (reversed: lower-left) */
#define G_BLOCK   (' ' | REV)

extern uint8_t bg[SCR_H][SCR_W];    /* the screen without sprites */
extern uint8_t h89_idle;            /* 1: all sent (the rigs wait for it) */
extern uint16_t h89_flushes;
extern uint8_t h19_first;           /* the slot whose rows go first, and on top */

/* Set up the terminal, and put it back. */
void h19_init(void);
void h19_exit(void);

/* A new screen: the next flush may clear the terminal first. */
void h19_cleared(void);

/* Drawing into bg. */
void h19_put(uint8_t x, uint8_t y, uint8_t c);
void h19_putn(uint8_t x, uint8_t y, const uint8_t *s, uint8_t n);   /* n cells */
void h19_fill(uint8_t x, uint8_t y, uint8_t w, uint8_t c);
void h19_str(uint8_t x, uint8_t y, const char *s, uint8_t attr);    /* text, | attr */
uint8_t h19_char(uint8_t c);    /* a text character as h19_str shows it */

/* The sprites, as they should be: art is { w, h, cells... } (0: see
 * through), its top-left cell at c, r; rev reverses it (our own bomber);
 * art 0: none. Set before a flush. */
extern const uint8_t *s_art[SPR_SLOTS];
extern uint8_t s_c[SPR_SLOTS], s_r[SPR_SLOTS], s_rev[SPR_SLOTS];

/* Send some of what changed (as much as the line takes in a few frames). */
void h19_flush(void);

/* Everything again (the terminal's screen was spoilt). */
void h19_repaint(void);

void h19_bell(void);

#endif

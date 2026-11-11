/*
 * gfx.c -- the Atari's screens (gfx.h): a display list of 25 rows of 8
 * scanlines, each text (ANTIC 2, the ROM font), big text (ANTIC 6) or
 * multicolour (ANTIC 4: the arena's charsets or the menus'); gfxa.s runs
 * the interrupts and draws the players.
 *
 * The arena: row 0 is the HUD, rows 1-24 the arena in its border (28 cells
 * from column 6), the player's stats to the left, the round to the right.
 * A tile is 2x2 cells (8 colour clocks by 16 lines), as wide as a player.
 */
#ifdef __ATARI__

#include <string.h>
#include "gfx.h"
#include "plat.h"
#include "proto.h"
#include "gfxcodes.h"

#define SCREEN   ((uint8_t *)0xA400)
#define PMG      0xB800
#define PMG2     0xB000
#define CHB_ROM  0xE0
#define CHB_MENU 0xA0
#define CHB_ANIM 0xFF
#define AR_COL   7              /* the arena's first cell column */
#define AR_ROW   2              /* and row */

#define PMBASE  (*(volatile uint8_t *)0xD407)
#define GRACTL  (*(volatile uint8_t *)0xD01D)

/* gfxa.s */
void dsp_init(void);
extern uint8_t dsp_dma, anim;
extern uint8_t dlist[];
extern uint8_t dli_list[];
extern uint8_t rw_chb[], rw_bk[], rw_c0[], rw_c1[], rw_c2[], rw_c3[];
extern uint8_t spr_x[4], spr_y[4], spr_fr[4], spr_big[4], spr_col[4];

/* the seats' colours */
const uint8_t seat_colour[SEATS] = { 0x0E, 0x44, 0x86, 0x00 };

/* palettes: background, PF0-PF3 */
static const uint8_t pal[][5] = {
    { 0x90, 0x1E, 0x0E, 0x90, 0x1E },   /* MENU: text white on blue; big: 0 yellow, 1 white */
    { 0x90, 0x0E, 0x02, 0x2A, 0x1E },   /* GFX: trophies gold */
    { 0x06, 0x0E, 0x46, 0x9C, 0x00 },   /* HUD: the seats' colours on grey */
    { 0xC6, 0x0C, 0x02, 0x36, 0x1E },   /* ARENA: green floor, grey, dark, brick, flame */
    { 0x90, 0x0E, 0x0E, 0x0E, 0x1E },   /* SEAT0..3: big text in the seat's colour */
    { 0x90, 0x46, 0x46, 0x46, 0x1E },
    { 0x90, 0x9C, 0x9C, 0x9C, 0x1E },
    { 0x90, 0x06, 0x06, 0x06, 0x1E },   /* (black reads as grey on blue) */
    { 0x90, 0x0E, 0x02, 0x36, 0x1E },   /* TITLE: the logo */
};

static uint8_t row_kind[SCR_ROWS], is_pal;

/* A PAL GTIA shows a hue about one step on from NTSC's: back it off (not
 * greys, nor hue 1, which would turn grey). */
static uint8_t hue(uint8_t c)
{
    return is_pal && c >= 0x20 ? c - 0x10 : c;
}

static uint8_t *line(uint8_t row)
{
    return SCREEN + row * 40;
}

void gfx_init(void)
{
    is_pal = (*(volatile uint8_t *)0xD014 & 0x0E) == 0;
    memset(SCREEN, 0, SCR_ROWS * 40);
    memset((void *)(PMG + 0x300), 0, 0x500);    /* missiles, players: both copies */
    memset((void *)(PMG2 + 0x300), 0, 0x500);
    PMBASE = PMG >> 8;
    GRACTL = 3;
    dsp_dma = 0;
    dsp_init();
    gfx_begin();
}

void gfx_begin(void)
{
    uint8_t r;
    dsp_dma = 0;
    frame_wait();
    spr_hide_all();
    memset(SCREEN, 0, SCR_ROWS * 40);
    for (r = 0; r < SCR_ROWS; ++r)
        gfx_row(r, ROW_TEXT, PAL_MENU);
}

void gfx_row(uint8_t row, uint8_t kind, uint8_t p)
{
    const uint8_t *c = pal[p];
    row_kind[row] = kind;
    rw_chb[row] = kind == ROW_ARENA ? CHB_ANIM : kind == ROW_GFX ? CHB_MENU : CHB_ROM;
    rw_bk[row] = hue(c[0]);
    rw_c0[row] = hue(c[1]);
    rw_c1[row] = hue(c[2]);
    rw_c2[row] = hue(c[3]);
    rw_c3[row] = hue(c[4]);
}

static uint8_t same(uint8_t a, uint8_t b)
{
    return rw_chb[a] == rw_chb[b] && rw_bk[a] == rw_bk[b] && rw_c0[a] == rw_c0[b] &&
           rw_c1[a] == rw_c1[b] && rw_c2[a] == rw_c2[b] && rw_c3[a] == rw_c3[b];
}

void gfx_show(void)
{
    uint8_t r, p = 2, n = 0, ins;
    uint16_t a;
    dsp_dma = 0;
    frame_wait();
    dlist[0] = 0x70;
    dlist[1] = 0x70;
    for (r = 0; r < SCR_ROWS; ++r) {
        ins = row_kind[r] == ROW_TEXT ? 0x42 : row_kind[r] == ROW_BIG ? 0x46 : 0x44;
        if (r + 1 < SCR_ROWS && !same(r, r + 1)) {
            ins |= 0x80;
            dli_list[n++] = r + 1;
        }
        a = (uint16_t)line(r);
        dlist[p++] = ins;
        dlist[p++] = (uint8_t)a;
        dlist[p++] = a >> 8;
    }
    dlist[p++] = 0x41;
    dlist[p++] = (uint8_t)(uint16_t)dlist;
    dlist[p] = (uint16_t)dlist >> 8;
    dsp_dma = 0x3E;             /* normal width, players and missiles, one line */
}

void gfx_clear_row(uint8_t row)
{
    memset(line(row), 0, 40);
}

static uint8_t internal(char c)
{
    uint8_t a = (uint8_t)c;
    if (a >= 'a' && a <= 'z')
        a -= 32;
    if (a < 32)
        return a + 64;
    if (a < 96)
        return a - 32;
    return a;
}

void gfx_big_line(uint8_t row, const char *s, const uint8_t *colour)
{
    static uint8_t b[20];
    uint8_t i;
    for (i = 0; i < 20; ++i)
        b[i] = (internal(s[i]) & 0x3F) | colour[i] << 6;
    memcpy(line(row), b, 20);
}

void gfx_text(uint8_t row, uint8_t col, const char *s, uint8_t colour)
{
    uint8_t *p = line(row) + col;
    uint8_t big = row_kind[row] == ROW_BIG;
    uint8_t add = big ? colour << 6 : colour ? 0x80 : 0;
    while (*s)
        *p++ = (internal(*s++) & (big ? 0x3F : 0x7F)) | add;
}

void gfx_number(uint8_t row, uint8_t col, uint8_t n, uint8_t colour)
{
    char b[4], *p = b + 3;
    *p = 0;
    do {
        *--p = '0' + n % 10;
        n /= 10;
    } while (n);
    gfx_text(row, col, p, colour);
}

void gfx_logo(uint8_t row)
{
    uint8_t r;
    for (r = 0; r < 3; ++r) {
        gfx_row(row + r, ROW_GFX, PAL_TITLE);
        memcpy(line(row + r) + (40 - LOGO_W) / 2, logo_chars[r], LOGO_W);
    }
}

void gfx_trophies(uint8_t row, uint8_t col, uint8_t n)
{
    uint8_t *a = line(row) + col, *b = a + 40;
    gfx_row(row, ROW_GFX, PAL_GFX);
    gfx_row(row + 1, ROW_GFX, PAL_GFX);
    while (n--) {
        *a++ = trophy_chars[0];
        *a++ = trophy_chars[1];
        *b++ = trophy_chars[2];
        *b++ = trophy_chars[3];
    }
}

/* ---- the arena ------------------------------------------------------------ */

void gfx_arena(void)
{
    uint8_t r, *p;
    gfx_row(0, ROW_BIG, PAL_HUD);
    for (r = 1; r < SCR_ROWS; ++r) {
        gfx_row(r, ROW_ARENA, PAL_ARENA);
        p = line(r);
        memset(p, CH_PANEL, 40);
        if (r == 1 || r == SCR_ROWS - 1) {
            memset(p + AR_COL - 1, CH_WALL, ARENA_W * 2 + 2);
        } else {
            p[AR_COL - 1] = CH_WALL;
            memset(p + AR_COL, 0, ARENA_W * 2);
            p[AR_COL + ARENA_W * 2] = CH_WALL;
        }
    }
}

void gfx_tile(uint8_t c, uint8_t r, uint8_t tile)
{
    uint8_t *p = line(AR_ROW + r * 2) + AR_COL + c * 2;
    const uint8_t *t = tile_chars[tile];
    p[0] = t[0];
    p[1] = t[1];
    p[40] = t[2];
    p[41] = t[3];
}

void gfx_bomb_at(uint8_t hx, uint8_t hy, uint8_t dud)
{
    uint8_t *p = line(AR_ROW + hy) + AR_COL + hx;
    const uint8_t *t = tile_chars[dud ? T_DUD : T_BOMB];
    p[0] = t[0];
    p[1] = t[1];
    p[40] = t[2];
    p[41] = t[3];
}

/* Left panel: powerup i's icon at rows 3+4i, its count beside it. */
void gfx_stat(uint8_t i, uint8_t n)
{
    uint8_t *p = line(3 + i * 4) + 1;
    const uint8_t *t = tile_chars[T_FIRE + i];
    if (i >= 3 && !n) {
        p[0] = p[1] = p[40] = p[41] = CH_PANEL;
        return;
    }
    p[0] = t[0];
    p[1] = t[1];
    p[40] = t[2];
    p[41] = t[3];
    if (i < 3) {
        p[42] = CH_FONT_X;
        p[43] = n >= 10 ? CH_FONT_0 + n / 10 : CH_PANEL;
        p[44] = CH_FONT_0 + n % 10;
    }
}

/* Right panel: RND and the round. */
void gfx_round(uint8_t round)
{
    uint8_t *p = line(3) + 35;
    p[0] = CH_FONT_R;
    p[1] = CH_FONT_N;
    p[2] = CH_FONT_D;
    p += 41;
    p[0] = round >= 10 ? CH_FONT_0 + (round / 10) % 10 : CH_PANEL;
    p[1] = CH_FONT_0 + round % 10;
}

/* ---- sprites ------------------------------------------------------------- */

void spr_at(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat, uint8_t big)
{
    spr_x[n] = 48 + x;
    spr_fr[n] = frame;
    spr_big[n] = big;
    spr_col[n] = hue(seat_colour[seat & 3]);
    spr_y[n] = 24 + y;
}

void spr_arena(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat)
{
    spr_x[n] = 48 + AR_COL * 4 + (x >> 1);
    spr_fr[n] = frame;
    spr_big[n] = 0;
    spr_col[n] = hue(seat_colour[seat & 3]);
    spr_y[n] = 24 + AR_ROW * 8 + y;
}

void spr_hide(uint8_t n)
{
    spr_y[n] = 0;
}

void spr_hide_all(void)
{
    spr_y[0] = spr_y[1] = spr_y[2] = spr_y[3] = 0;
}

#endif

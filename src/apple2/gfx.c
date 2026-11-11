/*
 * gfx.c -- the Apple II's screens (gfx.h), in double hi-res: 140x192 in 16
 * colours, both pages, drawn one while the other shows.
 *
 * The gfx_* calls only change a model of the screen, as on the Atari: 25
 * rows of cells (text, big text, the logo or trophies) or the arena's
 * tiles, and the sprites. gfx_render() (frame_wait calls it) brings the page
 * not shown up to date with the model -- what each page last drew is kept
 * apart -- and asks frame_tick to show it at the next blank.
 *
 *   text rows    35 columns of 4 pixels (the 3x7 font); the Atari's 40
 *                columns come 2 to the left, so its centred lines stay centred
 *   big rows     20 columns of 7 pixels (a group each; the 5x7 font)
 *   menu rows    row r is lines 8(r-1) to 8r-1: the menus use rows 1-24
 *   the arena    lines 0-7 the HUD (row 0); a 4-pixel wall round 13x11
 *                tiles of 7x16 at pixel 28, line 12; the stats left, the
 *                round right
 *
 * Sprites are drawn last, and taken off the next time that page is drawn
 * by drawing what is under them again (tiles, or the rows' cells).
 *
 * The rows' cells live in text page 1 ($0400-$07FF), which double hi-res
 * leaves unused: three rows of 35 in each 120 bytes between the screen
 * holes (the cards' scratch bytes, never touched).
 */
#ifdef __APPLE2__

#include <string.h>
#include "gfx.h"
#include "plat.h"
#include "proto.h"
#include "gfxcodes.h"

/* dhgr.s */
extern uint8_t dh_page, dh_y, dh_n, dh_g, dh_g1, dh_x, dh_big, dh_bg, flip;
extern uint16_t dh_src, dh_tb, dh_ts, dh_tv;
#pragma zpsym("dh_page")
#pragma zpsym("dh_y")
#pragma zpsym("dh_n")
#pragma zpsym("dh_g")
#pragma zpsym("dh_g1")
#pragma zpsym("dh_x")
#pragma zpsym("dh_big")
#pragma zpsym("dh_bg")
#pragma zpsym("flip")
#pragma zpsym("dh_src")
#pragma zpsym("dh_tb")
#pragma zpsym("dh_ts")
#pragma zpsym("dh_tv")
void dh_tile(void);
void dh_lines(void);
void dh_frags(void);
void dh_fill(void);
void dh_sprite(void);
void dh_mover(void);
void frame_tick(void);

/* gfxa.s */
void pack_text(void);
void __fastcall__ tile_mark(uint8_t t);
void tile_drain(void);
void __fastcall__ tile_part(uint8_t t);
extern uint8_t *ptx_cells;
extern uint8_t ptx_line, ptx_fg, ptx_hi, tm_page, tm_phase, tp_l0, tp_n;
extern uint8_t tile_want[], tile_shown0[], tile_shown1[], tqn0, tqn1;

uint8_t fragbuf[20], fgbuf[20];         /* dh_frags' line */
uint16_t renders;                       /* pages drawn (the rigs time them) */
uint8_t ov_here;                        /* the overlay is in page 2's main half (overlay.c) */
static uint8_t one;                     /* page 1 only, shown as it is drawn: page 2 holds code */

#define AR_G     4              /* the arena's first group (pixel 28) */
#define AR_X     28
#define AR_Y     12             /* and line */
#define SLOTS    8              /* sprites 0-3, movers 4-7 */
#define MOVER    4
#define CELLS    35
#define NONE_FREE 0xFF

/* palettes: background, text, highlighted text, big colours 0-3 */
static const uint8_t pal[][7] = {
    { 2, 15, 13, 13, 15, 14, 7 },       /* MENU: white on dark blue */
    { 2, 15, 13, 13, 15, 14, 7 },       /* GFX */
    { 0, 15, 13, 15, 11, 7, 12 },       /* HUD: the seats' colours on black */
    { 0, 15, 13, 15, 15, 15, 15 },      /* ARENA */
    { 2, 15, 13, 15, 15, 15, 15 },      /* SEAT0..3 */
    { 2, 15, 13, 11, 11, 11, 11 },
    { 2, 15, 13, 7, 7, 7, 7 },
    { 2, 15, 13, 12, 12, 12, 12 },
    { 2, 15, 13, 13, 15, 14, 7 },       /* TITLE */
};

/* the cells: text page 1, three rows a 128 bytes (the last row apart) */
static uint8_t row24[CELLS];
#define TP(r) ((uint8_t *)0x0400 + (r) / 3 * 128 + (r) % 3 * CELLS)
static uint8_t *const cellp[SCR_ROWS] = {
    TP(0), TP(1), TP(2), TP(3), TP(4), TP(5), TP(6), TP(7), TP(8), TP(9),
    TP(10), TP(11), TP(12), TP(13), TP(14), TP(15), TP(16), TP(17), TP(18),
    TP(19), TP(20), TP(21), TP(22), TP(23), row24,
};
#define GFX_LOGO   1            /* a GFX row's cells: kind, part, group, count */
#define GFX_TROPHY 2

static uint8_t row_kind[SCR_ROWS], row_pal[SCR_ROWS], row_ver[SCR_ROWS];
static uint8_t showing, arena, stat_want[5], round_want;
static uint8_t sp_on[SLOTS], sp_x[SLOTS], sp_y[SLOTS], sp_fr[SLOTS], sp_seat[SLOTS], sp_big[SLOTS];

/* what each page has: index k + page * count */
static uint8_t back = 1;                /* the page drawn next (page 1 shows) */
static uint8_t pg;                      /* the page being drawn */
static uint8_t full[2], phase_shown[2], round_shown[2];
static uint8_t row_seen[2 * SCR_ROWS], row_force[2 * SCR_ROWS];
static uint8_t sum_lo[2 * SCR_ROWS], sum_hi[2 * SCR_ROWS];
static uint8_t stat_shown[2 * 5];
static uint8_t dr_on[2 * SLOTS], dr_g[2 * SLOTS], dr_ng[2 * SLOTS], dr_y[2 * SLOTS], dr_h[2 * SLOTS];
static uint8_t dr_x[2 * SLOTS], dr_f[2 * SLOTS], dr_s[2 * SLOTS];   /* what it was */
static uint8_t keep[SLOTS];             /* this render: as it is on the page */
static uint8_t redrawn[SCR_ROWS];
static uint8_t hud_sh[2 * 20];          /* the HUD's cells as each page has them */
#define ANIMS 24
static uint8_t anim_t[ANIMS], anim_n;   /* the tiles that animate (bombs, duds) */

/* ---- the model's calls ---------------------------------------------------- */

void gfx_init(void)
{
    gfx_begin();
}

void gfx_begin(void)
{
    uint8_t r;
    showing = 0;
    arena = 0;
    memset(sp_on, 0, SLOTS);
    for (r = 0; r < SCR_ROWS; ++r) {
        memset(cellp[r], 0, CELLS);
        row_kind[r] = ROW_TEXT;
        row_pal[r] = PAL_MENU;
        ++row_ver[r];
    }
}

void gfx_row(uint8_t row, uint8_t kind, uint8_t p)
{
    row_kind[row] = kind;
    row_pal[row] = p;
    ++row_ver[row];
}

void gfx_show(void)
{
    full[0] = full[1] = 1;
    showing = 1;
}

void gfx_clear_row(uint8_t row)
{
    memset(cellp[row], 0, CELLS);
    ++row_ver[row];
}

static uint8_t internal(char c)
{
    uint8_t a = (uint8_t)c;
    if (a >= 'a' && a <= 'z')
        a -= 32;
    if (a < 32 || a > 95)
        a = 32;
    return a - 32;
}

void gfx_big_line(uint8_t row, const char *s, const uint8_t *colour)
{
    uint8_t i, *p = cellp[row];
    for (i = 0; i < 20; ++i)
        p[i] = internal(s[i]) | colour[i] << 6;
    ++row_ver[row];
}

void gfx_text(uint8_t row, uint8_t col, const char *s, uint8_t colour)
{
    uint8_t *p = cellp[row], add, width = 20;
    frame_tick();
    if (row_kind[row] == ROW_BIG) {
        add = colour << 6;
    } else {
        add = colour ? 0x80 : 0;
        width = CELLS;
        for (; col < 2 && *s; ++col)    /* the Atari's 40 columns, 2 in */
            ++s;
        col -= 2;
    }
    for (; *s && col < width; ++col)
        p[col] = internal(*s++) | add;
    ++row_ver[row];
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

static void gfx_cells(uint8_t row, uint8_t kind, uint8_t part, uint8_t g, uint8_t n, uint8_t p)
{
    uint8_t *c = cellp[row];
    gfx_row(row, ROW_GFX, p);
    c[0] = kind;
    c[1] = part;
    c[2] = g;
    c[3] = n;
}

void gfx_logo(uint8_t row)
{
    gfx_cells(row, GFX_LOGO, 0, 0, 0, PAL_TITLE);
    gfx_cells(row + 1, GFX_LOGO, 1, 0, 0, PAL_TITLE);
    gfx_cells(row + 2, GFX_LOGO, 2, 0, 0, PAL_TITLE);
}

void gfx_trophies(uint8_t row, uint8_t col, uint8_t n)
{
    gfx_cells(row, GFX_TROPHY, 0, col >> 1, n, PAL_GFX);      /* 2 columns a trophy */
    gfx_cells(row + 1, GFX_TROPHY, 1, col >> 1, n, PAL_GFX);
}

/* ---- the arena ------------------------------------------------------------ */

void gfx_arena(void)
{
    uint8_t r;
    arena = 1;
    gfx_row(0, ROW_BIG, PAL_HUD);
    for (r = 1; r < SCR_ROWS; ++r)
        gfx_row(r, ROW_ARENA, PAL_ARENA);
    memset(tile_want, T_FLOOR, TILES);
    anim_n = 0;
    memset(stat_want, 0, sizeof(stat_want));
    round_want = 0;
}

void gfx_tile(uint8_t c, uint8_t r, uint8_t tile)
{
    uint8_t t = r * ARENA_W + c, m, x0, y0;
    frame_tick();
    if (tile_want[t] != tile) {
        if (tile_n[tile_want[t]] > 1)          /* off the animated list */
            for (m = 0; m < anim_n; ++m)
                if (anim_t[m] == t)
                    anim_t[m] = anim_t[--anim_n];
        if (tile_n[tile] > 1 && anim_n < ANIMS)
            anim_t[anim_n++] = t;
        tile_want[t] = tile;
        tm_page = 0;
        tile_mark(t);
        tm_page = 1;
        tile_mark(t);
    }
    /* a mover over it is gone: play_state draws the tiles under last
     * state's movers again, then puts this state's back */
    x0 = AR_X + c * 7;
    y0 = AR_Y + r * 16;
    for (m = MOVER; m < SLOTS; ++m)
        if (sp_on[m] && sp_x[m] < x0 + 7 && sp_x[m] + 7 > x0 && sp_y[m] < y0 + 16 && sp_y[m] + 16 > y0)
            sp_on[m] = 0;
}

void gfx_bomb_at(uint8_t hx, uint8_t hy, uint8_t dud)
{
    uint8_t m, x = AR_X + (hx >> 1) * 7 + (hx & 1) * 3, y = AR_Y + hy * 8, f = NONE_FREE;
    for (m = MOVER; m < SLOTS; ++m) {
        if (sp_on[m] && sp_x[m] == x && sp_y[m] == y)
            f = m;
        else if (!sp_on[m] && f == NONE_FREE)
            f = m;
    }
    if (f == NONE_FREE)
        return;
    sp_on[f] = 1;
    sp_x[f] = x;
    sp_y[f] = y;
    sp_fr[f] = dud ? 1 : 0;
    sp_seat[f] = AR_G + (hx >> 1);      /* a mover's group, */
    sp_big[f] = hx & 1;                 /* and half a tile on */
}

void gfx_stat(uint8_t i, uint8_t n)
{
    stat_want[i] = n;
}

void gfx_round(uint8_t round)
{
    round_want = round;
}

/* ---- sprites -------------------------------------------------------------- */

static void spr_set(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat, uint8_t big)
{
    frame_tick();               /* (the neutral code's long stretches pass here) */
    sp_on[n] = 1;
    sp_x[n] = x;
    sp_y[n] = y;
    sp_fr[n] = frame;
    sp_seat[n] = seat & 3;
    sp_big[n] = big;
}

void spr_at(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat, uint8_t big)
{
    uint8_t px = x - (x >> 3);  /* x * 7 / 8 */
    if (px > (big ? 126 : 133) || y < 8)
        sp_on[n] = 0;
    else
        spr_set(n, px, y - 8, frame, seat, big);
}

void spr_arena(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat)
{
    spr_set(n, AR_X + ((uint8_t)(x - (x >> 3)) >> 1), AR_Y + y, frame, seat, 0);    /* x * 7 / 16 */
}

void spr_hide(uint8_t n)
{
    sp_on[n] = 0;
}

void spr_hide_all(void)
{
    sp_on[0] = sp_on[1] = sp_on[2] = sp_on[3] = 0;
}

void spr_flush(void)
{
}

/* ---- rendering ------------------------------------------------------------ */

static uint16_t row_sum_of(uint8_t r)
{
    uint8_t a = row_kind[r], b = row_pal[r] + 1, i;
    const uint8_t *c = cellp[r];
    for (i = 0; i < CELLS; ++i) {
        a += c[i];
        b += a;
    }
    return a | (uint16_t)b << 8;
}

/* Lines l0..l1 (0-7) of row r, groups g0..g1. */
static void draw_row(uint8_t r, uint8_t l0, uint8_t l1, uint8_t g0, uint8_t g1)
{
    const uint8_t *c = cellp[r], *pl = pal[row_pal[r]];
    uint8_t l, g, k = row_kind[r];
    if (k == ROW_ARENA || (!arena && !r))
        return;
    dh_bg = pl[0];
    dh_y = (arena ? 0 : (r - 1) * 8) + l0;
    dh_n = l1 - l0 + 1;
    if (k == ROW_GFX) {
        if (c[0] == GFX_LOGO) {
            dh_g = g0 * 2;
            dh_g1 = g1 * 2 + 1;
            dh_src = AX_LOGO + (c[1] * 8 + l0) * 80;
            dh_lines();
            return;
        }
        dh_g = g0;
        dh_g1 = g1;
        dh_fill();
        if (c[0] != GFX_TROPHY)
            return;
        for (g = c[2]; g < c[2] + c[3] && g < 20; ++g) {
            if (g < g0 || g > g1)
                continue;
            dh_g = g;
            dh_y = (arena ? 0 : (r - 1) * 8) + l0;
            dh_n = l1 - l0 + 1;
            dh_src = AX_TILES + (tile_first[T_TROPHY] * 16 + c[1] * 8 + l0) * 4;
            dh_tile();
        }
        return;
    }
    ptx_cells = (uint8_t *)c;
    ptx_fg = pl[1];
    ptx_hi = pl[2];
    for (l = l0; l <= l1; ++l) {
        if (k == ROW_BIG) {
            for (g = g0; g <= g1; ++g) {
                fragbuf[g] = font5[l][c[g] & 0x3F] << 1;
                fgbuf[g] = pl[3 + (c[g] >> 6)];
            }
        } else {
            ptx_line = l;
            pack_text();
        }
        dh_g = g0;
        dh_g1 = g1;
        dh_frags();
        ++dh_y;
    }
}

/* 3x7 text in the arena's panels: s at group g (its first 7 pixels hold
 * two characters), from line y. */
static void panel_text(uint8_t g, uint8_t ng, uint8_t y, const char *s, uint8_t fg)
{
    static uint8_t c[CELLS];
    uint8_t l, i;
    memset(c, 0, CELLS);
    for (i = 0; s[i]; ++i)
        c[i] = internal(s[i]);
    ptx_cells = c;
    ptx_fg = ptx_hi = fg;
    dh_bg = 0;
    for (l = 0; l < 7; ++l) {
        ptx_line = l;
        pack_text();
        for (i = 0; i < ng; ++i) {
            fragbuf[g + i] = fragbuf[i];
            fgbuf[g + i] = fg;
        }
        dh_y = y + l;
        dh_g = g;
        dh_g1 = g + ng - 1;
        dh_frags();
    }
}

static void draw_stat(uint8_t i, uint8_t n)
{
    static char b[3];
    uint8_t y = 14 + i * 36;
    dh_g = 1;
    dh_y = y;
    dh_n = 16;
    if (i >= 3 && !n) {
        dh_g1 = 1;
        dh_bg = 0;
        dh_fill();
        return;
    }
    dh_src = AX_TILES + (tile_first[T_FIRE + i] + tile_n[T_FIRE + i] - 1) * 64;
    dh_tile();
    if (i < 3) {
        b[0] = 'X';
        b[1] = n < 10 ? '0' + n : '+';
        panel_text(2, 1, y + 8, b, 15);
    }
}

static void draw_round(uint8_t round)
{
    static char b[4];
    panel_text(18, 2, 20, "RND", 15);
    b[0] = ' ';
    b[1] = round >= 10 ? '0' + (round / 10) % 10 : ' ';
    b[2] = '0' + round % 10;
    panel_text(18, 2, 30, b, 13);
}

/* The arena's frame: the HUD's row aside, every line from 8 down. */
static void draw_frame(void)
{
    uint8_t y;
    for (y = 8; y < 192; y += 4) {
        dh_g = 0;
        dh_g1 = 39;
        dh_y = y;
        dh_n = 4;
        dh_src = AX_FRAME + (y < AR_Y || y >= AR_Y + 176 ? 0 : 4 * 80);
        dh_lines();
    }
}

/* What sprite slot k (at index i = k + page) drew comes off. */
static void erase(uint8_t i)
{
    uint8_t y0 = dr_y[i], y1 = y0 + dr_h[i] - 1, g0 = dr_g[i], g1 = g0 + dr_ng[i] - 1;
    uint8_t c0, c1, r0, r1, r, c, l0, l1;
    if (y1 > 191)
        y1 = 191;
    if (g1 > 19)
        g1 = 19;
    if (arena) {
        /* just the lines it covered, of the tiles under it */
        if (g1 < AR_G || g0 >= AR_G + ARENA_W || y1 < AR_Y)
            return;
        c0 = g0 < AR_G ? 0 : g0 - AR_G;
        c1 = g1 >= AR_G + ARENA_W ? ARENA_W - 1 : g1 - AR_G;
        if (y0 < AR_Y)
            y0 = AR_Y;
        if (y1 > AR_Y + ARENA_H * 16 - 1)
            y1 = AR_Y + ARENA_H * 16 - 1;
        r0 = (y0 - AR_Y) >> 4;
        r1 = (y1 - AR_Y) >> 4;
        for (r = r0; r <= r1; ++r) {
            frame_tick();
            l0 = r == r0 ? (y0 - AR_Y) & 15 : 0;
            l1 = r == r1 ? (y1 - AR_Y) & 15 : 15;
            tp_l0 = l0;
            tp_n = l1 - l0 + 1;
            for (c = c0; c <= c1; ++c)
                tile_part(r * ARENA_W + c);
        }
        return;
    }
    for (r = (y0 >> 3) + 1; r <= (y1 >> 3) + 1 && r < SCR_ROWS; ++r) {
        if (redrawn[r] || row_kind[r] == ROW_ARENA)
            continue;
        l0 = y0 > (r - 1) * 8 ? y0 - (r - 1) * 8 : 0;
        l1 = y1 < r * 8 - 1 ? y1 - (r - 1) * 8 : 7;
        draw_row(r, l0, l1, g0, g1);
    }
}

static void sprites(uint8_t so, uint8_t phase)
{
    static const uint8_t order[SLOTS] = { 4, 5, 6, 7, 0, 1, 2, 3 };
    uint8_t i, k, j, s;
    for (i = 0; i < SLOTS; ++i) {
        frame_tick();
        k = order[i];
        j = k + so;
        if (keep[k])
            continue;
        dr_on[j] = sp_on[k];
        if (!sp_on[k])
            continue;
        dr_x[j] = sp_x[k];
        dr_y[j] = dh_y = sp_y[k];
        dr_s[j] = sp_seat[k];
        dr_ng[j] = 2 + (k < MOVER ? sp_big[k] : 0);
        dr_h[j] = k < MOVER && sp_big[k] ? 32 : 16;
        if (k >= MOVER) {
            dr_f[j] = sp_fr[k] * 4 + phase;
            dr_g[j] = dh_g = sp_seat[k];
            dh_src = AX_MOVERS + (uint16_t)((sp_fr[k] * 2 + sp_big[k]) * 4 + phase) * 256;
            dh_mover();
            continue;
        }
        dr_f[j] = sp_fr[k] | sp_big[k] << 7;
        s = sp_seat[k] * 2;
        dh_x = sp_x[k];
        dh_n = 16;
        dh_big = sp_big[k];
        dh_src = sp_big[k] ? AX_SPRBIG + sp_fr[k] * 128 : AX_SPR + sp_fr[k] * 64;
        dh_tb = AX_CTAB + ctab_of[s] * 512;
        dh_ts = AX_CTAB + ctab_of[s + 1] * 512;
        dh_tv = AX_CTAB + ctab_of[8] * 512;
        dh_sprite();
        dr_g[j] = dh_g;
    }
}

/* Drawn boxes a and b (dr_ indices) meet. */
static uint8_t meet(uint8_t a, uint8_t b)
{
    return dr_g[a] < dr_g[b] + dr_ng[b] && dr_g[b] < dr_g[a] + dr_ng[a] &&
           dr_y[a] < dr_y[b] + dr_h[b] && dr_y[b] < dr_y[a] + dr_h[a];
}

/* Which sprites can stay as they are on page so: drawn there as they are
 * now, and nothing drawn over them this time. */
static void keepers(uint8_t so, uint8_t phase)
{
    uint8_t k, j, m, r, c, c0, c1, r0, r1;
    const uint8_t *shown = so ? tile_shown1 : tile_shown0;
    for (k = 0; k < SLOTS; ++k) {
        j = k + so;
        keep[k] = sp_on[k] && dr_on[j] && dr_x[j] == sp_x[k] && dr_y[j] == sp_y[k] &&
                  dr_s[j] == sp_seat[k] &&
                  dr_f[j] == (k >= MOVER ? sp_fr[k] * 4 + phase : sp_fr[k] | sp_big[k] << 7);
    }
    for (k = 0; k < SLOTS; ++k) {
        frame_tick();
        if (!keep[k])
            continue;
        j = k + so;
        for (m = 0; m < SLOTS; ++m)     /* another's old place, taken off */
            if (!keep[m] && dr_on[m + so] && meet(j, m + so))
                keep[k] = 0;
        if (arena) {                    /* tiles to be drawn under it */
            c0 = dr_g[j] < AR_G ? 0 : dr_g[j] - AR_G;
            c1 = dr_g[j] + dr_ng[j] - 1 - AR_G;
            if (c1 >= ARENA_W)
                c1 = ARENA_W - 1;
            r0 = (dr_y[j] - AR_Y) >> 4;
            r1 = (dr_y[j] + dr_h[j] - 1 - AR_Y) >> 4;
            if (r1 >= ARENA_H)
                r1 = ARENA_H - 1;
            for (r = r0; r <= r1; ++r)
                for (c = c0; c <= c1; ++c)
                    if (shown[r * ARENA_W + c] == 0xFF)
                        keep[k] = 0;
        } else {                        /* rows drawn again under it */
            for (r = (dr_y[j] >> 3) + 1; r <= ((dr_y[j] + dr_h[j] - 1) >> 3) + 1 && r < SCR_ROWS; ++r)
                if (redrawn[r])
                    keep[k] = 0;
        }
    }
}

/* Bring the page not shown up to the model, and have it shown. */
void gfx_render(void)
{
    uint8_t r, k, phase, ro, so, so5;
    uint16_t sum;
    if (!showing)
        return;
    while (flip)                        /* the last page drawn isn't up yet */
        frame_tick();
    pg = one ? 0 : back;
    if (pg)
        ov_here = 0;                    /* (drawing page 2 is the overlay's end) */
    ro = pg ? SCR_ROWS : 0;
    so = pg ? SLOTS : 0;
    so5 = pg ? 5 : 0;
    dh_page = pg ? 0x40 : 0x20;
    tm_page = pg;
    tm_phase = phase = (uint8_t)(frame_count >> 4) & 3;
    if (full[pg]) {
        full[pg] = 0;
        memset(dr_on + so, 0, SLOTS);
        memset(keep, 0, SLOTS);
        memset(row_force + ro, 1, SCR_ROWS);
        if (arena) {
            draw_frame();
            memset(pg ? tile_shown1 : tile_shown0, T_FLOOR, TILES);
            if (pg)
                tqn1 = 0;
            else
                tqn0 = 0;
            for (k = 0; k < TILES; ++k)
                tile_mark(k);
            phase_shown[pg] = phase;
            memset(stat_shown + so5, NONE_FREE, 5);
            round_shown[pg] = NONE_FREE;
        }
    }
    memset(redrawn, 0, SCR_ROWS);
    if (arena) {
        /* the HUD: just the groups that changed (the timer, a name) */
        uint8_t *sh = hud_sh + (pg ? 20 : 0), g, g0;
        const uint8_t *c = cellp[0];
        if (row_force[ro]) {
            row_force[ro] = 0;
            draw_row(0, 0, 7, 0, 19);
            memcpy(sh, c, 20);
        } else {
            for (g = 0; g < 20; ++g) {
                if (sh[g] == c[g])
                    continue;
                for (g0 = g; g < 20 && sh[g] != c[g]; ++g)
                    ;
                draw_row(0, 0, 7, g0, g - 1);
                memcpy(sh + g0, c + g0, g - g0);
            }
        }
    }
    for (r = 1; !arena && r < SCR_ROWS; ++r) {
        frame_tick();
        k = r + ro;
        if (row_force[k] || row_seen[k] != row_ver[r]) {
            row_seen[k] = row_ver[r];
            sum = row_sum_of(r);
            if (row_force[k] || (uint8_t)sum != sum_lo[k] || (uint8_t)(sum >> 8) != sum_hi[k]) {
                row_force[k] = 0;
                sum_lo[k] = (uint8_t)sum;
                sum_hi[k] = sum >> 8;
                draw_row(r, 0, 7, 0, 19);
                redrawn[r] = 1;
            }
        }
    }
    if (arena) {
        tm_page = pg;
        if (phase != phase_shown[pg]) {  /* the animated tiles' next frames */
            phase_shown[pg] = phase;
            for (k = 0; k < anim_n; ++k)
                tile_mark(anim_t[k]);
        }
    }
    frame_tick();
    keepers(so, phase);
    for (k = 0; k < SLOTS; ++k)
        if (!keep[k] && dr_on[k + so])
            erase(k + so);
    frame_tick();
    if (arena) {
        tm_page = pg;
        tile_drain();
        frame_tick();
        for (k = 0; k < 5; ++k)
            if (stat_shown[k + so5] != stat_want[k]) {
                stat_shown[k + so5] = stat_want[k];
                draw_stat(k, stat_want[k]);
            }
        if (round_shown[pg] != round_want) {
            round_shown[pg] = round_want;
            draw_round(round_want);
        }
    }
    sprites(so, phase);
    if (!one) {
        flip = pg + 1;                  /* frame_tick shows it at the blank */
        back = pg ^ 1;
    }
    ++renders;
}

/* Page 1 only (on: page 2's main half is about to take the menus' code,
 * overlay.c), or both pages again (page 2 then drawn afresh). */
void gfx_single(uint8_t on)
{
    if (on) {
        flip = 0;
        *(volatile uint8_t *)0xC054 = 0;        /* page 1 shows (it is a page behind, or up to date) */
        one = 1;
    } else {
        one = 0;
        full[1] = 1;
        back = 1;
    }
}

/* Both pages black, page 1 shown (plat.c, before the display is on). */
void gfx_clear_pages(void)
{
    uint8_t p;
    for (p = 0; p < 2; ++p) {
        dh_page = p ? 0x40 : 0x20;
        dh_y = 0;
        dh_n = 192;
        dh_g = 0;
        dh_g1 = 19;
        dh_bg = 0;
        dh_fill();
    }
}

#endif

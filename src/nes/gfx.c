/*
 * gfx.c -- the NES's screens (gfx.h): a name table of 8 x 8 cells, 2 bits a
 * pixel, one of 4 palettes to each 16 x 16 block (the attribute table).
 *
 * The game's 40-column, 25-row screens are mapped on: row r is name table
 * row r+1 (the arena has its own layout), a text column c is c-4 (the
 * game centres on 40), a BIG row (20 big characters on the Atari) is 20
 * cells from column 6, a sprite's x is x * 13/8 and its y 8 more.
 *
 * Text is the Namco font, one cell a character, drawn in the palette's
 * slot 1: the colour wanted is the palette picked for the cell's block
 * (SEAT_PAL: the seat's, else TEXTPAL by the colour asked for).
 *
 * The arena (the art and its rules: tools/drawart_nes.py): the HUD on row 2,
 * on a black status bar (the backdrop colour is written again at line 30
 * by the scanline interrupt, crt0.s), the arena's 13 x 11 tiles of 2 x 2
 * cells from column 4, row 4, its stone frame at column 3 and 30 and row
 * 26, the stats in columns 0-2. A tile owns its attribute block; the ones
 * that animate are name table writes (their cells are all in the set),
 * a few a frame (anim_step).
 *
 * A bomber is two 8 x 16 sprites in its seat's palette.
 */
#ifdef __NES__

#include <string.h>
#include "gfx.h"
#include "plat.h"
#include "proto.h"
#include "neshw.h"
#include "gfxdata.h"

#define BIG_COL    6
#define TEXT_SHIFT 4
#define AX         4            /* the arena's first cell column and row */
#define AY         4
#define NT(row, col)  (0x2000u + ((uint16_t)(row) << 5) + (col))
#define ATTR_BASE  0x23C0u
#define NONE       0xFF
#define T_TROPHY   18

#define FRAMES()   (*(volatile uint16_t *)&frame_count)

static const uint8_t seat_pal[SEATS] = { 0, 1, 3, 2 };     /* the seat's palette, every set */
static const uint8_t text_pal[4] = { 0, 2, 3, 1 };         /* a colour asked for: white, orange, sky, pink */

static uint8_t row_kind[SCR_ROWS], row_pal[SCR_ROWS];
static uint8_t chr_set;                 /* the background set loaded: 0 none, 1 menu, 2 arena */
static uint8_t attr[64];                /* the attribute table's shadow */
static uint8_t arena_on;

/* ---- the queue ------------------------------------------------------------- */

static void q_room(uint8_t n)
{
    uint16_t f;
    while ((uint8_t)(qtail - qhead - 1) < n) {
        if (ppu_direct) {
            ppu_drain(255);
        } else {
            f = FRAMES();
            while (FRAMES() == f)
                ;
        }
    }
}

/* n (1-127) bytes at a PPU address; a fill: d points at the one byte. */
static void q_cmd(uint16_t addr, uint8_t n, const uint8_t *d, uint8_t fill)
{
    uint8_t h, i;
    q_room(n + 4);
    h = qhead;
    ppuq[h++] = fill ? 0x80 | n : n;
    ppuq[h++] = (uint8_t)(addr >> 8);
    ppuq[h++] = (uint8_t)addr;
    if (fill) {
        ppuq[h++] = *d;
    } else {
        for (i = 0; i < n; ++i)
            ppuq[h++] = d[i];
    }
    qhead = h;
}

static void q_fill(uint16_t addr, uint8_t n, uint8_t v)
{
    q_cmd(addr, n, &v, 1);
}

/* ---- attributes ------------------------------------------------------------ */

/* The block (bx 0-15, by 0-14) gets palette p. */
static void set_block(uint8_t bx, uint8_t by, uint8_t p)
{
    uint8_t i = (by >> 1) * 8 + (bx >> 1);
    uint8_t sh = ((by & 1) << 2) | ((bx & 1) << 1);
    uint8_t v = (uint8_t)((attr[i] & ~(3 << sh)) | (p << sh));
    if (v != attr[i]) {
        attr[i] = v;
        q_cmd(ATTR_BASE + i, 1, &v, 0);
    }
}

/* the blocks that cells col..col+w-1, row..row+h-1 touch */
static void claim(uint8_t col, uint8_t row, uint8_t w, uint8_t h, uint8_t p)
{
    uint8_t bx, by;
    for (by = row >> 1; by <= (uint8_t)(row + h - 1) >> 1; ++by)
        for (bx = col >> 1; bx <= (uint8_t)(col + w - 1) >> 1; ++bx)
            set_block(bx, by, p);
}

/* ---- the display's state ----------------------------------------------------- */

static void direct_on(void)
{
    ppu_off = 0;
    ppu_direct = 1;
    while (!ppu_off)
        ;
    qtail = qhead;              /* (what was queued was for the last screen) */
}

static void chr_load(uint16_t addr, const uint8_t *src, uint16_t n)
{
    (void)PPU_STATUS;
    PPU_ADDR = (uint8_t)(addr >> 8);
    PPU_ADDR = (uint8_t)addr;
    while (n--)
        PPU_DATA = *src++;
}

static void chr_clear(uint16_t addr, uint16_t n)
{
    (void)PPU_STATUS;
    PPU_ADDR = (uint8_t)(addr >> 8);
    PPU_ADDR = (uint8_t)addr;
    while (n--)
        PPU_DATA = 0;
}

static void load_pal(const uint8_t *p)
{
    q_cmd(0x3F00, 16, p, 0);
    bd_top = bd_bot = p[0];
    split_on = 0;
}

static void clear_screen(void)
{
    uint8_t i;
    for (i = 0; i < 8; ++i)
        q_fill(0x2000u + i * 120u, 120, 0);
    q_fill(ATTR_BASE, 64, 0);
    memset(attr, 0, sizeof(attr));
}

static uint8_t glyph_of(char ch)
{
    uint8_t a = (uint8_t)ch;
    if (a >= 'a' && a <= 'z')
        a -= 32;
    return a >= 32 && a < 96 ? a - 32 : 0;
}

/* ---- the sprites' state --------------------------------------------------------- */

static uint8_t son[SEATS], sbig[SEATS], sfr[SEATS], sseat[SEATS];
static uint8_t sx[SEATS], sy[SEATS];
static uint8_t oam_used;

void gfx_init(void)
{
    uint8_t i;
    direct_on();
    chr_clear(0x1000, 0x1000);          /* (the empty sprite slots fetch tile $FF: it must be blank) */
    chr_load(0x1000, spr_chr, SPR_PAIRS * 32);
    for (i = 0; i < 4; ++i)
        q_cmd(0x3F11 + i * 4, 3, pal_spr + i * 4 + 1, 0);
    ppu_drain(255);
    memset(oam, 0xF8, 256);
    memset(row_kind, ROW_TEXT, sizeof(row_kind));
    gfx_begin();
}

void gfx_begin(void)
{
    uint8_t r;
    direct_on();
    arena_on = 0;
    spr_hide_all();
    spr_flush();
    if (chr_set != 1) {
        chr_load(0x0000, menu_chr, MENU_CELLS * 16);
        chr_set = 1;
    }
    load_pal(pal_menu);
    clear_screen();
    for (r = 0; r < SCR_ROWS; ++r) {
        row_kind[r] = ROW_TEXT;
        row_pal[r] = PAL_MENU;
    }
}

void gfx_row(uint8_t row, uint8_t kind, uint8_t p)
{
    row_kind[row] = kind;
    row_pal[row] = p;
}

void gfx_show(void)
{
    uint16_t f;
    ppu_drain(255);
    ppu_direct = 0;
    f = FRAMES();
    while (FRAMES() == f)
        ;
}

void gfx_clear_row(uint8_t row)
{
    q_fill(NT(row + 1, 0), 32, 0);
}

/* ---- text ----------------------------------------------------------------------- */

/* The palette a cell of a text row is in. */
static uint8_t text_palette(uint8_t row, uint8_t colour)
{
    uint8_t p = row_pal[row];
    if (p >= PAL_SEAT0 && p <= PAL_SEAT3)
        return seat_pal[p - PAL_SEAT0];
    return text_pal[colour & 3];
}

/* The HUD: 5 groups of 4 characters (a seat's name and wins, the timer),
 * each in its seat's palette, in blocks of their own. */
void gfx_big_line(uint8_t row, const char *s, const uint8_t *colour)
{
    uint8_t line[32], i, c;
    memset(line, 0, sizeof(line));
    for (i = 0; i < 20; ++i) {
        c = 2 + 6 * (i >> 2) + (i & 3);
        line[c] = glyph_of(s[i]);
    }
    q_cmd(NT(row + 2, 0), 32, line, 0);
    for (i = 0; i < 20; ++i) {
        c = 2 + 6 * (i >> 2) + (i & 3);
        if (line[c])
            set_block(c >> 1, (row + 2) >> 1, seat_pal[colour[i] & 3]);
    }
}

void gfx_text(uint8_t row, uint8_t col, const char *s, uint8_t colour)
{
    uint8_t line[32], n = 0, big = row_kind[row] == ROW_BIG, p, i;
    /* text that starts left of the centred 32 (the room list and its cursor)
     * is shifted by 2 instead, so it all shows */
    int16_t pos = big ? BIG_COL + col : (int16_t)col - (col < TEXT_SHIFT ? 2 : TEXT_SHIFT), start = -1;
    p = text_palette(row, colour);
    for (; *s; ++s, ++pos) {
        if (pos < 0)
            continue;
        if (pos > 31)
            break;
        if (start < 0)
            start = pos;
        line[n++] = glyph_of(*s);
    }
    if (!n)
        return;
    q_cmd(NT(row + 1, (uint8_t)start), n, line, 0);
    for (i = 0; i < n; ++i)
        if (line[i])
            set_block((uint8_t)(start + i) >> 1, (row + 1) >> 1, p);
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
    uint8_t r, i, nr = row + 1;
    load_pal(pal_title);
    for (r = 0; r < 3; ++r) {
        gfx_row(row + r, ROW_GFX, PAL_TITLE);
        q_cmd(NT(nr + r, (32 - LOGO_W) / 2), LOGO_W, logo_cell[r], 0);
        for (i = 0; i < LOGO_W; i += 2)
            set_block(((32 - LOGO_W) / 2 + i) >> 1, (nr + r) >> 1, logo_pal[r]);
        set_block(((32 - LOGO_W) / 2 + LOGO_W - 1) >> 1, (nr + r) >> 1, logo_pal[r]);
    }
}

static void put_tile(uint8_t col, uint8_t row, uint8_t tile, uint8_t ph)
{
    const uint8_t *c = tile_cell[tile][ph];
    q_cmd(NT(row, col), 2, c, 0);
    q_cmd(NT(row + 1, col), 2, c + 2, 0);
    claim(col, row, 2, 2, tile_pal[tile]);
}

/* The trophies: 2 rows of 16 x 16 tiles (the menu set's cells), from an even
 * row (a tile owns its attribute block), under the name's row. */
void gfx_trophies(uint8_t row, uint8_t col, uint8_t n)
{
    uint8_t i, c = BIG_COL + col / 2, nr = (row + 2) & 0xFE;
    gfx_row(row, ROW_GFX, PAL_GFX);
    gfx_row(row + 1, ROW_GFX, PAL_GFX);
    if (n > (32 - c) / 2)
        n = (32 - c) / 2;
    for (i = 0; i < n; ++i) {
        q_cmd(NT(nr, c + i * 2), 2, trophy_cell, 0);
        q_cmd(NT(nr + 1, c + i * 2), 2, trophy_cell + 2, 0);
        claim(c + i * 2, nr, 2, 2, tile_pal[T_TROPHY]);
    }
}

/* ---- the arena ---------------------------------------------------------------------- */

#define ARENA_TILES (ARENA_W * ARENA_H)

static uint8_t drawn[ARENA_TILES], dph[ARENA_TILES];
static uint8_t anim_flag[N_TILEGFX];
static uint8_t anim_pos, anim_c, anim_r;

void gfx_arena(void)
{
    uint8_t i, r;
    uint8_t wall = CH_WALL;
    chr_load(0x0000, arena_chr, ARENA_CELLS * 16);
    chr_set = 2;
    load_pal(pal_arena);
    bd_top = BLACK_TOP;                 /* the status bar: black, then the floor's green */
    split_on = 1;
    for (i = 0; i < N_TILEGFX; ++i)
        anim_flag[i] = memcmp(tile_cell[i][0], tile_cell[i][1], 4) || memcmp(tile_cell[i][0], tile_cell[i][2], 4)
                       || memcmp(tile_cell[i][0], tile_cell[i][3], 4);
    memset(drawn, NONE, sizeof(drawn));
    memset(dph, 0, sizeof(dph));
    anim_pos = anim_c = anim_r = 0;
    arena_on = 1;
    for (r = 4; r <= 26; ++r) {
        q_fill(NT(r, 3), 1, wall);
        q_fill(NT(r, 30), 1, wall);
    }
    q_fill(NT(26, 4), 26, wall);
    claim(3, 4, 1, 23, 0);
    claim(30, 4, 1, 23, 0);
    claim(4, 26, 26, 1, 0);
}

void gfx_tile(uint8_t c, uint8_t r, uint8_t tile)
{
    uint8_t k = r * ARENA_W + c, ph = (FRAMES() >> 3) & 3;
    drawn[k] = tile;
    dph[k] = ph;
    put_tile(AX + c * 2, AY + r * 2, tile, ph);
}

void gfx_bomb_at(uint8_t hx, uint8_t hy, uint8_t dud)
{
    put_tile(AX + hx, AY + hy, dud ? T_DUD : T_BOMB, (FRAMES() >> 3) & 3);
}

/* Left panel: powerup i's icon at row 6+4i, its count (to 9) beside it. */
void gfx_stat(uint8_t i, uint8_t n)
{
    uint8_t row = 6 + i * 4, d, blank[2];
    memset(blank, 0, sizeof(blank));
    if (i >= 3 && !n) {
        q_cmd(NT(row, 0), 2, blank, 0);
        q_cmd(NT(row + 1, 0), 2, blank, 0);
        q_fill(NT(row + 1, 2), 1, 0);
        return;
    }
    put_tile(0, row, T_FIRE + i, 1);
    if (i < 3) {
        d = glyph_of('0' + (n > 9 ? 9 : n));
        q_cmd(NT(row + 1, 2), 1, &d, 0);
        set_block(1, (row + 1) >> 1, 0);
    }
}

/* Under the stats: RND and the round. */
void gfx_round(uint8_t round)
{
    uint8_t d[3];
    d[0] = glyph_of('R');
    d[1] = glyph_of('N');
    d[2] = glyph_of('D');
    q_cmd(NT(24, 0), 3, d, 0);
    set_block(0, 12, 0);
    set_block(1, 12, 0);
    d[0] = glyph_of('0' + round % 10);
    q_cmd(NT(26, 1), 1, d, 0);
    set_block(0, 13, 2);
}

/* The animated tiles' cells, redrawn for the phase: what this frame has time for. */
static void anim_step(void)
{
    uint8_t ph = (FRAMES() >> 3) & 3, n, k, t, left = 3;
    for (n = 0; n < 24 && left; ++n) {
        k = anim_r * ARENA_W + anim_c;
        t = drawn[k];
        if (t != NONE && anim_flag[t] && dph[k] != ph) {
            put_tile(AX + anim_c * 2, AY + anim_r * 2, t, ph);
            dph[k] = ph;
            --left;
        }
        if (++anim_c >= ARENA_W) {
            anim_c = 0;
            if (++anim_r >= ARENA_H)
                anim_r = 0;
        }
    }
}

/* ---- sprites ------------------------------------------------------------------------- */

void spr_at(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat, uint8_t big)
{
    uint16_t px = (uint16_t)x * 13 >> 3;
    son[n] = px < 256;
    sx[n] = (uint8_t)px;
    sy[n] = y + 8;
    sfr[n] = frame;
    sseat[n] = seat & 3;
    sbig[n] = big;
}

void spr_arena(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat)
{
    son[n] = 1;
    sx[n] = AX * 8 + x;
    sy[n] = AY * 8 + y;
    sfr[n] = frame;
    sseat[n] = seat & 3;
    sbig[n] = 0;
}

void spr_hide(uint8_t n)
{
    son[n] = 0;
}

void spr_hide_all(void)
{
    son[0] = son[1] = son[2] = son[3] = 0;
}

/* Draw the sprites as set (frame_wait does), and the animation. */
void spr_flush(void)
{
    uint8_t n, q, o = 0, f;
    const uint8_t *t;
    oam_lock = 1;
    for (n = 0; n < SEATS; ++n) {
        if (!son[n])
            continue;
        if (sbig[n]) {
            f = sfr[n] - FR_WIN0 > 1 ? 0 : sfr[n] - FR_WIN0;
            for (q = 0; q < 8; ++q) {
                t = spr_big[f][q];
                oam[o++] = sy[n] + (q >> 2) * 16 - 1;
                oam[o++] = t[0];
                oam[o++] = t[1] | sseat[n];
                oam[o++] = sx[n] + (q & 3) * 8;
            }
        } else {
            for (q = 0; q < 2; ++q) {
                t = spr_frame[sfr[n]][q];
                oam[o++] = sy[n] - 1;
                oam[o++] = t[0];
                oam[o++] = t[1] | sseat[n];
                oam[o++] = sx[n] + q * 8;
            }
        }
    }
    for (q = o; q < oam_used; q += 4)
        oam[q] = 0xF8;
    oam_used = o;
    oam_lock = 0;
    if (arena_on)
        anim_step();
}

#endif

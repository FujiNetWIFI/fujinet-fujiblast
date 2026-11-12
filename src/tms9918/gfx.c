/*
 * gfx.c -- the Adam's screens (gfx.h): the TMS9918A in Graphics II used as
 * a character map. The screen is 32 x 24 cells, each its own pattern and
 * colours (two colours in a line of 8 pixels); each third of the screen
 * has its own 256 characters, loaded alike in all three.
 *
 * The game's 40-column, 25-row screens are mapped on: row r is row r-1 (the
 * arena has its own layout), a text column c is c-2, a BIG row (20 big
 * characters on the Atari) is 20 cells from column 6, and a sprite's x is
 * x * 3/2, its y 8 less.
 *
 * The arena: row 0 the HUD, rows 1-22 the arena (13 x 11 tiles of 2 x 2
 * cells from column 5), row 23 and columns 4 and 31 its wall, columns 0-3
 * the player's stats and the round. A tile's 4 cells are characters fixed
 * for the screen (gfxcodes.h); the tiles that animate have their characters
 * redrawn in the frame's blank, two a frame (anim_step).
 *
 * Text is the Namco font, a cell a character in the colour wanted: a
 * character for each (glyph, colours) in use is made when first drawn, and
 * forgotten with the screen (glyph_code).
 *
 * Each bomber is two 16 x 16 sprites (the VDP's sprites have a colour
 * each): A in the seat's colour, B in black in front of it with the eyes
 * cut out. Four sprites fit on a line: when 3 bombers share lines they
 * take turns at the front.
 */
#if defined(__ADAM__) || defined(__MSX__)

#include <string.h>
#include "gfx.h"
#include "plat.h"
#include "proto.h"
#include "gfxcodes.h"
#include "vdp.h"

#define R1_OFF  0xA2            /* 16K, the interrupt, 16 x 16 sprites; display off */
#define R1_ON   0xE2
#define TEXT_SHIFT 2
#define BIG_COL    6
#define ARENA_COL  5            /* the arena's first cell column */
#define NT(row, col)  (VR_NAME + (row) * 32 + (col))

/* colours (the TMS9918A's) */
#define BLACK 1
#define BLUE  4
#define WHITE 15
static const uint8_t seat_colour[SEATS] = { 15, 8, 7, 11 };

/* palettes: background, then the 4 colours text asks for */
static const uint8_t pal[][5] = {
    { BLUE, 15, 11, 7, 9 },     /* MENU */
    { BLUE, 15, 11, 7, 9 },     /* GFX */
    { BLACK, 15, 8, 7, 11 },    /* HUD: the seats' colours */
    { BLACK, 15, 8, 7, 11 },    /* ARENA */
    { BLUE, 15, 15, 15, 15 },   /* SEAT0..3: text in the seat's colour */
    { BLUE, 8, 8, 8, 15 },
    { BLUE, 7, 7, 7, 15 },
    { BLUE, 11, 11, 11, 15 },
    { BLUE, 15, 11, 7, 9 },     /* TITLE */
};

static uint8_t row_kind[SCR_ROWS], row_pal[SCR_ROWS];
static uint8_t scr_bg = BLUE;
static uint8_t blanked;                 /* the display is off: the VDP is ours */
static uint8_t arena_on;

/* ---- the VRAM ------------------------------------------------------------ */

static void vwrite(uint16_t addr, const uint8_t *src, uint16_t n)
{
    va_addr = addr;
    va_src = src;
    va_len = n;
    if (blanked)
        va_writef();
    else
        va_write();
}

static void vfill(uint16_t addr, uint8_t v, uint16_t n)
{
    va_addr = addr;
    va_val = v;
    va_len = n;
    va_fill();
}

static void vreg(uint8_t r, uint8_t v)
{
    VDP_LOCK();
    VDP_CTRL = v;
    VDP_CTRL = 0x80 | r;
    VDP_UNLOCK();
}

/* A cell (8 pattern bytes, 8 colours) as character code, in all 3 thirds. */
static void put_cell(uint8_t code, const uint8_t *cell, uint8_t fast)
{
    uint8_t t, was = blanked;
    uint16_t a;
    blanked = fast;
    for (t = 0; t < 3; ++t) {
        a = (uint16_t)code * 8 + t * 0x800;
        vwrite(VR_PAT + a, cell, 8);
        vwrite(VR_COL + a, cell + 8, 8);
    }
    blanked = was;
}

/* ---- characters ----------------------------------------------------------- */

#define FC_MAX 200
static uint8_t next_code;               /* the next character free (0: none) */
static uint16_t fc_key[FC_MAX];
static uint8_t fc_code[FC_MAX], fc_n;
static uint8_t logo_code, trophy_code;

static uint8_t alloc_codes(uint8_t n)
{
    uint8_t c;
    if (!next_code || (uint16_t)next_code + n > 256)
        return 0;
    c = next_code;
    next_code += n;                     /* (256 wraps to 0: none left) */
    return c;
}

/* The character for a glyph (ASCII - 32) in colours fg on bg. */
static uint8_t glyph_code(uint8_t g, uint8_t fg, uint8_t bg)
{
    uint16_t key = (uint16_t)g << 8 | fg << 4 | bg;
    uint8_t i, c, cell[16];
    if (!g && bg == scr_bg)
        return CH_PANEL;
    for (i = 0; i < fc_n; ++i)
        if (fc_key[i] == key)
            return fc_code[i];
    if (fc_n >= FC_MAX || !(c = alloc_codes(1)))
        return CH_PANEL;
    ART_BANK(ART_B);
    memcpy(cell, font_gfx + g * 8, 8);
    memset(cell + 8, fg << 4 | bg, 8);
    put_cell(c, cell, blanked);
    fc_key[fc_n] = key;
    fc_code[fc_n++] = c;
    return c;
}

static uint8_t glyph_of(char ch)
{
    uint8_t a = (uint8_t)ch;
    if (a >= 'a' && a <= 'z')
        a -= 32;
    return a >= 32 && a < 128 ? a - 32 : 0;
}

static uint8_t tms_row(uint8_t row)
{
    return row ? row - 1 : 0;
}

/* A screen's characters start again: code 0 the blank in the screen's colour. */
static void charset_reset(uint8_t bg)
{
    uint8_t blank[16];
    scr_bg = bg;
    memset(blank, 0, 8);
    memset(blank + 8, bg << 4 | bg, 8);
    put_cell(CH_PANEL, blank, 1);
    next_code = 1;
    fc_n = 0;
    logo_code = trophy_code = 0;
    arena_on = 0;
    vreg(7, bg);
}

/* ---- screens ---------------------------------------------------------------- */

static uint8_t sat[64];                 /* the first 16 sprites' attributes */
static uint8_t son[SEATS], sbig[SEATS], sfr[SEATS], sseat[SEATS];
static uint8_t sx[SEATS], sy[SEATS];

void gfx_init(void)
{
    uint8_t f, q;
    static const uint8_t end[1] = { 0xD0 };
    blanked = 1;
    vreg(0, 0x02);              /* Graphics II */
    vreg(1, R1_OFF);
    vreg(2, VR_NAME >> 10);
    vreg(3, 0xFF);              /* colours at 2000h, patterns at 0000h, each third its own */
    vreg(4, 0x03);
    vreg(5, VR_SAT >> 7);
    vreg(6, VR_SPAT >> 11);
    vfill(0, 0, 0x4000);
    ART_BANK(ART_B);
    for (f = 0; f < N_FRAMES; ++f) {
        vwrite(VR_SPAT + (uint16_t)f * 64, spr_b[f], 32);
        vwrite(VR_SPAT + (uint16_t)f * 64 + 32, spr_a[f], 32);
    }
    for (f = 0; f < 2; ++f)
        for (q = 0; q < 4; ++q) {
            vwrite(VR_SPAT + (uint16_t)(N_FRAMES * 2 + f * 8 + q) * 32, spr_bigB[f][q], 32);
            vwrite(VR_SPAT + (uint16_t)(N_FRAMES * 2 + f * 8 + 4 + q) * 32, spr_bigA[f][q], 32);
        }
    vwrite(VR_SAT + 64, end, 1);        /* the 17th sprite ends the list */
    memset(row_kind, ROW_TEXT, sizeof(row_kind));
    gfx_begin();
}

void gfx_begin(void)
{
    uint8_t r;
    frame_wait();
    vreg(1, R1_OFF);
    blanked = 1;
    spr_hide_all();
    gfx_fast = 1;
    spr_flush();
    charset_reset(BLUE);
    vfill(VR_NAME, 0, 768);
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
    frame_wait();
    vreg(1, R1_ON);
    blanked = 0;
}

void gfx_clear_row(uint8_t row)
{
    vfill(NT(tms_row(row), 0), CH_PANEL, 32);
}

/* The HUD's 20 characters are 5 groups of 4 (a seat's name and wins, the
 * timer): each group a cell apart from the next, across the screen. */
void gfx_big_line(uint8_t row, const char *s, const uint8_t *colour)
{
    uint8_t line[24], i, g;
    const uint8_t *p = pal[row_pal[row]];
    for (i = 0; i < 24; ++i)
        line[i] = CH_PANEL;
    for (i = 0; i < 20; ++i) {
        g = i >> 2;
        line[i + g] = glyph_code(glyph_of(s[i]), p[1 + (colour[i] & 3)], p[0]);
    }
    vwrite(NT(tms_row(row), 4), line, 24);
}

void gfx_text(uint8_t row, uint8_t col, const char *s, uint8_t colour)
{
    uint8_t line[32], n = 0, fg, bg, big = row_kind[row] == ROW_BIG;
    int16_t pos = big ? BIG_COL + col : (int16_t)col - TEXT_SHIFT, start = -1;
    const uint8_t *p = pal[row_pal[row]];
    bg = p[0];
    if (big) {
        fg = p[1 + (colour & 3)];
    } else if (colour) {
        fg = 8;                         /* inverse: on a white band */
        bg = WHITE;
    } else {
        fg = p[1];
    }
    for (; *s; ++s, ++pos) {
        if (pos < 0)
            continue;
        if (pos > 31)
            break;
        if (start < 0)
            start = pos;
        line[n++] = glyph_code(glyph_of(*s), fg, bg);
    }
    if (n)
        vwrite(NT(tms_row(row), (uint8_t)start), line, n);
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
    uint8_t r, i, line[LOGO_W];
    if (!logo_code) {
        logo_code = alloc_codes(LOGO_CELLS);
        if (logo_code) {
            ART_BANK(ART_B);
            for (i = 0; i < LOGO_CELLS; ++i)
                put_cell(logo_code + i, logo_cells[i], blanked);
        }
    }
    for (r = 0; r < 3; ++r) {
        gfx_row(row + r, ROW_GFX, PAL_TITLE);
        for (i = 0; i < LOGO_W; ++i)
            line[i] = logo_code ? logo_code + logo_map[r][i] : CH_PANEL;
        vwrite(NT(tms_row(row + r), (32 - LOGO_W) / 2), line, LOGO_W);
    }
}

void gfx_trophies(uint8_t row, uint8_t col, uint8_t n)
{
    uint8_t i, a[32], b[32], c = BIG_COL + col / 2;
    if (!trophy_code) {
        trophy_code = alloc_codes(4);
        if (trophy_code) {
            ART_BANK(ART_B);
            for (i = 0; i < 4; ++i)
                put_cell(trophy_code + i, trophy_cells[i], blanked);
        }
    }
    gfx_row(row, ROW_GFX, PAL_GFX);
    gfx_row(row + 1, ROW_GFX, PAL_GFX);
    if (!trophy_code)
        return;
    if (n > (32 - c) / 2)
        n = (32 - c) / 2;
    for (i = 0; i < n; ++i) {
        a[i * 2] = trophy_code;
        a[i * 2 + 1] = trophy_code + 1;
        b[i * 2] = trophy_code + 2;
        b[i * 2 + 1] = trophy_code + 3;
    }
    if (n) {
        vwrite(NT(tms_row(row), c), a, n * 2);
        vwrite(NT(tms_row(row + 1), c), b, n * 2);
    }
}

/* ---- the arena ----------------------------------------------------------------- */

static uint8_t anim_ph[N_ANIM], anim_next;

void gfx_arena(void)
{
    uint8_t i, r, line[32];
    charset_reset(BLACK);
    ART_BANK(ART_A);
    for (i = 0; i < ARENA_CELLS; ++i)
        put_cell(i, arena_cells[i], 1);
    next_code = ARENA_CELLS;
    memset(anim_ph, 0, sizeof(anim_ph));
    anim_next = 0;
    arena_on = 1;
    gfx_row(0, ROW_BIG, PAL_HUD);
    for (r = 1; r < SCR_ROWS; ++r)
        gfx_row(r, ROW_ARENA, PAL_ARENA);
    for (r = 0; r < 24; ++r) {
        memset(line, CH_PANEL, 32);
        if (r == 23) {
            memset(line + 4, CH_WALL, 28);
        } else if (r) {
            line[4] = line[31] = CH_WALL;
            memset(line + 5, CH_FLOOR, 26);
        }
        vwrite(NT(r, 0), line, 32);
    }
}

static void put4(uint16_t at, const uint8_t *t)
{
    vwrite(at, t, 2);
    vwrite(at + 32, t + 2, 2);
}

void gfx_tile(uint8_t c, uint8_t r, uint8_t tile)
{
    put4(NT(1 + r * 2, ARENA_COL + c * 2), tile_chars[tile]);
}

void gfx_bomb_at(uint8_t hx, uint8_t hy, uint8_t dud)
{
    put4(NT(1 + hy, ARENA_COL + hx), tile_chars[dud ? T_DUD : T_BOMB]);
}

/* Left panel: powerup i's icon at rows 3+4i, its count beside it. */
void gfx_stat(uint8_t i, uint8_t n)
{
    uint16_t at = NT(3 + i * 4, 0);
    uint8_t d[2];
    static const uint8_t blank[2] = { CH_PANEL, CH_PANEL }, zero4[4] = { CH_PANEL, CH_PANEL, CH_PANEL, CH_PANEL };
    if (i >= 3 && !n) {
        put4(at, zero4);
        vwrite(at + 34, blank, 2);
        return;
    }
    put4(at, tile_chars[T_FIRE + i]);
    if (i < 3) {
        d[0] = n >= 10 ? glyph_code(16 + n / 10, WHITE, BLACK) : CH_PANEL;
        d[1] = glyph_code(16 + n % 10, WHITE, BLACK);
        vwrite(at + 32 + 2, d, 2);
    }
}

/* Right of the stats: RND and the round. */
void gfx_round(uint8_t round)
{
    uint8_t d[3];
    d[0] = glyph_code(glyph_of('R'), WHITE, BLACK);
    d[1] = glyph_code(glyph_of('N'), WHITE, BLACK);
    d[2] = glyph_code(glyph_of('D'), WHITE, BLACK);
    vwrite(NT(21, 0), d, 3);
    d[0] = round >= 10 ? glyph_code(16 + (round / 10) % 10, 7, BLACK) : CH_PANEL;
    d[1] = glyph_code(16 + round % 10, 7, BLACK);
    vwrite(NT(22, 0), d, 2);
}

/* ---- sprites ----------------------------------------------------------------- */

void spr_at(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat, uint8_t big)
{
    uint16_t px = (uint16_t)x * 3 >> 1;
    son[n] = px < 256;
    sx[n] = (uint8_t)px;
    sy[n] = y < 8 ? 0 : y - 8;
    sfr[n] = frame;
    sseat[n] = seat & 3;
    sbig[n] = big;
}

void spr_arena(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat)
{
    son[n] = 1;
    sx[n] = 40 + x;
    sy[n] = 8 + y;
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

static void put_sprite(uint8_t slot, uint8_t x, uint8_t y, uint8_t name, uint8_t colour)
{
    uint8_t *p = sat + slot * 4;
    p[0] = y - 1;
    p[1] = x;
    p[2] = name;
    p[3] = colour;
}

/* The animated tiles' characters, redrawn for the phase: what this frame has time for. */
static void anim_step(void)
{
    uint8_t ph = (frame_count >> 3) & 3, k, tries, budget = gfx_fast ? 2 : ANIM_SLOW, j;
    for (tries = 0; tries < N_ANIM && budget; ++tries) {
        k = anim_next;
        if (++anim_next >= N_ANIM)
            anim_next = 0;
        if (anim_ph[k] == ph)
            continue;
        ART_BANK(ART_A);
        for (j = 0; j < 4; ++j)
            put_cell(ANIM_BASE + k * 4 + j, anim_cells[k][ph][j], gfx_fast);
        anim_ph[k] = ph;
        --budget;
    }
}

/* Draw the sprites as set (frame_wait does): the attribute table, and the
 * animation. */
void spr_flush(void)
{
    static uint8_t rot;
    uint8_t n, m, k, near, crowd = 0, base, f, q, was = blanked;
    memset(sat, 0, sizeof(sat));
    for (k = 0; k < 16; ++k)
        sat[k * 4] = 0xC0;              /* off the bottom */
    for (n = 0; n < SEATS; ++n) {
        if (!son[n] || sbig[n])
            continue;
        near = 0;
        for (m = 0; m < SEATS; ++m)
            if (son[m] && !sbig[m] && (uint8_t)(sy[m] - sy[n]) < 16)
                ++near;
        if (near >= 3)
            crowd = 1;
    }
    rot = crowd ? (rot + 1) & 3 : 0;
    for (n = 0; n < SEATS; ++n) {
        if (!son[n])
            continue;
        if (sbig[n]) {
            f = sfr[n] - FR_WIN0 > 1 ? 0 : sfr[n] - FR_WIN0;
            for (q = 0; q < 4; ++q) {
                base = (N_FRAMES * 2 + f * 8 + q) * 4;
                put_sprite(8 + q, sx[n] + (q & 1) * 16, sy[n] + (q >> 1) * 16, base, BLACK);
                put_sprite(12 + q, sx[n] + (q & 1) * 16, sy[n] + (q >> 1) * 16, base + 16, seat_colour[sseat[n]]);
            }
        } else {
            k = ((n + rot) & 3) * 2;
            put_sprite(k, sx[n], sy[n], sfr[n] * 8, BLACK);
            put_sprite(k + 1, sx[n], sy[n], sfr[n] * 8 + 4, seat_colour[sseat[n]]);
        }
    }
    blanked = was || gfx_fast;
    vwrite(VR_SAT, sat, sizeof(sat));
    blanked = was;
    if (arena_on)
        anim_step();
}

#endif

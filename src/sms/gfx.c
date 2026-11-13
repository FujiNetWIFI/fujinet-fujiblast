/*
 * gfx.c -- the Master System's screens (gfx.h): Mode 4, 32 x 24 cells of
 * 8 x 8 pixels at 4 bits, two 16-colour palettes (the art: tools/
 * drawart_sms.py, converted by tools/mkgfx_sms.py; the layout of
 * gen/sms/gfxdata.h).
 *
 * The game's 40-column, 25-row screens are mapped on as on the Adam (the
 * screen is the same 32 x 24): row r is row r-1, a text column c is c-4 (a
 * column left of that, the room list and its cursor, c-2), a BIG row (20
 * big characters on the Atari) is 20 cells from column 6, a sprite's x is
 * x * 13/8 and its y 8 less.
 *
 * Video memory: tiles 0-255 at 0000h are the sprites' (a set a screen
 * group: arena or menus), tiles 256-447 at 2000h the background's (the
 * set, then the glyph cells made as text is drawn, a cell for each
 * (glyph, foreground, background) in use), names at 3800h, the sprite
 * table at 3F00h.
 *
 * The arena: row 0 the HUD, a black status bar, rows 1-22 the arena (13 x 11
 * tiles of 2 x 2 cells from column 5), row 23 and columns 4 and 31 its
 * wall, columns 0-3 the stats. The tiles that animate are name table
 * writes (their cells are all in the set), a few a frame (anim_step).
 *
 * A bomber is four 8 x 8 sprites in its seat's tiles: a scan line crosses
 * two of them, so four bombers fill the 8 a line.
 */
#ifdef __SMS__

#include <string.h>
#include "gfx.h"
#include "plat.h"
#include "proto.h"
#include "gfxdata.h"

#define BIG_COL    6
#define TEXT_SHIFT 4
#define AX         5            /* the arena's first cell column and row */
#define AY         1
#define NT(row, col)  (0x3800u + ((uint16_t)(row) << 6) + ((col) << 1))
#define SAT_Y      0x3F00u
#define SAT_XT     0x3F80u
#define NONE       0xFF
#define T_TROPHY   18
#define ARENA_TILES (ARENA_W * ARENA_H)

#define FRAMES()   (*(volatile uint16_t *)&frame_count)

extern uint16_t va_addr, va_len;
extern const void *va_src;
extern uint8_t va_val;
void va_write(void);
void va_fill(void);
void va_cram(void);
void va_reg(void);
extern void sms_slot2(uint8_t bank) __z88dk_fastcall;

static const uint8_t seat_arena[SEATS] = TEXT_ARENA_SEAT;
static const uint8_t seat_menu[SEATS] = TEXT_MENU_SEAT;
static const uint8_t text_menu[4] = { TEXT_MENU_WHITE, TEXT_MENU_HI, 9, 8 };    /* a colour asked for: white, highlight, sky, pink */

static uint8_t row_kind[SCR_ROWS], row_pal[SCR_ROWS];
static uint8_t group = 0xFF;            /* the set loaded: 0 menus, 1 arena */
static uint8_t arena_on;

/* ---- the VDP ----------------------------------------------------------------- */

static void vreg(uint8_t r, uint8_t v)
{
    va_addr = r;
    va_val = v;
    va_reg();
}

static void vwrite(uint16_t addr, const void *src, uint16_t n)
{
    va_addr = addr;
    va_src = src;
    va_len = n;
    va_write();
}

static void vfill(uint16_t addr, uint8_t v, uint16_t n)
{
    va_addr = addr;
    va_val = v;
    va_len = n;
    va_fill();
}

/* ---- glyph cells ---------------------------------------------------------------- */

#define FC_MAX 160
static uint16_t fc_key[FC_MAX], fc_word[FC_MAX];
static uint8_t fc_n;
static uint16_t next_cell, cell_end;

static uint8_t glyph_of(char ch)
{
    uint8_t a = (uint8_t)ch;
    if (a >= 'a' && a <= 'z')
        a -= 32;
    return a >= 32 && a < 96 ? a - 32 : 0;
}

/* The name table word of glyph g (ASCII - 32) in palette entries fg on
 * bg: a cell made when first wanted, forgotten with the screen. */
static uint16_t glyph_word(uint8_t g, uint8_t fg, uint8_t bg)
{
    uint8_t i, y, p, gb, cell[32];
    uint16_t key;
    if (!g) {
        if (!group) {
            if (bg == 0)
                return W_BLUE;
        } else if (bg == TEXT_ARENA_FLOOR) {
            return W_FLOOR;
        } else if (bg == TEXT_ARENA_BLACK) {
            return W_BLACK;
        }
        fg = bg;
    }
    key = (uint16_t)g << 8 | (uint8_t)(fg << 4) | bg;
    for (i = 0; i < fc_n; ++i)
        if (fc_key[i] == key)
            return fc_word[i];
    if (fc_n >= FC_MAX || next_cell >= cell_end)
        return group ? W_FLOOR : W_BLUE;
    for (y = 0; y < 8; ++y) {
        gb = font_gfx[g * 8 + y];
        for (p = 0; p < 4; ++p)
            cell[y * 4 + p] = ((fg >> p) & 1 ? gb : 0) | ((bg >> p) & 1 ? (uint8_t)~gb : 0);
    }
    vwrite(next_cell * 32u, cell, 32);
    fc_key[fc_n] = key;
    fc_word[fc_n++] = next_cell;
    return next_cell++;
}

/* ---- screens ------------------------------------------------------------------ */

static uint16_t rowbuf[32];

static void fill_row(uint8_t row, uint16_t w)
{
    uint8_t i;
    for (i = 0; i < 32; ++i)
        rowbuf[i] = w;
    vwrite(NT(row, 0), rowbuf, 64);
}

static void load_pal(const uint8_t *bg, uint8_t backdrop)
{
    uint8_t spr[16];
    memcpy(spr, pal_spr, 16);
    spr[0] = backdrop;
    va_addr = 0;
    va_src = bg;
    va_len = 16;
    va_cram();
    va_addr = 16;
    va_src = spr;
    va_len = 16;
    va_cram();
}

/* The tiles for the screen group (display off): the background's set from
 * 2000h, the sprites' from 0. The art banks are in slot 2: the arena's
 * is the power-on one. */
static void load_group(uint8_t g)
{
    if (group != g) {
        if (g) {
            vwrite(BG_BASE * 32u, arena_chr, ARENA_STATIC * 32u);
            vwrite(0, spr_arena_chr, SPR_ARENA_TILES * 32u);
        } else {
            sms_slot2(3);
            vwrite(BG_BASE * 32u, menu_chr, MENU_STATIC * 32u);
            vwrite(0, spr_menu_chr, SPR_MENU_TILES * 32u);
            sms_slot2(2);
        }
        group = g;
    }
    next_cell = BG_BASE + (g ? ARENA_STATIC : MENU_STATIC);
    cell_end = g ? next_cell + ARENA_TEXT : BG_BASE + BG_TILES;
    fc_n = 0;
}

static uint8_t son[SEATS], sbig[SEATS], sfr[SEATS], sseat[SEATS];
static uint8_t sx[SEATS], sy[SEATS];
static uint8_t oy[65], oxt[128];

void gfx_init(void)
{
    vreg(1, 0xA0);              /* display off, the frame interrupt, 8 x 8 sprites */
    vreg(0, 0x06);              /* Mode 4 */
    vreg(2, 0xFF);              /* names at 3800h */
    vreg(3, 0xFF);
    vreg(4, 0xFF);
    vreg(5, 0xFF);              /* sprite table at 3F00h */
    vreg(6, 0xFB);              /* sprite tiles 0-255 */
    vreg(7, 0x00);              /* the backdrop: sprite palette entry 0 */
    vreg(8, 0x00);
    vreg(9, 0x00);
    vreg(10, 0xFF);             /* no line interrupts */
    vfill(0, 0, 0x4000);
    vfill(SAT_Y, 0xD0, 1);      /* no sprites */
    memset(row_kind, ROW_TEXT, sizeof(row_kind));
    gfx_begin();
}

void gfx_begin(void)
{
    uint8_t r;
    frame_wait();
    vreg(1, 0xA0);
    arena_on = 0;
    spr_hide_all();
    spr_flush();
    load_group(0);
    load_pal(pal_menu, BD_MENU);
    for (r = 0; r < 24; ++r)
        fill_row(r, W_BLUE);
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
    vreg(1, 0xE0);
}

static uint8_t sms_row(uint8_t row)
{
    return row ? row - 1 : 0;
}

void gfx_clear_row(uint8_t row)
{
    fill_row(sms_row(row), W_BLUE);
}

/* ---- text --------------------------------------------------------------------- */

void gfx_text(uint8_t row, uint8_t col, const char *s, uint8_t colour)
{
    uint16_t line[32];
    uint8_t n = 0, fg, big = row_kind[row] == ROW_BIG;
    int16_t pos = big ? BIG_COL + col : (int16_t)col - (col < TEXT_SHIFT ? 2 : TEXT_SHIFT), start = -1;
    fg = row_pal[row] >= PAL_SEAT0 && row_pal[row] <= PAL_SEAT3 ? seat_menu[row_pal[row] - PAL_SEAT0]
                                                                : text_menu[colour & 3];
    for (; *s; ++s, ++pos) {
        if (pos < 0)
            continue;
        if (pos > 31)
            break;
        if (start < 0)
            start = pos;
        line[n++] = glyph_word(glyph_of(*s), fg, 0);
    }
    if (n)
        vwrite(NT(sms_row(row), (uint8_t)start), line, n * 2u);
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
        vwrite(NT(sms_row(row + r), (32 - LOGO_W) / 2), logo_cell[r], LOGO_W * 2u);
    }
}

void gfx_trophies(uint8_t row, uint8_t col, uint8_t n)
{
    uint16_t a[32], b[32];
    uint8_t i, c = BIG_COL + col / 2;
    gfx_row(row, ROW_GFX, PAL_GFX);
    gfx_row(row + 1, ROW_GFX, PAL_GFX);
    if (n > (32 - c) / 2)
        n = (32 - c) / 2;
    for (i = 0; i < n; ++i) {
        a[i * 2] = trophy_cell[0];
        a[i * 2 + 1] = trophy_cell[1];
        b[i * 2] = trophy_cell[2];
        b[i * 2 + 1] = trophy_cell[3];
    }
    if (n) {
        vwrite(NT(sms_row(row), c), a, n * 4u);
        vwrite(NT(sms_row(row + 1), c), b, n * 4u);
    }
}

/* ---- the arena ----------------------------------------------------------------- */

static uint8_t drawn[ARENA_TILES], dph[ARENA_TILES];
static uint8_t anim_flag[N_TILEGFX];
static uint8_t anim_pos, anim_c, anim_r;

static void put_tile(uint8_t col, uint8_t row, uint8_t tile, uint8_t ph)
{
    const uint16_t *w = tile_cell[tile][ph];
    vwrite(NT(row, col), w, 4);
    vwrite(NT(row + 1, col), w + 2, 4);
}

void gfx_arena(void)
{
    uint8_t i, r, c;
    load_group(1);
    load_pal(pal_arena, BD_ARENA);
    for (i = 0; i < N_TILEGFX; ++i)
        anim_flag[i] = memcmp(tile_cell[i][0], tile_cell[i][1], 8) || memcmp(tile_cell[i][0], tile_cell[i][2], 8)
                       || memcmp(tile_cell[i][0], tile_cell[i][3], 8);
    memset(drawn, NONE, sizeof(drawn));
    memset(dph, 0, sizeof(dph));
    anim_pos = anim_c = anim_r = 0;
    arena_on = 1;
    for (r = 0; r < 24; ++r) {
        for (c = 0; c < 32; ++c)
            rowbuf[c] = r == 0 ? W_BLACK : c < 4 ? W_FLOOR : (c == 4 || c == 31 || r == 23) ? W_WALL : W_FLOOR;
        vwrite(NT(r, 0), rowbuf, 64);
        gfx_row(r, r ? ROW_ARENA : ROW_BIG, r ? PAL_ARENA : PAL_HUD);
    }
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

/* The HUD: 5 groups of 4 characters (a seat's name and wins, the timer),
 * in the seats' colours, on the black bar. */
void gfx_big_line(uint8_t row, const char *s, const uint8_t *colour)
{
    uint8_t i, g;
    for (i = 0; i < 32; ++i)
        rowbuf[i] = W_BLACK;
    for (i = 0; i < 20; ++i) {
        g = i >> 2;
        rowbuf[4 + i + g] = glyph_word(glyph_of(s[i]), seat_arena[colour[i] & 3], TEXT_ARENA_BLACK);
    }
    vwrite(NT(row, 0), rowbuf, 64);
}

/* Left panel: powerup i's icon at rows 1+4i... (rows 3+4i of the game's), its count beside it. */
void gfx_stat(uint8_t i, uint8_t n)
{
    uint8_t row = 3 + i * 4;
    uint16_t d[2];
    if (i >= 3 && !n) {
        d[0] = d[1] = W_FLOOR;
        vwrite(NT(row, 0), d, 4);
        vwrite(NT(row + 1, 0), d, 4);
        vwrite(NT(row + 1, 2), d, 4);
        return;
    }
    put_tile(0, row, T_FIRE + i, 1);
    if (i < 3) {
        d[0] = n >= 10 ? glyph_word(glyph_of('0' + n / 10), TEXT_ARENA_WHITE, TEXT_ARENA_FLOOR) : W_FLOOR;
        d[1] = glyph_word(glyph_of('0' + n % 10), TEXT_ARENA_WHITE, TEXT_ARENA_FLOOR);
        vwrite(NT(row + 1, 2), d, 4);
    }
}

/* Under the stats: RND and the round. */
void gfx_round(uint8_t round)
{
    uint16_t d[3];
    d[0] = glyph_word(glyph_of('R'), TEXT_ARENA_WHITE, TEXT_ARENA_FLOOR);
    d[1] = glyph_word(glyph_of('N'), TEXT_ARENA_WHITE, TEXT_ARENA_FLOOR);
    d[2] = glyph_word(glyph_of('D'), TEXT_ARENA_WHITE, TEXT_ARENA_FLOOR);
    vwrite(NT(21, 0), d, 6);
    d[0] = round >= 10 ? glyph_word(glyph_of('0' + (round / 10) % 10), seat_arena[3], TEXT_ARENA_FLOOR) : W_FLOOR;
    d[1] = glyph_word(glyph_of('0' + round % 10), seat_arena[3], TEXT_ARENA_FLOOR);
    vwrite(NT(22, 0), d, 4);
}

/* The animated tiles' words, redrawn for the phase: what this frame has time for. */
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

/* ---- sprites -------------------------------------------------------------------- */

void spr_at(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat, uint8_t big)
{
    uint16_t px = (uint16_t)x * 13 >> 3;
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

static uint8_t put_sprite(uint8_t o, uint8_t x, uint8_t y, uint8_t tile)
{
    if (tile == SPR_NONE || o >= 64)
        return o;
    oy[o] = y - 1;
    oxt[o * 2] = x;
    oxt[o * 2 + 1] = tile;
    return o + 1;
}

/* Draw the sprites as set (frame_wait does): the sprite table, and the animation. */
void spr_flush(void)
{
    uint8_t n, q, o = 0, f;
    const uint8_t *t;
    for (n = 0; n < SEATS; ++n) {
        if (!son[n])
            continue;
        if (sbig[n]) {
            f = sfr[n] - FR_WIN0 > 1 ? 0 : sfr[n] - FR_WIN0;
            t = sprt_big[sseat[n]][f];
            for (q = 0; q < 16; ++q)
                o = put_sprite(o, sx[n] + (q & 3) * 8, sy[n] + (q >> 2) * 8, t[q]);
        } else {
            t = group == 1 ? sprt_arena[sseat[n]][sfr[n]] : sprt_menu[sseat[n]][sfr[n]];
            for (q = 0; q < 4; ++q)
                o = put_sprite(o, sx[n] + (q & 1) * 8, sy[n] + (q >> 1) * 8, t[q]);
        }
    }
    oy[o] = 0xD0;
    vwrite(SAT_Y, oy, o + 1u);
    if (o)
        vwrite(SAT_XT, oxt, o * 2u);
    if (arena_on)
        anim_step();
}

#endif

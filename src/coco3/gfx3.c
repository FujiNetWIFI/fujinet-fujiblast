/*
 * gfx3.c -- the CoCo 3's screens (gfx.h): 320x192 in 16 colours, from the
 * Adam's art. The Atari's 40 columns are 40 cells of 8 pixels, so the layout
 * is the Atari's, a pixel a dot: text 40 columns (the Namco font a cell
 * each), BIG text 20 characters of 16 pixels, the arena at cell column 7
 * (pixel 56) with its wall a cell either side, the HUD (row 0) two cells high.
 *
 * The GIME's horizontal virtual screen puts the lines 256 bytes apart, so
 * the 8K block of the MMU that holds the screen holds 32 whole lines (the
 * screen is blocks 0-5): drawing maps a line's block at $C000 (MMU slot 6)
 * and puts the Disk ROM back after, for DriveWire. Row r (menus 1-24) is
 * lines 8 (r - 1) to 8 r - 1, which stay in one block; the arena's tiles
 * start at line 16, so a tile row is in one block too; a bomber on a block's
 * edge is drawn in two parts. The art is in blocks 6 (bombers, font: slot 7)
 * and 7 (tiles: slot 7), read through $E000.
 *
 * There is no bottom wall: the 11 tile rows end at line 191, so the border
 * colour is the wall's. The palette is the RGB or the composite one (CLEAR).
 */
#ifdef COCO3
#include <string.h>
#include "fbcoco.h"
#include "gfx.h"
#include "gfxcodes.h"
#include "plat.h"
#include "proto.h"
#include "sound.h"

#define MMU6 (*(volatile uint8_t *)0xFFA6)
#define MMU7 (*(volatile uint8_t *)0xFFA7)
#define WINDOW ((uint8_t *)0xC000)
#define ROM_BLK 0x3E            /* $C000's block at boot: the Disk ROM, DriveWire's vectors */
#define BLK_SPR 6
#define BLK_TILE 7

#define AR_X   56               /* the arena, in pixels (a cell column 7) */
#define AR_Y   16
#define AR_BX  (AR_X / 2)       /* and its first byte */

/* palette indexes (tools/mkgfx_coco3.py's ART) */
#define C_BLACK 0
#define C_RED   7
#define C_PINK  8
#define C_TAN   10
#define C_CYAN  12
#define C_GREY  13
#define C_WHITE 14

/* what a BIG row's colours 0-3 are, by palette */
static const uint8_t pal_map[][4] = {
    { C_WHITE, C_TAN, C_CYAN, C_PINK },             /* MENU */
    { C_WHITE, C_TAN, C_CYAN, C_PINK },             /* GFX */
    { C_WHITE, C_RED, C_CYAN, C_TAN },              /* HUD: the seats */
    { C_WHITE, C_TAN, C_CYAN, C_PINK },             /* ARENA */
    { C_WHITE, C_WHITE, C_WHITE, C_WHITE },         /* SEAT0..3 */
    { C_RED, C_RED, C_RED, C_RED },
    { C_CYAN, C_CYAN, C_CYAN, C_CYAN },
    { C_TAN, C_TAN, C_TAN, C_TAN },
    { C_WHITE, C_TAN, C_CYAN, C_PINK },             /* TITLE */
};
/* the bombers: body, trim, visor of each seat (the eyes are black) */
static const uint8_t seat_roles[SEATS][3] = {
    { C_WHITE, C_RED, C_CYAN }, { C_RED, C_WHITE, C_CYAN }, { C_CYAN, C_WHITE, C_RED }, { C_TAN, C_WHITE, C_RED },
};

/* role byte (2 roles) -> colours, a table a seat (in the high core's constants, which are RAM) */
static const uint8_t seat_ro[SEATS * 256] = { 0 };
#define SEAT(t) ((uint8_t *)seat_ro + ((uint16_t)(t) << 8))

uint8_t gfx3_cmp;                       /* the composite palette is up */
static uint8_t blanked;                 /* the palette is all black while a screen is drawn */
static uint8_t row_kind[SCR_ROWS], row_pal[SCR_ROWS];

/* gfx3a.asm's arguments */
const uint8_t *tp_src;
uint8_t *tp_dst;
uint8_t tp_n;
const uint8_t *sd_src;
uint8_t *sd_dst;
uint8_t *sd_tab;
uint8_t sd_n;
uint8_t *sd_buf;
void tile_put3(void);
void spr_draw3(void);
void spr_save3(void);
void spr_restore3(void);

/* ---- sprites (declared early: the screens hide them) ---- */
uint8_t w_on[4], w_x[4], w_y[4], w_fr[4], w_seat[4], w_big[4];
static uint8_t s_on[4], s_x[4], s_y[4], s_fr[4], s_seat[4], s_big[4];
static uint8_t in_arena;
static uint8_t tile_at[TILES];
static uint8_t phase;
static const uint8_t *tcur[N_TILES];    /* a tile's frame now (in block 7) */
static uint8_t drawn_hud[20], drawn_hudc[20];

/* ---- the window ----------------------------------------------------------- */

/* the line's first byte, its block in the window */
static uint8_t *line(uint8_t y)
{
    MMU6 = y >> 5;
    return WINDOW + ((uint16_t)(y & 31) << 8);
}

#define DONE() (MMU6 = ROM_BLK)

static uint8_t under(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
static uint8_t under_rows(uint8_t y, uint8_t h);
static void lift_all(void);

static void set_pal(void)
{
    uint8_t i;
    const uint8_t *p = gfx3_cmp ? pal_cmp : pal_rgb;
    for (i = 0; i < 16; ++i)
        ((volatile uint8_t *)0xFFB0)[i] = blanked ? 0 : p[i];
    *(volatile uint8_t *)0xFF9A = blanked ? 0 : p[C_GREY];
}

static void build_tables(void)
{
    uint8_t t, c, role, hi, lo;
    uint16_t rb;
    for (t = 0; t < SEATS; ++t)
        for (rb = 0; rb < 256; ++rb) {
            role = (rb >> 4) & 3;
            hi = role ? seat_roles[t][role - 1] : 0;
            role = rb & 3;
            lo = role ? seat_roles[t][role - 1] : 0;
            SEAT(t)[rb] = hi << 4 | lo;
        }
    (void)c;
}

static void set_phase(uint8_t p)
{
    uint8_t t;
    phase = p;
    for (t = 0; t < N_TILES; ++t)
        tcur[t] = (const uint8_t *)tile_gfx +
                  (uint16_t)tile_anim[t][t == T_BOMB || t == T_DUD ? p : t >= T_FIRE && t <= T_GLOVE ? 2 : 0] * 128;
}

static void clear_screen(void)
{
    uint8_t b;
    for (b = 0; b < 6; ++b) {
        MMU6 = b;
        memset(WINDOW, 0, 8192);
    }
    DONE();
}

void gfx_init(void)
{
    MMU7 = BLK_TILE;
    build_tables();
    set_phase(0);
    MMU7 = BLK_SPR;
    clear_screen();
    blanked = 0;
    *(volatile uint8_t *)0xFF90 = 0x4C;     /* GIME modes, MMU on, vectors at $FE00 (the sound turns FIRQ on while it plays) */
    *(volatile uint8_t *)0xFF98 = 0x80;     /* graphics */
    *(volatile uint8_t *)0xFF99 = 0x1E;     /* 192 lines, 160 bytes a line, 16 colours */
    *(volatile uint16_t *)0xFF9D = 0;       /* the screen: physical 0 */
    *(volatile uint8_t *)0xFF9C = 0;
    *(volatile uint8_t *)0xFF9F = 0x80;     /* lines 256 bytes apart */
    set_pal();
}

void gfx_clear(void)
{
    clear_screen();
}

void gfx_begin(void)
{
    uint8_t r;
    blanked = 1;
    set_pal();
    clear_screen();
    for (r = 0; r < SCR_ROWS; ++r) {
        row_kind[r] = ROW_TEXT;
        row_pal[r] = PAL_MENU;
    }
    in_arena = 0;
    snd_slice = 9000;
    spr_hide_all();
    memset(s_on, 0, sizeof(s_on));
}

void gfx_show(void)
{
    blanked = 0;
    set_pal();
}

void gfx_row(uint8_t row, uint8_t kind, uint8_t pal)
{
    row_kind[row] = kind;
    row_pal[row] = pal;
}

static uint8_t row_line(uint8_t row)
{
    return row ? 8 * (row - 1) : 0;
}

void gfx_clear_row(uint8_t row)
{
    uint8_t l, y = row_line(row), n = row ? 8 : 16;
    if (under_rows(y, n))
        lift_all();
    for (l = 0; l < n; ++l)
        memset(line(y + l), 0, 160);
    DONE();
}

/* ---- text ---------------------------------------------------------------- */

/* a text character (the Namco font, 8x8: 4 bytes a line) at byte x, line y */
static void text_char(uint8_t x, uint8_t y, char ch, uint8_t c)
{
    const uint8_t *g;
    uint8_t l, b, ex[4];
    uint8_t *p;
    if (ch < 32 || ch > 127)
        ch = 32;
    ex[0] = 0;
    ex[1] = c;
    ex[2] = c << 4;
    ex[3] = c * 17;
    MMU7 = BLK_SPR;
    g = font_gfx + (uint16_t)(ch - 32) * 8;
    for (l = 0; l < 8; ++l) {
        b = g[l];
        p = line(y + l) + x;            /* (a line at a time: y may be anywhere in its block) */
        p[0] = ex[(b >> 6) & 3];
        p[1] = ex[(b >> 4) & 3];
        p[2] = ex[(b >> 2) & 3];
        p[3] = ex[b & 3];
    }
}

/* a BIG character (16 pixels wide, 8 lines or, tall, 16) at byte x, line y */
static void big_char(uint8_t x, uint8_t y, char ch, uint8_t c, uint8_t tall)
{
    const uint8_t *g;
    uint8_t l, k, b, v = c * 17, bit;
    uint8_t *p;
    if (ch < 32 || ch > 127)
        ch = 32;
    MMU7 = BLK_SPR;
    g = font_gfx + (uint16_t)(ch - 32) * 8;
    p = line(y) + x;
    for (l = 0; l < 8; ++l) {
        b = g[l];
        for (k = 0; k < tall + 1; ++k) {
            for (bit = 0; bit < 8; ++bit)
                p[bit] = b & (0x80 >> bit) ? v : 0;
            p += 256;
        }
    }
}

void gfx_big_line(uint8_t row, const char *s, const uint8_t *colour)
{
    uint8_t i, y = row_line(row), c;
    if (under_rows(y, row ? 8 : 16))
        lift_all();
    if (row == 0 && row_pal[0] == PAL_HUD) {
        for (i = 0; i < 20; ++i) {
            c = pal_map[PAL_HUD][colour[i]];
            if (drawn_hud[i] != (uint8_t)s[i] || drawn_hudc[i] != c) {
                drawn_hud[i] = s[i];
                drawn_hudc[i] = c;
                big_char(8 * i, 0, s[i], c, 1);
            }
        }
        DONE();
        return;
    }
    for (i = 0; i < 20; ++i)
        big_char(8 * i, y, s[i], pal_map[row_pal[row]][colour[i]], 0);
    DONE();
}

void gfx_text(uint8_t row, uint8_t col_, const char *s, uint8_t colour)
{
    uint8_t y = row_line(row);
    if (under_rows(y, row ? 8 : 16))
        lift_all();
    if (row_kind[row] == ROW_BIG) {
        uint8_t c = pal_map[row_pal[row]][colour];
        for (; *s && col_ < 20; ++col_, ++s)
            big_char(8 * col_, y, *s, c, 0);
    } else {
        colour = colour ? C_TAN : C_WHITE;
        for (; *s && col_ < 40; ++col_, ++s)
            text_char(4 * col_, y, *s, colour);
    }
    DONE();
}

void gfx_number(uint8_t row, uint8_t col_, uint8_t n, uint8_t colour)
{
    char b[4], *p = b + 3;
    *p = 0;
    do {
        *--p = '0' + n % 10;
        n /= 10;
    } while (n);
    gfx_text(row, col_, p, colour);
}

/* ---- art ------------------------------------------------------------------ */

void gfx_logo(uint8_t row)
{
    uint8_t l, y = row_line(row);
    for (l = 0; l < LOGO_LINES; ++l)
        memcpy(line(y + l) + 26, logo_gfx[l], LOGO_BYTES);
    DONE();
}

/* a tile frame (blocks 7) of tp_n lines at byte x, line y: in two parts on a block's edge */
static void put_frame(const uint8_t *src, uint8_t x, uint8_t y)
{
    uint8_t n = 16, n1 = 32 - (y & 31);
    MMU7 = BLK_TILE;
    if (n1 < n) {
        tp_src = src;
        tp_dst = line(y) + x;
        tp_n = n1;
        tile_put3();
        src += n1 * 8;
        y += n1;
        n -= n1;
    }
    tp_src = src;
    tp_dst = line(y) + x;
    tp_n = n;
    tile_put3();
}

static void put_tile(uint8_t x, uint8_t y, uint8_t frame)
{
    put_frame((const uint8_t *)tile_gfx + (uint16_t)frame * 128, x, y);
    DONE();
}

void gfx_trophies(uint8_t row, uint8_t col_, uint8_t n)
{
    uint8_t x = 4 * col_, y = row_line(row);
    MMU7 = BLK_TILE;
    for (; n && x < 152; --n, x += 8)
        put_frame((const uint8_t *)tile_gfx + (uint16_t)tile_anim[T_TROPHY][0] * 128, x, y);
    DONE();
}

/* ---- the arena --------------------------------------------------------------- */

static void wall_cell(uint8_t x, uint8_t y)
{
    uint8_t l, k;
    uint8_t *p;
    for (l = 0; l < 8; ++l) {
        p = line(y + l) + x;
        for (k = 0; k < 4; ++k)
            p[k] = cell_gfx[CELL_WALL][l * 4 + k];
    }
}

void gfx_arena(void)
{
    uint8_t y;
    gfx_row(0, ROW_BIG, PAL_HUD);
    memset(drawn_hud, 0, sizeof(drawn_hud));
    memset(drawn_hudc, 0xFF, sizeof(drawn_hudc));
    for (y = AR_Y; y < AR_Y + 176; y += 8) {
        wall_cell(24, y);
        wall_cell(132, y);
    }
    DONE();
    memset(tile_at, T_FLOOR, sizeof(tile_at));
    spr_hide_all();
    memset(s_on, 0, sizeof(s_on));
    in_arena = 1;
    snd_slice = 2400;           /* the game needs the CPU */
}

static void draw_tile(uint8_t c, uint8_t r, uint8_t tile)
{
    MMU7 = BLK_TILE;
    tp_src = tcur[tile];
    tp_dst = line(AR_Y + 16 * r) + AR_BX + 8 * c;
    tp_n = 16;
    tile_put3();
}

/* The bombers' own: what each covers is kept (sv_ro, in the high core's
 * constants, which are RAM) and put back before it moves, so a move costs
 * a copy and a draw, not the tiles under it blitted again. */
static const uint8_t sv_ro[4 * 128] = { 0 };
#define SV(n) ((uint8_t *)sv_ro + ((uint16_t)(n) << 7))

/* the 16 lines of 8 bytes at byte x, line y: kept (save) or put back; in two parts on a block's edge */
static void spr_io(uint8_t n, uint8_t x, uint8_t y, uint8_t save)
{
    uint8_t k = 16, n1 = 32 - (y & 31);
    uint8_t *b = SV(n);
    if (n1 < k) {
        sd_dst = line(y) + x;
        sd_buf = b;
        sd_n = n1;
        if (save)
            spr_save3();
        else
            spr_restore3();
        b += n1 * 8;
        y += n1;
        k -= n1;
    }
    sd_dst = line(y) + x;
    sd_buf = b;
    sd_n = k;
    if (save)
        spr_save3();
    else
        spr_restore3();
}

static void restore_spr(uint8_t n)
{
    if (s_big[n]) {                     /* (twice the size, on black: filled) */
        uint8_t l;
        for (l = 0; l < 32; ++l)
            memset(line(s_y[n] + l) + (s_x[n] >> 1), 0, 16);
    } else {
        spr_io(n, s_x[n] >> 1, s_y[n], 0);
    }
}

/* every bomber off the screen (before something is drawn over them) */
static void lift_all(void)
{
    uint8_t n;
    for (n = 4; n--; )
        if (s_on[n]) {
            restore_spr(n);
            s_on[n] = 0;
        }
}

/* pixels x..x+w-1, lines y..y+h-1 under a bomber on the screen? (x, w in 2-pixel units) */
static uint8_t under(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    uint8_t j, sx;
    for (j = 0; j < 4; ++j)
        if (s_on[j]) {
            sx = s_x[j] >> 1;
            if (sx < x + w && x < sx + (s_big[j] ? 16 : 8) && s_y[j] < y + h && y < s_y[j] + (s_big[j] ? 32 : 16))
                return 1;
        }
    return 0;
}

static uint8_t under_rows(uint8_t y, uint8_t h)
{
    uint8_t j;
    for (j = 0; j < 4; ++j)
        if (s_on[j] && s_y[j] < y + h && y < s_y[j] + (s_big[j] ? 32 : 16))
            return 1;
    return 0;
}

/* tile (c, r) is about to be drawn: bombers on it come off first */
static void pre_tile(uint8_t c, uint8_t r)
{
    if (under(AR_BX + 8 * c, AR_Y + 16 * r, 8, 16))
        lift_all();
}

void gfx_tile(uint8_t c, uint8_t r, uint8_t tile)
{
    if (tile >= N_TILES)
        tile = T_FLOOR;
    tile_at[r * ARENA_W + c] = tile;
    pre_tile(c, r);
    draw_tile(c, r, tile);
    DONE();
}

void gfx_bomb_at(uint8_t hx, uint8_t hy, uint8_t dud)
{
    if (under(AR_BX + 4 * hx, AR_Y + 8 * hy, 8, 16))
        lift_all();
    put_frame(tcur[dud ? T_DUD : T_BOMB], AR_BX + 4 * hx, AR_Y + 8 * hy);
    DONE();
}

/* Left panel: powerup i's icon, its count beside it. */
void gfx_stat(uint8_t i, uint8_t n)
{
    static const uint8_t blank[128];
    uint8_t y = 24 + i * 32;
    if (i >= 3 && !n) {
        put_frame(blank, 4, y);
        DONE();
        return;
    }
    put_tile(4, y, tile_anim[T_FIRE + i][2]);
    if (i < 3) {
        text_char(16, y + 4, n >= 10 ? '0' + n / 10 : ' ', C_WHITE);
        text_char(20, y + 4, '0' + n % 10, C_WHITE);
    }
    DONE();
}

void gfx_round(uint8_t round)
{
    gfx_text(4, 35, "RND", 0);
    text_char(4 * 36, 32, round >= 10 ? '0' + (round / 10) % 10 : ' ', C_TAN);
    text_char(4 * 37, 32, '0' + round % 10, C_TAN);
    DONE();
}

/* ---- sprites -------------------------------------------------------------------- */

/* the Atari's pixels and lines -> ours (clocks are 2 pixels, one row less) */
void spr_at(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat, uint8_t big)
{
    if (y < 8 || x > (big ? 140 : 148) || y - 8 > (big ? 160 : 176)) {
        w_on[n] = 0;
        return;
    }
    w_on[n] = 1;
    w_x[n] = 2 * x;
    w_y[n] = y - 8;
    w_fr[n] = frame;
    w_seat[n] = seat;
    w_big[n] = big;
}

void spr_hide(uint8_t n)
{
    w_on[n] = 0;
}

void spr_hide_all(void)
{
    w_on[0] = w_on[1] = w_on[2] = w_on[3] = 0;
}

/* a bomber at pixel x (even), line y: in two parts on a block's edge */
static void draw_spr(uint8_t n)
{
    uint8_t x = w_x[n] >> 1, y = w_y[n], k = 16, n1 = 32 - (y & 31);
    sd_src = spr_data[w_fr[n]];
    sd_tab = SEAT(w_seat[n]);
    MMU7 = BLK_SPR;
    if (n1 < k) {
        sd_dst = line(y) + x;
        sd_n = n1;
        spr_draw3();
        sd_src += n1 * 16;
        y += n1;
        k -= n1;
    }
    sd_dst = line(y) + x;
    sd_n = k;
    spr_draw3();
}

/* a bomber twice the size (the winner on the results screen), pixel by pixel */
static void draw_big(uint8_t n)
{
    const uint8_t *d;
    uint8_t x, y, role, c, X, Y, k;
    uint8_t *p;
    MMU7 = BLK_SPR;
    d = spr_data[w_fr[n]];
    for (y = 0; y < 16; ++y)
        for (x = 0; x < 16; ++x) {
            if (((d[y * 16 + 8 + (x >> 1)] >> (x & 1 ? 0 : 4)) & 15))
                continue;
            role = (d[y * 16 + (x >> 1)] >> (x & 1 ? 0 : 4)) & 3;
            c = role ? seat_roles[w_seat[n]][role - 1] : 0;
            for (k = 0; k < 4; ++k) {
                X = w_x[n] + 2 * x + (k & 1);
                Y = w_y[n] + 2 * y + (k >> 1);
                p = line(Y) + (X >> 1);
                *p = (X & 1) ? (*p & 0xF0) | c : (*p & 0x0F) | (c << 4);
            }
        }
}

/* close enough to touch? (a's and b's pixels, 16 and 16 lines) */
static uint8_t closer(uint8_t ax, uint8_t ay, uint8_t bx, uint8_t by)
{
    return (ax > bx ? ax - bx : bx - ax) < 32 && (ay > by ? ay - by : by - ay) < 32;
}

/* a new animation phase (every 16 jiffies): the bombs and duds again */
static void animate(void)
{
    uint8_t p = (uint8_t)(frame_count >> 4) & 3, c, r, t;
    const uint8_t *m = tile_at;
    if (p == phase)
        return;
    MMU7 = BLK_TILE;
    set_phase(p);
    for (r = 0; r < ARENA_H; ++r)
        for (c = 0; c < ARENA_W; ++c) {
            t = *m++;
            if (t == T_BOMB || t == T_DUD) {
                pre_tile(c, r);
                draw_tile(c, r, t);
            }
        }
}

uint16_t renders;               /* passes that drew (the benchmark's count) */

void spr_flush(void)
{
    uint8_t n, j, ch = 0, again;
    uint8_t in[4];
    ++renders;
    if (in_arena)
        animate();
    for (n = 0; n < 4; ++n) {
        in[n] = w_on[n] != s_on[n] || (w_on[n] && (w_x[n] != s_x[n] || w_y[n] != s_y[n] || w_fr[n] != s_fr[n] ||
                                                  w_seat[n] != s_seat[n] || w_big[n] != s_big[n]));
        ch |= in[n];
    }
    if (!ch) {
        DONE();
        return;
    }
    do {                                /* those that touch a changed one change with it */
        again = 0;
        for (n = 0; n < 4; ++n)
            if (in[n])
                for (j = 0; j < 4; ++j)
                    if (!in[j] && s_on[j] &&
                        ((s_on[n] && closer(s_x[n], s_y[n], s_x[j], s_y[j])) ||
                         (w_on[n] && closer(w_x[n], w_y[n], s_x[j], s_y[j])))) {
                        in[j] = 1;
                        again = 1;
                    }
    } while (again);
    for (n = 4; n--; )                  /* off, last drawn first */
        if (in[n] && s_on[n])
            restore_spr(n);
    for (n = 0; n < 4; ++n) {
        if (!in[n])
            continue;
        if (w_on[n]) {
            if (w_big[n]) {
                draw_big(n);
            } else {
                spr_io(n, w_x[n] >> 1, w_y[n], 1);
                draw_spr(n);
            }
        }
        s_on[n] = w_on[n];
        s_x[n] = w_x[n];
        s_y[n] = w_y[n];
        s_fr[n] = w_fr[n];
        s_seat[n] = w_seat[n];
        s_big[n] = w_big[n];
    }
    DONE();
}

/* ---- colours ------------------------------------------------------------------- */

/* CLEAR: the other monitor's palette */
void gfx_swap_colours(void)
{
    gfx3_cmp ^= 1;
    set_pal();
}

void gfx_bar(void)
{
}
#endif

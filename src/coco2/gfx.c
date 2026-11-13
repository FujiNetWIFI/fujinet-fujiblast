/*
 * gfx.c -- the CoCo 1/2's screens (gfx.h) in PMODE 4 artifact colour.
 *
 * 256 dots by 192 lines, one bit a dot; two dots make a colour pixel, so the
 * screen is 128 colour pixels across (4 a byte, 2 bits each: 0 black, 1 and 2
 * blue and orange, whichever way round this machine's phase has them, 3
 * white) by 192 lines at $0E00, 32 bytes a line. That is the Atari's cell
 * (4 colour clocks by 8 lines) a byte on 8 lines: the Atari's art is native,
 * a tile 2 bytes by 16 lines. The machine has 32 columns where the Atari
 * has 40, so a text column c is the byte c - 2 (the Atari's first and last
 * 2 are cut), and a BIG character (2 bytes, 8 pixels) i is the byte
 * 2 (i - 1): 16 of the 20 show. The HUD, whose 20 characters have to
 * show, is drawn with the 5-pixel font, 6 pixels apart.
 *
 *   rows     row r (menus 1-24) is lines 8 (r - 1) to 8 r - 1; row 0, the
 *            arena's HUD, is lines 0-7
 *   arena    HUD, a 4-line wall, 13x11 tiles (bytes 5-30, lines 12-187), a wall
 *            4 lines, a byte beside it; the powerups' counts in bytes 0-3
 *
 * Text rows use the 3-pixel font (a byte a character), BIG rows the Namco
 * font a pixel a dot-pair (2 bytes a character), in the colours the row's
 * palette gives its four.
 *
 * The screen is drawn on as it shows: a tile is blitted, a bomber is
 * composited (src/coco/gfxa.asm) over the tiles under it. Where a bomber
 * was or is, the tiles are blitted again first (so nothing has to be
 * saved), and then every bomber over a tile that was drawn again.
 */
#ifndef COCO3
#include <string.h>
#include "fbcoco.h"
#include "gfx.h"
#include "gfxcodes.h"
#include "plat.h"
#include "proto.h"

#define SCREEN ((uint8_t *)0x0E00)

#define AR_BYTE 5               /* the arena: its first byte, */
#define AR_LINE 12              /* its first line */
#define AR_PX   (AR_BYTE * 4)   /* and in pixels */

#define STRIPES 4               /* a colour code: orange on even lines, white on odd */

/* what a BIG row's colours 0-3 are, by palette */
static const uint8_t pal_map[][4] = {
    { 3, 1, 2, 3 },             /* MENU */
    { 3, 1, 2, 3 },             /* GFX */
    { 3, 2, 1, STRIPES },       /* HUD: the seats */
    { 3, 1, 2, 3 },             /* ARENA */
    { 3, 3, 3, 3 },             /* SEAT0..3 */
    { 2, 2, 2, 2 },
    { 1, 1, 1, 1 },
    { STRIPES, STRIPES, STRIPES, STRIPES },
    { 3, 1, 2, 3 },             /* TITLE */
};
/* the bombers: body, trim, visor of each seat (the eyes are black); the
 * fourth's body is two tables, orange then white */
static const uint8_t seat_roles[SEATS][3] = { { 3, 2, 1 }, { 2, 3, 1 }, { 1, 3, 3 }, { 0, 3, 3 } };

static uint8_t swapped;                 /* blue and orange the other way round */
static uint8_t cpat[4];                 /* a colour as a byte of pixels */
/* role byte -> colours: seats 0-2, the 4th's even, odd, even. (In the high core's constants, which
 * are RAM: the low core's bss has no room.) */
static const uint8_t seat_ro[6 * 256] = { 0 };
#define SEAT(t) ((uint8_t *)seat_ro + ((uint16_t)(t) << 8))
static uint16_t laddr[192];             /* a line's offset in the screen */
static uint8_t nx[16];                  /* 4 dot-pairs of mask -> 8 dots */
static uint8_t row_kind[SCR_ROWS], row_pal[SCR_ROWS];

/* gfxa.asm's arguments */
const uint8_t *tp_src;
uint8_t *tp_dst;
const uint8_t *sd_src;
uint8_t *sd_dst;
uint8_t *sd_tab;
uint8_t *sd_buf;
void tile_put(void);
void spr_save(void);
void spr_restore(void);
void spr_draw(void);
void spr_draw_s(void);

static uint8_t col(uint8_t c)           /* a colour as this machine shows it */
{
    return swapped && (c == 1 || c == 2) ? 3 - c : c;
}

static void build_tables(void)
{
    uint8_t t, j, c, role;
    uint16_t rb;
    uint8_t out;
    for (t = 0; t < 6; ++t)
        for (rb = 0; rb < 256; ++rb) {
            out = 0;
            for (j = 0; j < 4; ++j) {
                role = (rb >> (6 - 2 * j)) & 3;
                if (t < 3) {
                    c = role ? seat_roles[t][role - 1] : 0;
                } else {
                    c = role == 1 ? (t == 4 ? 3 : 2) : role ? 3 : 0;
                }
                out |= col(c) << (6 - 2 * j);
            }
            SEAT(t)[rb] = out;
        }
    cpat[0] = 0;
    cpat[1] = col(1) == 1 ? 0x55 : 0xAA;
    cpat[2] = col(2) == 2 ? 0xAA : 0x55;
    cpat[3] = 0xFF;
}

/* ---- the mode ---------------------------------------------------------- */

static void video_gfx(void)
{
    asm {
        sta     $FFC0           ; SAM V2..V0 = 110: 6K graphics
        sta     $FFC3
        sta     $FFC5
        sta     $FFC7           ; display at $0E00: F0-F2 set, F3-F6 clear
        sta     $FFC9
        sta     $FFCB
        sta     $FFCC
        sta     $FFCE
        sta     $FFD0
        sta     $FFD2
        lda     $FF22           ; VDG: RG6 (PMODE 4), CSS 1 (black/white)
        anda    #$07
        ora     #$F8
        sta     $FF22
    }
}

/* off: the text screen, filled with black blocks */
static void video_off(void)
{
    memset((uint8_t *)0x0400, 0x80, 512);
    asm {
        sta     $FFC0           ; V = 000: text
        sta     $FFC2
        sta     $FFC4
        sta     $FFC6           ; display at $0400: F1 set
        sta     $FFC9
        sta     $FFCA
        sta     $FFCC
        sta     $FFCE
        sta     $FFD0
        sta     $FFD2
        lda     $FF22
        anda    #$07
        sta     $FF22
    }
}

/* ---- sprites (declared early: the screens hide them) ----
 * What the game wants (w_*, set by gfx.h's spr_arena) and what is on the
 * screen (s_*): x in pixels, y in lines of the screen; info: seat and frame. */
uint8_t w_on[4], w_x[4], w_y[4], w_fr[4], w_seat[4], w_big[4];
static uint8_t s_on[4], s_x[4], s_y[4], s_fr[4], s_seat[4], s_big[4];
static uint8_t in_arena;
static uint8_t tile_at[TILES];
static uint8_t phase;
static const uint8_t *tcur[N_TILES];    /* a tile's frame now */
static const uint8_t *sprptr[N_SPR_FRAMES * 4];
static uint16_t trow[ARENA_H];          /* the arena's tile rows: line offsets in the screen */

static void set_phase(uint8_t p)
{
    uint8_t t;
    phase = p;
    if (!tcur[T_FLOOR + 1]) {           /* the first time: every tile */
        for (t = 0; t < N_TILES; ++t)
            tcur[t] = (const uint8_t *)tile_gfx +
                      (uint16_t)tile_anim[t][t >= T_FIRE && t <= T_GLOVE ? 2 : 0] * 32;
    }
    tcur[T_BOMB] = (const uint8_t *)tile_gfx + (uint16_t)tile_anim[T_BOMB][p] * 32;
    tcur[T_DUD] = (const uint8_t *)tile_gfx + (uint16_t)tile_anim[T_DUD][p] * 32;
}

static uint8_t under(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
static void lift_all(void);

void gfx_init(void)
{
    uint16_t i;
    uint8_t n;
    for (i = 0; i < 192; ++i)
        laddr[i] = i * 32;
    for (n = 0; n < 16; ++n)
        nx[n] = (n & 8 ? 0xC0 : 0) | (n & 4 ? 0x30 : 0) | (n & 2 ? 0x0C : 0) | (n & 1 ? 0x03 : 0);
    build_tables();
    for (n = 0; n < N_SPR_FRAMES * 4; ++n)
        sprptr[n] = (const uint8_t *)spr_data + (uint16_t)n * 96;
    for (n = 0; n < ARENA_H; ++n)
        trow[n] = laddr[AR_LINE + 16 * n] + AR_BYTE;
    set_phase(0);
    memset(SCREEN, 0, 6144);
    video_gfx();
}

void gfx_clear(void)
{
    memset(SCREEN, 0, 6144);
}

void gfx_begin(void)
{
    uint8_t r;
    video_off();
    memset(SCREEN, 0, 6144);
    for (r = 0; r < SCR_ROWS; ++r) {
        row_kind[r] = ROW_TEXT;
        row_pal[r] = PAL_MENU;
    }
    in_arena = 0;
    snd_slice = 9000;           /* the menus don't */
    spr_hide_all();
    memset(s_on, 0, sizeof(s_on));
}

void gfx_show(void)
{
    video_gfx();
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
    uint8_t l, y = row_line(row);
    if (under(0, y, 128, 8))
        lift_all();
    for (l = 0; l < 8; ++l)
        memset(SCREEN + laddr[y + l], 0, 32);
}

/* ---- text ---------------------------------------------------------------- */

static uint8_t cbyte(uint8_t c, uint8_t line)       /* a colour code's pixels, on this line */
{
    return cpat[c == STRIPES ? ((line & 1) ? 3 : 2) : c];
}

/* a BIG character (Namco font, 2 bytes) at byte x, line y */
static void big_char(uint8_t x, uint8_t y, char ch, uint8_t c)
{
    const uint8_t *g;
    uint8_t l, p;
    uint8_t *d;
    if (ch < 32 || ch > 127)
        ch = 32;
    g = font_gfx + (uint16_t)(ch - 32) * 8;
    for (l = 0; l < 8; ++l) {
        p = cbyte(c, l);
        d = SCREEN + laddr[y + l] + x;
        d[0] = nx[g[l] >> 4] & p;
        d[1] = nx[g[l] & 15] & p;
    }
}

/* a text character (3 pixels and a gap, a byte) at byte x, line y */
static void text_char(uint8_t x, uint8_t y, char ch, uint8_t c)
{
    const uint8_t *g;
    uint8_t l, p = cpat[c];
    if (ch >= 96)
        ch -= 32;
    if (ch < 32)
        ch = 32;
    g = font3_gfx[ch - 32];
    for (l = 0; l < 8; ++l)
        SCREEN[laddr[y + l] + x] = g[l] & p;
}

/* the HUD: 20 characters, 6 pixels apart from pixel 4 */
static char hud_ch[20];
static uint8_t hud_co[20];

static void hud_char(uint8_t i, char ch, uint8_t c)
{
    const uint8_t *g;
    uint8_t l, x, p, sh, v, m;
    uint8_t *d;
    if (ch >= 96)
        ch -= 32;
    if (ch < 32)
        ch = 32;
    g = font5_gfx[ch - 32];
    for (l = 0; l < 8; ++l) {
        m = g[l];
        v = c == STRIPES ? ((l & 1) ? 3 : 2) : c;
        v = col(v);
        for (x = 0; x < 6; ++x) {
            p = 4 + 6 * i + x;
            sh = 6 - ((p & 3) << 1);
            d = SCREEN + laddr[l] + (p >> 2);
            *d = (*d & ~(3 << sh)) | ((m & (0x20 >> x) ? v : 0) << sh);
        }
    }
}

void gfx_big_line(uint8_t row, const char *s, const uint8_t *colour)
{
    uint8_t i, y = row_line(row), c;
    if (under(0, y, 128, 8))
        lift_all();
    if (row == 0 && row_pal[0] == PAL_HUD) {
        for (i = 0; i < 20; ++i) {
            c = pal_map[PAL_HUD][colour[i]];
            if (hud_ch[i] != s[i] || hud_co[i] != c) {
                hud_ch[i] = s[i];
                hud_co[i] = c;
                hud_char(i, s[i], c);
            }
        }
        return;
    }
    for (i = 1; i < 17; ++i)
        big_char(2 * (i - 1), y, s[i], pal_map[row_pal[row]][colour[i]]);
}

void gfx_text(uint8_t row, uint8_t col_, const char *s, uint8_t colour)
{
    uint8_t y = row_line(row);
    if (under(0, y, 128, 8))
        lift_all();
    if (row_kind[row] == ROW_BIG) {
        uint8_t c = pal_map[row_pal[row]][colour];
        for (; *s && col_ < 17; ++col_, ++s)
            if (col_ >= 1)
                big_char(2 * (col_ - 1), y, *s, c);
        return;
    }
    colour = colour ? 2 : 3;
    for (; *s && col_ < 34; ++col_, ++s)
        if (col_ >= 2)
            text_char(col_ - 2, y, *s, colour);
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
        memcpy(SCREEN + laddr[y + l] + 2, logo_gfx[l], LOGO_BYTES);
}

static void put_tile(uint8_t x, uint8_t y, uint8_t frame)
{
    tp_src = (const uint8_t *)tile_gfx + (uint16_t)frame * 32;
    tp_dst = SCREEN + laddr[y] + x;
    tile_put();
}

void gfx_trophies(uint8_t row, uint8_t col_, uint8_t n)
{
    uint8_t x = col_ - 2, y = row_line(row);
    for (; n && x < 31; --n, x += 2)
        put_tile(x, y, tile_anim[T_TROPHY][0]);
}

/* ---- the arena --------------------------------------------------------------- */

static void wall_bytes(uint8_t x, uint8_t y0, uint8_t lines)
{
    uint8_t l;
    for (l = 0; l < lines; ++l)
        SCREEN[laddr[y0 + l] + x] = cell_gfx[CELL_WALL][(y0 + l) & 7];
}

void gfx_arena(void)
{
    uint8_t x, y;
    gfx_row(0, ROW_BIG, PAL_HUD);
    memset(hud_ch, 0, sizeof(hud_ch));
    memset(hud_co, 0xFF, sizeof(hud_co));
    for (x = 4; x < 32; ++x) {
        wall_bytes(x, 8, 4);
        wall_bytes(x, 188, 4);
    }
    for (y = AR_LINE; y < AR_LINE + 176; y += 4) {
        wall_bytes(4, y, 4);
        wall_bytes(31, y, 4);
    }
    memset(tile_at, T_FLOOR, sizeof(tile_at));
    spr_hide_all();
    memset(s_on, 0, sizeof(s_on));
    in_arena = 1;
    snd_slice = 2400;           /* the game needs the CPU */
}

static void draw_tile(uint8_t c, uint8_t r, uint8_t tile)
{
    tp_src = tcur[tile];
    tp_dst = SCREEN + trow[r] + 2 * c;
    tile_put();
}

/* The bombers' own: what each covers is kept (sv_ro, in the high core's
 * constants, which are RAM) and put back before it moves, so a move costs
 * a copy and a draw, not the tiles under it blitted again. */
static const uint8_t sv_ro[4 * 48] = { 0 };
#define SV(n) ((uint8_t *)sv_ro + ((uint16_t)(n) << 5) + ((uint16_t)(n) << 4))

static void restore_spr(uint8_t n)
{
    if (s_big[n]) {                     /* (twice the size, on black: filled) */
        uint8_t l, b0 = s_x[n] >> 2, b1 = (s_x[n] + 15) >> 2;
        for (l = 0; l < 32; ++l)
            memset(SCREEN + laddr[s_y[n] + l] + b0, 0, b1 - b0 + 1);
    } else {
        sd_dst = SCREEN + laddr[s_y[n]] + (s_x[n] >> 2);
        sd_buf = SV(n);
        spr_restore();
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

/* pixels x..x+w-1, lines y..y+h-1 under a bomber on the screen? */
static uint8_t under(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    uint8_t j;
    for (j = 0; j < 4; ++j)
        if (s_on[j] && (s_x[j] < x + w) && (x < s_x[j] + (s_big[j] ? 16 : 8)) &&
            (s_y[j] < y + h) && (y < s_y[j] + (s_big[j] ? 32 : 16)))
            return 1;
    return 0;
}

/* tile (c, r) is about to be drawn: bombers on it come off first */
static void pre_tile(uint8_t c, uint8_t r)
{
    if (under(AR_PX + 8 * c, AR_LINE + 16 * r, 8, 16))
        lift_all();
}

void gfx_tile(uint8_t c, uint8_t r, uint8_t tile)
{
    if (tile >= N_TILES)
        tile = T_FLOOR;
    tile_at[r * ARENA_W + c] = tile;
    pre_tile(c, r);
    draw_tile(c, r, tile);
}

void gfx_bomb_at(uint8_t hx, uint8_t hy, uint8_t dud)
{
    if (under(AR_PX + 4 * hx, AR_LINE + 8 * hy, 8, 16))
        lift_all();
    tp_src = tcur[dud ? T_DUD : T_BOMB];
    tp_dst = SCREEN + laddr[AR_LINE + 8 * hy] + AR_BYTE + hx;
    tile_put();
}

/* Left panel: powerup i's icon, its count beside it. */
void gfx_stat(uint8_t i, uint8_t n)
{
    static const uint8_t blank[32];
    uint8_t y = 14 + i * 24;
    if (i >= 3 && !n) {
        tp_src = blank;
        tp_dst = SCREEN + laddr[y];
        tile_put();
        return;
    }
    put_tile(0, y, tile_anim[T_FIRE + i][2]);
    if (i < 3) {
        text_char(2, y + 4, n >= 10 ? '0' + n / 10 : ' ', 3);
        text_char(3, y + 4, '0' + n % 10, 3);
    }
}

void gfx_round(uint8_t round)
{
    text_char(0, 142, 'R', 3);
    text_char(1, 142, 'N', 3);
    text_char(2, 142, 'D', 3);
    text_char(1, 152, round >= 10 ? '0' + (round / 10) % 10 : ' ', 2);
    text_char(2, 152, '0' + round % 10, 2);
}

/* ---- sprites -------------------------------------------------------------------- */

/* the Atari's pixels and lines -> ours (2 columns, one row less) */
void spr_at(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat, uint8_t big)
{
    if (x < 8 || y < 8 || y - 8 > (big ? 160 : 176)) {
        w_on[n] = 0;
        return;
    }
    x -= 8;
    if (x > (big ? 104 : 116)) {        /* (the Atari's screen is wider: the winner's place is the far edge) */
        if (!big) {
            w_on[n] = 0;
            return;
        }
        x = 104;
    }
    w_on[n] = 1;
    w_x[n] = x;
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

static void draw_spr(uint8_t n)
{
    uint8_t x = w_x[n], y = w_y[n];
    sd_src = sprptr[w_fr[n] * 4 + (x & 3)];
    sd_dst = SCREEN + laddr[y] + (x >> 2);
    if (w_seat[n] == 3) {
        sd_tab = SEAT((y & 1) ? 4 : 3);
        spr_draw_s();
    } else {
        sd_tab = SEAT(w_seat[n]);
        spr_draw();
    }
}

/* a bomber twice the size (the winner on the results screen), pixel by pixel */
static void draw_big(uint8_t n)
{
    const uint8_t *d = sprptr[w_fr[n] * 4];
    uint8_t x, y, k, role, c, X, Y, sh;
    uint8_t *p;
    for (y = 0; y < 16; ++y)
        for (x = 0; x < 8; ++x) {
            if ((d[y * 6 + 3 + (x >> 2)] >> (6 - 2 * (x & 3))) & 3)
                continue;
            role = (d[y * 6 + (x >> 2)] >> (6 - 2 * (x & 3))) & 3;
            c = col(role ? seat_roles[w_seat[n]][role - 1] : 0);
            for (k = 0; k < 4; ++k) {
                X = w_x[n] + 2 * x + (k & 1);
                Y = w_y[n] + 2 * y + (k >> 1);
                sh = 6 - ((X & 3) << 1);
                p = SCREEN + laddr[Y] + (X >> 2);
                *p = (*p & ~(3 << sh)) | (c << sh);
            }
        }
}

/* bombers a and b (a's at its place on the screen or, a_new, where it is wanted) close enough to touch? */
static uint8_t closer(uint8_t ax, uint8_t ay, uint8_t bx, uint8_t by)
{
    return (ax > bx ? ax - bx : bx - ax) < 16 && (ay > by ? ay - by : by - ay) < 32;
}

/* a new animation phase (every 16 jiffies): the bombs and duds again */
static void animate(void)
{
    uint8_t p = (uint8_t)(frame_count >> 4) & 3, c, r, t;
    const uint8_t *m = tile_at;
    if (p == phase)
        return;
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
    if (!ch)
        return;
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
                sd_dst = SCREEN + laddr[w_y[n]] + (w_x[n] >> 2);
                sd_buf = SV(n);
                spr_save();
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
}

/* ---- colours ------------------------------------------------------------------- */

static uint8_t swap_byte(uint8_t b)
{
    return ((b & 0x55) << 1) | ((b >> 1) & 0x55);
}

static void swap_bytes(uint8_t *p, uint16_t n)
{
    while (n--) {
        *p = swap_byte(*p);
        ++p;
    }
}

/* CLEAR: blue and orange change places everywhere: the screen, the art in
 * memory (it's RAM), the colour tables */
void gfx_swap_colours(void)
{
    swap_bytes(SCREEN, 6144);
    swap_bytes((uint8_t *)tile_gfx, sizeof(tile_gfx));
    swap_bytes((uint8_t *)logo_gfx, sizeof(logo_gfx));
    swap_bytes((uint8_t *)cell_gfx, sizeof(cell_gfx));
    swapped ^= 1;
    build_tables();
}

/* a bar in colour 1 for the start-up question */
void gfx_bar(void)
{
    uint8_t l;
    for (l = 72; l < 96; ++l)
        memset(SCREEN + laddr[l] + 10, cpat[1], 12);
}
#endif

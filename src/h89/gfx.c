/*
 * gfx.c -- FujiBlast's screen on the H89's H19 terminal (src/platform-
 * specific/gfx.h), drawn into h19.c's cells, which sends what changed.
 *
 * The arena: tiles of 5 x 2 cells (square on the H19, whose cells are
 * about 2.4 times taller than wide), 65 x 22 in a border on lines 0-23,
 * columns 0-66; our stats in columns 68-79; the names, wins and clock on
 * the 25th line. A bomber is 3 x 2 cells with its seat's digit, ours
 * reversed; its column is the tile's plus where in the tile (5 columns to
 * 16 units: sub5), its line one a half tile.
 *
 * The other screens: a text row's 40 columns are the H19's 20-59, a BIG
 * row's 20 characters every other column there; menu sprites at x/4, y/8.
 * Art: gfx/h89/art.txt (tools/mkgfx_h89.py makes gen/h89).
 */
#include <string.h>
#include "gfx.h"
#include "gfxcodes.h"
#include "game.h"
#include "h19.h"
#include "h89.h"

#define TXT0  20                /* a text row's column 0 */
#define AY0   1                 /* the arena's first line */
#define PANEL 68                /* the stats' column */
#define HUD   ROW25

uint8_t gfx_in_arena;
static uint8_t kind[SCR_ROWS];
static uint8_t fig[SEATS][N_FRAMES][2 + FIG_W * FIG_H];    /* { w, h, cells } a seat */
static uint8_t *fig_of[SEATS];          /* fig[seat][0] (8 bytes a frame) */
static uint8_t cbuf[SCR_W];                                 /* (a row being made) */

/* a tile's first column; a unit's column in its tile, rounded */
static const uint8_t tcol[ARENA_W] = { 1, 6, 11, 16, 21, 26, 31, 36, 41, 46, 51, 56, 61 };
static const uint8_t sub5[16] = { 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 5 };

void gfx_init(void)
{
    uint8_t s, f, k, v, *d;
    const uint8_t *a;
    for (s = 0; s < SEATS; ++s)
        for (fig_of[s] = fig[s][0], f = 0; f < N_FRAMES; ++f) {
            d = fig[s][f];
            d[0] = FIG_W;
            d[1] = FIG_H;
            a = frame_art + f * (FIG_W * FIG_H);
            for (k = 0; k < FIG_W * FIG_H; ++k) {
                v = a[k];
                d[2 + k] = v == SEAT_CELL ? '1' + s : v;
            }
        }
}

void gfx_begin(void)
{
    uint8_t y;
    spr_hide_all();
    for (y = 0; y < SCR_H; ++y)
        h19_fill(0, y, SCR_W, ' ');
    memset(kind, ROW_TEXT, sizeof(kind));
    gfx_in_arena = 0;
    input_stop();
    h19_cleared();
}

void gfx_row(uint8_t row, uint8_t k, uint8_t pal)
{
    (void)pal;
    if (row < SCR_ROWS)
        kind[row] = k;
}

void gfx_show(void)
{
}

void gfx_clear_row(uint8_t row)
{
    h19_fill(0, row, SCR_W, ' ');
}

/* A BIG row's characters every other column; reversed, the gaps too. */
static void big_text(uint8_t row, uint8_t col, const char *s, uint8_t attr)
{
    uint8_t n = 0, x = TXT0 + 2 * col;
    while (*s && x + n < SCR_W) {
        cbuf[n++] = h19_char((uint8_t)*s++) | attr;
        if (*s)
            cbuf[n++] = ' ' | attr;
    }
    h19_putn(x, row, cbuf, n);
}

/* The arena's HUD on the 25th line: five places of 16 columns, the seats
 * (" 1 ANN 2 ", ours reversed) round the clock. The neutral line has a
 * seat's four characters at 0, 4, 12 and 16, the clock at 8. */
static void hud_line(const char *b)
{
    static const uint8_t from[5] = { 0, 4, 8, 12, 16 };
    static const uint8_t seat_of[5] = { 0, 1, NONE, 2, 3 };
    uint8_t k, i, s, x, attr;
    const char *g;
    memset(cbuf, ' ', SCR_W);
    for (k = 0; k < 5; ++k) {
        g = b + from[k];
        x = k * 16;
        s = seat_of[k];
        if (s == NONE) {
            for (i = 0; i < 4; ++i)
                cbuf[x + 6 + i] = h19_char((uint8_t)g[i]);
            continue;
        }
        if (g[0] == ' ' && g[1] == ' ' && g[2] == ' ' && g[3] == ' ')
            continue;                   /* (not playing) */
        attr = s == my_seat ? REV : 0;
        cbuf[x + 1] = ' ' | attr;
        cbuf[x + 2] = ('1' + s) | attr;
        cbuf[x + 3] = ' ' | attr;
        for (i = 0; i < 3; ++i)
            cbuf[x + 4 + i] = h19_char((uint8_t)g[i]) | attr;
        cbuf[x + 7] = ' ' | attr;
        cbuf[x + 8] = h19_char((uint8_t)g[3]) | attr;
        cbuf[x + 9] = ' ' | attr;
    }
    h19_putn(0, HUD, cbuf, SCR_W);
}

void gfx_big_line(uint8_t row, const char *s, const uint8_t *colour)
{
    uint8_t i, n = 0;
    if (gfx_in_arena && row == 0) {
        hud_line(s);
        return;
    }
    for (i = 0; i < 20 && s[i]; ++i) {
        cbuf[n++] = h19_char((uint8_t)s[i]) | (colour[i] ? REV : 0);
        cbuf[n++] = ' ';
    }
    h19_putn(TXT0, row, cbuf, n);
}

void gfx_text(uint8_t row, uint8_t col, const char *s, uint8_t colour)
{
    uint8_t attr = colour ? REV : 0;
    if (row >= SCR_ROWS)
        return;
    if (kind[row] == ROW_BIG)
        big_text(row, col, s, attr);
    else
        h19_str(TXT0 + col, row, s, attr);
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
    uint8_t y;
    for (y = 0; y < LOGO_H; ++y)
        h19_putn((SCR_W - LOGO_W) / 2, row + y, logo_art + y * LOGO_W, LOGO_W);
}

void gfx_trophies(uint8_t row, uint8_t col, uint8_t n)
{
    uint8_t x = TXT0 + col, y;
    for (; n && x + TROPHY_W <= SCR_W; --n, x += TROPHY_W + 1)
        for (y = 0; y < TROPHY_H; ++y)
            h19_putn(x, row + y, trophy_art + y * TROPHY_W, TROPHY_W);
}

/* ---- the arena ------------------------------------------------------- */

static void panel(uint8_t row, const char *s, uint8_t attr)
{
    h19_str(PANEL, row, s, attr);
}

void gfx_arena(void)
{
    uint8_t y;
    memset(cbuf, G_HLINE, 67);
    cbuf[0] = G_UL;
    cbuf[66] = G_UR;
    h19_putn(0, 0, cbuf, 67);
    cbuf[0] = G_LL;
    cbuf[66] = G_LR;
    h19_putn(0, 23, cbuf, 67);
    for (y = 1; y < 23; ++y) {
        h19_put(0, y, G_VLINE);
        h19_put(66, y, G_VLINE);
    }
    panel(1, " FUJIBLAST  ", REV);
    panel(14, "ARROWS: WALK", 0);
    panel(15, "5:      STOP", 0);
    panel(16, "SPACE:  BOMB", 0);
    panel(18, "ESC:   LEAVE", 0);
    panel(19, "M:     SOUND", 0);
    gfx_in_arena = 1;
    input_stop();
}

void gfx_tile(uint8_t c, uint8_t r, uint8_t tile)
{
    const uint8_t *a = tile_art + (uint8_t)(tile * (TILE_W * TILE_H));
    uint8_t x = tcol[c], y = AY0 + (r << 1);
    h19_putn(x, y, a, TILE_W);
    h19_putn(x, y + 1, a + TILE_W, TILE_W);
}

/* A bomb on its way (kicked or thrown), in half tiles: drawn into the
 * tiles, which play.c has redrawn once it has gone. */
void gfx_bomb_at(uint8_t hx, uint8_t hy, uint8_t dud)
{
    const uint8_t *a = tile_art + (dud ? T_DUD : T_BOMB) * (TILE_W * TILE_H);
    uint8_t x = 1 + ((uint8_t)(hx * 5) >> 1), y = AY0 + hy;
    h19_putn(x, y, a, TILE_W);
    h19_putn(x, y + 1, a + TILE_W, TILE_W);
}

void gfx_stat(uint8_t i, uint8_t n)
{
    static const char *const name[5] = { "FIRE", "BOMBS", "SPEED", "KICK", "GLOVE" };
    uint8_t k;
    memset(cbuf, ' ', 12);
    if (i < 3 || n) {
        for (k = 0; name[i][k]; ++k)
            cbuf[k] = name[i][k];
        if (i < 3) {
            if (n >= 10)
                cbuf[10] = '0' + n / 10;
            cbuf[11] = '0' + n % 10;
        }
    }
    h19_putn(PANEL, 4 + i, cbuf, 12);
}

void gfx_round(uint8_t round)
{
    char b[13];
    if (st[OFF_SUDDEN]) {
        strcpy(b, "SUDDEN DEATH");
    } else {
        strcpy(b, "ROUND 1 OF 1");
        b[6] = '0' + round % 10;
        b[11] = '0' + st[OFF_ROUNDS] % 10;
    }
    panel(2, b, 0);
}

/* ---- sprites ------------------------------------------------------------ */
/* spr_arena is gfx.h's macro: it sets the wants (w_on 1, arena units);
 * spr_at sets them too (w_on 2, the screen's units); gfx_sync lays them
 * out, so the logic's passes that don't draw cost nothing here. */
uint8_t w_on[SEATS], w_x[SEATS], w_y[SEATS], w_fr[SEATS], w_seat[SEATS];

void spr_at(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat, uint8_t big)
{
    (void)big;
    w_on[n] = 2;
    w_x[n] = x;
    w_y[n] = y;
    w_fr[n] = frame;
    w_seat[n] = seat;
}

void spr_hide(uint8_t n)
{
    w_on[n] = 0;
}

void spr_hide_all(void)
{
    memset(w_on, 0, sizeof(w_on));
}

/* (frame_wait sends what changed: gfx_sync) */
void spr_flush(void)
{
}

/* What changed, out to the terminal: the sprites laid out, our bomber's
 * rows first. */
void gfx_sync(void)
{
    static uint8_t n, x, on, seat;
    h19_first = my_seat < SEATS ? my_seat : NONE;
    for (n = 0; n < SPR_SLOTS; ++n) {
        if (!(on = w_on[n])) {
            s_art[n] = 0;
            continue;
        }
        seat = w_seat[n];
        s_art[n] = fig_of[seat] + (w_fr[n] << 3);
        x = w_x[n];
        if (on == 1) {
            s_c[n] = tcol[x >> 4] + sub5[x & 15] + 1;
            s_r[n] = AY0 + ((uint8_t)(w_y[n] + 4) >> 3);
        } else {
            s_c[n] = TXT0 + (x >> 2);
            s_r[n] = w_y[n] >> 3;
        }
        s_rev[n] = seat == my_seat ? REV : 0;
    }
    h19_flush();
}

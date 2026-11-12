/*
 * gfx.c -- the 7800's screens (platform-specific/gfx.h) on MARIA: 25 zones
 * of 8 lines, each a row of the neutral screen (SCREEN, 40 bytes a row),
 * its display list composed here into a back slot and shown by the frame
 * NMI (nmi.s). See DESIGN.md.
 *
 * Row kinds: TEXT (320A, the Namco font, a char a byte), BIG (160A, twice
 * the width, a colour a char), GFX and ARENA (160B cells: 12 colours a
 * pixel), HUD (the arena's top row: TEXT, a colour a char). The bombers are
 * two 160A objects each, from the pages in REPL (gen/atari7800/repl.bin).
 */
#ifdef __ATARI7800__

#include <string.h>
#include "gfx.h"
#include "plat.h"
#include "proto.h"
#include "gfxcodes.h"
#include "a78hw.h"
#include "bank.h"

#define AR_COL   7              /* the arena's first cell column */
#define AR_ROW   2              /* and row */

/* compose.s: a row's kind and neutral palette, the zones to compose, the HUD's
 * colour a char (seat 0-3, 4 white), the bombers shown */
extern uint8_t rk[ROWS], rp[ROWS], zdirty[ROWS], hud_pal[40];
extern uint8_t o_on[4], o_x[4], o_y[4], o_fr[4], o_seat[4];
void compose_dirty(void);

/* the bombers wanted (gfxsetup.c's gfx_show starts with them) */
uint8_t s_on[4], s_x[4], s_y[4], s_fr[4], s_seat[4];

static uint8_t *line(uint8_t row)
{
    return SCREEN + row * 40;
}

/* ---- building a screen --------------------------------------------------------- */

void gfx_row(uint8_t row, uint8_t kind, uint8_t p)
{
    rk[row] = kind;
    rp[row] = p;
    zdirty[row] = 1;
}

/* ---- text ------------------------------------------------------------------------ */

static uint8_t internal(char c)
{
    uint8_t a = (uint8_t)c;
    if (a >= 'a' && a <= 'z')
        a -= 32;
    if (a < 32 || a > 95)
        return 0;
    return a - 32;
}

void gfx_clear_row(uint8_t row)
{
    memset(line(row), 0, 40);
    zdirty[row] = 1;
}

void gfx_big_line(uint8_t row, const char *s, const uint8_t *colour)
{
    uint8_t i, *p = line(row);
    if (rk[row] == RK_HUD) {
        /* the 20 logical chars -> 40 columns: a name, a space and the wins,
         * the timer between */
        static const uint8_t from[5] = { 0, 4, 8, 12, 16 };
        static const uint8_t to[5] = { 0, 7, 18, 28, 35 };
        uint8_t g, k, t;
        memset(p, 0, 40);
        memset(hud_pal, 4, 40);
        for (g = 0; g < 5; ++g)
            for (k = 0; k < 4; ++k) {
                t = to[g] + k + (g != 2 && k == 3 ? 1 : 0);
                p[t] = internal(s[from[g] + k]);
                hud_pal[t] = g == 2 ? 4 : colour[from[g] + k];
            }
    } else {
        for (i = 0; i < 20; ++i) {
            p[i] = internal(s[i]) * 2;
            p[20 + i] = colour[i];
        }
    }
    zdirty[row] = 1;
}

void gfx_text(uint8_t row, uint8_t col, const char *s, uint8_t colour)
{
    uint8_t *p = line(row);
    if (rk[row] == RK_BIG) {
        while (*s && col < 20) {
            p[col] = internal(*s++) * 2;
            p[20 + col++] = colour;
        }
    } else {
        while (*s && col < 40)
            p[col++] = internal(*s++) | (colour ? 0x40 : 0);
    }
    zdirty[row] = 1;
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

/* ---- the arena ------------------------------------------------------------------- */

void gfx_tile(uint8_t c, uint8_t r, uint8_t tile)
{
    uint8_t *p = line(AR_ROW + r * 2) + AR_COL + c * 2;
    const uint8_t *t = tile_chars[tile];
    p[0] = t[0];
    p[1] = t[1];
    p[40] = t[2];
    p[41] = t[3];
    zdirty[AR_ROW + r * 2] = zdirty[AR_ROW + r * 2 + 1] = 1;
}

void gfx_bomb_at(uint8_t hx, uint8_t hy, uint8_t dud)
{
    uint8_t *p = line(AR_ROW + hy) + AR_COL + hx;
    const uint8_t *t = tile_chars[dud ? T_DUD : T_BOMB];
    p[0] = t[0];
    p[1] = t[1];
    p[40] = t[2];
    p[41] = t[3];
    zdirty[AR_ROW + hy] = zdirty[AR_ROW + hy + 1] = 1;
}

/* Left panel: powerup i's icon at rows 3+4i, its count beside it. */
void gfx_stat(uint8_t i, uint8_t n)
{
    uint8_t *p = line(3 + i * 4) + 1;
    const uint8_t *t = tile_chars[T_FIRE + i];
    zdirty[3 + i * 4] = zdirty[4 + i * 4] = 1;
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
        p[43] = n >= 10 ? CH_FONT_0 + 2 * (n / 10) : CH_PANEL;
        p[44] = CH_FONT_0 + 2 * (n % 10);
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
    p[0] = round >= 10 ? CH_FONT_0 + 2 * ((round / 10) % 10) : CH_PANEL;
    p[1] = CH_FONT_0 + 2 * (round % 10);
    zdirty[3] = zdirty[4] = 1;
}

/* ---- the bombers ----------------------------------------------------------------- */

void spr_at(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat, uint8_t big)
{
    (void)big;                  /* (the results' big bomber is drawn as the rest) */
    s_on[n] = 1;
    s_x[n] = x;
    s_y[n] = y;
    s_fr[n] = frame;
    s_seat[n] = seat & 3;
}

void spr_arena(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat)
{
    s_on[n] = 1;
    s_x[n] = AR_COL * 4 + (x >> 1);
    s_y[n] = AR_ROW * 8 + y;
    s_fr[n] = frame;
    s_seat[n] = seat & 3;
}

void spr_hide(uint8_t n)
{
    s_on[n] = 0;
}

void spr_hide_all(void)
{
    s_on[0] = s_on[1] = s_on[2] = s_on[3] = 0;
}

static void dirty_span(uint8_t y)
{
    uint8_t a = y >> 3, b = (uint8_t)(y + 15) >> 3;
    while (a <= b && a < ROWS)
        zdirty[a++] = 1;
}

/* what changed: its zones, old and new, composed */
void spr_flush(void)
{
    uint8_t n;
    for (n = 0; n < 4; ++n) {
        if (o_on[n] == s_on[n] && (!s_on[n] || (o_x[n] == s_x[n] && o_y[n] == s_y[n] &&
                                                o_fr[n] == s_fr[n] && o_seat[n] == s_seat[n])))
            continue;
        if (o_on[n])
            dirty_span(o_y[n]);
        if (s_on[n])
            dirty_span(s_y[n]);
        o_on[n] = s_on[n];
        o_x[n] = s_x[n];
        o_y[n] = s_y[n];
        o_fr[n] = s_fr[n];
        o_seat[n] = s_seat[n];
    }
    tables_check();
    compose_dirty();
}

#endif

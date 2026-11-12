/*
 * gfxsetup.c -- the 7800's screens (gfx.c): what happens when a screen is
 * built or changes, not every frame, so it lives in bank 0 with the other
 * phases (OVERLAY1, the Makefile): start-up, the DLL, the palettes, the
 * DLIs' tables, the character blocks, the screens that are made of rows.
 * Nothing here may switch the window's bank (bank.c's bank_copy, in the
 * fixed bank, does).
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
#define AR_ROW   2

extern const uint8_t art_txt[], art_big[], art_gfx[], art_a0[], art_a1[];   /* art.s */

/* compose.s: a row's kind and neutral palette, the zones to compose, the HUD's
 * colour a char (seat 0-3, 4 white), the bombers shown; gfx.c: the bombers wanted */
extern uint8_t rk[ROWS], rp[ROWS], zdirty[ROWS], hud_pal[40];
extern uint8_t o_on[4], o_x[4], o_y[4], o_fr[4], o_seat[4];
extern uint8_t s_on[4], s_x[4], s_y[4], s_fr[4], s_seat[4];
void compose_dirty(void);

static uint8_t is_pal;
static uint8_t region;                  /* what's in $4000-$5FFF: 0 nothing, 1 menus, 2 arena */

/* the tables the next screen shows (installed in a blank) */
static uint8_t nx_cb[NDLI], nx_ca[NDLI], nx_cb2[NDLI], nx_bg[NDLI], nx_f[NDLI], nx_dli[ROWS];
static uint8_t nx_n;
static uint8_t was160[ROWS];            /* zone z was a blank text row, shown as 160 mode */
static uint8_t null_dl[2];

/* the neutral PAL_* backgrounds */
static const uint8_t bgs[] = { 0x90, 0x90, 0x08, 0xC5, 0x90, 0x90, 0x90, 0x90, 0x90 };

/* a colour as this console shows it: its hue one on (greys and $F0 up as they are) */
static uint8_t hue(uint8_t c)
{
    return is_pal && c >= 0x10 && c < 0xF0 ? c + 0x10 : c;
}

/* A whole frame, for real (plat.c's frame_wait may return at once when the
 * game is behind): the DLI tables are installed right after the frame's
 * DLI, or the next DLIs would count from the wrong ordinal. */
static void wait_raw(void)
{
    uint16_t f = *(volatile uint16_t *)&frame_count;
    while (*(volatile uint16_t *)&frame_count == f)
        ;
}

static uint8_t *line(uint8_t row)
{
    return SCREEN + row * 40;
}

/* ---- start-up: NTSC or PAL, the DLL, the palettes ------------------------- */

static void tvdetect(void)
{
    uint16_t n = 0;
    while (!(MSTAT & 0x80))
        ;
    while (MSTAT & 0x80)
        ;
    while (!(MSTAT & 0x80)) {
        __asm__("sta $24");     /* WSYNC */
        ++n;
    }
    mt_ntsc = n < 267;
    is_pal = !mt_ntsc;
}

static void dll_entry(uint8_t i, uint8_t flags, const uint8_t *dl)
{
    mt_dll[i * 3] = flags;
    mt_dll[i * 3 + 1] = (uint16_t)dl >> 8;
    mt_dll[i * 3 + 2] = (uint8_t)(uint16_t)dl;
}

static void palettes(void)
{
    static const uint8_t reg[8] = { 0x21, 0x25, 0x29, 0x2D, 0x31, 0x35, 0x39, 0x3D };
    uint8_t p, c;
    volatile uint8_t *r;
    for (p = 0; p < 4; ++p) {
        r = (volatile uint8_t *)reg[p];
        for (c = 0; c < 3; ++c)
            r[c] = hue(pal_p0_3[p * 3 + c]);
    }
    for (p = 0; p < 4; ++p) {
        r = (volatile uint8_t *)reg[4 + p];
        for (c = 0; c < 3; ++c)
            r[c] = hue(seat_pal[p][c]);
    }
}

void gfx_init(void)
{
    uint8_t i, top;
    tvdetect();
    top = mt_ntsc ? 2 : 4;
    mt_top = top;
    memset(mt_dll, 0, 3 * (ROWS + 8));
    for (i = 0; i < top; ++i)
        dll_entry(i, i + 1 < top ? 0x0F : (mt_ntsc ? 0x08 : 0x01), null_dl);
    for (i = 0; i < ROWS; ++i) {
        dll_off[i] = (top + i) * 3;
        dll_entry(top + i, 0x07, null_dl);
    }
    dll_entry(top + ROWS, 0x8F, null_dl);       /* the frame's DLI */
    dll_entry(top + ROWS + 1, 0x0F, null_dl);
    dll_entry(top + ROWS + 2, 0x0F, null_dl);
    dll_entry(top + ROWS + 3, 0x0F, null_dl);
    palettes();
    {
        volatile uint8_t *dp = (volatile uint8_t *)0x2C;        /* DPPH, DPPL */
        dp[0] = (uint16_t)mt_dll >> 8;
        *(volatile uint8_t *)0x30 = (uint8_t)(uint16_t)mt_dll;
    }
    nx_n = 0;
    lv_f[0] = LF_FRAME;                 /* until the first screen: only the frame's */
    lv_cb[0] = CB_TXT;
    lv_ca[0] = lv_cb2[0] = CT_TEXT;
    lv_bg[0] = hue(0x90);
    mt_dll[(top + ROWS) * 3] = 0x8F;
    mt_start();
    gfx_begin();
}

/* ---- the character blocks in cart RAM -------------------------------------- */


static void need_region(uint8_t want)
{
    if (region == want)
        return;
    if (!region)
        bank_copy(TXTB, 5, art_txt, 2048);
    if (want == 1) {
        bank_copy(GFXB, 5, art_gfx, 2048);
        bank_copy(BIGB, 5, art_big, 2048);
    } else {
        bank_copy(A0B, 5, art_a0, 2048);
        bank_copy(A1B, 6, art_a1, 2048);
        bank_copy(A2B, 6, art_a1 + 2048, 2048);
        bank_copy(A3B, 6, art_a1 + 4096, 2048);
    }
    region = want;
}


void gfx_begin(void)
{
    uint8_t r;
    wait_raw();
    spr_hide_all();
    for (r = 0; r < ROWS; ++r) {        /* blank: every zone shows nothing */
        mt_dll[dll_off[r] + 1] = (uint16_t)null_dl >> 8;
        mt_dll[dll_off[r] + 2] = (uint8_t)(uint16_t)null_dl;
        zflip[r] = 0;
    }
    dsp_ready = 0;
    memset(SCREEN, 0, ROWS * 40);
    memset(hud_pal, 4, sizeof(hud_pal));
    for (r = 0; r < ROWS; ++r)
        gfx_row(r, ROW_TEXT, PAL_MENU);
}


static uint8_t blank_row(uint8_t z)
{
    const uint8_t *p = line(z);
    uint8_t i;
    for (i = 0; i < 40; ++i)
        if (p[i])
            return 0;
    return 1;
}

/* the screen's DLIs: a zone whose CHARBASE, CTRL or background isn't the
 * zone above's */
static void tables_build(void)
{
    uint8_t z, cb, ca, bg, f, pcb = 0, pca = CT_CELL, pbg = 0, pf = 0, ord = 0, k;
    for (z = 0; z < ROWS; ++z) {
        k = rk[z];
        was160[z] = 0;
        if (k == RK_TEXT && blank_row(z)) {
            was160[z] = 1;
            k = RK_GFX;
        }             /* empty text: 160 mode (a bomber there would show as 320's bits) */
        bg = hue(bgs[rp[z]]);
#ifdef DBGBG
        bg = ((z & 15) << 4) | 0x08 | (z & 1 ? 4 : 0);
#endif
        f = 0;
        switch (k) {
        case RK_TEXT:
        case RK_HUD:
            cb = CB_TXT;
            ca = CT_TEXT;
            break;
        case RK_BIG:
            cb = CB_BIG;
            ca = CT_CELL;
            break;
        case RK_GFX:
            cb = CB_GFX;
            ca = CT_CELL;
            break;
        default:
            cb = CB_A0;
            ca = CT_CELL;
            f = LF_ANIM;
        }
#ifdef ALLDLI
        nx_dli[z] = 1;
        if (0)
#endif
        nx_dli[z] = !z || cb != pcb || ca != pca || bg != pbg || f != (pf & LF_ANIM);
        if (nx_dli[z]) {
            nx_cb[ord] = cb;
            nx_ca[ord] = ca;
            nx_cb2[ord] = ca;
            nx_bg[ord] = bg;
            if ((ca & 3) == 3 && (pca & 3) == 0) {     /* 160 -> 320A: late (the frame's entry leaves the last zone's mode) */
                nx_ca[ord] = (ca & ~3) | (pca & 3);
                f |= LF_LATE;
            }
            nx_f[ord] = f;
            ++ord;
        }
        pcb = cb;
        pca = ca;
        pbg = bg;
        pf = f;
    }
    nx_cb[ord] = pcb;               /* the frame's entry: nothing changes */
    nx_ca[ord] = nx_cb2[ord] = pca;
    nx_bg[ord] = pbg;
#ifdef DBGORD
    { uint8_t q; for (q = 0; q <= ord; ++q) nx_bg[q] = 0x08 + 0x50 * q; }
#endif
    nx_f[ord] = LF_FRAME;
    nx_n = ord + 1;
}

static void tables_install(void)
{
    uint8_t i, top = mt_top;
    for (i = 0; i < nx_n; ++i) {
        lv_cb[i] = nx_cb[i];
        lv_ca[i] = nx_ca[i];
        lv_cb2[i] = nx_cb2[i];
        lv_bg[i] = nx_bg[i];
        lv_f[i] = nx_f[i];
    }
    for (i = 0; i < ROWS; ++i)
        mt_dll[dll_off[i]] = nx_dli[i] ? 0x87 : 0x07;
    mt_dll[(top + ROWS) * 3] = 0x8F;
    mt_load0();
}

/* A text row that was blank when the screen was shown (160 mode, for a bomber)
 * has had text written, or one with text was cleared: build the tables
 * again (a frame: the ordinals must stay in step), for the rows that changed. */
void tables_check(void)
{
    uint8_t z, stale = 0;
    for (z = 0; z < ROWS; ++z)
        if (zdirty[z] && rk[z] == RK_TEXT && was160[z] != blank_row(z))
            stale = 1;
    if (!stale)
        return;
    tables_build();
    wait_raw();
    tables_install();
}


void gfx_show(void)
{
    uint8_t z;
    if (rk[1] == RK_ARENA || rk[2] == RK_ARENA)
        need_region(2);
    else
        need_region(1);
    tables_build();
    memcpy(o_on, s_on, 4);              /* (a screen starts with what was set) */
    memcpy(o_x, s_x, 4);
    memcpy(o_y, s_y, 4);
    memcpy(o_fr, s_fr, 4);
    memcpy(o_seat, s_seat, 4);
    for (z = 0; z < ROWS; ++z)
        zdirty[z] = 1;
    compose_dirty();
    wait_raw();
    tables_install();
    dsp_ready = 1;
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


void gfx_arena(void)
{
    uint8_t r, *p;
    gfx_row(0, RK_HUD, PAL_HUD);
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


#endif

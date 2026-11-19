/* Amiga OCS renderer. Original art/layout: the project's VGA backend.
 * Dirty background cells convert once to a planar backing bitmap. Cached
 * planar sprites are masked blits; no per-pixel sprite conversion in play.
 * The OS owns the display, copper and blitter; no chipset takeover.
 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <graphics/copper.h>
#include <graphics/gfxmacros.h>
#include <hardware/custom.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include "amiga.h"
#include "gfx.h"
#include "gfxcodes.h"
#include "proto.h"
#include "plat.h"
#include "perf.h"
#define AR_COL 7
#define AR_ROW 2
#define AR_X 56
#define AR_Y 16
extern const uint16_t amiga_palette[32];
extern const uint8_t amiga_pens[256];
struct Window *amiga_win;
static struct Screen *screen;
static UWORD *pointer;
static struct UCopList *copper;
static uint8_t bb[64000], dirty_cells[25][40];
static uint8_t row_kind[25], row_pal[25], tch[25][40], tco[25][40];
static const uint8_t *bgline = line_menu;
static uint8_t showing, in_arena, phase, tile_at[TILES];
static uint8_t title_mode;
static uint32_t spread[256];
typedef struct {
    unsigned x, y;
    uint8_t on, fr, seat, big;
} Sprite;
static Sprite want[4], shown[4];
static struct BitMap background, composition;
static struct RastPort composition_rp;
static UBYTE *composition_data;
static UBYTE *background_data;
static struct BitMap tile_bitmap;
static UBYTE *tile_data;
typedef struct {
    struct BitMap bitmap;
    UBYTE *data, *mask;
    unsigned bytes;
} BlitSprite;
static BlitSprite cached[4][N_SPR_FRAMES][2];
static BlitSprite *sprite_image(const Sprite *s)
{
    BlitSprite *b = &cached[s->seat][s->fr][!!s->big];
    unsigned w = s->big ? 32 : 16, x, y, p, plane_size = w * w / 8;
    if (b->data) return b;
    b->bytes = plane_size * 6;
    b->data = AllocMem(b->bytes, MEMF_CHIP | MEMF_CLEAR);
    if (!b->data) amiga_fail("No chip memory for sprite cache");
    InitBitMap(&b->bitmap, 5, w, w);
    for (p = 0; p < 5; ++p) b->bitmap.Planes[p] = b->data + p * plane_size;
    b->mask = b->data + 5 * plane_size;
    for (y = 0; y < w; ++y)
        for (x = 0; x < w; ++x) {
            unsigned a, off = y * (w / 8) + x / 8;
            uint8_t v, pen, bit = 0x80 >> (x & 7);
            if (s->big && (s->fr == FR_WIN0 || s->fr == FR_WIN1))
                a = A_BIG + (s->fr - FR_WIN0) * 1024 + y * 32 + x;
            else
                a = A_SPR + s->fr * 256 + (s->big ? y >> 1 : y) * 16 +
                    (s->big ? x >> 1 : x);
            v = gfx_art[a];
            if (!v) continue;
            pen = amiga_pens[v + E_SEAT + 16 * s->seat];
            b->mask[off] |= bit;
            for (p = 0; p < 5; ++p)
                if (pen & (1 << p)) b->bitmap.Planes[p][off] |= bit;
        }
    return b;
}

static void dirty(unsigned x, unsigned y, unsigned w, unsigned h)
{
    unsigned r, c;
    if (x >= 320 || y >= 200 || !w || !h)
        return;
    if (x + w > 320)
        w = 320 - x;
    if (y + h > 200)
        h = 200 - y;
    for (r = y / 8; r <= (y + h - 1) / 8; ++r)
        for (c = x / 8; c <= (x + w - 1) / 8; ++c)
            dirty_cells[r][c] = 1;
}
static void fill(unsigned x, unsigned w, unsigned y, unsigned h)
{
    unsigned i;
    for (i = y; i < y + h && i < 200; ++i)
        memset(bb + i * 320 + x, bgline[i], w);
    dirty(x, y, w, h);
}
static void out(unsigned x, unsigned w, unsigned y, unsigned h)
{
    dirty(x, y, w, h);
}
static void over_text(unsigned x, unsigned w, unsigned y, unsigned h)
{
    unsigned r;
    for (r = y / 8; r <= (y + h - 1) / 8 && r < 25; ++r)
        memset(tch[r] + x / 8, 255, (x + w - 1) / 8 - x / 8 + 1);
}
static void draw(unsigned x, unsigned w, unsigned y, unsigned h, unsigned a, uint8_t key)
{
    unsigned i, j;
    over_text(x, w, y, h);
    for (i = 0; i < h && y + i < 200; ++i)
        for (j = 0; j < w && x + j < 320; ++j) {
            uint8_t v = gfx_art[a + i * w + j];
            bb[(y + i) * 320 + x + j] = key && !v ? bgline[y + i] : v;
        }
    dirty(x, y, w, h);
}
static void backdrop(uint8_t title)
{
    unsigned y, last = 256;
    const uint8_t *col = title ? back_title : back_menu;
    struct Custom *custom = (struct Custom *)0xdff000;
    if (copper) {
        screen->ViewPort.UCopIns = 0;
        RethinkDisplay();
        WaitTOF();
        if (copper->FirstCopList)
            FreeCopList(copper->FirstCopList);
        FreeMem(copper, sizeof(*copper));
        copper = 0;
    }
    copper = AllocMem(sizeof(*copper), MEMF_PUBLIC | MEMF_CLEAR);
    if (!copper)
        amiga_fail("No memory for display colours");
    CINIT(copper, 100);
    if (!copper->FirstCopList)
        amiga_fail("No memory for copper list");
    for (y = 0; y < 200; ++y) {
        unsigned band = line_menu[y] - E_BACK;
        if (band != last) {
            unsigned rgb = (col[band * 3] >> 2) << 8 | (col[band * 3 + 1] >> 2) << 4 |
                           (col[band * 3 + 2] >> 2);
            CWAIT(copper, y, 0);
            CMOVE(copper, custom->color[0], rgb);
            last = band;
        }
    }
    CEND(copper);
    screen->ViewPort.UCopIns = copper;
    RethinkDisplay();
    title_mode = title;
}
void gfx_close(void)
{
    unsigned seat, fr, big;
    WaitBlit();
    for (seat = 0; seat < 4; ++seat)
        for (fr = 0; fr < N_SPR_FRAMES; ++fr)
            for (big = 0; big < 2; ++big) {
                BlitSprite *b = &cached[seat][fr][big];
                if (b->data) FreeMem(b->data, b->bytes);
                b->data = 0;
            }
    if (tile_data) FreeMem(tile_data, N_TFRAMES * 160);
    tile_data = 0;
    if (composition_data) FreeMem(composition_data, 2560);
    composition_data = 0;
    if (background_data) FreeMem(background_data, 40000);
    background_data = 0;
    if (amiga_win) {
        ClearPointer(amiga_win);
        CloseWindow(amiga_win);
        amiga_win = 0;
    }
    if (pointer) {
        FreeMem(pointer, 12);
        pointer = 0;
    }
    if (screen) {
        if (copper) {
            screen->ViewPort.UCopIns = 0;
            RethinkDisplay();
            WaitTOF();
            if (copper->FirstCopList)
                FreeCopList(copper->FirstCopList);
            FreeMem(copper, sizeof(*copper));
            copper = 0;
        }
        CloseScreen(screen);
        screen = 0;
    }
}
void gfx_init(void)
{
    struct NewScreen ns;
    struct NewWindow nw;
    memset(&ns, 0, sizeof ns);
    memset(&nw, 0, sizeof nw);
    ns.Width = 320;
    ns.Height = 200;
    ns.Depth = 5;
    ns.Type = CUSTOMSCREEN;
    ns.DefaultTitle = (UBYTE *)"FujiBlast NIO";
    screen = OpenScreen(&ns);
    if (!screen)
        amiga_fail("Cannot open 320x200 screen");
    background_data = AllocMem(40000, MEMF_CHIP | MEMF_CLEAR);
    if (!background_data) amiga_fail("No chip memory for background");
    InitBitMap(&background, 5, 320, 200);
    {
        unsigned p;
        for (p = 0; p < 5; ++p) background.Planes[p] = background_data + p * 8000;
    }
    composition_data = AllocMem(2560, MEMF_CHIP | MEMF_CLEAR);
    if (!composition_data) amiga_fail("No chip memory for composition");
    InitBitMap(&composition, 5, 64, 64);
    InitRastPort(&composition_rp);
    composition_rp.BitMap = &composition;
    {
        unsigned p;
        for (p = 0; p < 5; ++p) composition.Planes[p] = composition_data + p * 512;
    }
    tile_data = AllocMem(N_TFRAMES * 160, MEMF_CHIP | MEMF_CLEAR);
    if (!tile_data) amiga_fail("No chip memory for tile cache");
    InitBitMap(&tile_bitmap, 5, 16, N_TFRAMES * 16);
    {
        unsigned p, i;
        for (p = 0; p < 5; ++p) tile_bitmap.Planes[p] = tile_data + p * N_TFRAMES * 32;
        for (i = 0; i < N_TFRAMES * 256; ++i) {
            uint8_t pen = amiga_pens[gfx_art[A_TILES + i]], bit = 0x80 >> (i & 7);
            for (p = 0; p < 5; ++p)
                if (pen & (1 << p)) tile_bitmap.Planes[p][i >> 3] |= bit;
        }
    }
    LoadRGB4(&screen->ViewPort, (UWORD *)amiga_palette, 32);
    nw.Width = 320;
    nw.Height = 200;
    nw.Type = CUSTOMSCREEN;
    nw.Screen = screen;
    nw.Flags = BORDERLESS | BACKDROP | ACTIVATE | RMBTRAP | NOCAREREFRESH;
    nw.IDCMPFlags = IDCMP_RAWKEY | IDCMP_INACTIVEWINDOW;
    amiga_win = OpenWindow(&nw);
    if (!amiga_win)
        amiga_fail("Cannot open game window");
    ShowTitle(screen, FALSE);
    pointer = AllocMem(12, MEMF_CHIP | MEMF_CLEAR);
    if (pointer)
        SetPointer(amiga_win, pointer, 1, 16, 0, 0);
    backdrop(0);
    {
        unsigned i;
        for (i = 0; i < 256; ++i) {
            unsigned pen = amiga_pens[i];
            spread[i] = (pen & 1) | ((pen & 2) << 7) | ((pen & 4) << 14) | ((pen & 8) << 21);
        }
    }
    /* Cache all normal animation frames before play. No allocation or pixel
     * conversion is allowed on the movement/rendering path. */
    {
        unsigned seat, fr;
        for (seat = 0; seat < 4; ++seat)
            for (fr = 0; fr < N_SPR_FRAMES; ++fr) {
                Sprite s = {0, 0, 1, fr, seat, 0};
                sprite_image(&s);
            }
    }
    gfx_begin();
}
void gfx_begin(void)
{
    unsigned r;
    showing = in_arena = 0;
    memset(want, 0, sizeof want);
    memset(shown, 0, sizeof shown);
    bgline = line_menu;
    fill(0, 320, 0, 200);
    memset(tch, 255, sizeof tch);
    memset(tco, 255, sizeof tco);
    for (r = 0; r < 25; ++r)
        gfx_row(r, ROW_TEXT, PAL_MENU);
}
void gfx_row(uint8_t row, uint8_t kind, uint8_t pal)
{
    if (row >= 25)
        return;
    if (row_kind[row] != kind || row_pal[row] != pal)
        memset(tch[row], 255, 40);
    row_kind[row] = kind;
    row_pal[row] = pal;
}
void gfx_show(void)
{
    unsigned r;
    uint8_t title = 0;
    for (r = 0; r < 25; ++r)
        if (row_pal[r] == PAL_TITLE)
            title = 1;
    if (title != title_mode)
        backdrop(title);
    memset(dirty_cells, 1, sizeof dirty_cells);
    showing = 1;
    gfx_render();
}
void gfx_clear_row(uint8_t row)
{
    if (row >= 25)
        return;
    fill(0, 320, row * 8, 8);
    memset(tch[row], 255, 40);
}
static const uint8_t big_style[][4] = {
    {ST_GOLD, ST_SEAT + 2, ST_SEAT + 1, ST_SILVER},   /* MENU */
    {ST_GOLD, ST_SEAT + 2, ST_SEAT + 1, ST_SILVER},   /* GFX */
    {ST_SEAT, ST_SEAT + 1, ST_SEAT + 2, ST_SEAT + 3}, /* HUD: the seats */
    {ST_SILVER, ST_SEAT + 2, ST_SEAT + 1, ST_GOLD},   /* ARENA */
    {ST_SEAT, ST_SEAT, ST_SEAT, ST_SEAT},             /* SEAT0..3 */
    {ST_SEAT + 1, ST_SEAT + 1, ST_SEAT + 1, ST_SEAT + 1},
    {ST_SEAT + 2, ST_SEAT + 2, ST_SEAT + 2, ST_SEAT + 2},
    {ST_SEAT + 3, ST_SEAT + 3, ST_SEAT + 3, ST_SEAT + 3},
    {ST_GOLD, ST_SEAT + 2, ST_SEAT + 1, ST_SILVER}, /* TITLE */
};

static const uint8_t *glyph(char ch)
{
    uint8_t c = (uint8_t)ch;
    if (c >= 'a' && c <= 'z')
        c -= 32;
    if (c < 32 || c > 127)
        c = 32;
    return font_gfx + (c - 32) * 8;
}

static void put_char(uint8_t row, uint8_t col, char ch, uint8_t colour)
{
    uint8_t k, st;
    if (row >= SCR_ROWS)
        return;
    k = row_kind[row];
    uint16_t x, w;
    if (col >= (k == ROW_BIG ? 20 : 40) ||
        (tch[row][col] == (uint8_t)ch && tco[row][col] == colour))
        return; /* (there already) */
    st = k == ROW_BIG || row_pal[row] == PAL_HUD
             ? big_style[row_pal[row]][colour & 3] : colour ? ST_INV : ST_SILVER;
    x = col * (k == ROW_BIG ? 16 : 8);
    w = k == ROW_BIG ? 16 : 8;
    {
        const uint8_t *g = glyph(ch);
        unsigned yy, xx;
        for (yy = 0; yy < 8; ++yy)
            for (xx = 0; xx < w; ++xx)
                bb[(row * 8 + yy) * 320 + x + xx] =
                    g[yy] & (0x80 >> (k == ROW_BIG ? xx >> 1 : xx))
                        ? text_style[st][yy]
                        : (k != ROW_BIG && row_pal[row] != PAL_HUD && colour
                               ? inv_bar[yy] : bgline[row * 8 + yy]);
    }
    dirty(x, row * 8, w, 8);
    tch[row][col] = ch;
    tco[row][col] = colour;
}

void gfx_text(uint8_t row, uint8_t col, const char *s, uint8_t colour)
{
    while (*s)
        put_char(row, col++, *s++, colour);
}

void gfx_big_line(uint8_t row, const char *s, const uint8_t *colour)
{
    uint8_t i;
    for (i = 0; i < 20; ++i)
        put_char(row, i, s[i], colour[i]);
}

void gfx_hud_line(const char *s, const uint8_t *colour)
{
    uint8_t i;
    for (i = 0; i < 40; ++i)
        put_char(0, i, s[i], colour[i]);
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
    uint8_t i;
    for (i = 0; i < 3; ++i)
        gfx_row(row + i, ROW_GFX, PAL_TITLE);
    draw((320 - LOGO_W) / 2, LOGO_W, row * 8, LOGO_H, A_LOGO, 1);
}

void gfx_trophies(uint8_t row, uint8_t col, uint8_t n)
{
    gfx_row(row, ROW_GFX, PAL_GFX);
    gfx_row(row + 1, ROW_GFX, PAL_GFX);
    while (n-- && col < 39) {
        draw(col * 8, 16, row * 8, 16, A_TROPHY, 1);
        col += 2;
    }
}

void gfx_arena(void)
{
    unsigned c, r;
    bgline = line_arena;
    fill(0, 320, 0, 200);
    gfx_row(0, ROW_TEXT, PAL_HUD);
    for (r = 1; r < 25; ++r) {
        gfx_row(r, ROW_ARENA, PAL_ARENA);
        for (c = 0; c < 40; ++c) {
            if ((r == 1 || r == 24) ? c >= 6 && c <= 33 : c == 6 || c == 33)
                draw(c * 8, 8, r * 8, 8, A_CELLS, 0);
            else if (c < 6 || c > 33)
                draw(c * 8, 8, r * 8, 8, A_CELLS + 64, 0);
        }
    }
    for (r = 0; r < ARENA_H; ++r)
        for (c = 0; c < ARENA_W; ++c)
            gfx_tile(c, r, T_FLOOR);
    in_arena = 1;
}
static uint16_t look(uint8_t tile)
{
    return A_TILES + tile_anim[tile][tile == T_BOMB || tile == T_DUD     ? phase
                                     : tile >= T_FIRE && tile <= T_GLOVE ? 2
                                                                         : 0] *
                         256u;
}

static void tile_blit(unsigned x, unsigned y, uint8_t tile)
{
    unsigned c, r;
    if (!showing || !in_arena) {
        draw(x, 16, y, 16, look(tile), 0);
        return;
    }
    BltBitMap(&tile_bitmap, 0, ((look(tile) - A_TILES) / 256) * 16,
              &background, x, y, 16, 16, 0xc0, 0xff, 0);
    for (r = y / 8; r < y / 8 + 2; ++r)
        for (c = x / 8; c < x / 8 + 2; ++c)
            dirty_cells[r][c] = 2; /* already planar: copy only */
}
static void draw_tile(uint8_t c, uint8_t r, uint8_t tile)
{
    tile_blit(AR_X + c * 16, AR_Y + r * 16, tile);
}

void gfx_tile(uint8_t c, uint8_t r, uint8_t tile)
{
    if (tile >= N_TILES)
        tile = T_FLOOR;
    tile_at[r * ARENA_W + c] = tile;
    draw_tile(c, r, tile);
}

void gfx_bomb_at(uint8_t hx, uint8_t hy, uint8_t dud)
{
    tile_blit(AR_X + hx * 8, AR_Y + hy * 8, dud ? T_DUD : T_BOMB);
}

/* Left panel: powerup i's icon at rows 3+4i, its count beside it. */
void gfx_stat(uint8_t i, uint8_t n)
{
    uint8_t row = 3 + i * 4;
    char b[3];
    if (i >= 3 && !n) {
        over_text(8, 16, row * 8, 16);
        fill(8, 16, row * 8, 16);
        out(8, 16, row * 8, 16);
        return;
    }
    draw(8, 16, row * 8, 16, A_ICONS + i * 256, 1);
    if (i < 3) {
        b[0] = n >= 10 ? '0' + n / 10 : ' ';
        b[1] = '0' + n % 10;
        b[2] = 0;
        gfx_text(row + 1, 3, "X", 0);
        gfx_text(row + 1, 4, b, 0);
    }
}

/* Right panel: RND and the round. */
void gfx_round(uint8_t round)
{
    char b[3];
    gfx_text(3, 35, "RND", 0);
    b[0] = round >= 10 ? '0' + (round / 10) % 10 : ' ';
    b[1] = '0' + round % 10;
    b[2] = 0;
    gfx_text(4, 36, b, 0);
}

void spr_at(uint8_t n, uint8_t x, uint8_t y, uint8_t fr, uint8_t seat, uint8_t big)
{
    if (n >= 4)
        return;
    want[n] = (Sprite){x * 2u, y, fr < N_SPR_FRAMES, fr, (uint8_t)(seat & 3), big};
}
void spr_arena(uint8_t n, uint8_t x, uint8_t y, uint8_t fr, uint8_t seat)
{
    if (n >= 4)
        return;
    spr_at(n, 0, 0, fr, seat, 0);
    want[n].x = AR_X + x;
    want[n].y = AR_Y + y;
}
void spr_hide(uint8_t n)
{
    if (n < 4)
        want[n].on = 0;
}
void spr_hide_all(void)
{
    unsigned n;
    for (n = 0; n < 4; ++n)
        want[n].on = 0;
}
void spr_flush(void) {}
/* Compose bounded regions in 2.5 KB of chip scratch RAM. The visible
 * screen never receives an erased background awaiting a later sprite draw. */
static void compose_rect(unsigned x, unsigned y, unsigned w, unsigned h)
{
    unsigned xx, yy, n;
    if (x >= 320 || y >= 200) return;
    if (x + w > 320) w = 320 - x;
    if (y + h > 200) h = 200 - y;
    for (yy = 0; yy < h; yy += 64)
        for (xx = 0; xx < w; xx += 64) {
            unsigned dx = x + xx, dy = y + yy;
            unsigned cw = w - xx > 64 ? 64 : w - xx;
            unsigned ch = h - yy > 64 ? 64 : h - yy;
            WaitBlit();
            BltBitMap(&background, dx, dy, &composition, 0, 0, cw, ch, 0xc0, 0xff, 0);
            for (n = 0; n < 4; ++n) {
                const Sprite *s = &want[n];
                unsigned sw = s->big ? 32 : 16, left, top, right, bottom;
                BlitSprite *b;
                if (!s->on || s->x >= dx + cw || s->y >= dy + ch ||
                    s->x + sw <= dx || s->y + sw <= dy) continue;
                left = s->x > dx ? s->x : dx;
                top = s->y > dy ? s->y : dy;
                right = s->x + sw < dx + cw ? s->x + sw : dx + cw;
                bottom = s->y + sw < dy + ch ? s->y + sw : dy + ch;
                b = sprite_image(s);
                BltMaskBitMapRastPort(&b->bitmap, left - s->x, top - s->y,
                                     &composition_rp, left - dx, top - dy,
                                     right - left, bottom - top, 0xe0, b->mask);
            }
            BltBitMap(&composition, 0, 0, &screen->BitMap, dx, dy, cw, ch, 0xc0, 0xff, 0);
            WaitBlit();
        }
}
static void compose_sprite(const Sprite *s)
{
    if (s->on) compose_rect(s->x, s->y, s->big ? 32 : 16, s->big ? 32 : 16);
}
void gfx_render(void)
{
    uint16_t perf_start = amiga_perf_clock();
    unsigned n, r, c, y, xx;
    uint8_t next = (frame_count >> 4) & 3, changed = 0;
    if (!showing || !screen) return;
    if (next != phase && in_arena) {
        phase = next;
        for (r = 0; r < ARENA_H; ++r)
            for (c = 0; c < ARENA_W; ++c)
                if (tile_at[r * ARENA_W + c] == T_BOMB || tile_at[r * ARENA_W + c] == T_DUD)
                    draw_tile(c, r, tile_at[r * ARENA_W + c]);
    }
    for (n = 0; n < 4; ++n)
        changed |= memcmp(&want[n], &shown[n], sizeof(Sprite)) != 0;
    for (r = 0; r < 25; ++r)
        for (c = 0; c < 40; ++c) changed |= dirty_cells[r][c];
    if (changed) {
        /* Prepare any uncached animation before starting blits. */
        for (n = 0; n < 4; ++n) if (want[n].on) sprite_image(&want[n]);

        WaitBlit();
        for (r = 0; r < 25; ++r) {
            for (c = 0; c < 40; ++c) if (dirty_cells[r][c] == 1) {
                for (y = r * 8; y < r * 8 + 8; ++y) {
                    uint32_t planes = 0;
                    uint8_t fifth = 0;
                    unsigned offset = y * 40 + c;
                    const uint8_t *src = bb + y * 320 + c * 8;
                    for (xx = 0; xx < 8; ++xx) {
                        uint8_t v = *src++;
                        planes = (planes << 1) | spread[v];
                        fifth = (fifth << 1) | (amiga_pens[v] >> 4);
                    }
                    background.Planes[0][offset] = planes;
                    background.Planes[1][offset] = planes >> 8;
                    background.Planes[2][offset] = planes >> 16;
                    background.Planes[3][offset] = planes >> 24;
                    background.Planes[4][offset] = fifth;
                }
            }
        }
        /* The visible bitmap receives only finished compositions. Never expose
         * the restore/draw interval, even if it spans a raster refresh. */
        for (r = 0; r < 25; ++r)
            for (c = 0; c < 40;) {
                unsigned first;
                if (!dirty_cells[r][c]) { ++c; continue; }
                first = c;
                while (c < 40 && dirty_cells[r][c]) dirty_cells[r][c++] = 0;
                compose_rect(first * 8, r * 8, (c - first) * 8, 8);
            }
        for (n = 0; n < 4; ++n) {
            const Sprite *a = &shown[n], *b = &want[n];
            unsigned aw = a->big ? 32 : 16, bw = b->big ? 32 : 16;
            if (!memcmp(a, b, sizeof(*a))) continue;
            if (a->on && b->on && a->x < b->x + bw && b->x < a->x + aw &&
                a->y < b->y + bw && b->y < a->y + aw) {
                unsigned x = a->x < b->x ? a->x : b->x;
                unsigned y = a->y < b->y ? a->y : b->y;
                unsigned right = a->x + aw > b->x + bw ? a->x + aw : b->x + bw;
                unsigned bottom = a->y + aw > b->y + bw ? a->y + aw : b->y + bw;
                if (right > 320) right = 320;
                if (bottom > 200) bottom = 200;
                if (x < right && y < bottom)
                    compose_rect(x, y, right - x, bottom - y);
            } else {
                compose_sprite(a);
                compose_sprite(b);
            }
        }
        WaitBlit();
        memcpy(shown, want, sizeof want);
    }
    amiga_perf_add(PERF_RENDER, perf_start);
}
#ifdef AMIGA_ACCEPT
/* Independent pixel oracle for blitter masking, overlap and background restore.
 * Runs only at acceptance completion, outside performance measurements. */
void gfx_verify(void)
{
    unsigned x, y, p, n;
    WaitBlit();
    for (y = 0; y < 200; ++y)
        for (x = 0; x < 320; ++x) {
            uint8_t expected = 0, actual = 0, bit = 0x80 >> (x & 7);
            for (p = 0; p < 5; ++p) {
                if (background.Planes[p][y * 40 + x / 8] & bit) expected |= 1 << p;
                if (screen->BitMap.Planes[p][y * screen->BitMap.BytesPerRow + x / 8] & bit)
                    actual |= 1 << p;
            }
            for (n = 0; n < 4; ++n) {
                const Sprite *s = &shown[n];
                unsigned w = s->big ? 32 : 16, sx, sy, a;
                uint8_t v;
                if (!s->on || x < s->x || y < s->y || x >= s->x + w || y >= s->y + w) continue;
                sx = x - s->x; sy = y - s->y;
                if (s->big && (s->fr == FR_WIN0 || s->fr == FR_WIN1))
                    a = A_BIG + (s->fr - FR_WIN0) * 1024 + sy * 32 + sx;
                else
                    a = A_SPR + s->fr * 256 + (s->big ? sy / 2 : sy) * 16 +
                        (s->big ? sx / 2 : sx);
                v = gfx_art[a];
                if (v) expected = amiga_pens[v + E_SEAT + 16 * s->seat];
            }
            if (actual != expected) {
                printf("Pixel mismatch x=%u y=%u expected=%u actual=%u\n", x,y,expected,actual);
                amiga_fail("Blitter composition mismatch");
            }
        }
    puts("FUJIBLAST PIXELS OK");
}
#endif
/* Test evidence: actual display planes, not the backing model. */
void gfx_dump(const char *path)
{
    FILE *f = fopen(path, "wb");
    unsigned p, y;
    if (!f)
        return;
    fwrite(amiga_palette, 2, 32, f);
    fputc(title_mode, f);
    fputc(in_arena, f);
    for (p = 0; p < 5; ++p)
        for (y = 0; y < 200; ++y)
            fwrite(screen->BitMap.Planes[p] + y * screen->BitMap.BytesPerRow, 1, 40, f);
    fclose(f);
}

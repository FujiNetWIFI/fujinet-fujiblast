/*
 * gfx.c -- the PC's screens (gfx.h) in CGA's 320x200 4-colour mode.
 *
 * CGA fits the Atari's layout exactly: a cell (4 colour clocks by 8 lines
 * there) is 8x8 pixels here, 2 bytes on each of 8 lines; a tile is 2x2
 * cells, 16x16. Text is the Namco arcade font, 40 columns (BIG text: 20,
 * each glyph twice as wide).
 *
 * The colours are set through the BIOS: mode 5 (on a CGA, light cyan, light
 * red and white with the intensity bit), and per screen the background,
 * INT 10h AH=0Bh: green in the arena, blue on the title, black for the
 * rest. On an EGA, VGA, PCjr or Tandy 1000, mode 4 and INT 10h AH=10h,
 * the same four colours in the palette registers. The whole screen has one palette, so the
 * Atari's row palettes become colours: a seat's text and bomber are that
 * seat's colour (the fourth seat's are cyan and red stripes).
 *
 * The screen has one page, so the drawing goes to a copy of it in RAM, B
 * (the same 16K layout: a line's bytes at the same offset in both), and the
 * cells that changed are copied to the screen when frame_wait calls
 * gfx_render. Sprites are never drawn in B: where one is, or was, the lines
 * are put together from B and every sprite over them, then copied out, so
 * nothing on the screen is ever erased and redrawn.
 */
#ifdef __MSDOS__

#include <string.h>
#include <stdlib.h>
#include <dos.h>
#include <i86.h>
#include "gfx.h"
#include "plat.h"
#include "proto.h"
#include "gfxcodes.h"
#include "msdos.h"

#define AR_COL 7                /* the arena's first cell column */
#define AR_ROW 2                /* and row */
#define AR_X   (AR_COL * 8)     /* in pixels */
#define AR_Y   (AR_ROW * 8)

/* backgrounds (RGBI) and the three colours (mode 5, bright) */
#define BG_ARENA 2              /* green */
#define BG_TITLE 1              /* blue */
#define BG_MENU  0              /* black */
static const uint8_t fg[3] = { 11, 12, 15 };

/* colour codes: pixel values 0-3, or STRIPES (1 on even lines, 2 on odd) */
#define STRIPES 4
static const uint8_t seat_col[SEATS] = { 3, 2, 1, STRIPES };
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
/* per seat: body, trim, visor (the eyes are 0) */
static const uint8_t seat_roles[SEATS][3] = { { 3, 2, 1 }, { 2, 3, 1 }, { 1, 3, 2 }, { STRIPES, 3, 3 } };

static uint8_t __far *bb;       /* B, the screen's copy */
static uint8_t __far *vram;
#define rowoff gx_rowoff

/* B's segment: the screen's copy, then the seats' colour tables (a page
 * each, for xlat), the sprites' data (a role byte and a mask byte for each
 * 4 pixels, phase 0 4 bytes a line, phase 1 shifted 2 pixels, 5), the big
 * sprite's, and gx_compose's line buffer. */
#define B_TABS  0x4000          /* 5 pages: seats 0-3, the 4th's odd lines */
#define B_SPR0  0x4500          /* 18 frames x 16 lines x 8 */
#define B_SPR1  0x4E00          /* 18 frames x 16 lines x 10 */
#define B_BIG   0x5940          /* 32 lines x 18 */
#define B_LBUF  0x5B80          /* 84 */
#define B_PARAS 0x5BE

/* gfxa.asm's arguments */
typedef struct {
    uint8_t y0, y1, dst, n, stride, tog;
    uint16_t data, tab;
} Job;
uint16_t gx_rowoff[200];        /* a line's offset in B and on the screen */
uint16_t gx_bseg;               /* B's segment */
uint8_t gx_x, gx_w, gx_y, gx_h; /* a rectangle: bytes, lines */
const uint8_t *gx_src;
uint16_t gx_lbuf = B_LBUF;
uint8_t gx_tab, gx_tog;         /* gx_sprite's colours */
uint16_t gx_ctab, gx_tabk;      /* gx_char's table, and odd lines' xor */
uint8_t gx_both;                /* gx_char: onto the screen too */
Job gx_jobs[4];
uint8_t gx_njobs;
void gx_blit(void);             /* B to the screen */
void gx_put(void);              /* gx_src to B */
void gx_compose(void);          /* B and the jobs to the screen */
void gx_put2(void);             /* gx_src to B and the screen */
void gx_sprite(void);           /* a sprite, alone, onto the screen */
void gx_tile(void);             /* a 16x16 tile to B and the screen */
void gx_char(void);             /* a glyph to B (and the screen) */
#pragma aux gx_blit "*_" parm [] modify [ax bx cx dx si di es]
#pragma aux gx_put "*_" parm [] modify [ax bx cx dx si di es]
#pragma aux gx_put2 "*_" parm [] modify [ax bx cx dx si di es]
#pragma aux gx_compose "*_" parm [] modify [ax bx cx dx si di es]
#pragma aux gx_sprite "*_" parm [] modify [ax bx cx dx si di es]
#pragma aux gx_tile "*_" parm [] modify [ax bx cx dx si di es]
#pragma aux gx_char "*_" parm [] modify [ax bx cx dx si di es]

static uint8_t row_kind[SCR_ROWS], row_pal[SCR_ROWS];
static uint8_t showing;         /* the screen shows the model */
static uint8_t all_dirty;       /* copy the whole of B */
static uint8_t rdirty[SCR_ROWS];
static uint8_t cdirty[SCR_ROWS][40];
/* the text on the screen, a cell at a time (a BIG row's at its column):
 * the character and its colour; 0xFF where art went over it */
static uint8_t tch[SCR_ROWS][40], tco[SCR_ROWS][40];

/* gx_char's tables: a nibble of a glyph's row as the bytes it makes */
static uint8_t nib[5][16];      /* 4 pixels in colour 0-3; 4: inverse (red on white) */
static uint8_t dnib[4][2][16];  /* 8 pixels (each bit twice): first byte, second */

/* ---- the arena's tiles and their animation ---- */
static uint8_t in_arena;
static uint8_t tile_at[TILES];
static uint8_t phase;

/* ---- sprites ---- */
typedef struct {
    uint8_t on, fr, seat, big;
    uint16_t x;                 /* pixels, even */
    uint8_t y;
    /* derived: */
    uint8_t xb, w, h;           /* bytes from the left, bytes wide (clipped), lines */
    uint8_t sw;                 /* the image's bytes a line */
    uint16_t data;              /* the image (B's segment), line 0 */
} Spr;
static Spr want[4], shown[4];
#ifdef STATS
uint16_t g_cnt[8];              /* fast sprites, composes, their lines, blits,
                                 * their bytes/16, tiles, text cells, renders */
#define COUNT(i, n) (g_cnt[i] += (n))
uint32_t t_g[3];                /* animate, the dirty cells, the sprites (PIT ticks) */
uint32_t prof_ticks(void);
#define TG0 tq = prof_ticks()
#define TG1(i) t_g[i] += prof_ticks() - tq
#else
#define TG0
#define TG1(i)
#define COUNT(i, n)
#endif
static uint8_t big_fr = 0xFF, big_ph;

/* ---- set-up ---------------------------------------------------------------- */

static uint8_t ega(uint8_t c)
{
    return plat_pal == PAL_EGA ? (c & 7) | ((c & 8) << 1) : c;  /* (the PCjr's, Tandy's: RGBI) */
}

static void set_reg(uint8_t reg, uint8_t c)
{
    union REGS r;
    r.w.ax = 0x1000;
    r.h.bl = reg;
    r.h.bh = ega(c);
    int86(0x10, &r, &r);
}

/* The background (and the border): on a CGA AH=0Bh BH=0, bit 4 the bright
 * set; elsewhere the four colours in the palette registers (AH=10h). */
static void set_bg(uint8_t c)
{
    union REGS r;
    uint8_t i;
    if (plat_pal == PAL_CGA) {
        r.h.ah = 0x0B;
        r.h.bh = 0;
        r.h.bl = c | 0x10;
        int86(0x10, &r, &r);
        return;
    }
    /* (AH=0Bh would put CGA's palette back in registers 1-3 here) */
    set_reg(0, c);
    for (i = 0; i < 3; ++i)
        set_reg(i + 1, fg[i]);
    r.w.ax = 0x1001;            /* the border */
    r.h.bh = ega(c);
    int86(0x10, &r, &r);
}

static uint8_t pix4(uint8_t n, uint8_t c)
{
    uint8_t b = 0, i;
    for (i = 0; i < 4; ++i)
        b = b << 2 | (n & (8 >> i) ? c : 0);
    return b;
}

/* a byte of 4 roles -> 4 colours */
static uint8_t recolour(uint8_t b, const uint8_t *map)
{
    uint8_t o = 0, i;
    for (i = 0; i < 4; ++i)
        o = o << 2 | map[(b >> (6 - 2 * i)) & 3];
    return o;
}

/* The sprites' data into B's segment: phase 0 as drawn, phase 1 two
 * pixels to the right (a byte more), role and mask byte by byte. */
static void shift_frames(void)
{
    uint8_t f, y, i, pr, pm, r, m;
    uint8_t __far *p0 = bb + B_SPR0, __far *p1 = bb + B_SPR1;
    for (f = 0; f < N_SPR_FRAMES; ++f)
        for (y = 0; y < 16; ++y) {
            pr = 0;
            pm = 0xFF;
            for (i = 0; i < 5; ++i) {
                r = i < 4 ? spr_role[f][y * 4 + i] : 0;
                m = i < 4 ? spr_mask[f][y * 4 + i] : 0xFF;
                if (i < 4) {
                    *p0++ = r;
                    *p0++ = m;
                }
                *p1++ = (uint8_t)(pr << 4 | r >> 4);
                *p1++ = (uint8_t)(pm << 4 | m >> 4);
                pr = r;
                pm = m;
            }
        }
}

void gfx_init(void)
{
    static const char nomem[] = "FUJIBLAST: NOT ENOUGH MEMORY\r\n";
    union REGS r;
    unsigned seg, w;
    uint16_t y, n;
    uint8_t c, s, map[4];

    if (_dos_allocmem(B_PARAS, &seg)) {
        plat_restore();         /* (no room: say so, and leave) */
        _dos_write(1, nomem, sizeof(nomem) - 1, &w);
        exit(1);
    }
    plat_mem[2] = seg;
    bb = MK_FP(seg, 0);
    gx_bseg = seg;
    vram = MK_FP(0xB800, 0);
    for (y = 0; y < 200; ++y)
        rowoff[y] = (y & 1) * 0x2000 + (y >> 1) * 80;
    for (c = 0; c < 4; ++c)
        for (n = 0; n < 16; ++n) {
            nib[c][n] = pix4((uint8_t)n, c);
            dnib[c][0][n] = pix4((uint8_t)((n & 8 ? 0xC : 0) | (n & 4 ? 3 : 0)), c);
            dnib[c][1][n] = pix4((uint8_t)((n & 2 ? 0xC : 0) | (n & 1 ? 3 : 0)), c);
        }
    for (n = 0; n < 16; ++n)
        nib[4][n] = pix4((uint8_t)n, 2) | pix4((uint8_t)(~n & 15), 3);
    for (s = 0; s < 5; ++s) {
        uint8_t seat = s < 4 ? s : 3;
        map[0] = 0;
        map[1] = seat_roles[seat][0] == STRIPES ? (s == 4 ? 2 : 1) : seat_roles[seat][0];
        map[2] = seat_roles[seat][1];
        map[3] = seat_roles[seat][2];
        for (n = 0; n < 256; ++n)
            bb[B_TABS + s * 256 + n] = recolour((uint8_t)n, map);
    }
    shift_frames();

    /* 320x200, 4 colours: on a CGA mode 5 (colour burst off), whose
     * colours are light cyan, light red and white; the others (the PCjr's
     * and Tandy's show mode 5 in greys) mode 4, the same colours put in
     * its palette registers */
    r.w.ax = plat_pal == PAL_CGA ? 0x0005 : 0x0004;
    int86(0x10, &r, &r);
    set_bg(BG_MENU);
    gfx_begin();
}

/* ---- the model -------------------------------------------------------------- */

static void dirty(uint8_t c0, uint8_t r0, uint8_t c1, uint8_t r1)
{
    uint8_t *p;
    if (c1 > 39)
        c1 = 39;
    if (r1 > SCR_ROWS - 1)
        r1 = SCR_ROWS - 1;
    for (p = cdirty[r0] + c0; r0 <= r1; ++r0, p += 40) {
        rdirty[r0] = 1;
        memset(p, 1, c1 - c0 + 1);
    }
}

void gfx_begin(void)
{
    uint8_t r;
    showing = 0;
    in_arena = 0;
    spr_hide_all();
    memset(shown, 0, sizeof(shown));
    _fmemset(bb, 0, 0x4000);
    _fmemset(vram, 0, 0x4000);
    for (r = 0; r < SCR_ROWS; ++r)
        gfx_row(r, ROW_TEXT, PAL_MENU);
    memset(tch, 0, sizeof(tch));        /* (B is clear: all blank) */
    memset(tco, 0, sizeof(tco));
    memset(rdirty, 0, sizeof(rdirty));
    memset(cdirty, 0, sizeof(cdirty));
    all_dirty = 0;
}

void gfx_row(uint8_t row, uint8_t kind, uint8_t pal)
{
    if (row_kind[row] != kind || row_pal[row] != pal)
        memset(tch[row], 0xFF, 40);     /* (the same text looks different now) */
    row_kind[row] = kind;
    row_pal[row] = pal;
}

void gfx_show(void)
{
    uint8_t r, bg = BG_MENU;
    if (row_kind[1] == ROW_ARENA)
        bg = BG_ARENA;
    else
        for (r = 0; r < SCR_ROWS; ++r)
            if (row_pal[r] == PAL_TITLE)
                bg = BG_TITLE;
    set_bg(bg);
    memset(shown, 0, sizeof(shown));
    all_dirty = 1;
    showing = 1;
}

void gfx_clear_row(uint8_t row)
{
    uint8_t i;
    for (i = 0; i < 8; ++i)
        _fmemset(bb + rowoff[row * 8 + i], 0, 80);
    dirty(0, row, 39, row);
    memset(tch[row], 0, 40);    /* blank (a blank in colour 0) */
    memset(tco[row], 0, 40);
}

/* ---- text ------------------------------------------------------------------- */

static const uint8_t *glyph(char ch)
{
    uint8_t c = (uint8_t)ch;
    if (c >= 'a' && c <= 'z')
        c -= 32;
    if (c < 32 || c > 127)
        c = 32;
    return font_gfx + (c - 32) * 8;
}

/* Bytes into B at a cell, n a line; only if they change it. */
static void draw(uint8_t xb, uint8_t w, uint8_t y, uint8_t h, const uint8_t *g);
static uint8_t under_sprite(uint8_t xb, uint8_t w, uint8_t y, uint8_t h);



static void put_char(uint8_t row, uint8_t col, char ch, uint8_t colour)
{
    uint8_t k = row_kind[row], c, xb, w;
    if (col >= (k == ROW_BIG ? 20 : 40) || (tch[row][col] == (uint8_t)ch && tco[row][col] == colour))
        return;                 /* (there already) */
    gx_src = glyph(ch);
    gx_tabk = 0;
    if (k == ROW_BIG) {
        c = pal_map[row_pal[row]][colour & 3];
        if (c == STRIPES) {
            gx_ctab = (uint16_t)dnib[1];
            gx_tabk = (uint16_t)dnib[1] ^ (uint16_t)dnib[2];
        } else {
            gx_ctab = (uint16_t)dnib[c];
        }
        xb = col * 4;
        w = 4;
    } else {
        gx_ctab = (uint16_t)nib[colour ? 4 : 3];
        xb = col * 2;
        w = 2;
    }
    gx_x = xb;
    gx_w = w;
    gx_y = row * 8;
    gx_both = showing && !all_dirty && !under_sprite(xb, w, row * 8, 8);
    gx_char();
    if (!gx_both)
        dirty(xb >> 1, row, (xb + w - 1) >> 1, row);
    COUNT(6, 1);
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

/* ---- pictures ------------------------------------------------------------- */

static void put(uint8_t xb, uint8_t w, uint8_t y, uint8_t h, const uint8_t *g)
{
    gx_x = xb;
    gx_w = w;
    gx_y = y;
    gx_h = h;
    gx_src = g;
    gx_put();
}

/* Is a sprite (shown, or to be) over bytes xb.., lines y..? */
static uint8_t under_sprite(uint8_t xb, uint8_t w, uint8_t y, uint8_t h)
{
    uint8_t n;
    const Spr *s;
    for (n = 0; n < 8; ++n) {
        s = n < 4 ? &want[n] : &shown[n - 4];
        if (s->on && s->xb < xb + w && xb < s->xb + s->w && s->y < y + h && y < s->y + s->h)
            return 1;
    }
    return 0;
}

/* Art into B, and onto the screen at once; where a sprite is, the cells
 * are left for gfx_render to put the sprite back over them. */
static void draw(uint8_t xb, uint8_t w, uint8_t y, uint8_t h, const uint8_t *g)
{
    uint8_t r;
    for (r = y >> 3; r <= (y + h - 1) >> 3; ++r)
        memset(tch[r] + (xb >> 1), 0xFF, ((xb + w - 1) >> 1) - (xb >> 1) + 1);
    gx_x = xb;
    gx_w = w;
    gx_y = y;
    gx_h = h;
    gx_src = g;
    if (showing && !all_dirty && !under_sprite(xb, w, y, h)) {
        if (w == 4 && h == 16 && !(y & 1)) {
            gx_tile();
            COUNT(5, 1);
        } else
            gx_put2();
        return;
    }
    gx_put();
    dirty(xb >> 1, y >> 3, (xb + w - 1) >> 1, (y + h - 1) >> 3);
}

/* 16x16 at pixel (x, y), x a multiple of 8 */
static void put16(uint16_t x, uint8_t y, const uint8_t *g)
{
    draw((uint8_t)(x >> 2), 4, y, 16, g);
}

void gfx_logo(uint8_t row)
{
    uint8_t i;
    for (i = 0; i < 3; ++i)
        gfx_row(row + i, ROW_GFX, PAL_TITLE);
    put((80 - LOGO_BYTES) / 2, LOGO_BYTES, row * 8, LOGO_LINES, logo_gfx[0]);
    dirty(0, row, 39, row + 2);
}

void gfx_trophies(uint8_t row, uint8_t col, uint8_t n)
{
    gfx_row(row, ROW_GFX, PAL_GFX);
    gfx_row(row + 1, ROW_GFX, PAL_GFX);
    while (n-- && col < 39) {
        put16(col * 8, row * 8, tile_gfx[tile_anim[T_TROPHY][0]]);
        col += 2;
    }
}

/* ---- the arena ------------------------------------------------------------ */

/* A line of the arena's rows: the panels, the walls, the floor. */
static void arena_line(uint8_t *b, uint8_t line, uint8_t band)
{
    uint8_t c;
    const uint8_t *g;
    for (c = 0; c < 40; ++c) {
        if (band ? c >= AR_COL - 1 && c <= AR_COL + ARENA_W * 2
                 : c == AR_COL - 1 || c == AR_COL + ARENA_W * 2)
            g = cell_gfx[CELL_WALL];
        else if (c < AR_COL - 1 || c > AR_COL + ARENA_W * 2)
            g = cell_gfx[CELL_PANEL];
        else
            g = 0;
        b[c * 2] = g ? g[line * 2] : 0;
        b[c * 2 + 1] = g ? g[line * 2 + 1] : 0;
    }
}

void gfx_arena(void)
{
    static uint8_t side[8][80], band[8][80];
    uint8_t r, i;
    gfx_row(0, ROW_BIG, PAL_HUD);
    for (i = 0; i < 8; ++i) {
        arena_line(side[i], i, 0);
        arena_line(band[i], i, 1);
    }
    for (r = 1; r < SCR_ROWS; ++r) {
        gfx_row(r, ROW_ARENA, PAL_ARENA);
        put(0, 80, r * 8, 8, r == 1 || r == SCR_ROWS - 1 ? band[0] : side[0]);
    }
    dirty(0, 1, 39, SCR_ROWS - 1);
    memset(tile_at, T_FLOOR, sizeof(tile_at));
    in_arena = 1;
}

/* A tile's look now: bombs and duds animate (as on the Apple: each one
 * redrawn costs an 8088 too much to animate them all); powerups show lit. */
static const uint8_t *look(uint8_t tile)
{
    return tile_gfx[tile_anim[tile][tile == T_BOMB || tile == T_DUD ? phase :
                                    tile >= T_FIRE && tile <= T_GLOVE ? 2 : 0]];
}

static void draw_tile(uint8_t c, uint8_t r, uint8_t tile)
{
    put16(AR_X + c * 16, AR_Y + r * 16, look(tile));
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
    put16(AR_X + hx * 8, AR_Y + hy * 8, look(dud ? T_DUD : T_BOMB));
}

/* Left panel: powerup i's icon at rows 3+4i, its count beside it. */
void gfx_stat(uint8_t i, uint8_t n)
{
    static const uint8_t blank[64];
    uint8_t row = 3 + i * 4;
    char b[3];
    if (i >= 3 && !n) {
        put16(8, row * 8, blank);
        return;
    }
    put16(8, row * 8, tile_gfx[tile_anim[T_FIRE + i][2]]);
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

/* A new animation phase (every 16 jiffies): the bombs and duds again. */
static void animate(void)
{
    uint8_t p = (uint8_t)(frame_count >> 4) & 3, t, c, r, i = 0;
    if (p == phase)
        return;
    phase = p;
    if (!in_arena)
        return;
    for (r = 0; r < ARENA_H; ++r)
        for (c = 0; c < ARENA_W; ++c, ++i) {
            t = tile_at[i];
            if (t == T_BOMB || t == T_DUD)
                draw_tile(c, r, t);
        }
}

/* ---- sprites ----------------------------------------------------------------- */

static void big_image(uint8_t fr, uint8_t ph)
{
    uint8_t y, i, k, b, m, r2[8], m2[8], pr, pm, rr, mm, sb, sm;
    uint8_t __far *p = bb + B_BIG;
    if (fr == big_fr && ph == big_ph)
        return;
    big_fr = fr;
    big_ph = ph;
    for (y = 0; y < 16; ++y) {
        for (i = 0; i < 4; ++i) {       /* each pixel twice as wide */
            b = spr_role[fr][y * 4 + i];
            m = spr_mask[fr][y * 4 + i];
            for (k = 0; k < 2; ++k) {
                sb = k ? b : b >> 4;
                sm = k ? m : m >> 4;
                r2[i * 2 + k] = (uint8_t)((sb & 0xC) << 4 | (sb & 0xC) << 2 | (sb & 3) << 2 | (sb & 3));
                m2[i * 2 + k] = (uint8_t)((sm & 0xC) << 4 | (sm & 0xC) << 2 | (sm & 3) << 2 | (sm & 3));
            }
        }
        pr = 0;
        pm = 0xFF;
        for (i = 0; i < 9; ++i) {
            rr = i < 8 ? r2[i] : 0;
            mm = i < 8 ? m2[i] : 0xFF;
            p[i * 2] = ph ? (uint8_t)(pr << 4 | rr >> 4) : rr;
            p[i * 2 + 1] = ph ? (uint8_t)(pm << 4 | mm >> 4) : mm;
            pr = rr;
            pm = mm;
        }
        _fmemcpy(p + 18, p, 18);        /* each line twice */
        p += 36;
    }
}

static void spr_set(uint8_t n, uint16_t px, uint8_t y, uint8_t frame, uint8_t seat, uint8_t big)
{
    Spr *s = &want[n];
    uint8_t ph = (px >> 1) & 1;
    if (s->on && s->x == px && s->y == y && s->fr == frame && s->seat == (seat & 3) && s->big == big)
        return;                 /* (as it is: the usual case, every pass) */
    if (frame >= N_SPR_FRAMES || px >= 320 || y >= 200) {
        s->on = 0;
        return;
    }
    s->on = 1;
    s->fr = frame;
    s->seat = seat & 3;
    s->big = big;
    s->x = px;
    s->y = y;
    s->xb = (uint8_t)(px >> 2);
    if (big) {
        big_image(frame, ph);
        s->sw = 9;
        s->h = 32;
        s->data = B_BIG;
    } else if (ph) {
        s->sw = 5;
        s->h = 16;
        s->data = B_SPR1 + frame * (16 * 10);
    } else {
        s->sw = 4;
        s->h = 16;
        s->data = B_SPR0 + frame * (16 * 8);
    }
    s->w = s->xb + s->sw > 80 ? 80 - s->xb : s->sw;
    if (s->y + s->h > 200)
        s->h = 200 - s->y;
}

void spr_at(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat, uint8_t big)
{
    spr_set(n, (uint16_t)x * 2, y, frame, seat, big);
}

void spr_arena(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat)
{
    spr_set(n, AR_X + (x & 0xFE), AR_Y + y, frame, seat, 0);
}

void spr_hide(uint8_t n)
{
    want[n].on = 0;
}

void spr_hide_all(void)
{
    want[0].on = want[1].on = want[2].on = want[3].on = 0;
}

void spr_flush(void)
{
}

/* ---- to the screen ----------------------------------------------------------- */

/* Lines y0..y0+h-1, bytes xb..xb+w-1: B with every sprite over it. */
static void compose(uint8_t xb, uint8_t w, uint8_t y0, uint8_t h)
{
    uint8_t n, sy0, sy1, sx0, sx1;
    uint16_t off;
    const Spr *s;
    Job *j = gx_jobs;
    for (n = 0; n < 4; ++n) {
        s = &want[n];
        if (!s->on)
            continue;
        sy0 = s->y > y0 ? s->y : y0;
        sy1 = s->y + s->h < y0 + h ? s->y + s->h : y0 + h;
        sx0 = s->xb > xb ? s->xb : xb;
        sx1 = s->xb + s->w < xb + w ? s->xb + s->w : xb + w;
        if (sy0 >= sy1 || sx0 >= sx1)
            continue;
        j->y0 = sy0;
        j->y1 = sy1;
        j->dst = sx0 - xb;
        j->n = sx1 - sx0;
        j->stride = s->sw * 2;
        off = (sy0 - s->y) * (s->sw * 2) + (sx0 - s->xb) * 2;
        j->data = s->data + off;
        j->tab = (B_TABS >> 8) + s->seat << 8;
        j->tog = s->seat == 3 ? 3 ^ 4 : 0;
        ++j;
    }
    gx_njobs = (uint8_t)(j - gx_jobs);
    gx_x = xb;
    gx_w = w;
    gx_y = y0;
    gx_h = h;
    gx_compose();
    COUNT(1, 1);
    COUNT(2, h);
}

/* The sprites' boxes in cells (want, then shown), to see which runs of
 * dirty cells they are over. */
static uint8_t box_c0[8], box_c1[8], box_r0[8], box_r1[8], box_n;

static void boxes(void)
{
    uint8_t n, k = 0;
    const Spr *s;
    for (n = 0; n < 8; ++n) {
        s = n < 4 ? &want[n] : &shown[n - 4];
        if (!s->on) {
            box_r0[n] = 0xFF;   /* (no row is past it) */
            box_r1[n] = 0;
            continue;
        }
        box_c0[n] = s->xb >> 1;
        box_c1[n] = (s->xb + s->w - 1) >> 1;
        box_r0[n] = s->y >> 3;
        box_r1[n] = (s->y + s->h - 1) >> 3;
        k = 1;
    }
    box_n = k;
}

static uint8_t same(const Spr *a, const Spr *b)
{
    return a->on == b->on && (!a->on ||
           (a->x == b->x && a->y == b->y && a->fr == b->fr && a->seat == b->seat && a->big == b->big));
}

/* Does sprite n (where it is, and where it was) meet no other sprite, and
 * is it one gx_sprite draws (small, all on the screen)? */
static uint8_t alone(uint8_t n)
{
    const Spr *a = &want[n], *b = &shown[n], *o;
    uint8_t k, x0, x1, y0, y1;
    if (a->on && (a->big || a->w != a->sw || a->h != 16))
        return 0;
    if (a->on && b->on) {
        x0 = a->xb < b->xb ? a->xb : b->xb;
        x1 = a->xb + a->w > b->xb + b->w ? a->xb + a->w : b->xb + b->w;
        y0 = a->y < b->y ? a->y : b->y;
        y1 = a->y + a->h > b->y + b->h ? a->y + a->h : b->y + b->h;
    } else {
        o = a->on ? a : b;
        x0 = o->xb;
        x1 = o->xb + o->w;
        y0 = o->y;
        y1 = o->y + o->h;
    }
    for (k = 0; k < 8; ++k) {
        if ((k & 3) == n)
            continue;
        o = k < 4 ? &want[k] : &shown[k - 4];
        if (o->on && o->xb < x1 && x0 < o->xb + o->w && o->y < y1 && y0 < o->y + o->h)
            return 0;
    }
    return 1;
}

static void spr_draw(const Spr *s)
{
    gx_x = s->xb;
    gx_y = s->y;
    gx_h = s->h;
    gx_w = s->sw;
    gx_src = (const uint8_t *)s->data;
    gx_tab = (B_TABS >> 8) + s->seat;
    gx_tog = s->seat == 3 ? 3 ^ 4 : 0;
    gx_sprite();
    COUNT(0, 1);
}

static void blit(uint8_t xb, uint8_t w, uint8_t y, uint8_t h)
{
    gx_x = xb;
    gx_w = w;
    gx_y = y;
    gx_h = h;
    gx_blit();
    COUNT(3, 1);
    COUNT(4, (w * h) >> 4);
}

/* What of where sprite b was the new a doesn't cover: B again. */
static void uncover(const Spr *b, const Spr *a)
{
    uint8_t y0, y1, top, bot;
    if (!a->on || a->xb >= b->xb + b->w || b->xb >= a->xb + a->w ||
        a->y >= b->y + b->h || b->y >= a->y + a->h) {
        blit(b->xb, b->w, b->y, b->h);      /* apart: all of it */
        return;
    }
    top = a->y > b->y ? a->y : b->y;        /* the lines both have */
    bot = a->y + a->h < b->y + b->h ? a->y + a->h : b->y + b->h;
    if (b->y < top)
        blit(b->xb, b->w, b->y, top - b->y);
    if (b->y + b->h > bot)
        blit(b->xb, b->w, bot, b->y + b->h - bot);
    y0 = top;
    y1 = bot;
    if (b->xb < a->xb)
        blit(b->xb, a->xb - b->xb, y0, y1 - y0);
    if (b->xb + b->w > a->xb + a->w)
        blit(a->xb + a->w, b->xb + b->w - (a->xb + a->w), y0, y1 - y0);
}

#ifdef STATS
/* What each routine costs on this machine: 100 of each, timed (PIT ticks
 * for one, in bench[]): a 5-byte sprite, a 4-byte one, a tile, a 5x18
 * blit, a 6x18 compose with one sprite in it, a text cell. */
uint16_t bench[6];
static uint8_t n_b;
void gfx_bench(void)
{
    uint32_t t;
    uint8_t i;
    Spr s;
    static const uint8_t cell[16];
    memset(&s, 0, sizeof(s));
    s.on = 1;
    s.xb = 20;
    s.y = 40;
    s.h = 16;
    for (i = 0; i < 2; ++i) {
        s.sw = s.w = i ? 4 : 5;
        s.data = i ? B_SPR0 : B_SPR1;
        t = prof_ticks();
        for (n_b = 0; n_b < 100; ++n_b)
            spr_draw(&s);
        bench[i] = (uint16_t)((prof_ticks() - t) / 100);
    }
    t = prof_ticks();
    for (n_b = 0; n_b < 100; ++n_b) {
        gx_x = 20;
        gx_y = 40;
        gx_src = tile_gfx[0];
        gx_tile();
    }
    bench[2] = (uint16_t)((prof_ticks() - t) / 100);
    t = prof_ticks();
    for (n_b = 0; n_b < 100; ++n_b)
        blit(20, 5, 40, 18);
    bench[3] = (uint16_t)((prof_ticks() - t) / 100);
    want[0] = s;
    t = prof_ticks();
    for (n_b = 0; n_b < 100; ++n_b)
        compose(19, 6, 39, 18);
    bench[4] = (uint16_t)((prof_ticks() - t) / 100);
    want[0].on = 0;
    t = prof_ticks();
    for (n_b = 0; n_b < 100; ++n_b) {
        tch[10][10] = 0xFF;
        put_char(10, 10, 'A', 0);
    }
    bench[5] = (uint16_t)((prof_ticks() - t) / 100);
    (void)cell;
}
#endif

void gfx_render(void)
{
    uint8_t r, c, c0, n, redo = 0, x0, x1, y0, y1, *p;
    Spr *a, *b;
#ifdef STATS
    uint32_t tq;
#endif
    TG0;
    animate();
    TG1(0);
    if (!showing)
        return;
    COUNT(7, 1);
    TG0;
    if (all_dirty) {
        gx_x = 0;
        gx_w = 80;
        gx_y = 0;
        gx_h = 200;
        gx_blit();
        all_dirty = 0;
        memset(rdirty, 0, sizeof(rdirty));
        memset(cdirty, 0, sizeof(cdirty));
        redo = 0x0F;
    } else {
        box_n = 0xFF;           /* (boxes() when a row is dirty) */
        for (r = 0; r < SCR_ROWS; ++r) {
            if (!rdirty[r])
                continue;
            rdirty[r] = 0;
            if (box_n == 0xFF)
                boxes();
            p = cdirty[r];
            for (c = 0; c < 40; ) {
                if (!p[c]) {
                    ++c;
                    continue;
                }
                c0 = c;
                while (c < 40 && p[c])
                    p[c++] = 0;
                gx_x = c0 * 2;
                gx_w = (c - c0) * 2;
                gx_y = r * 8;
                gx_h = 8;
                gx_blit();
                if (box_n)
                    for (n = 0; n < 8; ++n)
                        if (r >= box_r0[n] && r <= box_r1[n] && c - 1 >= box_c0[n] && c0 <= box_c1[n])
                            redo |= 1 << (n & 3);
            }
        }
    }
    TG1(1);
    TG0;
    for (n = 0; n < 4; ++n) {
        a = &want[n];
        b = &shown[n];
        if (!(redo & 1 << n) && same(a, b))
            continue;
        if (alone(n)) {
            if (a->on)
                spr_draw(a);
            if (b->on)
                uncover(b, a);
            *b = *a;
            continue;
        }
        x0 = a->xb < b->xb ? a->xb : b->xb;
        x1 = a->xb + a->w > b->xb + b->w ? a->xb + a->w : b->xb + b->w;
        y0 = a->y < b->y ? a->y : b->y;
        y1 = a->y + a->h > b->y + b->h ? a->y + a->h : b->y + b->h;
        if (a->on && b->on && x1 - x0 <= a->w + b->w && y1 - y0 <= a->h + b->h) {
            compose(x0, x1 - x0, y0, y1 - y0);  /* near: as one box */
        } else {
            if (b->on)
                compose(b->xb, b->w, b->y, b->h);
            if (a->on)
                compose(a->xb, a->w, a->y, a->h);
        }
        *b = *a;
    }
    TG1(2);
}

#endif

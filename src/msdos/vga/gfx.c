/*
 * gfx.c -- the PC's screens (gfx.h) in VGA's and MCGA's mode 13h: 320x200,
 * a byte a pixel, 256 colours. FUJIBVGA.EXE's; FUJIBLST.EXE draws in CGA
 * (src/msdos/cga/gfx.c), and this follows its plan.
 *
 * The geometry is CGA's: a cell 8x8 pixels, a tile 16x16, 40 columns of
 * the Namco font (BIG text: 20, each glyph twice as wide). The art and the
 * 256 colours are tools/drawart_vga.py's (tools/vgapal.py says where each
 * lives in the DAC), set through the BIOS: the whole DAC at the start (INT
 * 10h AX=1012h), the backdrop's 32 colours per screen, the fire's 16 as
 * they cycle. Only those DAC calls: an MCGA has no attribute controller.
 *
 * Text is a colour a glyph line (a gradient down each letter) over the
 * backdrop's colour for that line, so letters sit on the gradients as if
 * drawn there. The flames, sparks and the fire icon flicker by palette
 * cycling: nothing is redrawn for it.
 *
 * The screen has one page, so the drawing goes to a copy of it in RAM, B
 * (the same layout), and the cells that changed are copied to the screen
 * when frame_wait calls gfx_render. Sprites are never drawn in B: where
 * one is, or was, the pixels are put together from B and every sprite over
 * them on their way to the screen, so nothing on the screen is ever erased
 * and redrawn.
 *
 * A machine with no VGA or MCGA is handed to FUJIBLST.EXE (as if typed).
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
#define VIDSEG 0xA000u
#define B_PARAS 0x1000u         /* B: 64K, the screen's copy then gx_compose's line buffer */
#define B_LBUF 64000u
#define FIRE_EVERY 5            /* jiffies a step of the fire's cycle */

/* gfxa.asm's arguments */
typedef struct {
    uint8_t y0, y1;
    uint16_t dst, n, stride, data;
} Job;
uint16_t gx_rowoff[200];        /* a line's offset in B and on the screen */
uint16_t gx_bseg, gx_aseg;      /* B's segment, the art's */
uint16_t gx_x, gx_w;            /* a rectangle: pixels */
uint8_t gx_y, gx_h;             /* and lines */
uint16_t gx_src;                /* the art's offset, a glyph, a sprite's image */
uint16_t gx_bgl;                /* a colour a line, from line 0 (gx_fill, gx_putk) */
uint16_t gx_fg, gx_bg;          /* gx_char: 8 colours each */
uint16_t gx_lbuf = B_LBUF;
Job gx_jobs[4];
uint8_t gx_njobs;
void gx_blit(void);             /* B to the screen */
void gx_fill(void);             /* B's lines in their colours */
void gx_put(void);              /* art to B */
void gx_putk(void);             /* art to B, 0 the line's colour */
void gx_char(void);             /* a glyph to B */
void gx_sprite(void);           /* a sprite, alone, onto the screen */
void gx_compose(void);          /* B and the jobs to the screen */
#pragma aux gx_blit "*_" parm [] modify [ax bx cx dx si di es]
#pragma aux gx_fill "*_" parm [] modify [ax bx cx dx si di es]
#pragma aux gx_put "*_" parm [] modify [ax bx cx dx si di es]
#pragma aux gx_putk "*_" parm [] modify [ax bx cx dx si di es]
#pragma aux gx_char "*_" parm [] modify [ax bx cx dx si di es]
#pragma aux gx_sprite "*_" parm [] modify [ax bx cx dx si di es]
#pragma aux gx_compose "*_" parm [] modify [ax bx cx dx si di es]

static uint16_t art;            /* gfx_art's offset in its segment */
static uint8_t __far *bb;       /* B */
static const uint8_t *bgline;   /* each line's backdrop colour (the arena's or the menus') */

static uint8_t row_kind[SCR_ROWS], row_pal[SCR_ROWS];
static uint8_t showing;         /* the screen shows the model */
static uint8_t all_dirty;       /* copy the whole of B */
static uint8_t rdirty[SCR_ROWS];
static uint8_t cdirty[SCR_ROWS][40];
/* the text on the screen, a cell at a time (a BIG row's at its column):
 * the character and its colour; 0xFF where art went over it */
static uint8_t tch[SCR_ROWS][40], tco[SCR_ROWS][40];

/* a BIG row's text styles, colours 0-3, by palette */
static const uint8_t big_style[][4] = {
    { ST_GOLD, ST_SEAT + 2, ST_SEAT + 1, ST_SILVER },          /* MENU */
    { ST_GOLD, ST_SEAT + 2, ST_SEAT + 1, ST_SILVER },          /* GFX */
    { ST_SEAT, ST_SEAT + 1, ST_SEAT + 2, ST_SEAT + 3 },         /* HUD: the seats */
    { ST_SILVER, ST_SEAT + 2, ST_SEAT + 1, ST_GOLD },          /* ARENA */
    { ST_SEAT, ST_SEAT, ST_SEAT, ST_SEAT },                     /* SEAT0..3 */
    { ST_SEAT + 1, ST_SEAT + 1, ST_SEAT + 1, ST_SEAT + 1 },
    { ST_SEAT + 2, ST_SEAT + 2, ST_SEAT + 2, ST_SEAT + 2 },
    { ST_SEAT + 3, ST_SEAT + 3, ST_SEAT + 3, ST_SEAT + 3 },
    { ST_GOLD, ST_SEAT + 2, ST_SEAT + 1, ST_SILVER },          /* TITLE */
};

/* ---- the arena's tiles and their animation ---- */
static uint8_t in_arena;
static uint8_t tile_at[TILES];
static uint8_t phase;
static uint8_t fire_p, fire_t;

/* ---- sprites ---- */
typedef struct {
    uint8_t on, fr, seat, big;
    uint16_t x;                 /* pixels */
    uint8_t y;
    /* derived: */
    uint16_t w;                 /* pixels wide on the screen (clipped) */
    uint8_t h;                  /* lines (clipped) */
    uint8_t ih;                 /* the image's lines */
    uint16_t sw;                /* the image's pixels a line */
    uint16_t data;              /* the image (DGROUP), line 0 */
} Spr;
static Spr want[4], shown[4];
/* every frame coloured for every seat (0 clear), made once: rebuilding a
 * walking bomber's image each step cost a 4.77 MHz 8088 a fifth of its
 * time; and the one big one */
static uint8_t spr_img[4][N_SPR_FRAMES][256];
static uint8_t big_img[1024], big_fr = 0xFF, big_seat;
#ifdef STATS
uint16_t g_cnt[8];              /* lone sprites, composes, their lines, blits,
                                 * their bytes/256, tiles, text cells, renders */
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

/* ---- set-up ---------------------------------------------------------------- */

static void colour(uint8_t *d, const uint8_t __far *s, uint16_t n, uint8_t base);

/* DAC entries first.. from a table of 6-bit RGB in DGROUP (INT 10h
 * AX=1012h, a VGA's and an MCGA's) */
static void set_dac(uint16_t first, uint16_t n, const uint8_t *tab)
{
    union REGS r;
    struct SREGS s;
    segread(&s);
    s.es = s.ds;
    r.w.ax = 0x1012;
    r.w.bx = first;
    r.w.cx = n;
    r.w.dx = (uint16_t)tab;
    int86x(0x10, &r, &r, &s);
}

/* A VGA or an MCGA: INT 10h AX=1A00h answers AL=1Ah, the display in BL
 * (07h/08h a VGA, 0Bh/0Ch an MCGA). */
static uint8_t vga(void)
{
    union REGS r;
    r.w.ax = 0x1A00;
    int86(0x10, &r, &r);
    return r.h.al == 0x1A && (r.h.bl == 7 || r.h.bl == 8 || r.h.bl == 0x0B || r.h.bl == 0x0C);
}

void gfx_init(void)
{
    static const char nomem[] = "FUJIBLAST: NOT ENOUGH MEMORY\r\n";
    union REGS r;
    unsigned seg, w;
    uint16_t y;

    if (!vga()) {
        plat_restore();         /* (FUJIBLST.EXE's, then: CGA, EGA, PCjr, Tandy) */
        type_in("FUJIBLST\r");
        exit(0);
    }
    if (_dos_allocmem(B_PARAS, &seg)) {
        plat_restore();         /* (no room: say so, and leave) */
        _dos_write(1, nomem, sizeof(nomem) - 1, &w);
        exit(1);
    }
    plat_mem[2] = seg;
    bb = MK_FP(seg, 0);
    gx_bseg = seg;
    gx_aseg = FP_SEG(gfx_art);
    art = FP_OFF(gfx_art);
    for (y = 0; y < 200; ++y)
        gx_rowoff[y] = y * 320;

    for (w = 0; w < 4; ++w)
        for (y = 0; y < N_SPR_FRAMES; ++y)
            colour(spr_img[w][y], gfx_art + A_SPR + y * 256u, 256, E_SEAT + w * 16);

    r.w.ax = 0x0013;            /* 320x200, 256 colours */
    int86(0x10, &r, &r);
    set_dac(0, 256, dac_init);
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

/* B's rectangle in the backdrop's colours */
static void fill(uint16_t x, uint16_t w, uint8_t y, uint8_t h)
{
    gx_x = x;
    gx_w = w;
    gx_y = y;
    gx_h = h;
    gx_bgl = (uint16_t)bgline;
    gx_fill();
}

void gfx_begin(void)
{
    uint8_t r;
    showing = 0;
    in_arena = 0;
    spr_hide_all();
    memset(shown, 0, sizeof(shown));
    bgline = line_menu;
    fill(0, 320, 0, 200);
    _fmemset(MK_FP(VIDSEG, 0), 0, 64000u);
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
    uint8_t r, title = 0;
    if (row_kind[1] != ROW_ARENA) {
        for (r = 0; r < SCR_ROWS; ++r)
            if (row_pal[r] == PAL_TITLE)
                title = 1;
        set_dac(E_BACK, 32, title ? back_title : back_menu);
    }
    memset(shown, 0, sizeof(shown));
    all_dirty = 1;
    showing = 1;
}

void gfx_clear_row(uint8_t row)
{
    fill(0, 320, row * 8, 8);
    dirty(0, row, 39, row);
    memset(tch[row], 0, 40);    /* blank */
    memset(tco[row], 0, 40);
}

/* ---- drawing into B, and out ------------------------------------------------- */

/* Is a sprite (shown, or to be) over pixels x.., lines y..? */
static uint8_t under_sprite(uint16_t x, uint16_t w, uint8_t y, uint8_t h)
{
    uint8_t n;
    const Spr *s;
    for (n = 0; n < 8; ++n) {
        s = n < 4 ? &want[n] : &shown[n - 4];
        if (s->on && s->x < x + w && x < s->x + s->w && s->y < y + h && y < s->y + s->h)
            return 1;
    }
    return 0;
}

/* What was just drawn in B onto the screen at once; where a sprite is, the
 * cells are left for gfx_render to put the sprite back over them. */
static void out(uint16_t x, uint16_t w, uint8_t y, uint8_t h)
{
    if (showing && !all_dirty && !under_sprite(x, w, y, h)) {
        gx_x = x;
        gx_w = w;
        gx_y = y;
        gx_h = h;
        gx_blit();
        return;
    }
    dirty((uint8_t)(x >> 3), y >> 3, (uint8_t)((x + w - 1) >> 3), (y + h - 1) >> 3);
}

/* The text cells under a picture: gone. */
static void over_text(uint16_t x, uint16_t w, uint8_t y, uint8_t h)
{
    uint8_t r;
    for (r = y >> 3; r <= (y + h - 1) >> 3; ++r)
        memset(tch[r] + (x >> 3), 0xFF, ((x + w - 1) >> 3) - (x >> 3) + 1);
}

/* Art (an offset in gfx_art, w pixels a line) into B, then out; key: its
 * 0s are the backdrop. */
static void draw(uint16_t x, uint16_t w, uint8_t y, uint8_t h, uint16_t a, uint8_t key)
{
    over_text(x, w, y, h);
    gx_x = x;
    gx_w = w;
    gx_y = y;
    gx_h = h;
    gx_src = art + a;
    if (key) {
        gx_bgl = (uint16_t)bgline;
        gx_putk();
    } else {
        gx_put();
    }
    out(x, w, y, h);
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

static void put_char(uint8_t row, uint8_t col, char ch, uint8_t colour)
{
    uint8_t k = row_kind[row], st;
    uint16_t x, w;
    if (col >= (k == ROW_BIG ? 20 : 40) || (tch[row][col] == (uint8_t)ch && tco[row][col] == colour))
        return;                 /* (there already) */
    gx_bg = (uint16_t)(bgline + row * 8);
    if (k == ROW_BIG) {
        st = big_style[row_pal[row]][colour & 3];
        x = col * 16;
        w = 16;
    } else {
        st = colour ? ST_INV : ST_SILVER;
        if (colour)
            gx_bg = (uint16_t)inv_bar;
        x = col * 8;
        w = 8;
    }
    gx_src = (uint16_t)glyph(ch);
    gx_fg = (uint16_t)text_style[st];
    gx_x = x;
    gx_w = w;
    gx_y = row * 8;
    gx_char();
    out(x, w, row * 8, 8);
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

/* ---- the arena ------------------------------------------------------------ */

/* An 8x8 cell of the frame (0 the wall, 1 a side panel) into B. */
static void cell(uint8_t c, uint8_t row, uint8_t which)
{
    gx_x = c * 8;
    gx_w = 8;
    gx_y = row * 8;
    gx_h = 8;
    gx_src = art + A_CELLS + which * 64;
    gx_put();
}

/* The HUD's bar, the frame, the side panels and a grass floor, all in B:
 * one row of tiles drawn, then copied down the rest. */
void gfx_arena(void)
{
    uint8_t r, c, wall_c0 = AR_COL - 1, wall_c1 = AR_COL + ARENA_W * 2;
    gfx_row(0, ROW_BIG, PAL_HUD);
    bgline = line_arena;
    fill(0, 320, 0, 8);
    for (r = 1; r < SCR_ROWS; ++r)
        gfx_row(r, ROW_ARENA, PAL_ARENA);
    for (r = 1; r < 4; r += 2)
        for (c = 0; c < 40; ++c) {
            if (r == 1 ? c >= wall_c0 && c <= wall_c1 : c == wall_c0 || c == wall_c1)
                cell(c, r, 0);
            else if (c < wall_c0 || c > wall_c1)
                cell(c, r, 1);
        }
    _fmemcpy(bb + 16 * 320, bb + 24 * 320, 8 * 320);     /* row 2: as row 3 */
    for (c = 0; c < ARENA_W; ++c) {
        gx_x = AR_X + c * 16;
        gx_w = 16;
        gx_y = AR_Y;
        gx_h = 16;
        gx_src = art + A_TILES + tile_anim[T_FLOOR][0] * 256u;
        gx_put();
    }
    for (r = 1; r < ARENA_H; ++r)
        _fmemcpy(bb + (AR_Y + r * 16) * 320u, bb + AR_Y * 320, 16 * 320);
    _fmemcpy(bb + 24u * 8 * 320, bb + 8 * 320, 8 * 320);   /* row 24: as row 1 */
    dirty(0, 0, 39, SCR_ROWS - 1);
    memset(tile_at, T_FLOOR, sizeof(tile_at));
    in_arena = 1;
}

/* A tile's look now: bombs and duds animate (each redrawn); powerups show
 * lit; the flames by the palette alone. */
static uint16_t look(uint8_t tile)
{
    return A_TILES + tile_anim[tile][tile == T_BOMB || tile == T_DUD ? phase :
                                     tile >= T_FIRE && tile <= T_GLOVE ? 2 : 0] * 256u;
}

static void draw_tile(uint8_t c, uint8_t r, uint8_t tile)
{
    draw(AR_X + c * 16, 16, AR_Y + r * 16, 16, look(tile), 0);
    COUNT(5, 1);
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
    draw(AR_X + hx * 8, 16, AR_Y + hy * 8, 16, look(dud ? T_DUD : T_BOMB), 0);
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

/* The fire's colours a step on (every FIRE_EVERY jiffies, in the arena);
 * a new animation phase (every 16): the bombs and duds again. */
static void animate(void)
{
    uint8_t p = (uint8_t)(frame_count >> 4) & 3, t, c, r, i = 0;
    if (in_arena && (uint8_t)((uint8_t)frame_count - fire_t) >= FIRE_EVERY) {
        fire_t = (uint8_t)frame_count;
        fire_p = (fire_p + 1) & 3;
        set_dac(E_FIRE, 16, fire_cyc[fire_p]);
    }
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

/* codes (0 clear) -> the seat's DAC entries */
static void colour(uint8_t *d, const uint8_t __far *s, uint16_t n, uint8_t base)
{
    uint8_t c;
    while (n--) {
        c = *s++;
        *d++ = c ? c + base : 0;
    }
}

/* The big one: the cheer has its own 32x32 frames; anything else is the
 * small one doubled. */
static void big_image(uint8_t fr, uint8_t seat)
{
    uint8_t y, x, c, base = E_SEAT + seat * 16, *p = big_img;
    const uint8_t __far *s;
    if (fr == big_fr && seat == big_seat)
        return;
    big_fr = fr;
    big_seat = seat;
    if (fr == FR_WIN0 || fr == FR_WIN1) {
        colour(big_img, gfx_art + A_BIG + (fr - FR_WIN0) * 1024u, 1024, base);
        return;
    }
    s = gfx_art + A_SPR + fr * 256u;
    for (y = 0; y < 16; ++y, s += 16) {
        for (x = 0; x < 16; ++x) {
            c = s[x] ? s[x] + base : 0;
            p[x * 2] = p[x * 2 + 1] = c;
        }
        memcpy(p + 32, p, 32);
        p += 64;
    }
}

static void spr_set(uint8_t n, uint16_t px, uint8_t y, uint8_t frame, uint8_t seat, uint8_t big)
{
    Spr *s = &want[n];
    seat &= 3;
    if (s->on && s->x == px && s->y == y && s->fr == frame && s->seat == seat && s->big == big)
        return;                 /* (as it is: the usual case, every pass) */
    if (frame >= N_SPR_FRAMES || px >= 320 || y >= 200) {
        s->on = 0;
        return;
    }
    s->on = 1;
    s->fr = frame;
    s->seat = seat;
    s->big = big;
    s->x = px;
    s->y = y;
    if (big) {
        big_image(frame, seat);
        s->sw = 32;
        s->ih = 32;
        s->data = (uint16_t)big_img;
    } else {
        s->sw = 16;
        s->ih = 16;
        s->data = (uint16_t)spr_img[seat][frame];
    }
    s->w = px + s->sw > 320 ? 320 - px : s->sw;
    s->h = s->y + s->ih > 200 ? 200 - s->y : s->ih;
}

void spr_at(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat, uint8_t big)
{
    spr_set(n, (uint16_t)x * 2, y, frame, seat, big);
}

void spr_arena(uint8_t n, uint8_t x, uint8_t y, uint8_t frame, uint8_t seat)
{
    spr_set(n, AR_X + x, AR_Y + y, frame, seat, 0);
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

/* Lines y0..y0+h-1, pixels x..x+w-1: B with every sprite over it. */
static void compose(uint16_t x, uint16_t w, uint8_t y0, uint8_t h)
{
    uint8_t n, sy0, sy1;
    uint16_t sx0, sx1;
    const Spr *s;
    Job *j = gx_jobs;
    for (n = 0; n < 4; ++n) {
        s = &want[n];
        if (!s->on)
            continue;
        sy0 = s->y > y0 ? s->y : y0;
        sy1 = s->y + s->h < y0 + h ? s->y + s->h : y0 + h;
        sx0 = s->x > x ? s->x : x;
        sx1 = s->x + s->w < x + w ? s->x + s->w : x + w;
        if (sy0 >= sy1 || sx0 >= sx1)
            continue;
        j->y0 = sy0;
        j->y1 = sy1;
        j->dst = sx0 - x;
        j->n = sx1 - sx0;
        j->stride = s->sw;
        j->data = s->data + (sy0 - s->y) * s->sw + (sx0 - s->x);
        ++j;
    }
    gx_njobs = (uint8_t)(j - gx_jobs);
    gx_x = x;
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
        box_c0[n] = (uint8_t)(s->x >> 3);
        box_c1[n] = (uint8_t)((s->x + s->w - 1) >> 3);
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
 * is it one gx_sprite draws (all on the screen)? */
static uint8_t alone(uint8_t n)
{
    const Spr *a = &want[n], *b = &shown[n], *o;
    uint8_t k, y0, y1;
    uint16_t x0, x1;
    if (a->on && (a->w != a->sw || a->h != a->ih))
        return 0;
    if (a->on && b->on) {
        x0 = a->x < b->x ? a->x : b->x;
        x1 = a->x + a->w > b->x + b->w ? a->x + a->w : b->x + b->w;
        y0 = a->y < b->y ? a->y : b->y;
        y1 = a->y + a->h > b->y + b->h ? a->y + a->h : b->y + b->h;
    } else {
        o = a->on ? a : b;
        x0 = o->x;
        x1 = o->x + o->w;
        y0 = o->y;
        y1 = o->y + o->h;
    }
    for (k = 0; k < 8; ++k) {
        if ((k & 3) == n)
            continue;
        o = k < 4 ? &want[k] : &shown[k - 4];
        if (o->on && o->x < x1 && x0 < o->x + o->w && o->y < y1 && y0 < o->y + o->h)
            return 0;
    }
    return 1;
}

static void spr_draw(const Spr *s)
{
    gx_x = s->x;
    gx_y = s->y;
    gx_h = s->h;
    gx_w = s->sw;
    gx_src = s->data;
    gx_sprite();
    COUNT(0, 1);
}

static void blit(uint16_t x, uint16_t w, uint8_t y, uint8_t h)
{
    gx_x = x;
    gx_w = w;
    gx_y = y;
    gx_h = h;
    gx_blit();
    COUNT(3, 1);
    COUNT(4, (w * h) >> 8);
}

/* What of where sprite b was the new a doesn't cover: B again. */
static void uncover(const Spr *b, const Spr *a)
{
    uint8_t top, bot;
    if (!a->on || a->x >= b->x + b->w || b->x >= a->x + a->w ||
        a->y >= b->y + b->h || b->y >= a->y + a->h) {
        blit(b->x, b->w, b->y, b->h);       /* apart: all of it */
        return;
    }
    top = a->y > b->y ? a->y : b->y;        /* the lines both have */
    bot = a->y + a->h < b->y + b->h ? a->y + a->h : b->y + b->h;
    if (b->y < top)
        blit(b->x, b->w, b->y, top - b->y);
    if (b->y + b->h > bot)
        blit(b->x, b->w, bot, b->y + b->h - bot);
    if (b->x < a->x)
        blit(b->x, a->x - b->x, top, bot - top);
    if (b->x + b->w > a->x + a->w)
        blit(a->x + a->w, b->x + b->w - (a->x + a->w), top, bot - top);
}

#ifdef STATS
/* What each routine costs on this machine: 100 of each, timed (PIT ticks
 * for one, in bench[]): a lone 16x16 sprite, a lone 32x32 one, a tile
 * (into B and out), a 20x18 blit, a 24x18 compose with one sprite in it,
 * a text cell. */
uint16_t bench[6];
static uint8_t n_b;
void gfx_bench(void)
{
    uint32_t t;
    uint8_t i;
    Spr s;
    memset(&s, 0, sizeof(s));
    s.on = 1;
    s.x = 80;
    s.y = 40;
    for (i = 0; i < 2; ++i) {
        s.sw = s.w = i ? 32 : 16;
        s.h = s.ih = (uint8_t)s.sw;
        s.data = i ? (uint16_t)big_img : (uint16_t)spr_img[0][0];
        t = prof_ticks();
        for (n_b = 0; n_b < 100; ++n_b)
            spr_draw(&s);
        bench[i] = (uint16_t)((prof_ticks() - t) / 100);
    }
    t = prof_ticks();
    for (n_b = 0; n_b < 100; ++n_b)
        draw(80, 16, 40, 16, A_TILES, 0);
    bench[2] = (uint16_t)((prof_ticks() - t) / 100);
    t = prof_ticks();
    for (n_b = 0; n_b < 100; ++n_b)
        blit(80, 20, 40, 18);
    bench[3] = (uint16_t)((prof_ticks() - t) / 100);
    s.sw = s.w = 16;
    s.h = s.ih = 16;
    s.data = (uint16_t)spr_img[0][0];
    want[0] = s;
    t = prof_ticks();
    for (n_b = 0; n_b < 100; ++n_b)
        compose(76, 24, 39, 18);
    bench[4] = (uint16_t)((prof_ticks() - t) / 100);
    want[0].on = 0;
    t = prof_ticks();
    for (n_b = 0; n_b < 100; ++n_b) {
        tch[10][10] = 0xFF;
        put_char(10, 10, 'A', 0);
    }
    bench[5] = (uint16_t)((prof_ticks() - t) / 100);
}
#endif

void gfx_render(void)
{
    uint8_t r, c, c0, n, redo = 0, y0, y1, *p;
    uint16_t x0, x1;
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
        gx_w = 320;
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
                gx_x = c0 * 8;
                gx_w = (c - c0) * 8;
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
        x0 = a->x < b->x ? a->x : b->x;
        x1 = a->x + a->w > b->x + b->w ? a->x + a->w : b->x + b->w;
        y0 = a->y < b->y ? a->y : b->y;
        y1 = a->y + a->h > b->y + b->h ? a->y + a->h : b->y + b->h;
        if (a->on && b->on && x1 - x0 <= a->w + b->w && y1 - y0 <= a->h + b->h) {
            compose(x0, x1 - x0, y0, y1 - y0);  /* near: as one box */
        } else {
            if (b->on)
                compose(b->x, b->w, b->y, b->h);
            if (a->on)
                compose(a->x, a->w, a->y, a->h);
        }
        *b = *a;
    }
    TG1(2);
}

#endif

/*
 * h19.c -- the H19's screen: what the game draws (bg), the sprites over
 * it, what the terminal shows (have), and for each row the span of
 * columns that may differ. h19_flush() composes a row's span (bg with the
 * sprites over it), and sends the cells that differ from have, with as few
 * escape sequences as it can:
 *
 *   - graphics mode stays on (ESC F once): text is upper case, which
 *     graphics mode prints as text, so the only attribute ever switched is
 *     reverse video (ESC p / ESC q);
 *   - the cursor moves the cheapest way: across up to three cells by
 *     sending them again, back by BS, down a row by LF, else ESC Y r c;
 *   - the rest of a row gone blank is ESC K;
 *   - a new screen with much to wipe is ESC E first.
 *
 * At 9600 baud the line takes a byte a millisecond, about 16 a frame. So
 * nothing goes twice: a cell is sent only when it differs from what the
 * terminal shows (M.U.L.E.'s client, short of memory, kept only dirty bits
 * and sent every cell a sprite crossed). A flush puts no more than FILL
 * bytes in the console's ring (h89io.asm), so whatever is queued is on the
 * screen within a few frames; the rows of our own bomber (h19_first) go
 * first, so it answers the keys at once even while flames are still being
 * drawn. A row not finished keeps the rest of its span for the next flush.
 *
 * The 2 MHz Z80 is the other limit: the cell writers (h19cell.asm) and the
 * sending of a row (h19run.asm) are assembly; this file decides what goes.
 */
#include <string.h>
#include "h19.h"
#include "h89.h"

#define ESC     0x1B
#define UNKNOWN 0xFF
#ifndef FILL
#define FILL    96              /* bytes in the ring after a flush, at most */
#endif
#define OPMAX   7               /* one cell's worst: ESC Y r c, ESC p, c */
#define STALE_FOR_ERASE 4
#define WIPE_FOR_CLEAR 64       /* cells to blank that make ESC E worth it */
#define ERASE_SPAN 12           /* a span this wide may end in ESC K */

uint8_t bg[SCR_H][SCR_W];
static uint8_t have[SCR_H][SCR_W];
uint8_t *rowp[SCR_H];                   /* (h19cell.asm reads these) */
static uint8_t *havep[SCR_H];
uint8_t d_lo[SCR_H], d_hi[SCR_H];       /* the span that may differ: lo..hi-1 */
uint8_t any_dirty;
static uint8_t hot[SCR_H], any_hot;
uint8_t line[SCR_W];                    /* (h19run.asm reads these) */
uint8_t sw_x, sw_hi, *sw_h;
uint8_t cur_x, cur_y, cur_rev;          /* (h19run.asm moves them) */
static uint8_t new_screen, active, rr;
int8_t budget;                          /* (h19run.asm spends it) */
uint8_t h89_idle;
uint16_t h89_flushes;
uint16_t h89_sweeps, h89_cells, h89_queued;     /* (counted for a rig) */
uint8_t h19_first = UNKNOWN;

/* sprites: wanted, and as they were last laid over the rows */
const uint8_t *s_art[SPR_SLOTS];
uint8_t s_c[SPR_SLOTS], s_r[SPR_SLOTS], s_rev[SPR_SLOTS];
static const uint8_t *s_sent[SPR_SLOTS];
static uint8_t s_sc[SPR_SLOTS], s_sr[SPR_SLOTS], s_srev[SPR_SLOTS];

/* Columns a..b-1 of row y may differ. */
static void span(uint8_t y, uint8_t a, uint8_t b)
{
    if (a < d_lo[y])
        d_lo[y] = a;
    if (b > d_hi[y])
        d_hi[y] = b;
    any_dirty = 1;
}

static void out(uint8_t c)
{
    con_put(c);
    --budget;
}

static void esc(uint8_t c)
{
    con_put(ESC);
    con_put(c);
    budget -= 2;
}

static void setrev(uint8_t r)
{
    if (r != cur_rev) {
        cur_rev = r;
        esc(r ? 'p' : 'q');
    }
}

/* Sent as they are. */
static void send_all(const char *s)
{
    while (*s)
        con_put((uint8_t)*s++);
}

static void all_spans(uint8_t y1)
{
    uint8_t y;
    for (y = 0; y <= y1; ++y) {
        d_lo[y] = 0;
        d_hi[y] = SCR_W;
    }
    any_dirty = 1;
}

/* The modes: the power-up ones (ESC z: no hold screen...), no reverse,
 * cursor off, no key click, no LF after CR nor CR after LF, discard at the
 * end of a line (never wrap), keypad shifted (its arrows are ESC A-D),
 * the 25th line on; lines 1-24 cleared, and the 25th; graphics mode, for
 * good. */
static const char init_seq[] =
    "\033z\033q\033x5\033x2\033y8\033y9\033w\033t\033x1\033E\033Y8 \033l\033F";
/* Graphics off, the 25th line off (which erases it), the DIP switches'
 * modes and a clear screen. */
static const char exit_seq[] = "\033G\033q\033y1\033z";

void h19_init(void)
{
    uint8_t y;
    for (y = 0; y < SCR_H; ++y) {
        rowp[y] = bg[y];
        havep[y] = have[y];
        d_lo[y] = SCR_W;
        d_hi[y] = 0;
    }
    memset(bg, ' ', sizeof(bg));
    memset(have, ' ', sizeof(have));
    send_all(init_seq);
    cur_x = cur_y = UNKNOWN;
    cur_rev = 0;
    active = 1;
}

void h19_exit(void)
{
    uint16_t t;
    if (!active)
        return;
    active = 0;
    send_all(exit_seq);
    while (!con_flushed())
        con_pump();
    t = ticks_now();            /* (the terminal takes a moment) */
    while ((uint16_t)(ticks_now() - t) < 100)
        ;
}

void h19_cleared(void)
{
    new_screen = 1;
}

/* ---- drawing into bg -------------------------------------------------- */
/* (static variables: zsdcc's frame variables cost 19 T-states a touch) */

/* (h19_put, h19_putn, h19_fill: h19cell.asm) */

/* Text: lower case is raised; what graphics mode would show as a graphic
 * ('^' '_' '`' and above) is shown as something like it, or '?'. */
uint8_t h19_char(uint8_t c)
{
    if (c >= 'a' && c <= 'z')
        return c - 32;
    if (c == '_')
        return G_BAR_B;
    if (c == '|')
        return G_VLINE;
    if (c < 0x20 || c > 0x5D)
        return '?';
    return c;
}

void h19_str(uint8_t x, uint8_t y, const char *s, uint8_t attr)
{
    static uint8_t c, *p, a, b;
    static const char *q;
    if (y >= SCR_H)
        return;
    p = rowp[y] + x;
    q = s;
    a = UNKNOWN;
    for (; x < SCR_W && (c = (uint8_t)*q) != 0; ++q, ++x, ++p) {
        c = h19_char(c) | attr;
        if (*p != c) {
            *p = c;
            if (a == UNKNOWN)
                a = x;
            b = x;
        }
    }
    if (a != UNKNOWN)
        span(y, a, b + 1);
}

/* ---- sprites ---------------------------------------------------------- */
/* (gfx.c's gfx_sync sets the wants: s_art, s_c, s_r, s_rev) */

/* Rows mk_r..mk_y1-1, columns mk_c..mk_x1-1 may differ; ours go first. */
static uint8_t mk_c, mk_x1, mk_r, mk_y1, mk_first;
static void mark(const uint8_t *art) __z88dk_fastcall
{
    static uint8_t y;
    mk_x1 = mk_c + art[0];
    if (mk_x1 > SCR_W)
        mk_x1 = SCR_W;
    mk_y1 = mk_r + art[1];
    if (mk_y1 > SCR_H)
        mk_y1 = SCR_H;
    for (y = mk_r; y < mk_y1; ++y) {
        if (mk_c < d_lo[y])
            d_lo[y] = mk_c;
        if (mk_x1 > d_hi[y])
            d_hi[y] = mk_x1;
        if (mk_first)
            hot[y] = any_hot = 1;
    }
    any_dirty = 1;
}

/* The sprites' moves since the last flush. */
static void spr_update(void)
{
    static uint8_t i;
    static const uint8_t *a, *o;
    for (i = 0; i < SPR_SLOTS; ++i) {
        a = s_art[i];
        o = s_sent[i];
        if (o == a && (!a || (s_sc[i] == s_c[i] && s_sr[i] == s_r[i] && s_srev[i] == s_rev[i])))
            continue;
        mk_first = i == h19_first;
        if (o) {
            mk_c = s_sc[i];
            mk_r = s_sr[i];
            mark(o);
        }
        if (a) {
            mk_c = s_c[i];
            mk_r = s_r[i];
            mark(a);
        }
        s_sent[i] = a;
        s_sc[i] = s_c[i];
        s_sr[i] = s_r[i];
        s_srev[i] = s_rev[i];
    }
}

/* Slot i laid over line[ly_lo..ly_hi-1] (row ly). */
static uint8_t ly, ly_lo, ly_hi;
static void lay(uint8_t i) __z88dk_fastcall
{
    static const uint8_t *a;
    static uint8_t k, w, x, v, rv, r;
    if (!(a = s_sent[i]))
        return;
    r = ly - s_sr[i];               /* (above it: large) */
    if (r >= a[1])
        return;
    w = a[0];
    for (a += 2; r; --r)
        a += w;
    x = s_sc[i];
    rv = s_srev[i];
    for (k = w; k; --k, ++a, ++x)
        if ((v = *a) != 0 && x >= ly_lo && x < ly_hi)
            line[x] = v ^ rv;
}

/* Row y's columns lo..hi-1 as they should look: bg with the sprites over
 * it, h19_first on top. */
static void compose(uint8_t y, uint8_t lo, uint8_t hi)
{
    uint8_t i;
    memcpy(line + lo, rowp[y] + lo, hi - lo);
    ly = y;
    ly_lo = lo;
    ly_hi = hi;
    for (i = 0; i < SPR_SLOTS; ++i)
        if (i != h19_first)
            lay(i);
    if (h19_first < SPR_SLOTS)
        lay(h19_first);
}

/* ---- sending ---------------------------------------------------------- */
/* (h19run.asm: the cursor moves and the cells) */
uint8_t sw_y, span_lo;
uint8_t h19_run(void);
void h19_goto(uint8_t x) __z88dk_fastcall;

/* Where the row as it should be (line within lo..hi-1, h past it) is blank
 * to its end. */
static uint8_t blank_from(uint8_t lo, uint8_t hi, const uint8_t *h)
{
    static uint8_t i;
    static const uint8_t *p;
    i = SCR_W;
    for (p = h + SCR_W; i > hi; --i)
        if (*--p != ' ')
            return i;
    for (p = line + hi; i > lo; --i)
        if (*--p != ' ')
            return i;
    return i;
}

/* Row y's span, while the budget lasts: 0 if it ran out (the rest of the
 * span stays). A wide span (a new screen's, a blast's) whose row ends
 * blank where the terminal shows enough there has that wiped by ESC K
 * first. */
static uint8_t sweep(uint8_t y)
{
    static uint8_t lo, hi, *h, tail, i, n;

    lo = d_lo[y];
    hi = d_hi[y];
    if (lo >= hi) {
        d_lo[y] = SCR_W;
        d_hi[y] = 0;
        return 1;
    }
    compose(y, lo, hi);
    ++h89_sweeps;
    h89_cells += hi - lo;
    sw_h = h = havep[y];
    sw_y = y;
    span_lo = lo;
    if (hi - lo >= ERASE_SPAN && budget >= OPMAX) {
        tail = blank_from(lo, hi, h);
        if (tail < lo)
            tail = lo;
        if (tail < SCR_W - 1) {
            for (n = 0, i = tail; i < SCR_W; ++i)
                if (h[i] != ' ')
                    ++n;
            if (n >= STALE_FOR_ERASE) {
                h19_goto(tail);
                setrev(0);
                esc('K');
                memset(h + tail, ' ', SCR_W - tail);
                if (tail < hi)
                    hi = tail;          /* (the rest is done) */
            }
        }
    }
    sw_x = lo;
    sw_hi = hi;
    if (!h19_run()) {
        d_lo[y] = sw_x;
        d_hi[y] = hi;
        return 0;
    }
    d_lo[y] = SCR_W;
    d_hi[y] = 0;
    return 1;
}

/* A new screen: if much of the old one must be wiped, ESC E does it. */
static void maybe_clear(void)
{
    static uint16_t wipe;
    static uint8_t y, x, *b, *h;
    new_screen = 0;
    wipe = 0;
    for (y = 0; y < ROW25; ++y) {
        b = rowp[y];
        h = havep[y];
        for (x = d_lo[y]; x < d_hi[y]; ++x)
            if (h[x] != ' ' && b[x] == ' ')
                ++wipe;
    }
    if (wipe >= WIPE_FOR_CLEAR) {
        setrev(0);
        esc('E');
        cur_x = cur_y = 0;
        memset(have, ' ', ROW25 * SCR_W);
        all_spans(ROW25 - 1);
    }
}

void h19_flush(void)
{
    static uint8_t y, k, more;

    if (!active)
        return;
    ++h89_flushes;
    con_pump();
    budget = (int8_t)(FILL - (TX_RING - 1 - con_free()));
    h89_queued += (uint8_t)(TX_RING - 1 - con_free());
    spr_update();
    if (new_screen)
        maybe_clear();
    more = 0;
    if (any_hot) {
        for (y = 0; y < SCR_H; ++y)
            if (hot[y]) {
                if (budget < OPMAX || !sweep(y)) {
                    more = 1;
                    goto done;
                }
                hot[y] = 0;
            }
        any_hot = 0;
    }
    if (any_dirty) {
        for (k = 0; k < SCR_H; ++k) {
            y = rr + k;
            if (y >= SCR_H)
                y -= SCR_H;
            if (d_lo[y] >= d_hi[y])
                continue;
            if (budget < OPMAX || !sweep(y)) {
                rr = y;
                more = 1;
                goto done;
            }
        }
        any_dirty = 0;
    }
done:
    h89_idle = !more && con_flushed();
}

void h19_repaint(void)
{
    if (!active)
        return;
    setrev(0);
    send_all("\033E\033Y8 \033l");
    cur_x = cur_y = UNKNOWN;
    memset(have, ' ', sizeof(have));
    all_spans(SCR_H - 1);
}

void h19_bell(void)
{
    con_put(7);
}

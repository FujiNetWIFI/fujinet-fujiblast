/*
 * walkref.c -- src/move.c (the walker the server checks against) replayed
 * on the Studio II's walk trace: each jiffy's ways held, and x, y, dir, acc
 * after it (a DEBUG build's TRACE page, p_play.inc p_trace). Every jiffy is
 * started from the trace's state before it, so one difference shows once.
 *
 *   cc -I src -o build/walkref tools/s2sim/walkref.c
 *   build/walkref MAPHEX TRACEHEX [SPEED]
 *
 * MAPHEX the 143 map bytes, TRACEHEX the trace's 5-byte entries in order
 * (hex, spaces ignored). Prints each jiffy that differs; exit 1 if any.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"

uint8_t st[STATE_SIZE];
#include "move.c"

#define KB_UP 1
#define KB_DOWN 2
#define KB_LEFT 4
#define KB_RIGHT 8

/* play.c try_dirs, the keys in KB_* bits */
static uint8_t try_dirs(uint8_t s, uint8_t lv)
{
    uint8_t h = NONE, v = NONE, a, b, x, y, acc;
    if (s & KB_UP)
        v = DIR_UP;
    else if (s & KB_DOWN)
        v = DIR_DOWN;
    if (s & KB_LEFT)
        h = DIR_LEFT;
    else if (s & KB_RIGHT)
        h = DIR_RIGHT;
    if (h == NONE || v == NONE)
        return mv_step(h == NONE ? v : h, lv);
    a = (mv_dir == DIR_LEFT || mv_dir == DIR_RIGHT) ? v : h;
    b = a == v ? h : v;
    x = mv_x;
    y = mv_y;
    acc = mv_acc;
    if (mv_step(a, lv))
        return 1;
    mv_x = x;
    mv_y = y;
    mv_acc = acc;
    return mv_step(b, lv);
}

static int hex(const char *s, uint8_t *out, int max)
{
    int n = 0;
    unsigned v;
    while (*s && n < max) {
        while (*s == ' ')
            ++s;
        if (!*s || sscanf(s, "%2x", &v) != 1)
            break;
        out[n++] = v;
        s += 2;
    }
    return n;
}

int main(int argc, char **argv)
{
    static uint8_t tr[16384];
    int n, i, bad = 0, lv = argc > 3 ? atoi(argv[3]) : 0;
    if (argc < 3)
        return fprintf(stderr, "usage: walkref MAPHEX TRACEHEX [SPEED]\n"), 2;
    if (hex(argv[1], st + OFF_MAP, TILES) != TILES)
        return fprintf(stderr, "walkref: the map needs %d bytes\n", TILES), 2;
    n = hex(argv[2], tr, sizeof tr) / 5;
    for (i = 1; i < n; ++i) {
        const uint8_t *p = tr + 5 * (i - 1), *q = tr + 5 * i;
        mv_reset(p[1], p[2], p[3]);
        mv_acc = p[4];
        if (q[0])
            try_dirs(q[0], lv);
        else
            mv_step(NONE, lv);
        if (mv_x != q[1] || mv_y != q[2] || mv_dir != q[3] || mv_acc != q[4]) {
            printf("jiffy %d: keys %02X from %d,%d dir %d acc %d: move.c %d,%d dir %d acc %d, Studio II %d,%d dir %d acc %d\n",
                   i, q[0], p[1], p[2], p[3], p[4], mv_x, mv_y, mv_dir, mv_acc, q[1], q[2], q[3], q[4]);
            ++bad;
        }
    }
    printf("walkref: %d jiffies, %d differ\n", n - 1, bad);
    return bad != 0;
}

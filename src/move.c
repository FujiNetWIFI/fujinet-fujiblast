/*
 * move.c -- the player's own walking, exactly as the server's
 * internal/game/move.go (Walker): the server checks it, the bots walk
 * with it.
 *
 * The player is a tile-sized box at mv_x, mv_y (16 units a tile); one of
 * the two is always on a lane (a multiple of 16). Pushing into a cross
 * lane from up to LANE units off it slides onto it.
 */
#include "game.h"

uint8_t mv_x, mv_y, mv_dir, mv_acc, mv_bump = NONE, pend_tile = NONE;

void mv_reset(uint8_t x, uint8_t y, uint8_t dir)
{
    mv_x = x;
    mv_y = y;
    mv_dir = dir;
    mv_acc = 0;
}

uint8_t centre_tile(uint8_t x, uint8_t y)
{
    uint8_t c = (uint8_t)(x + UNIT / 2) >> 4, r = (uint8_t)(y + UNIT / 2) >> 4;
    if (c >= ARENA_W)
        c = ARENA_W - 1;
    if (r >= ARENA_H)
        r = ARENA_H - 1;
    return r * ARENA_W + c;
}

/* c, r may be -1: as unsigned it is out of the arena too */
static uint8_t walkable(uint8_t c, uint8_t r)
{
    uint8_t t;
    if (c >= ARENA_W || r >= ARENA_H)
        return 0;
    t = r * ARENA_W + c;
    if (t == pend_tile)
        return 0;
    switch (MAPT(t)) {
    case T_PILLAR:
    case T_BLOCK:
    case T_BURN:
    case T_BOMB:
    case T_DUD:
        return 0;
    }
    return 1;
}

static void bumped(uint8_t c, uint8_t r)
{
    uint8_t t;
    if (c >= ARENA_W || r >= ARENA_H)
        return;
    t = r * ARENA_W + c;
    if (MAPT(t) == T_BOMB || MAPT(t) == T_DUD)
        mv_bump = t;
}

static uint8_t unit_step(uint8_t d)
{
    uint8_t c, r, off;
    if (d == DIR_LEFT || d == DIR_RIGHT) {
        uint8_t dx = d == DIR_RIGHT ? 1 : 0xFF;
        off = mv_y & 15;
        c = (mv_x >> 4) + dx;
        r = mv_y >> 4;
        if (off) {
            if (off <= LANE && walkable(c, r)) {
                --mv_y;
                return 1;
            }
            if (off >= UNIT - LANE && walkable(c, r + 1)) {
                ++mv_y;
                return 1;
            }
            return 0;
        }
        if (!(mv_x & 15) && !walkable(c, r)) {
            bumped(c, r);
            return 0;
        }
        mv_x += dx;
        return 1;
    } else {
        uint8_t dy = d == DIR_DOWN ? 1 : 0xFF;
        off = mv_x & 15;
        c = mv_x >> 4;
        r = (mv_y >> 4) + dy;
        if (off) {
            if (off <= LANE && walkable(c, r)) {
                --mv_x;
                return 1;
            }
            if (off >= UNIT - LANE && walkable(c + 1, r)) {
                ++mv_x;
                return 1;
            }
            return 0;
        }
        if (!(mv_y & 15) && !walkable(c, r)) {
            bumped(c, r);
            return 0;
        }
        mv_y += dy;
        return 1;
    }
}

/* One jiffy toward d (NONE: still) at speed level lv: 1 if it moved. */
uint8_t mv_step(uint8_t d, uint8_t lv)
{
    uint8_t n, moved = 0;
    mv_bump = NONE;
    if (d == NONE) {
        mv_acc = 0;
        return 0;
    }
    mv_dir = d;
    mv_acc += SPEED_BASE + lv * SPEED_STEP;
    n = mv_acc >> 4;
    mv_acc &= 15;
    for (; n; --n) {
        if (!unit_step(d)) {
            mv_acc = 0;
            break;
        }
        moved = 1;
    }
    return moved;
}

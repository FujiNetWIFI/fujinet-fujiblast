/* Exercise the actual play-loop selector; discard unrelated UI at link time. */
#include <assert.h>
#include <string.h>
#include "../../src/play.c"
uint8_t st[STATE_SIZE], inp_dir;
static const uint8_t bits[] = { IN_UP, IN_RIGHT, IN_DOWN, IN_LEFT };
static void rotate(unsigned *x, unsigned *y, unsigned turns)
{
    while (turns--) { unsigned old = *x; *x = 128 - *y; *y = old; }
}
static void tile(unsigned x, unsigned y, unsigned rotation, uint8_t type)
{
    rotate(&x, &y, rotation);
    MAPT((y / UNIT) * ARENA_W + x / UNIT) = type;
}
static void fits(void)
{
    unsigned c, r;
    assert(!(mv_x & 15) || !(mv_y & 15));
    for (r = mv_y / 16; r <= (mv_y + 15) / 16; ++r)
        for (c = mv_x / 16; c <= (mv_x + 15) / 16; ++c) {
            unsigned t = r * ARENA_W + c;
            assert(MAPT(t) != T_PILLAR && MAPT(t) != T_BOMB && t != pend_tile);
        }
}
static void corner(unsigned rot, unsigned speed, unsigned batch, uint8_t obstacle, unsigned approach, unsigned mirror)
{
    unsigned x = approach, y = 64, endx = 64, endy = 80, tick, j;
    memset(st, 0, sizeof st);
    pend_tile = NONE;
    turn_stick = 0;
    tile(48, 80, rot, T_PILLAR);
    tile(80, 80, rot, T_PILLAR);
    tile(64, 96, rot, obstacle);
    if (!obstacle) {
        x = 64; y = 96; rotate(&x, &y, rot);
        pend_tile = (y / 16) * ARENA_W + x / 16;
        x = approach; y = 64;
    }
    if (mirror) x = 128 - x;
    rotate(&x, &y, rot);
    rotate(&endx, &endy, rot);
    mv_reset(x, y, ((mirror ? DIR_LEFT : DIR_RIGHT) + rot) & 3);
    inp_dir = bits[((mirror ? DIR_LEFT : DIR_RIGHT) + rot) & 3];
    try_dirs(speed);
    inp_dir |= bits[(DIR_DOWN + rot) & 3];
    /* Same held diagonal across slow display passes and batched catch-up ticks.
     * No server response is needed to finish this turn. */
    for (tick = 0; tick < 120; tick += batch)
        for (j = 0; j < batch; ++j) {
            try_dirs(speed);
            fits();
        }
    assert(mv_x == endx && mv_y == endy);
    /* Releasing one axis immediately takes the new cardinal input. */
    inp_dir = bits[(DIR_UP + rot) & 3];
    assert(try_dirs(speed));
    assert(mv_x != endx || mv_y != endy);
    pend_tile = NONE;
}
static void cardinal(void)
{
    unsigned offset, i;
    memset(st, 0, sizeof st);
    pend_tile = NONE;
    for (offset = 1; offset <= LANE; ++offset) {
        mv_reset(64 - offset, 64, DIR_RIGHT);
        inp_dir = IN_DOWN;
        for (i = 0; i < offset + 4; ++i) try_dirs(0);
        assert(mv_x == 64 && mv_y == 68);
        mv_reset(64 + offset, 64, DIR_LEFT);
        for (i = 0; i < offset + 4; ++i) try_dirs(0);
        assert(mv_x == 64 && mv_y == 68);
    }
    /* Outside the lane assist, do not teleport or cut diagonally. */
    mv_reset(56, 64, DIR_RIGHT);
    for (i = 0; i < 20; ++i) assert(!try_dirs(0));
    assert(mv_x == 56 && mv_y == 64);
}
int main(void)
{
    unsigned rot, speed, batch, approach, mirror;
    for (rot = 0; rot < 4; ++rot)
        for (speed = 0; speed <= MAX_SPEED; ++speed)
            for (batch = 1; batch <= 6; ++batch)
              for (approach = 50; approach <= 58; approach += 8)
               for (mirror = 0; mirror < 2; ++mirror) {
                corner(rot, speed, batch, T_PILLAR, approach, mirror);
                corner(rot, speed, batch, T_BOMB, approach, mirror);
                corner(rot, speed, batch, 0, approach, mirror); /* locally predicted bomb */
            }
    cardinal();
}

#include <assert.h>
#include <string.h>
#include "../../src/amiga/controls.h"
#include "game.h"
uint8_t st[STATE_SIZE];
int main(void)
{
    uint8_t held[128] = {0}, fraction = 0;
    uint16_t ticks = 0;
    unsigned i;
    assert(amiga_directions(held, 0) == 0);
    held[0x4c] = 1;
    assert(amiga_directions(held, 0) == IN_UP);
    held[0x4e] = 1;
    assert(amiga_directions(held, 0) == (IN_UP | IN_RIGHT));
    memset(held, 0, sizeof held);
    assert(amiga_directions(held, 0x300) == IN_LEFT);
    assert(amiga_directions(held, 3) == IN_RIGHT);
    assert(amiga_directions(held, 0x100) == IN_UP);
    assert(amiga_directions(held, 1) == IN_DOWN);
    for (i = 0; i < 500; ++i)
        ticks += amiga_jiffies(50, &fraction);
    assert(ticks == 600 && fraction == 0);
    ticks = 65530;
    for (i = 0; i < 60; ++i)
        ticks += amiga_jiffies(60, &fraction);
    assert(ticks == 54);
    {
        uint16_t previous = 65534;
        assert(amiga_elapsed_ticks(1, &previous) == 3);
        assert(amiga_elapsed_ticks(100, &previous) == 6);
        assert(amiga_elapsed_ticks(100, &previous) == 0);
    }
    /* Same production walker distance after one second on PAL and NTSC. */
    {
        uint8_t pal_x, steps;
        uint16_t now = 0, previous = 0;
        fraction = 0;
        mv_reset(0, 0, DIR_RIGHT);
        for (i = 0; i < 50; ++i) {
            now += amiga_jiffies(50, &fraction);
            steps = amiga_elapsed_ticks(now, &previous);
            while (steps--)
                mv_step(DIR_RIGHT, 0);
        }
        pal_x = mv_x;
        now = previous = 0;
        mv_reset(0, 0, DIR_RIGHT);
        for (i = 0; i < 60; ++i) {
            now += amiga_jiffies(60, &fraction);
            steps = amiga_elapsed_ticks(now, &previous);
            while (steps--)
                mv_step(DIR_RIGHT, 0);
        }
        assert(pal_x > 0 && mv_x == pal_x);
    }
}

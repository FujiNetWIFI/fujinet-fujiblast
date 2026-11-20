#ifndef FUJIBLAST_AMIGA_CONTROLS_H
#define FUJIBLAST_AMIGA_CONTROLS_H
#include <stdint.h>
#include "input.h"
/* Pure decoding helpers, also exercised by native tests. */
static inline uint8_t amiga_directions(const uint8_t *held, uint16_t joy)
{
    uint16_t mix = joy ^ (joy >> 1);
    return ((held[0x4c] || (mix & 0x100)) ? IN_UP : 0) | ((held[0x4d] || (mix & 1)) ? IN_DOWN : 0) |
           ((held[0x4f] || (joy & 0x200)) ? IN_LEFT : 0) |
           ((held[0x4e] || (joy & 2)) ? IN_RIGHT : 0);
}
static inline uint8_t amiga_jiffies(uint8_t hz, uint8_t *fraction)
{
    if (hz == 50 && ++*fraction == 5) {
        *fraction = 0;
        return 2;
    }
    return 1;
}
/* A stalled exchange must not turn into a large, unchecked movement jump. */
static inline uint8_t amiga_elapsed_ticks(uint16_t now, uint16_t *previous)
{
    uint16_t elapsed = (uint16_t)(now - *previous);
    *previous = now;
    return elapsed > 6 ? 6 : (uint8_t)elapsed;
}
#endif

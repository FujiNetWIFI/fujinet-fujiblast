#ifndef AMIGA_PERF_H
#define AMIGA_PERF_H
#include <stdint.h>
enum
{
    PERF_RENDER,
    PERF_READ,
    PERF_WRITE,
    PERF_COUNT
};
#ifdef AMIGA_PERF
uint16_t amiga_perf_clock(void);
void amiga_perf_add(unsigned kind, uint16_t start);
void amiga_perf_steps(unsigned requested, unsigned applied);
void amiga_perf_movement(unsigned long *applied, unsigned long *discarded);
#else
#define amiga_perf_clock() 0
#define amiga_perf_add(kind, start) ((void)(start))
#define amiga_perf_steps(requested, applied) ((void)(requested))
#endif
#endif

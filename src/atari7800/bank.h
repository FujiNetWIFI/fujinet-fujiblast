/* bank.h -- the SuperGame window at $8000-$BFFF (fujiblst-a78.cfg): bank 0
 * is the code that doesn't fit below, 5 and 6 the art. Only bank.c stores
 * to the window (tools/atari7800/mkrom.py checks). */
#ifndef BANK_H
#define BANK_H
#include <stdint.h>
void bank_set(uint8_t n);
void bank_copy(uint16_t dst, uint8_t bank, const uint8_t *src, uint16_t n);
#endif

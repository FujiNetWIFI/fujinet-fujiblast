/* bank.h -- the SuperGame window at $8000-$BFFF (fujiblst-a78.cfg): bank 0
 * is the code that doesn't fit below, 1 the sound, 2 the appkeys, 5 and 6 the art. Only bank.c stores
 * to the window (tools/atari7800/mkrom.py checks). */
#ifndef BANK_H
#define BANK_H
#include <stdint.h>
extern uint8_t bank_cur;     /* the bank in (the NMI restores it) */
void snd_frame(void);       /* the NMI's: the sound step, in bank 1 */
void bank_set(uint8_t n);
void bank_copy(uint16_t dst, uint8_t bank, const uint8_t *src, uint16_t n);
#endif

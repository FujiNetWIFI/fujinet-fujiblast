/*
 * bank.c -- bank.h on the 7800: a store anywhere in the window picks the
 * bank; this file is the only code that stores there. The code that
 * switches is outside the window (the fixed bank), and puts bank 0 back.
 */
#ifdef __ATARI7800__
#include "bank.h"
#include "a78hw.h"

#include <string.h>

void bank_set(uint8_t n)
{
    BANKSEL = n;
}

/* n bytes of bank `bank`'s window into RAM, bank 0 put back: this runs from
 * the fixed bank, which the switch can't pull from under it */
void bank_copy(uint16_t dst, uint8_t bank, const uint8_t *src, uint16_t n)
{
    BANKSEL = bank;
    memcpy((void *)dst, src, n);
    BANKSEL = 0;
}
#endif

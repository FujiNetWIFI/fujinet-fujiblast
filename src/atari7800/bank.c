/*
 * bank.c -- bank.h on the 7800: a store anywhere in the window picks the
 * bank; this file is the only code that stores there. The code that
 * switches is outside the window (the fixed bank), and puts bank 0 back.
 */
#ifdef __ATARI7800__
#include "bank.h"
#include "a78hw.h"

#include <string.h>

/* The bank the main thread has in: set before the store, so the frame's NMI
 * (snd_frame) puts back the right one. */
uint8_t bank_cur;

void bank_set(uint8_t n)
{
    bank_cur = n;
    BANKSEL = n;
}

/* n bytes of bank `bank`'s window into RAM, bank 0 put back: this runs from
 * the fixed bank, which the switch can't pull from under it */
void bank_copy(uint16_t dst, uint8_t bank, const uint8_t *src, uint16_t n)
{
    bank_cur = bank;
    BANKSEL = bank;
    memcpy((void *)dst, src, n);
    bank_cur = 0;
    BANKSEL = 0;
}

/* The lobby's appkeys (appkey.c, built with each name b2_) are bank 2's
 * (OVERLAY2): its code calls only the fixed bank's. The callers have bank 0
 * in (main.c, OVERLAY1), and get it back. */
void b2_appkey_load(void);
void b2_appkey_save_name(void);
void b2_appkey_save_room(void);
void b2_appkey_clear_room(void);

void appkey_load(void)
{
    bank_set(2);
    b2_appkey_load();
    bank_set(0);
}

void appkey_save_name(void)
{
    bank_set(2);
    b2_appkey_save_name();
    bank_set(0);
}

void appkey_save_room(void)
{
    bank_set(2);
    b2_appkey_save_room();
    bank_set(0);
}

void appkey_clear_room(void)
{
    bank_set(2);
    b2_appkey_clear_room();
    bank_set(0);
}

extern void snd_vbi(void);

/* The frame NMI's sound step: the player and the music are in bank 1
 * (SOUND). The NMI can come with any bank in; this one puts it back. */
void snd_frame(void)
{
    BANKSEL = 1;
    snd_vbi();
    BANKSEL = bank_cur;
}
#endif

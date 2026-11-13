/*
 * atari.h -- where the Atari clients keep the chips and the graphics (hw.inc
 * is the assembly's copy). The 5200 has the same ANTIC, GTIA and POKEY at
 * other addresses and its graphics in its 16K RAM (fujiblst-a52.cfg); its
 * BIOS font is at $F800.
 */
#ifndef ATARI_H
#define ATARI_H

#include <stdint.h>

#ifdef __ATARI5200__
#define GTIA_BASE  0xC000
#define POKEY_BASE 0xE800
#define SCREEN_ADDR 0x1800
#define PMG_ADDR   0x3800
#define PMG2_ADDR  0x3000
#define CHB_ROM_PG 0xF8         /* the BIOS font */
#define CHB_MENU_PG 0x1C
#define RTCLOK_ADDR 0x02        /* the crt0's VBI keeps it */
#else
#define GTIA_BASE  0xD000
#define POKEY_BASE 0xD200
#define SCREEN_ADDR 0xA400
#define PMG_ADDR   0xB800
#define PMG2_ADDR  0xB000
#define CHB_ROM_PG 0xE0         /* the OS font */
#define CHB_MENU_PG 0xA0
#define RTCLOK_ADDR 0x14
#endif

#define GTIA_PAL   (*(volatile uint8_t *)(GTIA_BASE + 0x14))
#define GTIA_GRACTL (*(volatile uint8_t *)(GTIA_BASE + 0x1D))
#define POKEY_RANDOM (*(volatile uint8_t *)(POKEY_BASE + 0x0A))

#endif

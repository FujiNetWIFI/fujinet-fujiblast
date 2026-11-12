/*
 * a78hw.h -- FujiBlast's 7800 port: the memory map, and what the display's
 * C and asm share (DESIGN.md).
 */
#ifndef A78HW_H
#define A78HW_H

#include <stdint.h>

#define ROWS     25
#define SCREEN   ((uint8_t *)0x6800)
#define MSTAT    (*(volatile uint8_t *)0x28)
#define RANDOM   (*(volatile uint8_t *)0x045A)
#define BANKSEL  (*(volatile uint8_t *)0x8000)

/* row kinds (the neutral ROW_*, then the HUD) */
#define RK_TEXT  0
#define RK_BIG   1
#define RK_GFX   2
#define RK_ARENA 3
#define RK_HUD   4

#define CT_TEXT  0x4B
#define CT_CELL  0x58
#define NDLI     28
#define LF_LATE  0x80       /* lv_f: the read mode switches late (160 -> 320A) */
#define LF_FRAME 0x40       /* the frame's entry */
#define LF_ANIM  0x01       /* CHARBASE moves with the arena's set */

/* nmi.s */
void mt_start(void);                    /* the clock, DMA on */
void tables_check(void);                /* gfxsetup.c */
void mt_load0(void);                    /* zone 0's DLI values (the tables just installed) */
extern uint8_t lv_cb[NDLI], lv_ca[NDLI], lv_cb2[NDLI], lv_bg[NDLI], lv_f[NDLI];
extern uint8_t dll_off[ROWS], mt_ntsc, mt_top, anim;
extern uint8_t zflip[ROWS], zfront[ROWS];
extern volatile uint8_t dsp_ready;
extern uint8_t mt_dll[];
extern const uint8_t slot_lo[], slot_hi[];

#endif

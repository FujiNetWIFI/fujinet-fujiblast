/*
 * h89.h -- the Heathkit H89's console and clock (h89io.asm), its FujiNet
 * card (fbport.c), and what the H89 files share among themselves.
 */
#ifndef H89_H
#define H89_H

#include <stdint.h>

#define TX_RING 128             /* h89io.asm's TXN */

/* h89io.asm */
uint8_t con_install(void);
void con_remove(void);
void con_put(uint8_t c) __z88dk_fastcall;
uint8_t con_free(void);
uint8_t con_flushed(void);
void con_pump(void);
int kbd_get(void);
uint16_t ticks_now(void);
void os_print(const char *s) __z88dk_fastcall;   /* a '$'-ended string */
void os_exit(void);
extern uint16_t tx_count;       /* bytes sent to the terminal (the rigs read it) */
extern uint8_t tx_held;         /* an XOFF has come */

/* fbport.c: the card's 82C55, polled */
#define PPI_A    0xD0
#define PPI_C    0xD2
#define PPI_CTRL 0xD3
void fb_putc(uint8_t c);
uint8_t fb_ready(void);
uint8_t fb_getc(void);
void fb_sent(void);
uint8_t fb_stuck(void);         /* the card stopped taking bytes */
uint8_t plat_net_probe(void);   /* 1: an 82C55 is there */

/* gfx.c: what changed, out to the terminal */
void gfx_sync(void);
extern uint8_t gfx_in_arena;    /* the arena is up: arrows latch */

/* input.c: an arrow latched stops (a new screen) */
void input_stop(void);

/* plat.c: the counters a rig reads */
extern uint16_t h89_passes, h89_skip, h89_drawn;
extern uint8_t h89_stall;

#endif

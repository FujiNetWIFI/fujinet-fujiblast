/* fbcoco.h -- what the CoCo's modules share (the CoCo 1/2 and, with COCO3,
 * the CoCo 3). */
#ifndef FBCOCO_H
#define FBCOCO_H

#include <stdint.h>

/* bank.c */
const char *bank_init(void);            /* all RAM, the high core and art loaded; 0 or a message */
/* plat.c */
void plat_irq_init(void);               /* the IRQ: the field sync */
extern uint8_t kb_rows[8], kb_prev[8];  /* the keyboard matrix, by column (1 = down) */
extern uint8_t kb_q[8], kb_head, kb_tail;
/* sound.c */
void snd_tick(void);                    /* once a pass: what the sound does between frames */
void snd_gate(uint8_t on);               /* the DAC and the mux for sound (0: for the joystick reads) */
extern uint16_t snd_slice;              /* cycles a jiffy the sound may take */
/* gfx.c, gfx3.c */
void gfx_swap_colours(void);            /* the artifact colours' phase, or RGB <-> composite */
void gfx_clear(void);
void gfx_bar(void);                     /* a bar in the first artifact colour: the start-up question */
void gfx_boot_text(const char *s);      /* a message on the text screen, before the graphics */

#endif

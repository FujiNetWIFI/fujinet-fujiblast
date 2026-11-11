/*
 * msdos.h -- what src/msdos's files share.
 */
#ifndef MSDOS_H
#define MSDOS_H

#include <stdint.h>

/* plat.c: the machine */
enum { PAL_CGA, PAL_EGA, PAL_PCJR, PAL_TANDY };     /* who sets the palette */
extern uint8_t plat_pcjr;       /* F000:FFFE is FDh */
extern uint8_t plat_pal;        /* PAL_*: a CGA by INT 10h AH=0Bh; the others'
                                 * palette registers by AH=10h too */
extern uint8_t plat_drew;       /* this pass drew */
uint8_t plat_net_probe(void);   /* FUJINET.SYS is there */
/* (tools/dossim) our PSP, the memory DOS has (paragraphs), B's segment,
 * the largest block left after it */
extern uint16_t plat_mem[4];
void plat_restore(void);

/* gfx.c */
void gfx_render(void);          /* the screen brought up to date (frame_wait) */

/* sound.c */
void snd_isr(void);             /* a jiffy, from the timer interrupt */
void snd_off(void);

/* input.c */
void input_restore(void);

/* nstrans.c, fbport.c */
void nst_close_quiet(void);     /* the socket closed, before quitting */
void fb_off(void);              /* the serial port given back to FUJINET.SYS */
uint8_t fb_busy(void);          /* a reply is on its way */

/* lobby.c */
void lobby_return(void);
void type_in(const char *k);

#endif

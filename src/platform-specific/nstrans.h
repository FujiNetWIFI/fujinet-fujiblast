/* nstrans.h -- the byte link to the server: a stream of COBS frames (on the
 * Atari, FujiNet netstream: src/atari/nstream.s, nsatari.c). */
#ifndef NSTRANS_H
#define NSTRANS_H

#include <stdint.h>

#ifdef __ATARI__
#define nst_buf   ns_fbuf     /* nstream.s */
#define nst_taken ns_taken
#endif

uint8_t nst_open(const char *host, uint16_t port);  /* 1: the stream is on */
void nst_close(void);                               /* stream off (goodbye first) */
void __fastcall__ nst_put(uint8_t c);
/* Take bytes up to a frame's end: 0 none yet, 255 a bad one, else its
 * length (type and payload) in nst_buf. */
uint8_t nst_frame(void);
extern uint8_t nst_buf[];
extern uint8_t nst_taken;       /* bytes taken from the link, mod 256 */
uint8_t nst_lost(void);         /* the link stopped taking bytes */
uint8_t nst_garbled(void);      /* bytes were lost or garbled (clears) */
void nst_pause(void);           /* other bus traffic (appkeys) between these */
void nst_resume(void);

#endif

/*
 * fbport.h -- the wire under fbnet.c: bytes to and from the FujiNet over
 * FUJINET.SYS's serial port, through rings the UART's interrupt fills and
 * empties (fbport.c, uartisr.asm). From M.U.L.E.'s src/fbport.h.
 *
 *   fb_putc(c)   a byte toward the FujiNet (a request is written whole)
 *   fb_ready()   non-zero when a reply byte waits
 *   fb_getc()    that byte
 *   fb_sent()    the request is all put: let it go
 *   fb_release() the port is FUJINET.SYS's again (between our calls)
 *   fb_try(i)    sets up the i-th port that looks like FUJINET.SYS's (0: no more)
 *   fb_keep()    keeps the one that answered
 *   fb_off()     everything back as FUJINET.SYS had it
 */
#ifndef FBPORT_H
#define FBPORT_H

#include <stdint.h>

void fb_putc(uint8_t c);
uint8_t fb_ready(void);
uint8_t fb_getc(void);
void fb_sent(void);
void fb_release(void);
uint8_t fb_try(uint8_t i);
void fb_keep(void);
void fb_off(void);

/* fbnet.c: bytes on the server's socket */
#define NT_LOST 255             /* the socket is gone */
#define NT_DROP 254             /* a read was lost: nt_dropped bytes */
extern uint16_t nt_dropped;
uint8_t nt_open(const char *host, uint16_t port);
void nt_close(void);
uint8_t nt_take(const uint8_t **p, uint8_t *n);  /* the bytes read: 1, else 0, NT_LOST, NT_DROP */
uint8_t nt_put(uint8_t c);      /* a frame's byte */
uint8_t nt_flush(void);         /* the frame is whole: it goes */
void nt_quiesce(void);          /* the call going finished */

#endif

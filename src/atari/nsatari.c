/*
 * nsatari.c -- the link to the server (nstrans.h) on the Atari: FujiNet
 * netstream, TCP, with nstream.s moving the bytes (from M.U.L.E.'s
 * src/atari/nsnet.c).
 */
#ifdef __ATARI__

#include <string.h>
#include "nstrans.h"

/* nstream.s */
void ns_begin(void);
void ns_end(void);
void ns_reset(void);
void __fastcall__ ns_put(uint8_t c);
uint8_t ns_frame(void);
extern uint8_t ns_on, ns_audf3, ns_skctl, ns_err, ns_taken;
extern uint8_t ns_fbuf[];

#define NS_AUDF3 39             /* 19200 baud (NTSC and PAL alike) */
#define NS_FLAGS 0x05           /* TCP, TX clock external (FujiNet's) */
#define NS_SKCTL 0x10           /* the same, in SKCTL: TX external, RX async */
#define SETTLE   10             /* jiffies between the motor off and SIO */

#define RTCLOK (*(volatile uint8_t *)0x14)
#define PAL    (*(volatile uint8_t *)0xD014)
#define DDEVIC (*(volatile uint8_t *)0x0300)
#define DUNIT  (*(volatile uint8_t *)0x0301)
#define DCOMND (*(volatile uint8_t *)0x0302)
#define DSTATS (*(volatile uint8_t *)0x0303)
#define DBUF   (*(volatile uint16_t *)0x0304)
#define DTIMLO (*(volatile uint8_t *)0x0306)
#define DBYT   (*(volatile uint16_t *)0x0308)
#define DAUX1  (*(volatile uint8_t *)0x030A)
#define DAUX2  (*(volatile uint8_t *)0x030B)

static uint8_t open_, paused;

/* FujiNet's $70/$F0: netstream to host:port. The frame is host, 0, flags,
 * AUDF3, padded to 64 (built in ns_fbuf: the stream is off). DAUX1 is the
 * port's high byte. "STOP" turns it off. */
static uint8_t ns_enable(const char *host, uint16_t port)
{
    uint8_t *pl = ns_fbuf;
    uint8_t n = strlen(host);
    if (n > 60)
        n = 60;
    memset(pl, 0, 64);
    memcpy(pl, host, n);
    pl[n + 1] = NS_FLAGS | ((PAL & 0x0E) == 0 ? 0x10 : 0);
    pl[n + 2] = NS_AUDF3;
    DDEVIC = 0x70;
    DUNIT = 1;
    DCOMND = 0xF0;
    DSTATS = 0x80;
    DBUF = (uint16_t)pl;
    DTIMLO = 15;
    DBYT = 64;
    DAUX1 = port >> 8;
    DAUX2 = port & 0xFF;
    asm("jsr $E459");           /* SIOV */
    return DSTATS < 128;
}

uint8_t nst_open(const char *host, uint16_t port)
{
    nst_close();
    ns_audf3 = NS_AUDF3;
    ns_skctl = NS_SKCTL;
    ns_reset();
    if (!ns_enable(host, port))
        return 0;
    ns_begin();
    ns_put(0);                  /* ends whatever the switch-over left at the
                                 * server, so our first frame comes clean */
    open_ = 1;
    return 1;
}

void nst_close(void)
{
    if (open_) {
        ns_end();
        open_ = paused = 0;
        ns_enable("STOP", 0);
    }
}

void __fastcall__ nst_put(uint8_t c)
{
    ns_put(c);
}

uint8_t nst_frame(void)
{
    return ns_frame();
}

uint8_t nst_lost(void)
{
    return (ns_err & 0x20) != 0;    /* FujiNet stopped taking what we send */
}

uint8_t nst_garbled(void)
{
    if (ns_err & 0xD0) {        /* framing, overrun, or a full ring */
        ns_err &= 0x20;
        return 1;
    }
    return 0;
}

/* Off for plain SIO, and back on. FujiNet must see the motor go off before
 * the next command, or it takes the command as the stream's end. */
void nst_pause(void)
{
    uint8_t t, n;
    if (ns_on) {
        ns_end();
        paused = 1;
        for (n = 0; n < SETTLE; ++n)
            for (t = RTCLOK; t == RTCLOK; )
                ;
    }
}

void nst_resume(void)
{
    if (paused) {
        paused = 0;
        ns_begin();
    }
}

#endif

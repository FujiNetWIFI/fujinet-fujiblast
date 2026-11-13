/*
 * nstrans.c -- the link to the server on the NES: an N:TCP socket on the
 * FujiNet cartridge, its mailbox driven by hand, never waited on (the
 * 5200's src/atari5200/nstrans.c on the NES's mailbox, as M.U.L.E.'s
 * src/nes/nstrans.c has it).
 *
 * fujinet-lib's fuji_bus_call writes a request into the mailbox, launches
 * it and spins until the cartridge has the reply: the game would stop for
 * every round trip. Here a request is launched and the mailbox looked at
 * again on the frames that follow.
 *
 * The mailbox (fujinet-firmware pico/nes/firmware/include/fuji_mailbox.h):
 * a register is set by a store to $5500 + its number; each store to $5700
 * adds a byte to the request (each parameter its size, then its value, low
 * byte first; then the data). A sequence number different from the last
 * one acknowledged launches it; when ACKSEQ ($5400) has caught up, the
 * reply is in the 1K window at $5000, its length at $5404. Only plain
 * stores may touch $5500-$57FF (a read-modify-write's dummy write would be
 * an event of its own: tools/nes/checkrom.py looks).
 *
 * Each step is one of: a WRITE of the frames queued (it goes first), else
 * a STATUS, whose reply says how many bytes wait and whether the socket is
 * up, and a READ of exactly that many (never a blind read: a short read
 * drops the bytes in older firmware). The open and the close are
 * fujinet-lib's (a wait, once).
 *
 * The time is the NMI's frame count (60 a second), which counts on while a
 * lib call waits.
 *
 * nst_frame takes the bytes up to a frame's end, undoes the COBS and checks
 * the Fletcher sum.
 */
#ifdef __NES__

#include <string.h>
#include "nstrans.h"
#include "fujinet-network.h"
#include "fujinet-nes.h"
#include "plat.h"

#define MB_REPLY    ((volatile uint8_t *)0x5000)
#define MB_ACKSEQ   (*(volatile uint8_t *)0x5400)
#define MB_ERR      (*(volatile uint8_t *)0x5402)
#define MB_REPLYCMD (*(volatile uint8_t *)0x5403)
#define MB_RXLEN_LO (*(volatile uint8_t *)0x5404)
#define MB_RXLEN_HI (*(volatile uint8_t *)0x5405)

#define MB_REG(r)   (*(volatile uint8_t *)(0x5500 + (r)))
#define MB_TX       (*(volatile uint8_t *)0x5700)

/* the jiffies, 16 bits (the high byte read twice: a carry between the reads) */
static uint16_t jiffies(void)
{
    uint8_t h, l;
    do {
        h = *((volatile uint8_t *)&frame_count + 1);
        l = *(volatile uint8_t *)&frame_count;
    } while (h != *((volatile uint8_t *)&frame_count + 1));
    return (uint16_t)h << 8 | l;
}

#define R_DEVICE   0x00
#define R_CMD      0x01
#define R_NPARAM   0x02
#define R_DATA_RST 0x05
#define R_SEQ      0x10

#define DEV_NET   0x71          /* N1: */
#define C_STATUS  0x53
#define C_READ    0x52
#define C_WRITE   0x57
#define ACK       0x06

#define WAIT_MAX   330          /* frames for a reply (the cart's own is 5 s) */
#define DOWN_AFTER 60           /* statuses saying the socket is shut */
#define WRITE_TRIES 10
#define RMAX 256                /* a read, at most (the server keeps < 192 unacknowledged) */
#define FMAX 136                /* an encoded frame, at most */
#define OMAX 64                 /* a frame we send, at most */
#define QMAX 96                 /* frames flushed, not yet written */

enum { T_IDLE, T_STATUS, T_READ, T_WRITE };

uint8_t nst_buf[FMAX];
uint8_t nst_taken;

static char dev[64];
static uint8_t state, down, wtries, seq, hold, is_open, lost, garbled;
static uint8_t rbuf[RMAX];
static uint16_t rlen, rpos, polled, started, want;
static uint8_t dlen, drem, dzero, dbad;     /* the frame being decoded */
static uint8_t obuf[OMAX], olen, oover;     /* the frame being put */
static uint8_t qbuf[QMAX], qlen;            /* frames flushed, not yet written */
static uint8_t tlen;                        /* how many of them the write going has */

/* A request goes: the registers, the parameter (a 16-bit one, when n16),
 * the data; then the launch. */
static void request(uint8_t cmd, uint8_t n16, uint16_t v, const uint8_t *data, uint8_t n)
{
    MB_REG(R_DATA_RST) = 0;
    MB_REG(R_DEVICE) = DEV_NET;
    MB_REG(R_CMD) = cmd;
    MB_REG(R_NPARAM) = 0;
    if (n16) {
        MB_TX = 2;
        MB_TX = (uint8_t)v;
        MB_TX = (uint8_t)(v >> 8);
        MB_REG(R_NPARAM) = 1;
    }
    while (n--)
        MB_TX = *data++;
    /* the cart's sequence, not ours: a console Reset doesn't reset it */
    seq = MB_ACKSEQ + 1;
    if (!seq)
        seq = 1;
    MB_REG(R_SEQ) = seq;
    started = jiffies();
}

/* 0 not done yet, 1 an ACK, 2 failed (or no answer in time). */
static uint8_t collect(void)
{
    if (MB_ACKSEQ != seq)
        return (uint16_t)(jiffies() - started) < WAIT_MAX ? 0 : 2;
    return (MB_ERR == 0 && MB_REPLYCMD == ACK) ? 1 : 2;
}

static void start_write(void)
{
    request(C_WRITE, 1, tlen, qbuf, tlen);
    state = T_WRITE;
}

/* One look at the mailbox: what finished is taken, and the next call
 * started. */
static void step(void)
{
    uint8_t r;
    uint16_t k;
    volatile uint8_t *s;

    if (state != T_IDLE) {
        r = collect();
        if (r == 0)
            return;                         /* still going */
        switch (state) {
        case T_STATUS:
            state = T_IDLE;
            if (r != 1)
                break;
            if (MB_REPLY[2])                /* connected */
                down = 0;
            else if (down < 255)
                ++down;
            want = MB_REPLY[0] | (uint16_t)MB_REPLY[1] << 8;
            if (want && !hold) {
                if (want > RMAX)
                    want = RMAX;
                request(C_READ, 1, want, 0, 0);
                state = T_READ;
                return;
            }
            break;
        case T_READ:
            state = T_IDLE;
            if (r == 1) {
                k = MB_RXLEN_LO | (uint16_t)MB_RXLEN_HI << 8;
                if (k > RMAX)
                    k = RMAX;
                s = MB_REPLY;
                for (rlen = 0; rlen < k; ++rlen)
                    rbuf[rlen] = s[rlen];
                rpos = 0;
            } else {
                garbled = 1;                /* the bytes are gone: the credit goes short */
            }
            return;                         /* (the bytes first) */
        case T_WRITE:
            state = T_IDLE;
            if (r != 1 && ++wtries < WRITE_TRIES) {
                start_write();              /* the same again */
                return;
            }
            if (r != 1)
                lost = 1;
            qlen -= tlen;                   /* (frames flushed meanwhile stay) */
            memmove(qbuf, qbuf + tlen, qlen);
            tlen = 0;
            break;
        }
    }
    if (hold)
        return;
    if (qlen) {
        tlen = qlen;
        wtries = 0;
        start_write();
        return;
    }
    k = jiffies();
    if (polled != k) {                      /* once a frame at most */
        polled = k;
        request(C_STATUS, 0, 0, 0, 0);
        state = T_STATUS;
    }
}

/* What's going finishes (bounded: a reply that never comes times out). */
static void quiesce(void)
{
    hold = 1;                               /* (nothing new starts) */
    while (state != T_IDLE)
        step();
    hold = 0;
}

/* "N:TCP://host:port/": the connection is made here (a wait, once). */
uint8_t nst_open(const char *host, uint16_t port)
{
    char *p = dev, num[6];
    const char *s = "N:TCP://";
    uint8_t i;

    nst_close();
    while (*s)
        *p++ = *s++;
    while (*host && p < dev + sizeof(dev) - 8)
        *p++ = *host++;
    *p++ = ':';
    i = 5;
    num[5] = 0;
    do {
        num[--i] = '0' + port % 10;
        port /= 10;
    } while (port);
    strcpy(p, num + i);
    strcat(p, "/");
    state = T_IDLE;
    rlen = rpos = 0;
    dlen = drem = dzero = dbad = 0;
    olen = oover = qlen = tlen = 0;
    down = lost = garbled = hold = 0;
    nst_taken = 0;
    polled = jiffies() - 1;
    if (network_open(dev, OPEN_MODE_RW, OPEN_TRANS_NONE) != FN_ERR_OK)
        return 0;
    is_open = 1;
    nst_put(0);                     /* ends whatever came before: our first frame is clean */
    return 1;
}

void nst_close(void)
{
    if (is_open) {
        is_open = 0;
        quiesce();
        qlen = tlen = 0;
        network_close(dev);
    }
}

/* A frame is gathered, then queued for the next write at its closing 0. */
void __fastcall__ nst_put(uint8_t c)
{
    if (olen < OMAX)
        obuf[olen++] = c;
    else
        oover = 1;
    if (c)
        return;
    if (!oover && is_open && qlen + olen <= QMAX) {
        memcpy(qbuf + qlen, obuf, olen);
        qlen += olen;
        if (state == T_IDLE)
            step();                         /* it goes now */
    } else if (is_open) {
        lost = 1;                           /* a frame that can't go: the link is out of step */
    }
    olen = oover = 0;
}

/* a + b, mod 255, with no division (256 is 1, mod 255) */
static uint8_t add255(uint8_t a, uint8_t b)
{
    a += b;
    if (a < b)
        ++a;
    return a == 255 ? 0 : a;
}

/* The frame's end: its Fletcher sum checked; its length, or 255. */
static uint8_t frame_end(void)
{
    uint8_t i, o = dlen, s1 = 0, s2 = 0;
    if (dbad || drem || o < 3)
        return 255;
    o -= 2;
    for (i = 0; i < o; ++i) {
        s1 = add255(s1, nst_buf[i]);
        s2 = add255(s2, s1);
    }
    if (s1 != nst_buf[o] || s2 != nst_buf[o + 1])
        return 255;
    return o;
}

/* Bytes up to a frame's end, the COBS undone as they come: a code byte n
 * says n-1 bytes follow, then a 0 (unless n is $FF, or the frame ends). */
uint8_t nst_frame(void)
{
    uint8_t c, n;
    if (rpos >= rlen && is_open)
        step();
    while (rpos < rlen) {
        c = rbuf[rpos++];
        ++nst_taken;
        if (!c) {
            n = dlen || drem || dbad ? frame_end() : 0;
            dlen = drem = dzero = dbad = 0;
            if (n)
                return n;
            continue;
        }
        if (dlen >= FMAX - 1) {
            dbad = 1;           /* too long: wait for the next 0 */
            continue;
        }
        if (!drem) {
            if (dzero)
                nst_buf[dlen++] = 0;
            drem = c - 1;
            dzero = c < 0xFF;
        } else {
            nst_buf[dlen++] = c;
            --drem;
        }
    }
    return 0;
}

uint8_t nst_lost(void)
{
    return is_open && (lost || down >= DOWN_AFTER);
}

uint8_t nst_garbled(void)
{
    uint8_t g = garbled;
    garbled = 0;
    return g;
}

/* The appkeys (fujinet-lib) take the mailbox: what's going first finishes. */
void nst_pause(void)
{
    quiesce();
}

void nst_resume(void)
{
}

#endif

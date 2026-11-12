/*
 * nstrans.c -- the link to the server on the Adam: an N:TCP socket on the
 * FujiNet, its DCB driven by hand, never waited on (from M.U.L.E.'s
 * src/adam/nstrans.c; the frame layer is the Apple II's).
 *
 * fujinet-lib's calls set a DCB going and spin until it finishes, retrying
 * a timeout 20 times, and a RECEIVE the FujiNet leaves unanswered is a
 * five second timeout. Here a call is started and looked at again on the
 * next frame, so the game goes on drawing whatever the bus does.
 *
 * Each step is one of: a WRITE of the frames queued (it goes first), else
 * an AdamNet STATUS, whose device status byte (DCB+20) says whether bytes
 * wait (bit 0) and the socket is up (bit 1), and a READ only when they do:
 * the FujiNet answers nothing to a read with nothing waiting. A read
 * returns a whole packet, whatever length was asked: the server keeps
 * under 192 bytes unacknowledged, so one 256-byte buffer takes it all.
 *
 * nst_frame takes the bytes up to a frame's end, undoes the COBS and
 * checks the Fletcher sum. The open and the close are fujinet-lib's (a
 * wait, once).
 */
#ifdef __ADAM__

#include <string.h>
#include "nstrans.h"
#include "plat.h"
#include "adam.h"
#include "fujinet-network.h"
#include "fujinet-commands.h"

/* The DCB (EOS keeps FEC3h of them at FEC4h, 21 bytes each) */
#define D_STATUS  0             /* the command; 80h and up when done */
#define D_BUF     1
#define D_LEN     3             /* the length; a read's is put back */
#define D_DEV     16
#define D_DEVSTAT 20            /* the device's status byte */

#define C_STATUS  1
#define C_WRITE   3
#define C_READ    4
#define S_OK      0x80

#define NET_DEV   0x09          /* N1: */
#define DS_DATA   0x01          /* bytes wait */
#define DS_CONN   0x02          /* the socket is up */
#define DOWN_AFTER 60           /* statuses saying the socket is shut */
#define WRITE_TRIES 10
#define FMAX 136                /* an encoded frame, at most */
#define OMAX 64                 /* a frame we send, at most */

enum { T_IDLE, T_STATUS, T_READ, T_WRITE };

uint8_t nst_buf[FMAX];
uint8_t nst_taken;

static char dev[72];
static volatile uint8_t *dcb;
static uint8_t state, down, wtries, is_open, lost, garbled;
static uint8_t rbuf[256];
static uint16_t rlen, rpos, polled;
static uint8_t dlen, drem, dzero, dbad;     /* the frame being decoded */
static uint8_t obuf[OMAX], olen, oover;     /* the frame being put */
static uint8_t qbuf[96], qlen;              /* frames flushed, not yet written */
static uint8_t tbuf[1 + 96];                /* the write going: 'W', then the frames */
static uint8_t tlen;

static void start(uint8_t cmd, void *buf, uint16_t len)
{
    dcb[D_BUF] = (uint8_t)(uint16_t)buf;
    dcb[D_BUF + 1] = (uint16_t)buf >> 8;
    dcb[D_LEN] = (uint8_t)len;
    dcb[D_LEN + 1] = len >> 8;
    dcb[D_STATUS] = cmd;            /* (last: this sets it going) */
}

/* What's going finishes (bounded: an unanswered call ends in a timeout). */
static void quiesce(void)
{
    volatile uint16_t *fc = (volatile uint16_t *)&frame_count;
    uint16_t t0 = *fc;
    if (!dcb || state == T_IDLE)
        return;
    while (dcb[D_STATUS] < 0x80 && (uint16_t)(*fc - t0) < 400)
        ;
    state = T_IDLE;
}

static void start_write(void)
{
    start(C_WRITE, tbuf, tlen);
    state = T_WRITE;
}

/* One look at the DCB: what finished is taken, and the next call started. */
static void step(void)
{
    uint8_t s;

    if (state != T_IDLE) {
        s = dcb[D_STATUS];
        if (s < 0x80)
            return;                         /* still going */
        switch (state) {
        case T_STATUS:
            state = T_IDLE;
            if (s != S_OK)
                break;
            s = dcb[D_DEVSTAT];
            if (s & DS_CONN)
                down = 0;
            else if (down < 255)
                ++down;
            if (s & DS_DATA) {
                start(C_READ, rbuf, sizeof(rbuf));
                state = T_READ;
                return;
            }
            break;
        case T_READ:
            state = T_IDLE;
            if (s == S_OK) {
                rlen = dcb[D_LEN] | (uint16_t)dcb[D_LEN + 1] << 8;
                if (rlen > sizeof(rbuf))
                    rlen = sizeof(rbuf);
                rpos = 0;
            } else {
                garbled = 1;
            }
            return;                         /* (the bytes first) */
        case T_WRITE:
            state = T_IDLE;
            if (s != S_OK && ++wtries < WRITE_TRIES) {
                start_write();              /* the same again */
                return;
            }
            if (s != S_OK)
                lost = 1;
            tlen = 0;
            break;
        }
    }
    if (qlen) {
        tbuf[0] = NETCMD_WRITE;
        memcpy(tbuf + 1, qbuf, qlen);
        tlen = qlen + 1;
        qlen = 0;
        wtries = 0;
        start_write();
        return;
    }
    if (polled != frame_count) {            /* once a frame at most */
        polled = frame_count;
        start(C_STATUS, 0, 0);
        state = T_STATUS;
    }
}

uint8_t nst_open(const char *host, uint16_t port)
{
    uint8_t n = *(uint8_t *)0xFEC3, i;
    volatile uint8_t *d = (volatile uint8_t *)0xFEC4;
    char *p = dev, num[6];
    const char *s = "N:TCP://";

    nst_close();
    dcb = 0;
    if (!plat_net_probe())
        return 0;                   /* no FujiNet */
    for (i = 0; i < n; ++i, d += 21)
        if ((d[D_DEV] & 0x0F) == NET_DEV) {
            dcb = d;
            break;
        }
    if (!dcb)
        return 0;
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
    down = lost = garbled = 0;
    nst_taken = 0;
    polled = frame_count - 1;
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
    if (!oover && is_open && qlen + olen <= sizeof(qbuf)) {
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
    if (rpos >= rlen && dcb && is_open)
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

/* The appkeys (fujinet-lib) take the bus: what's going first finishes. */
void nst_pause(void)
{
    quiesce();
}

void nst_resume(void)
{
}

#endif

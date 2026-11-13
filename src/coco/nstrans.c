/*
 * nstrans.c -- the link to the server on the CoCo: an N:TCP socket on the
 * FujiNet, over DriveWire (the frame layer is the Apple II's, the bus calls
 * M.U.L.E.'s: src/nstrans_fn.c there).
 *
 * A DriveWire exchange holds the machine with the IRQ masked, so there are
 * as few as can be. fujinet-lib asks the FujiNet "ready?" before each part
 * of a call (three round trips for a status, five for a read, two for a
 * write); here a call is one write and one read: the FujiNet answers its
 * commands in order and keeps a reply until it is asked for, so a command
 * and the fetch of its reply go out together (dw_call). The status (bytes
 * waiting) is asked at most every POLL_EVERY jiffies, at once again while a
 * read left bytes behind. If an answer doesn't come, the FujiNet is asked
 * "ready?" (bus_ready) and the call is tried again next time.
 *
 * The bytes go into a ring; nst_frame takes them up to a frame's end, then
 * undoes the COBS and checks the Fletcher sum (as src/atari/nstream.s
 * does). The server keeps no more than 192 bytes unacknowledged, and we
 * acknowledge only what nst_frame took, so the ring can't overflow.
 */
#include <string.h>
#include "fbcoco.h"
#include "nstrans.h"
#include "plat.h"
#include "fujinet-network.h"

/* fujinet-lib's DriveWire (through the Disk ROM's vectors) */
unsigned char dwread(unsigned char *s, int l);
unsigned char dwwrite(unsigned char *s, int l);
void bus_ready(void);

#define OP_NET          0xE3
#define DW_GET_RESPONSE 0x01
#define DW_SEND_ERROR   0x02

#ifndef POLL_EVERY
#define POLL_EVERY 2
#endif
#define FMAX 136                /* an encoded frame, at most */
#define OMAX 64                 /* a frame we send, at most */
#define RMAX 128                /* bytes read at a time, at most */

uint8_t nst_buf[FMAX];
uint8_t nst_taken;

static char dev[72];
static uint8_t unit;                    /* the N: unit */
static uint8_t dwq[10];
static uint8_t ring[256], rhead, rtail;
static uint8_t dlen, drem, dzero, dbad; /* the frame being decoded */
static uint8_t obuf[OMAX], olen, oover; /* the frame being sent */
static uint8_t is_open, lost, garbled, more;
static uint16_t polled;

/* An N: command (its aux, big-endian as the CoCo is; a write's data after
 * it), then the fetch of its answer (GET_RESPONSE, len bytes, or
 * SEND_ERROR, one), all written at once; the answer in one read. 1: it
 * came; 0: it didn't. */
static uint8_t dw_call(uint8_t cmd, uint16_t aux, uint8_t *data, uint8_t fetch, uint8_t *reply, uint16_t len)
{
    dwq[0] = dwq[5] = OP_NET;
    dwq[1] = dwq[6] = unit;
    dwq[2] = cmd;
    dwq[3] = aux >> 8;
    dwq[4] = aux;
    dwq[7] = fetch;
    dwq[8] = len >> 8;
    dwq[9] = len;
    dwwrite(dwq, 5);
    if (data)
        dwwrite(data, (int)aux);
    dwwrite(dwq + 5, 5);
    if (dwread(reply, (int)len))
        return 1;
    bus_ready();
    return 0;
}

/* bytes waiting, or 0; sets lost when the socket is closed */
static uint16_t waiting(void)
{
    uint8_t sr[4];                      /* bytes waiting (big-endian), connected, error */
    uint16_t bw;
    if (!dw_call('S', 0, 0, DW_GET_RESPONSE, sr, 4))
        return 0;
    bw = ((uint16_t)sr[0] << 8) | sr[1];
    if (!sr[2] && !bw) {
        lost = 1;
        return 0;
    }
    return bw;
}

uint8_t nst_open(const char *host, uint16_t port)
{
    char *p = dev;
    const char *s = "N:TCP://";
    uint8_t n;
    char num[6];
    uint8_t sr[4];
    nst_close();
    while (*s)
        *p++ = *s++;
    while (*host && p < dev + sizeof(dev) - 8)
        *p++ = *host++;
    *p++ = ':';
    n = 5;
    num[5] = 0;
    do {
        num[--n] = '0' + port % 10;
        port /= 10;
    } while (port);
    strcpy(p, num + n);
    strcat(p, "/");
    rhead = rtail = dlen = drem = dzero = dbad = 0;
    olen = oover = lost = garbled = more = 0;
    nst_taken = 0;
    if (network_init() != FN_ERR_OK)
        return 0;
    unit = network_unit(dev);
    if (network_open(dev, OPEN_MODE_RW, OPEN_TRANS_NONE) != FN_ERR_OK)
        return 0;
    /* a failed open answers fine here: the status tells */
    if (!dw_call('S', 0, 0, DW_GET_RESPONSE, sr, 4) || !sr[2]) {
        network_close(dev);
        return 0;
    }
    is_open = 1;
    polled = frame_count;
    nst_put(0);                 /* ends whatever came before: our first frame is clean */
    return 1;
}

void nst_close(void)
{
    if (is_open) {
        is_open = 0;
        network_close(dev);
    }
}

/* Gathered in obuf (no bus call comes between a frame's bytes), then sent
 * in one write at the frame's end. */
void nst_put(uint8_t c)
{
    uint8_t e = 0;
    if (olen < OMAX)
        obuf[olen++] = c;
    else
        oover = 1;
    if (c)
        return;
    if (!oover && is_open) {
        /* how it went: 1, the FujiNet's SUCCESS */
        if (!dw_call('W', olen, obuf, DW_SEND_ERROR, &e, 1) || e != 1)
            garbled = 1;
    }
    olen = oover = 0;
}

/* Fill the ring from the socket (a status, then a read if bytes wait). */
static void poll(void)
{
    static uint8_t tmp[RMAX];
    uint16_t bw;
    uint8_t room, n, i;
    if (!is_open || (!more && (uint16_t)(frame_count - polled) < POLL_EVERY))
        return;
    polled = frame_count;
    more = 0;
    bw = waiting();
    if (!bw)
        return;
    room = 255 - (uint8_t)(rhead - rtail);
    if (room > RMAX)
        room = RMAX;
    if (!room)
        return;
    n = bw > room ? room : (uint8_t)bw;
    if (bw > n)
        more = 1;
    if (!dw_call('R', n, 0, DW_GET_RESPONSE, tmp, n)) {
        /* the FujiNet took them from the socket all the same: they are
         * lost to us, and the server's credit is short for good */
        garbled = 1;
        return;
    }
    for (i = 0; i < n; ++i)
        ring[rhead++] = tmp[i];
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
    poll();
    while (rtail != rhead) {
        c = ring[rtail++];
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
    return lost;
}

uint8_t nst_garbled(void)
{
    uint8_t g = garbled;
    garbled = 0;
    return g;
}

void nst_pause(void)
{
}

void nst_resume(void)
{
}

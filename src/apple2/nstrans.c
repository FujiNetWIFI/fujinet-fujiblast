/*
 * nstrans.c -- the link to the server on the Apple II: an N:TCP socket on
 * the FujiNet, over SmartPort.
 *
 * A SmartPort call holds the machine until it is over, so there are as few
 * as can be, each a single bus transaction: fujinet-lib opens the socket,
 * then the status (bytes waiting), the read and the write are sp_status_nw,
 * sp_read_nw and sp_write_nw themselves (the library's network_status
 * sends a CONTROL first). The status is asked at most every POLL_EVERY
 * jiffies, at once again while a read left bytes behind.
 *
 * The bytes go into a ring; nst_frame takes them up to a frame's end, then
 * undoes the COBS and checks the Fletcher sum (as src/atari/nstream.s
 * does). The server keeps no more than 192 bytes unacknowledged, and we
 * acknowledge only what nst_frame took, so the ring can't overflow.
 */
#ifdef __APPLE2__

#include <string.h>
#include "nstrans.h"
#include "plat.h"
#include "fujinet-network.h"

/* fujinet-lib's SmartPort side (exported, but not in its headers) */
extern uint8_t sp_network;
extern uint8_t sp_payload[];
extern uint16_t sp_count;
int8_t sp_status_nw(uint8_t dest, uint8_t statcode);
int8_t sp_read_nw(uint8_t dest, uint16_t len);
int8_t sp_write_nw(uint8_t dest, uint16_t len);
void frame_tick(void);                  /* hgr.s */

#ifndef POLL_EVERY
#define POLL_EVERY 2
#endif
#define FMAX 136                /* an encoded frame, at most */

uint8_t nst_buf[FMAX];
uint8_t nst_taken;

static char dev[72];
static uint8_t ring[256], rhead, rtail;
static uint8_t dlen, drem, dzero, dbad;  /* the frame being decoded */
static uint8_t olen, oover;             /* the frame being sent (in sp_payload) */
static uint8_t is_open, lost, garbled, more;
static uint16_t polled;

/* bytes waiting, or 0; sets lost when the socket is closed */
static uint16_t waiting(void)
{
    if (sp_status_nw(sp_network, 'S'))
        return 0;
    if (!sp_payload[2] && !(sp_payload[0] | sp_payload[1])) {
        lost = 1;
        return 0;
    }
    return sp_payload[0] | (uint16_t)sp_payload[1] << 8;
}

uint8_t nst_open(const char *host, uint16_t port)
{
    char *p = dev;
    const char *s = "N:TCP://";
    uint8_t n;
    char num[6];
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
    if (network_open(dev, OPEN_MODE_RW, OPEN_TRANS_NONE) != FN_ERR_OK)
        return 0;
    /* a failed open answers fine here: the status tells */
    if (sp_status_nw(sp_network, 'S') || !sp_payload[2]) {
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

/* Gathered in fujinet-lib's sp_payload (free: no bus call comes between a
 * frame's bytes), then sent in one write at the frame's end. */
void __fastcall__ nst_put(uint8_t c)
{
    if (!(olen & 7))
        frame_tick();
    if (olen < 64)
        sp_payload[olen++] = c;
    else
        oover = 1;
    if (c)
        return;
    if (!oover && is_open && sp_write_nw(sp_network, olen))
        lost = 1;
    olen = oover = 0;
}

/* Fill the ring from the socket (a status, then a read if bytes wait). */
static void poll(void)
{
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
    if (!room)
        return;
    n = bw > room ? room : (uint8_t)bw;
    if (bw > n)
        more = 1;
    if (sp_read_nw(sp_network, n)) {
        garbled = 1;
        return;
    }
    if (sp_count && sp_count < n)
        n = (uint8_t)sp_count;  /* only what it says it brought */
    for (i = 0; i < n; ++i)
        ring[rhead++] = sp_payload[i];
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
    frame_tick();
    poll();
    while (rtail != rhead) {
        c = ring[rtail++];
        if (!(rtail & 15))
            frame_tick();
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

#endif

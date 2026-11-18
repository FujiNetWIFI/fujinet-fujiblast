/*
 * nstrans.c -- the link to the server on the MSX: an N:TCP socket on the
 * FujiNet cartridge, reached through fujinet-lib's network calls (on the
 * MSX they go by the cartridge's UNAPI ROM; the frame layer is the Adam's).
 *
 * Every call is made and finished on the spot, nothing is ever in flight.
 * nst_put gathers a frame and writes it whole at its closing 0. nst_frame,
 * when the bytes it has are used up, asks the socket once a frame at most:
 * a STATUS, which says how many bytes wait and whether the socket is up,
 * and a READ only when some do (one trip to the cartridge: the lib's
 * network_read would ask for the status again).
 *
 * nst_frame takes the bytes up to a frame's end, undoes the COBS and
 * checks the Fletcher sum. A read that fails still took its bytes from the
 * socket: they count as taken (the server's credit), and the frame they
 * cut is garbled, so net.c asks for the whole state again.
 */
#ifdef __MSX__

#include <string.h>
#include "nstrans.h"
#include "plat.h"
#include "msx.h"
#include "fujinet-network.h"
#include "fujinet-bus.h"

#define DOWN_AFTER 60           /* statuses saying the socket is shut */
#define WRITE_TRIES 3
#define FMAX 136                /* an encoded frame, at most */
#define OMAX 64                 /* a frame we send, at most */
#define RBUF 256                /* the most a read takes */

uint8_t nst_buf[FMAX];
uint8_t nst_taken;

static char dev[72];
static uint8_t devid;                       /* the socket's bus device, for reads */
static uint8_t down, is_open, lost, garbled;
static uint8_t rbuf[RBUF];
static uint16_t rlen, rpos, polled;
static uint8_t dlen, drem, dzero, dbad;     /* the frame being decoded */
static uint8_t obuf[OMAX], olen, oover;     /* the frame being put */

/* A look at the socket, once a frame at most: what waits is read. */
static void poll(void)
{
    uint16_t bw, n;
    uint8_t conn, err;

    if (polled == frame_count)
        return;
    polled = frame_count;
    if (network_status(dev, &bw, &conn, &err) != FN_ERR_OK)
        return;
    if (conn)
        down = 0;
    else if (down < 255)
        ++down;
    if (!bw)
        return;
    n = bw < RBUF ? bw : RBUF;
    if (network_bus_read(devid, rbuf, n) != n) {
        garbled = 1;                /* (the FujiNet took them from the socket all the same) */
        return;
    }
    rlen = n;
    rpos = 0;
}

uint8_t nst_open(const char *host, uint16_t port)
{
    uint8_t i;
    char *p = dev, num[6];
    const char *s = "N:TCP://";

    nst_close();
    if (!plat_net_probe())
        return 0;                   /* no FujiNet */
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
    rlen = rpos = 0;
    dlen = drem = dzero = dbad = 0;
    olen = oover = 0;
    down = lost = garbled = 0;
    nst_taken = 0;
    if (network_open(dev, OPEN_MODE_RW, OPEN_TRANS_NONE) != FN_ERR_OK)
        return 0;
    devid = FUJI_DEVICEID_NETWORK + network_unit(dev) - 1;
    is_open = 1;
    polled = frame_count - 1;
    nst_put(0);                     /* ends whatever came before: our first frame is clean */
    return 1;
}

void nst_close(void)
{
    if (is_open) {
        is_open = 0;
        network_close(dev);
    }
}

/* A frame is gathered, then written whole at its closing 0. */
void __fastcall__ nst_put(uint8_t c)
{
    uint8_t t;

    if (olen < OMAX)
        obuf[olen++] = c;
    else
        oover = 1;
    if (c)
        return;
    if (is_open) {
        if (oover) {
            lost = 1;                       /* a frame that can't go: the link is out of step */
        } else {
            for (t = 0; t < WRITE_TRIES; ++t)
                if (network_write(dev, obuf, olen) == FN_ERR_OK)
                    break;
            if (t == WRITE_TRIES)
                lost = 1;
        }
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
        poll();
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

/* every call is made and finished on the spot: nothing to wait for */
void nst_pause(void)
{
}

void nst_resume(void)
{
}

#endif

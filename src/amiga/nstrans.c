/* NIO TCP session, retaining the common game's COBS/Fletcher protocol.
 * No raw serial ownership: fujinet-nio.device brokers every exchange.
 */
#include <stdint.h>
#include <fujinet-nio.h>
#include "nstrans.h"
#include "plat.h"
#include "perf.h"
#ifdef AMIGA_ASYNC
#include "worker.h"
#endif
#define FMAX 136
#define OMAX 64
#define RBUF 128
uint8_t nst_buf[FMAX], nst_taken;
#ifndef AMIGA_ASYNC
static fn_handle_t handle;
#endif
static uint32_t roff, woff;
static uint8_t is_open, lost, eof, quiet, garbled;
static uint8_t rbuf[RBUF], obuf[OMAX], olen, oover;
static uint16_t rlen, rpos, polled;
static uint8_t dlen, drem, dzero, dbad;
static void poll(void)
{
#ifdef AMIGA_ASYNC
    /* Bound foreground parsing and retain modulo-256 credit accounting: at
     * most 128 new bytes before net_poll can return its consumed credit. */
    if (polled == frame_count) return;
    polled = frame_count;
    rpos = 0;
    rlen = amiga_tcp_read(rbuf, sizeof rbuf);
    if (!rlen && amiga_tcp_lost()) lost = 1;
#else
    uint16_t got = 0;
    uint8_t flags = 0;
    uint8_t rc;
    if (eof || lost || (uint16_t)(frame_count - polled) < (quiet ? 3 : 1))
        return;
    polled = frame_count;
    {
        uint16_t start = amiga_perf_clock();
        rc = fn_read(handle, roff, rbuf, sizeof rbuf, &got, &flags);
        amiga_perf_add(PERF_READ, start);
    }
    if (rc == FN_ERR_NOT_READY || rc == FN_ERR_BUSY) {
        quiet = 1;
        return;
    }
    if (rc != FN_OK || got > sizeof rbuf) {
        lost = 1;
        return;
    }
    rpos = 0;
    rlen = got;
    roff += got;
    quiet = !got;
    if (flags & FN_READ_EOF)
        eof = 1;
#endif
}
uint8_t nst_open(const char *host, uint16_t port)
{
    nst_close();
    roff = woff = 0;
    rpos = rlen = 0;
    dlen = drem = dzero = dbad = 0;
    olen = oover = lost = eof = quiet = garbled = nst_taken = 0;
#ifdef AMIGA_ASYNC
    if (!amiga_tcp_open(host, port))
#else
    if (fn_tcp_open(&handle, host, port) != FN_OK)
#endif
        return 0;
    is_open = 1;
    polled = frame_count - 1;
    nst_put(0);
    return !lost;
}
void nst_close(void)
{
    if (is_open) {
#ifdef AMIGA_ASYNC
        amiga_tcp_close();
#else
        fn_close(handle);
#endif
    }
    is_open = 0;
}
void nst_put(uint8_t c)
{
#ifndef AMIGA_ASYNC
    uint16_t written = 0;
    uint8_t rc;
#endif
    if (olen < OMAX)
        obuf[olen++] = c;
    else
        oover = 1;
    if (c)
        return;
    if (is_open && !lost) {
        if (oover) {
            lost = 1;
#ifdef AMIGA_ASYNC
            amiga_tcp_note(FB_NET_FRAME_OVERFLOW);
#endif
        }
        else {
#ifdef AMIGA_ASYNC
            if (!amiga_tcp_send(obuf, olen)) lost = 1;
#else
            uint16_t start = amiga_perf_clock();
            rc = fn_write(handle, woff, obuf, olen, &written);
            amiga_perf_add(PERF_WRITE, start);
            woff += written;
            /* The library handles partial writes. An incomplete/error return
             * cannot be retried as a whole frame without duplicating bytes. */
            if (rc != FN_OK || written != olen)
                lost = 1;
#endif
        }
    }
    olen = oover = 0;
    quiet = 0;
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
#ifdef AMIGA_ASYNC
            if (n == 255) amiga_tcp_note(FB_NET_BAD_FRAME);
#endif
            if (n)
                return n;
            continue;
        }
        if (dlen >= FMAX - 1) {
            dbad = 1; /* too long: wait for the next 0 */
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
#ifdef AMIGA_ASYNC
    return is_open && rpos == rlen && (lost || amiga_tcp_lost());
#else
    return is_open && (lost || (eof && rpos == rlen));
#endif
}
uint8_t nst_garbled(void)
{
    uint8_t g = garbled;
    garbled = 0;
    return g;
}
void nst_pause(void) {}
void nst_resume(void) {}

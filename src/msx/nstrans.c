/*
 * nstrans.c -- the link to the server on the MSX: an N:TCP socket on the
 * FujiNet cartridge, spoken to in FujiBus packets by hand, never waited on
 * (the packets from M.U.L.E.'s src/nstrans_fb.c; the frame layer is the
 * Adam's).
 *
 * fujinet-lib's fuji_bus_call writes a request, then spins until the reply
 * has come in whole: the game would stop for every round trip. Here a
 * request is written whole and its reply gathered from the cartridge's
 * registers (msx.h) on the frames that follow.
 *
 * Each step is one of: a WRITE of the frames queued (it goes first), else
 * a STATUS, whose reply says how many bytes wait and whether the socket is
 * up, and a READ only when some do.
 *
 * A packet: device, command, length (16 bits, the header's 6 bytes
 * included), a checksum of it all (taken with itself 0), a field
 * descriptor, then the fields and data; SLIP-framed on the wire. The reply
 * is the same, with ACK (06h) for the command.
 *
 * nst_frame takes the bytes up to a frame's end, undoes the COBS and
 * checks the Fletcher sum. The open and the close are fujinet-lib's (a
 * wait, once); before anything else uses the bus (appkeys), nst_pause lets
 * the call going finish.
 */
#ifdef __MSX__

#include <string.h>
#include "nstrans.h"
#include "plat.h"
#include "msx.h"
#include "fujinet-network.h"

#define DEV_NET   0x71          /* N1: */
#define C_STATUS  0x53
#define C_READ    0x52
#define C_WRITE   0x57
#define F_REPLY   0x10
#define F_B12_D   0x0D          /* a 16-bit field, then data */
#define F_B12_R   0x15          /* a 16-bit field; data back */
#define ACK       0x06

#define SL_END    0xC0
#define SL_ESC    0xDB
#define SL_ESCEND 0xDC
#define SL_ESCESC 0xDD

#define WAIT_MAX   90           /* frames for a reply before it's given up */
#define DOWN_AFTER 60           /* statuses saying the socket is shut */
#define WRITE_TRIES 10
#define FMAX 136                /* an encoded frame, at most */
#define OMAX 64                 /* a frame we send, at most */
#define RBUF 256                /* the most a read takes */
#define QBUF 96                 /* frames flushed, not yet written */

enum { T_IDLE, T_STATUS, T_READ, T_WRITE };

uint8_t nst_buf[FMAX];
uint8_t nst_taken;

static char dev[72];
static uint8_t state, down, wtries, is_open, lost, garbled, hold;
static uint8_t rbuf[RBUF];
static uint16_t rlen, rpos, polled, started, want;
static uint8_t dlen, drem, dzero, dbad;     /* the frame being decoded */
static uint8_t obuf[OMAX], olen, oover;     /* the frame being put */
static uint8_t qbuf[QBUF], qlen;            /* frames flushed, not yet written */
static uint8_t tbuf[2 + QBUF];              /* the write going: its length, the frames */
static uint8_t tlen;

/* the reply coming in */
static uint8_t hdr[6], sbuf[4];
static uint8_t *dst;
static uint16_t dmax, got, sum;
static uint8_t inframe, esc;
static volatile uint8_t junk;   /* request's drain: see there */

static uint16_t fold(uint16_t chk, const uint8_t *p, uint8_t n)
{
    while (n--) {
        chk += *p++;
        chk = (chk >> 8) + (chk & 0xFF);
    }
    return chk;
}

static void out(uint8_t c)
{
    if (c == SL_END) {
        FB_PUTC = SL_ESC;
        c = SL_ESCEND;
    } else if (c == SL_ESC) {
        FB_PUTC = SL_ESC;
        c = SL_ESCESC;
    }
    FB_PUTC = c;
}

/* A request goes, whole; its reply's data will land in to[0..max). */
static void request(uint8_t cmd, uint8_t fields, const uint8_t *data, uint8_t n,
                    uint8_t *to, uint16_t max)
{
    uint8_t h[6], i;
    dst = to;
    dmax = max;
    got = sum = 0;
    inframe = 1;                    /* (its first END may be lost) */
    esc = 0;
    started = frame_count;
    h[0] = DEV_NET;
    h[1] = cmd;
    h[2] = 6 + n;
    h[3] = 0;
    h[4] = 0;
    h[5] = fields;
    h[4] = (uint8_t)fold(fold(0, h, 6), data, n);
    /* A late reply to something gone, or the last END of the one before
     * (judged on its length): 255 bytes at most. Each byte is kept
     * somewhere: zsdcc drops a read whose value goes nowhere (here the
     * register itself), and the drain would never end. */
    i = 0;
    while ((FB_STATUS & 0x80) && ++i)
        junk = FB_GETC;
    FB_PUTC = SL_END;
    for (i = 0; i < 6; ++i)
        out(h[i]);
    for (i = 0; i < n; ++i)
        out(data[i]);
    FB_PUTC = SL_END;
}

/* The reply, judged once its header's length is in (its last END isn't
 * waited for: it may be lost): 1 an ACK, 2 bad (or a NAK). */
static uint8_t judge(void)
{
    inframe = 0;
    if (got != (hdr[2] | (uint16_t)hdr[3] << 8) || (uint8_t)sum != hdr[4]
        || hdr[0] != DEV_NET)
        return 2;
    return hdr[1] == ACK ? 1 : 2;
}

/* The reply's bytes, as many as wait: 0 not all here yet, else judge's. */
static uint8_t collect(void)
{
    uint8_t c, n = 0;
    while (FB_STATUS & 0x80) {
        c = FB_GETC;
        if (c == SL_END) {
            if (!inframe || !got) {
                inframe = 1;        /* (the opening END) */
                continue;
            }
            return judge();         /* short of its length: bad */
        }
        if (!inframe)
            continue;
        if (esc) {
            esc = 0;
            if (c == SL_ESCEND)
                c = SL_END;
            else if (c == SL_ESCESC)
                c = SL_ESC;
        } else if (c == SL_ESC) {
            esc = 1;
            continue;
        }
        if (got < 6)
            hdr[got] = c;
        else if (got - 6 < dmax)
            dst[got - 6] = c;
        sum += got == 4 ? 0 : c;    /* (taken with the checksum 0) */
        sum = (sum >> 8) + (sum & 0xFF);
        ++got;
        if (got >= 6 && got == (hdr[2] | (uint16_t)hdr[3] << 8))
            return judge();
        if (++n == 0)
            break;                  /* 255 a call is plenty */
    }
    return 0;
}

static void start_status(void)
{
    request(C_STATUS, F_REPLY, 0, 0, sbuf, sizeof(sbuf));
    state = T_STATUS;
}

static void start_read(void)
{
    uint8_t l[2];
    l[0] = (uint8_t)want;
    l[1] = want >> 8;
    request(C_READ, F_B12_R, l, 2, rbuf, sizeof(rbuf));
    state = T_READ;
}

static void start_write(void)
{
    request(C_WRITE, F_B12_D, tbuf, tlen, 0, 0);
    state = T_WRITE;
}

/* One look at the wire: what finished is taken, and the next call started. */
static void step(void)
{
    uint8_t r;

    if (state != T_IDLE) {
        r = collect();
        if (r == 0 && (uint16_t)(frame_count - started) < WAIT_MAX)
            return;                         /* still going */
        switch (state) {
        case T_STATUS:
            state = T_IDLE;
            if (r != 1)
                break;
            if (sbuf[2])                    /* connected */
                down = 0;
            else if (down < 255)
                ++down;
            want = sbuf[0] | (uint16_t)sbuf[1] << 8;
            if (want && !hold) {
                if (want > sizeof(rbuf))
                    want = sizeof(rbuf);
                start_read();
                return;
            }
            break;
        case T_READ:
            state = T_IDLE;
            if (r == 1) {
                rlen = got - 6;
                if (rlen > sizeof(rbuf))
                    rlen = sizeof(rbuf);
                rpos = 0;
            } else {
                garbled = 1;                /* (the FujiNet took them from the socket all the same) */
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
            tlen = 0;
            break;
        }
    }
    if (hold)
        return;
    if (qlen) {
        tbuf[0] = qlen;
        tbuf[1] = 0;
        memcpy(tbuf + 2, qbuf, qlen);
        tlen = qlen + 2;
        qlen = 0;
        wtries = 0;
        start_write();
        return;
    }
    if (polled != frame_count) {            /* once a frame at most */
        polled = frame_count;
        start_status();
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

/* The appkeys (fujinet-lib) take the bus: what's going first finishes. */
void nst_pause(void)
{
    quiesce();
}

void nst_resume(void)
{
}

#endif

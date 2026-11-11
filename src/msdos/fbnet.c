/*
 * fbnet.c -- the bytes on the server's socket (an N:TCP on the FujiNet),
 * as FujiBus packets on FUJINET.SYS's serial port, put together here and
 * never waited on. From M.U.L.E.'s src/nstrans_fb.c.
 *
 * fujinet-lib's calls write a request through FUJINET.SYS, then spin until
 * the reply is in: the game would stop for every round trip. Here a
 * request is written and its reply gathered from the wire (fbport.c) on
 * the passes that follow.
 *
 * Each step is one of: a WRITE of the frames nstrans.c has finished, a
 * STATUS (how many bytes wait, and is the socket up) every POLL_EVERY
 * jiffies, and a READ only when some do (FB_RBUF at most; when it is
 * full, the next status is at once). A write and a status take turns: at 9600 bps
 * a write is about 26 ms, and the moves FujiBlast sends up to 20 times a
 * second would starve the reads. A move still waiting at the end of the
 * queue is replaced by a newer one: only the latest says where we are.
 *
 * A packet: device, command, length (16 bits, the header's 6 bytes
 * included), a checksum of it all (taken with itself 0), a field
 * descriptor, then the fields and data; SLIP-framed on the wire. The reply
 * is the same, with ACK (06h) for the command.
 *
 * The open and the close are fujinet-lib's (a wait, once). Before anything
 * else uses the bus (appkeys), nt_quiesce lets the call going finish. The
 * port is found at the first open: a STATUS on each candidate (fb_try),
 * and the first to answer is kept. If none does, the same calls go through
 * fujinet-lib instead, each one waited on (lib), and the socket is asked
 * only every LIB_EVERY passes.
 */
#ifdef __MSDOS__

#include <string.h>
#include "plat.h"
#include "proto.h"
#include "msdos.h"
#include "fbport.h"
#include "fujinet-network.h"
#include "fujinet-bus.h"

#define DEV_NET   0x71          /* N1: */
#define C_STATUS  0x53
#define C_READ    0x52
#define C_WRITE   0x57
#define C_RESEND  0x05          /* fujinet-pc-rs232: the last reply again
                                 * (elsewhere a NAK, at once) */
#define F_REPLY   0x10
#define F_B12_D   0x0D          /* a 16-bit field, then data */
#define F_B12_R   0x15          /* a 16-bit field; data back */
#define ACK       0x06

#define SL_END    0xC0
#define SL_ESC    0xDB
#define SL_ESCEND 0xDC
#define SL_ESCESC 0xDD

#define WAIT_MAX    90          /* jiffies for a reply before it's given up */
#define DOWN_AFTER  60          /* statuses saying the socket is shut */
#define WRITE_TRIES 10
#define FB_RESENDS  2           /* asks for a garbled reply again */
#define FB_RBUF     128         /* the most a read takes */
#define FB_QBUF     64          /* finished frames waiting */
#define PROBE_WAIT  30          /* jiffies for a candidate's answer */
#define LIB_EVERY   6           /* jiffies between statuses through the lib */
#define POLL_EVERY  4           /* and on our own wire (the server pushes at
                                 * most every 3; each status costs an 8088
                                 * dearly) */

enum { T_IDLE, T_STATUS, T_READ, T_WRITE };

uint16_t nt_dropped;
uint8_t fb_cnt[4];              /* replies bad, never came, resends, reads lost (tools/dossim) */

static char dev[72];
static uint8_t state, down, wtries, resends, dropped, hold, wrote;
static uint8_t rbuf[FB_RBUF];
static uint16_t rlen, rpos, polled, started, want;
static uint8_t fbuf[32];        /* the frame being put */
static uint8_t flen, fover;
static uint8_t qbuf[FB_QBUF], qlen;     /* finished frames, not yet written */
static uint8_t qmove = 0xFF;    /* where the last of them starts, if a move */
static uint8_t tbuf[2 + FB_QBUF];       /* the write going: its length, the frames */
static uint8_t tlen;

/* the reply coming in (fbasm.asm's fb_collect fills them) */
uint8_t fbc_hdr[6];
uint8_t *fbc_dst;
uint16_t fbc_dmax, fbc_got, fbc_sum;
uint8_t fbc_inframe, fbc_esc;
#define hdr fbc_hdr
#define dst fbc_dst
#define dmax fbc_dmax
#define got fbc_got
#define sum fbc_sum
#define inframe fbc_inframe
#define esc fbc_esc
uint8_t fb_collect(void);
#pragma aux fb_collect "*_" parm [] value [al] modify [ax bx cx dx]
void fb_request(uint16_t cmd_fields, const uint8_t *data, uint16_t n);
#pragma aux fb_request "*_" parm [ax] [si] [cx] modify [ax bx cx dx si]
extern volatile uint8_t fb_rhead, fb_rtail;     /* uartisr.asm */
#pragma aux fb_rhead "_*"
#pragma aux fb_rtail "_*"
static uint8_t sbuf[4];

static uint8_t found, lib, libr;

uint8_t fb_busy(void)
{
    return state != T_IDLE && !lib;
}

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
        fb_putc(SL_ESC);
        c = SL_ESCEND;
    } else if (c == SL_ESC) {
        fb_putc(SL_ESC);
        c = SL_ESCESC;
    }
    fb_putc(c);
}

/* No wire of our own: the call through fujinet-lib (FUJINET.SYS), done
 * when it returns. */
static void lib_call(uint8_t cmd, const uint8_t *data, uint8_t n, uint8_t *to)
{
    uint16_t k;
    libr = 2;
    got = 6;
    if (cmd == C_STATUS) {
        if (network_status(dev, (uint16_t *)to, to + 2, to + 3) == FN_ERR_OK)
            libr = 1;
    } else if (cmd == C_READ) {
        k = network_bus_read(DEV_NET, to, want);
        if (k) {
            got = 6 + k;
            libr = 1;
        }
    } else if (cmd == C_WRITE && network_bus_write(DEV_NET, data + 2, n - 2)) {
        libr = 1;
    }
}

/* A request goes, whole; its reply's data will land in to[0..max). */
static void request(uint8_t cmd, uint8_t fields, const uint8_t *data, uint8_t n,
                    uint8_t *to, uint16_t max)
{
    uint8_t h[6], i;
    dst = to;
    dmax = max;
    got = sum = 0;
    inframe = 1;                /* (its first END may be lost) */
    esc = 0;
    started = frame_count;
    if (lib) {
        lib_call(cmd, data, n, to);
        return;
    }
    /* a late reply to something gone, or the last END of the one before
     * (judged on its length): dropped */
    fb_rtail = fb_rhead;
    if (n <= 25) {              /* (escaped, it fits the empty ring) */
        fb_request(cmd | (uint16_t)fields << 8, data, n);
    } else {
        h[0] = DEV_NET;
        h[1] = cmd;
        h[2] = 6 + n;
        h[3] = 0;
        h[4] = 0;
        h[5] = fields;
        h[4] = (uint8_t)fold(fold(0, h, 6), data, n);
        fb_putc(SL_END);
        for (i = 0; i < 6; ++i)
            out(h[i]);
        for (i = 0; i < n; ++i)
            out(data[i]);
        fb_putc(SL_END);
    }
    fb_sent();
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
    if (lib)
        return libr;
    return fb_collect() ? judge() : 0;
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
    uint8_t r, due;

    if (state != T_IDLE) {
        r = collect();
        if (r == 0 && (uint16_t)(frame_count - started) < WAIT_MAX)
            return;             /* still going */
        if (r != 1)
            ++fb_cnt[r == 2 ? 0 : 1];
        if (r != 1 && resends < FB_RESENDS) {
            ++resends;          /* garbled, or cut short */
            ++fb_cnt[2];
            request(C_RESEND, 0, 0, 0, dst, dmax);
            return;
        }
        resends = 0;
        switch (state) {
        case T_STATUS:
            state = T_IDLE;
            if (r != 1)
                break;
            if (sbuf[2])        /* connected */
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
                if (rlen == sizeof(rbuf))
                    polled -= POLL_EVERY;   /* more may wait: ask again at once */
            } else {
                /* the FujiNet took them from the socket all the same: they
                 * are counted as taken, or the server's credit would be
                 * short for good */
                dropped = 1;
                ++fb_cnt[3];
            }
            return;             /* (the bytes first) */
        case T_WRITE:
            state = T_IDLE;
            if (r != 1 && ++wtries < WRITE_TRIES) {
                start_write();  /* the same again */
                return;
            }
            tlen = 0;
            break;
        }
    }
    if (hold)
        return;
    due = (uint16_t)(frame_count - polled) >= (lib ? LIB_EVERY : POLL_EVERY);
    /* a write, unless the last call was one and a status is due */
    if (qlen && !(wrote && due)) {
        tbuf[0] = qlen;
        tbuf[1] = 0;
        memcpy(tbuf + 2, qbuf, qlen);
        tlen = qlen + 2;
        qlen = 0;
        qmove = 0xFF;
        wtries = 0;
        wrote = 1;
        start_write();
        return;
    }
    if (due) {
        polled = frame_count;
        wrote = 0;
        start_status();
    }
}

/* What's going finished (bounded: a reply that never comes times out). */
void nt_quiesce(void)
{
    hold = 1;                   /* (nothing new starts) */
    while (state != T_IDLE)
        step();
    hold = 0;
    fb_release();
}

/* "N:TCP://host:port/": the connection is made here (a wait, once). */
uint8_t nt_open(const char *host, uint16_t port)
{
    char *p = dev, num[6];
    const char *s = "N:TCP://";
    uint8_t n = 5;

    nt_quiesce();
    while (*s)
        *p++ = *s++;
    while (*host && p < dev + sizeof(dev) - 8)
        *p++ = *host++;
    *p++ = ':';
    num[5] = 0;
    do {
        num[--n] = '0' + port % 10;
        port /= 10;
    } while (port);
    strcpy(p, num + n);
    strcat(p, "/");
    rlen = rpos = 0;
    flen = fover = qlen = tlen = 0;
    qmove = 0xFF;
    down = resends = dropped = wrote = 0;
    polled = frame_count - 1;
    if (network_open(dev, OPEN_MODE_RW, OPEN_TRANS_NONE) != FN_ERR_OK)
        return 0;
    if (!found && !lib) {
        uint8_t i, r = 0;
        for (i = 0; r != 1 && fb_try(i); ++i) {
            start_status();
            while (!(r = collect()) && (uint16_t)(frame_count - started) < PROBE_WAIT)
                ;
        }
        state = T_IDLE;
        found = r == 1;
        if (found)
            fb_keep();
        else
            lib = 1;            /* no wire answered: the lib's */
        fb_release();
    }
    return 1;
}

void nt_close(void)
{
    nt_quiesce();
    qlen = tlen = 0;
    network_close(dev);
}

/* The bytes read and not yet taken, all at once (*n of them at *p): 1;
 * else 0 none yet, NT_LOST, NT_DROP. */
uint8_t nt_take(const uint8_t **p, uint8_t *n)
{
    *n = 0;
    if (rpos >= rlen) {
        step();
        if (dropped) {
            dropped = 0;
            nt_dropped = want;
            return NT_DROP;
        }
        if (down >= DOWN_AFTER)
            return NT_LOST;
        if (rpos >= rlen)
            return 0;
    }
    *p = rbuf + rpos;
    *n = (uint8_t)(rlen - rpos);
    rpos = rlen;
    return 1;
}

/* A frame is gathered, then queued for the next write. */
uint8_t nt_put(uint8_t c)
{
    if (flen >= sizeof(fbuf)) {
        fover = 1;
        return 0;
    }
    fbuf[flen++] = c;
    return 1;
}

uint8_t nt_flush(void)
{
    uint8_t ok = 0, move;
    /* (COBS: a frame's first byte is a code, its type the second) */
    move = flen > 1 && fbuf[1] == SC_MOVE;
    if (move && qmove != 0xFF)
        qlen = qmove;           /* an older move, not yet written: this replaces it */
    if (flen && !fover && qlen + flen <= sizeof(qbuf)) {
        qmove = move ? qlen : 0xFF;
        memcpy(qbuf + qlen, fbuf, flen);
        qlen += flen;
        ok = 1;
        if (state == T_IDLE)
            step();             /* it goes now */
    }
    flen = fover = 0;
    return ok;
}

#endif

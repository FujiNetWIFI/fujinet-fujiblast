/*
 * nstrans.c -- the link to the server on the H89: an N:TCP socket on the
 * H89-ESP32 card, spoken to in FujiBus packets by hand, never waited on
 * (the MSX's src/msx/nstrans.c on the card's 82C55: fbport.c).
 *
 * fujinet-lib's fuji_bus_call writes a request, then spins until the reply
 * has come in whole: the game would stop for every round trip. Here a
 * request is written whole and its reply gathered from the card on the
 * passes that follow. The ESP32 waits only a second for each reply byte
 * to be taken, so every pass looks (nst_frame).
 *
 * Each step is one of: a WRITE of the frames queued (it goes first), else
 * a STATUS, whose reply says how many bytes wait and whether the socket is
 * up, and a READ only when some do (older firmware lost the bytes a short
 * READ left behind).
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
#include <string.h>
#include "nstrans.h"
#include "plat.h"
#include "h89.h"
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

#define WAIT_MAX   90           /* jiffies for a reply before it's given up */
#define DOWN_AFTER 60           /* statuses saying the socket is shut */
#define WRITE_TRIES 10
#ifndef STATUS_EVERY
#define STATUS_EVERY 6          /* jiffies between STATUSes, at least (each is a
                                 * round trip through the 82C55: the server
                                 * pushes 20 a second at most anyway) */
#endif
#define FMAX 136                /* an encoded frame, at most */
#define OMAX 64                 /* a frame we send, at most */
#define RBUF 255                /* the most a read takes (8-bit: nsfast.asm) */
#define QBUF 96                 /* frames flushed, not yet written */

enum { T_IDLE, T_STATUS, T_READ, T_WRITE };

uint8_t nst_buf[FMAX];
uint8_t nst_taken;
uint16_t nst_reads, nst_bad;    /* for a rig: READs done, replies bad or late */

static char dev[72];
static uint8_t state, down, wtries, is_open, lost, garbled, hold;
uint8_t rbuf[RBUF];             /* (nsfast.asm decodes it) */
uint8_t rlen, rpos;
static uint16_t polled, started, want;
uint8_t ns_dec[4];              /* the frame being decoded: */
#define dlen  ns_dec[0]         /* its length so far */
#define drem  ns_dec[1]         /* bytes before the next COBS code */
#define dzero ns_dec[2]         /* a 0 comes before the next code */
#define dbad  ns_dec[3]         /* too long: wait for the next 0 */
uint8_t ns_decode(void);        /* nsfast.asm */
static uint8_t obuf[OMAX], olen, oover;     /* the frame being put */
static uint8_t qbuf[QBUF], qlen;            /* frames flushed, not yet written */
static uint8_t tbuf[2 + QBUF];              /* the write going: its length, the frames */
static uint8_t tlen;

/* the reply coming in (nsfast.asm's ns_collect reads it) */
uint8_t ns_hdr[6];
uint8_t *ns_dptr;               /* the data's place, less 6 (the header's) */
uint16_t ns_dend, ns_got;       /* 6 + the room for the data; bytes in */
uint8_t ns_sum, ns_inframe, ns_esc;
static uint8_t sbuf[4];
uint8_t ns_collect(void);                       /* nsfast.asm */
uint8_t ns_fletcher(uint8_t n) __z88dk_fastcall;
uint16_t nst_bytes;             /* for a rig: bytes read */

/* A request goes, whole (nsfast.asm's ns_request); its reply's data will
 * land in to[0..max). */
uint8_t ns_rq[6];
const uint8_t *ns_rq_data;
uint8_t ns_rq_n, ns_stuck;
void ns_request(void);

static void request(uint8_t cmd, uint8_t fields, const uint8_t *data, uint8_t n,
                    uint8_t *to, uint16_t max)
{
    ns_dptr = to - 6;
    ns_dend = max + 6;
    ns_got = 0;
    ns_sum = 0;
    ns_inframe = 1;                 /* (its first END may be lost) */
    ns_esc = 0;
    started = frame_count;
    ns_rq[0] = DEV_NET;
    ns_rq[1] = cmd;
    ns_rq[2] = 6 + n;
    ns_rq[3] = 0;
    ns_rq[4] = 0;
    ns_rq[5] = fields;
    ns_rq_data = data;
    ns_rq_n = n;
    ns_request();
}

/* The reply, judged once its header's length is in (its last END isn't
 * waited for: it may be lost): 1 an ACK, 2 bad (or a NAK). */
static uint8_t judge(void)
{
    ns_inframe = 0;
    if (ns_got != (ns_hdr[2] | (uint16_t)ns_hdr[3] << 8) || ns_sum != ns_hdr[4]
        || ns_hdr[0] != DEV_NET)
        return 2;
    return ns_hdr[1] == ACK ? 1 : 2;
}

/* The reply's bytes, as many as wait (nsfast.asm): 0 not all here yet,
 * else judge's. */
static uint8_t collect(void)
{
    return ns_collect() ? judge() : 0;
}

uint16_t nst_req[3];            /* for a rig: STATUS, READ, WRITE requests */
static void start_status(void)
{
    ++nst_req[0];
    request(C_STATUS, F_REPLY, 0, 0, sbuf, sizeof(sbuf));
    state = T_STATUS;
}

static void start_read(void)
{
    uint8_t l[2];
    l[0] = (uint8_t)want;
    l[1] = want >> 8;
    ++nst_req[1];
    request(C_READ, F_B12_R, l, 2, rbuf, sizeof(rbuf));
    state = T_READ;
}

static void start_write(void)
{
    ++nst_req[2];
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
        if (r != 1)
            ++nst_bad;
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
            ++nst_reads;
            if (r == 1) {
                rlen = ns_got - 6 > sizeof(rbuf) ? sizeof(rbuf) : (uint8_t)(ns_got - 6);
                rpos = 0;
                nst_bytes += rlen;
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
    if ((uint16_t)(frame_count - polled) >= STATUS_EVERY) {
        polled = frame_count;
        start_status();
    }
}

/* What's going finishes (bounded: a reply that never comes times out). */
static void quiesce(void)
{
    hold = 1;                               /* (nothing new starts) */
    while (state != T_IDLE) {
        step();
        con_pump();
    }
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
    polled = frame_count - STATUS_EVERY;
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
void nst_put(uint8_t c)
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

/* The frame's end: its Fletcher sum checked (nsfast.asm); its length, or
 * 255. */
static uint8_t frame_end(void)
{
    uint8_t o = dlen;
    if (dbad || drem || o < 3)
        return 255;
    o -= 2;
    return ns_fletcher(o) ? o : 255;
}

/* Bytes up to a frame's end, the COBS undone as they come (nsfast.asm's
 * ns_decode, up to and with a 0): a code byte n says n-1 bytes follow, then
 * a 0 (unless n is $FF, or the frame ends). */
uint8_t nst_frame(void)
{
    uint8_t n;
    if (rpos >= rlen && is_open)
        step();
    while (rpos < rlen) {
        if (!ns_decode())
            break;                      /* (all taken, no end yet) */
        n = dlen || drem || dbad ? frame_end() : 0;
        dlen = drem = dzero = dbad = 0;
        if (n)
            return n;
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

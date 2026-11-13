/*
 * net.c -- the stream protocol (docs/PROTOCOL.md) over the link (nstrans.h).
 *
 * The server pushes the state as patches to a staged copy, then a commit,
 * when the staged copy becomes st. We send where our player is (SC_MOVE)
 * and what it does (SC_ACT); we drain the link every frame and tell the
 * server what we took (SC_CREDIT), so it never sends more than our ring
 * holds. From M.U.L.E.'s src/nsnet.c.
 */
#include <string.h>
#include "game.h"
#include "nstrans.h"
#include "plat.h"

#define PING_EVERY 300          /* frames between keepalives (each is answered) */
#define DEAD_AFTER 900          /* frames without a frame: the link is gone */
#define ASK_AGAIN  60           /* HELLO unanswered: again (the first after the
                                 * link opens can be lost while FujiNet dials) */
#define RESYNC_GAP 30           /* frames between RESYNCs */

uint8_t st[STATE_SIZE];
static uint8_t stage[STATE_SIZE];
uint8_t tables[MAX_TABLES][TABLE_REC];
uint8_t net_err, net_heard;
uint8_t net_map;                /* a commit since play_state looked changed the map */
static uint8_t stage_map;       /* a patch to the staged state touched the map */
#ifdef PLAT_COMMIT_SPAN
/* (a slow CPU: a commit copies only what the patches since the last one
 * touched, not all 247 bytes; st and stage are the same outside it) */
static uint8_t span_lo, span_hi = STATE_SIZE;
#endif

static uint8_t link, joined, seq, credited, resync_hold, resync_owed;
static uint8_t table_n, table_have;
static uint16_t quiet;          /* passes since a frame came */
static uint16_t q_ping;         /* the same, mod PING_EVERY and ASK_AGAIN (no */
static uint8_t q_ask;           /* division in a pass: cc65's is slow) */

const uint8_t seat_off[SEATS] = { 0, PLAYER_SIZE, 2 * PLAYER_SIZE, 3 * PLAYER_SIZE };
const uint8_t seat_bit[SEATS] = { 1, 2, 4, 8 };
static uint8_t raw[24];

/* a + b, mod 255, with no division (256 is 1, mod 255) */
static uint8_t add255(uint8_t a, uint8_t b)
{
    a += b;
    if (a < b)
        ++a;
    return a == 255 ? 0 : a;
}

/* A frame: COBS of type, payload, Fletcher-16; then 0. */
static void send(uint8_t type, const uint8_t *p, uint8_t n)
{
    uint8_t s1 = 0, s2 = 0, i, from = 0;
    raw[0] = type;
    memcpy(raw + 1, p, n);
    ++n;
    for (i = 0; i < n; ++i) {
        s1 = add255(s1, raw[i]);
        s2 = add255(s2, s1);
    }
    raw[n++] = s1;
    raw[n++] = s2;
    for (i = 0; i <= n; ++i) {
        if (i == n || raw[i] == 0) {
            nst_put(i - from + 1);
            for (; from < i; ++from)
                nst_put(raw[from]);
            ++from;
        }
    }
    nst_put(0);
}

static void send_credit(void)
{
    credited = nst_taken;
    send(SC_CREDIT, &credited, 1);
}

static void resync(void)
{
    resync_owed = 1;
    if (!resync_hold) {
        send(SC_RESYNC, 0, 0);
        resync_hold = RESYNC_GAP;
        resync_owed = 0;
    }
}

uint8_t net_open(void)
{
    if (link)
        return 1;
    joined = 0;
    credited = 0;
    quiet = q_ping = q_ask = 0;
    link = nst_open(srv_host, srv_port);
    return link;
}

static void hello_frame(void)
{
    static uint8_t h[2 + TABLE_LEN + NAME_LEN];
    memset(h, 0, sizeof(h));
    h[0] = PROTO_VERSION;
    strncpy((char *)h + 2, srv_table, TABLE_LEN);
    strncpy((char *)h + 2 + TABLE_LEN, my_name, NAME_LEN);
    send(SC_HELLO, h, sizeof(h));
}

void net_hello(void)
{
    joined = 1;
    net_heard = 0;
    net_err = 0;
    quiet = q_ping = q_ask = 0;
    resync_owed = 0;
    memset(stage, 0, sizeof(stage));
#ifdef PLAT_COMMIT_SPAN
    span_lo = 0;
    span_hi = STATE_SIZE;
#endif
    hello_frame();
}

static void drop(void)
{
    nst_close();
    link = joined = 0;
}

uint8_t net_poll(void)
{
    uint8_t n, p, off, len, r = NET_NONE;
    if (!link)
        return NET_LOST;
    if (nst_lost()) {
        drop();
        return NET_LOST;
    }
    if (resync_hold)
        --resync_hold;
    else if (resync_owed)
        resync();
    while ((n = nst_frame()) != 0) {
        quiet = q_ping = q_ask = 0;
        if (n == 255) {
            resync();
            continue;
        }
        switch (nst_buf[0]) {
        case CS_PATCH:
            for (p = 1; p + 2 <= n; p += len) {
                off = nst_buf[p];
                len = nst_buf[p + 1];
                p += 2;
                if (p + len > n || off + len > STATE_SIZE)
                    break;
                memcpy(stage + off, nst_buf + p, len);
#ifdef PLAT_COMMIT_SPAN
                if (off < span_lo)
                    span_lo = off;
                if (off + len > span_hi)
                    span_hi = off + len;
#endif
                if (off < OFF_MAP + TILES && off + len > OFF_MAP)
                    stage_map = 1;
            }
            break;
        case CS_COMMIT:
            if (joined) {
#ifdef PLAT_COMMIT_SPAN
                if (span_lo < span_hi)
                    memcpy(st + span_lo, stage + span_lo, span_hi - span_lo);
                span_lo = STATE_SIZE;
                span_hi = 0;
#else
                memcpy(st, stage, STATE_SIZE);
#endif
                net_map |= stage_map;
                stage_map = 0;
                net_heard = 1;
                r = NET_STATE;
            }
            break;
        case CS_TABLE:
            if (n >= 3 + TABLE_REC && nst_buf[1] < MAX_TABLES) {
                memcpy(tables[nst_buf[1]], nst_buf + 3, TABLE_REC);
                table_have |= 1 << nst_buf[1];
            }
            if (n >= 3)
                table_n = nst_buf[2] > MAX_TABLES ? MAX_TABLES : nst_buf[2];
            break;
        case CS_ERROR:
            net_err = nst_buf[1];
            return NET_ERROR;
        }
    }
    if ((uint8_t)(nst_taken - credited) >= 48)
        send_credit();
    if (nst_garbled())
        resync();
    ++quiet;
    if (++q_ping == PING_EVERY) {
        q_ping = 0;
        send_credit();
        send(SC_PING, 0, 0);
    }
    if (++q_ask == ASK_AGAIN) {
        q_ask = 0;
        if (joined && !net_heard)
            hello_frame();
    }
    if (quiet > DEAD_AFTER) {
        drop();
        return NET_LOST;
    }
    return r;
}

/* The table list (blocking): asked again until it comes. */
uint8_t net_tables(void)
{
    uint16_t t;
    uint8_t want;
    if (!net_open())
        return 0;
    table_n = 0xFF;
    table_have = 0;
    for (t = 0; t < 600; ++t) {
        if (table_n == 0xFF && t % ASK_AGAIN == 0)
            send(SC_TABLES, 0, 0);
        frame_wait();
        if (net_poll() == NET_LOST)
            return 0;
        if (table_n != 0xFF) {
            want = (uint8_t)((1 << table_n) - 1);
            if ((table_have & want) == want)
                return table_n;
        }
    }
    return 0;
}

void net_move(uint8_t x, uint8_t y, uint8_t dir, uint8_t warp)
{
    uint8_t m[4];
    m[0] = x;
    m[1] = y;
    m[2] = dir;
    m[3] = warp;
    send(SC_MOVE, m, 4);
}

uint8_t net_act(uint8_t act, uint8_t a1, uint8_t a2)
{
    uint8_t a[4];
    if (++seq == 0)
        seq = 1;
    a[0] = seq;
    a[1] = act;
    a[2] = a1;
    a[3] = a2;
    send(SC_ACT, a, 4);
    return seq;
}

void net_leave(void)
{
    if (joined)
        net_act(ACT_LEAVE, 0, 0);
    joined = 0;
}

void net_close(void)
{
    if (link) {
        if (joined)
            send(SC_BYE, 0, 0);
        drop();
    }
}

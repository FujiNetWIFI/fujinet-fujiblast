/*
 * nstrans.c -- the link to the server on the PC: an N:TCP socket on the
 * FujiNet, its bytes carried by fbnet.c over FUJINET.SYS's serial port.
 *
 * nst_put gathers a frame and hands it over whole at its closing 0;
 * nst_frame takes bytes up to a frame's end, undoes the COBS and checks
 * the Fletcher sum (as src/apple2/nstrans.c does). A read lost on the
 * line still took its bytes from the socket: they count as taken (the
 * server's credit), and the frame they cut is garbled, so net.c asks for
 * the whole state again.
 */
#ifdef __MSDOS__

#include <string.h>
#include "nstrans.h"
#include "msdos.h"
#include "fbport.h"
#include "fujinet-network.h"

#define FMAX 136                /* an encoded frame, at most */

uint8_t nst_buf[FMAX];
uint8_t nst_taken;

static uint8_t dlen, drem, dzero, dbad;  /* the frame being decoded */
static uint8_t is_open, lost, garbled, inited;
static const uint8_t *cur;      /* bytes read, not yet looked at */
static uint8_t left;

uint8_t nst_open(const char *host, uint16_t port)
{
    nst_close();
    dlen = drem = dzero = dbad = 0;
    left = 0;
    lost = garbled = 0;
    nst_taken = 0;
    if (!plat_net_probe())
        return 0;               /* no FUJINET.SYS */
    if (!inited) {
        if (network_init() != FN_ERR_OK)
            return 0;
        inited = 1;
    }
    if (!nt_open(host, port))
        return 0;
    is_open = 1;
    nst_put(0);                 /* ends whatever came before: our first frame is clean */
    return 1;
}

void nst_close(void)
{
    if (is_open) {
        is_open = 0;
        nt_close();
    }
}

void nst_close_quiet(void)
{
    nst_close();
}

void nst_put(uint8_t c)
{
    nt_put(c);
    if (!c && is_open)
        nt_flush();
}

/* s += b, mod 255, with no division (256 is 1, mod 255) */
#define ADD255(s, b) do { (s) += (b); if ((s) < (b)) ++(s); if ((s) == 255) (s) = 0; } while (0)

/* The frame's end: its Fletcher sum checked; its length, or 255. */
static uint8_t frame_end(void)
{
    uint8_t i, o = dlen, s1 = 0, s2 = 0, *p = nst_buf;
    if (dbad || drem || o < 3)
        return 255;
    o -= 2;
    for (i = o; i; --i) {
        ADD255(s1, *p);
        ++p;
        ADD255(s2, s1);
    }
    if (s1 != p[0] || s2 != p[1])
        return 255;
    return o;
}

/* Bytes up to a frame's end, the COBS undone: a code byte n says n-1
 * bytes follow (copied as a run), then a 0 (unless n is $FF, or the frame
 * ends). */
#ifdef STATS
extern uint32_t t_net;
uint32_t prof_ticks(void);
static uint8_t frame(void);

uint8_t nst_frame(void)
{
    uint32_t t = prof_ticks();
    uint8_t n = frame();
    t_net += prof_ticks() - t;
    return n;
}

static uint8_t frame(void)
#else
uint8_t nst_frame(void)
#endif
{
    uint8_t c, n, r, k;
    if (!is_open)
        return 0;
    for (;;) {
        if (!left) {
            r = nt_take(&cur, &left);
            if (r == NT_LOST) {
                lost = 1;
                return 0;
            }
            if (r == NT_DROP) {
                nst_taken += (uint8_t)nt_dropped;
                garbled = 1;
                dbad = 1;       /* the frame it cut is no good */
                continue;
            }
            if (!left)
                return 0;
            nst_taken += left;
        }
        while (left) {
            if (drem) {         /* a run of data */
                k = drem < left ? drem : left;
                if (dlen + k > FMAX - 1) {
                    dbad = 1;   /* too long: wait for the next 0 */
                    dlen = 0;
                } else {
                    memcpy(nst_buf + dlen, cur, k);
                    dlen += k;
                }
                cur += k;
                left -= k;
                drem -= k;
                continue;
            }
            c = *cur++;
            --left;
            if (!c) {
                n = dlen || dbad ? frame_end() : 0;
                dlen = dzero = dbad = 0;
                if (n)
                    return n;
                continue;
            }
            if (dzero) {
                if (dlen >= FMAX - 1)
                    dbad = 1;
                else
                    nst_buf[dlen++] = 0;
            }
            drem = c - 1;
            dzero = c < 0xFF;
        }
    }
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

/* other bus traffic (appkeys) between these */
void nst_pause(void)
{
    nt_quiesce();
}

void nst_resume(void)
{
}

#endif

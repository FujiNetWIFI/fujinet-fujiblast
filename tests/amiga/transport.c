#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fujinet-nio.h>
#include "nstrans.h"
uint16_t frame_count;
static uint8_t wire[2048], tx[2048], read_rc, read_flags, write_rc;
static unsigned length, pos, cap = 128, txlen, opens, closes, writes;
static uint16_t short_write = 65535;
static uint32_t expected_read, expected_write;
uint8_t fn_tcp_open(fn_handle_t *h, const char *host, uint16_t port)
{
    assert(!strcmp(host, "test"));
    assert(port == 9910);
    *h = 1;
    opens++;
    expected_read = expected_write = 0;
    return FN_OK;
}
uint8_t fn_close(fn_handle_t h)
{
    closes++;
    return FN_OK;
}
uint8_t fn_read(fn_handle_t h, uint32_t off, uint8_t *b, uint16_t max, uint16_t *n, uint8_t *flags)
{
    unsigned take = length - pos;
    assert(off == expected_read);
    assert(max == 128);
    *n = 0;
    *flags = 0;
    if (read_rc)
        return read_rc;
    if (take > cap)
        take = cap;
    if (take > max)
        take = max;
    memcpy(b, wire + pos, take);
    pos += take;
    expected_read += take;
    *n = take;
    if (pos == length)
        *flags = read_flags;
    return take || *flags ? FN_OK : FN_ERR_NOT_READY;
}
uint8_t fn_write(fn_handle_t h, uint32_t off, const uint8_t *b, uint16_t n, uint16_t *written)
{
    assert(off == expected_write);
    writes++;
    *written = n < short_write ? n : short_write;
    memcpy(tx + txlen, b, *written);
    txlen += *written;
    expected_write += *written;
    return write_rc;
}
#ifdef AMIGA_ASYNC
void amiga_tcp_note(uint8_t reason) { (void)reason; }
/* Adapter test: the foreground parser only sees the worker's bounded copies.
 * These mocks make replies available immediately; the guest tests inject lag. */
#include "../../src/amiga/worker.h"
static uint8_t worker_error, worker_eof;
uint8_t amiga_tcp_open(const char *host, uint16_t port)
{
    fn_handle_t h;
    worker_error = worker_eof = 0;
    return fn_tcp_open(&h, host, port) == FN_OK;
}
void amiga_tcp_close(void) { fn_close(1); }
uint8_t amiga_tcp_send(const uint8_t *b, unsigned n)
{
    uint16_t wrote;
    uint8_t rc = fn_write(1, expected_write, b, n, &wrote);
    if (rc != FN_OK || wrote != n) worker_error = 1;
    return !worker_error;
}
unsigned amiga_tcp_read(uint8_t *b, unsigned n)
{
    uint16_t got;
    uint8_t flags, rc = fn_read(1, expected_read, b, n, &got, &flags);
    if (rc != FN_OK && rc != FN_ERR_NOT_READY && rc != FN_ERR_BUSY) worker_error = 1;
    if (flags & FN_READ_EOF) worker_eof = 1;
    return got;
}
uint8_t amiga_tcp_lost(void) { return worker_error || worker_eof; }
#endif
static void start(void)
{
    length = pos = txlen = 0;
    read_rc = read_flags = write_rc = 0;
    cap = 128;
    short_write = 65535;
    assert(nst_open("test", 9910));
    assert(txlen == 1 && tx[0] == 0);
}
/* Independently generated COBS of type=1,payload=0,2, Fletcher=(3,5). */
static const uint8_t packet[] = {2, 1, 4, 2, 3, 5, 0};
static uint8_t next(void)
{
    frame_count += 3;
    return nst_frame();
}
int main(void)
{
    unsigned i, prior;
    start();
    memcpy(wire, packet, sizeof packet);
    length = sizeof packet;
    cap = 1;
    for (i = 0; i < sizeof packet - 1; ++i)
        assert(next() == 0);
    assert(next() == 3 && nst_buf[0] == 1 && nst_buf[1] == 0 && nst_buf[2] == 2);
    assert(nst_taken == 7);
    /* Coalesced packets and modulo credit accounting. */
    start();
    for (i = 0; i < 40; ++i)
        memcpy(wire + 7 * i, packet, 7);
    length = 280;
    i = 0;
    while (pos < length || i < 40) {
        uint8_t n = next();
        if (n) {
            assert(n == 3);
            i++;
        }
    }
    assert(nst_taken == (uint8_t)280);
#ifdef AMIGA_ASYNC
    start();
    for (i = 0; i < 40; ++i) memcpy(wire + 7 * i, packet, 7);
    length = 280;
    frame_count += 3;
    i = 0;
    while (nst_frame()) ++i;
    assert(i == 18 && pos == 128);
    assert(nst_frame() == 0 && pos == 128);
#endif
    /* Corrupt checksum, oversize then valid delimiter recovery. */
    start();
    memcpy(wire, packet, 7);
    wire[5] = 6;
    length = 7;
    assert(next() == 255);
    start();
    memset(wire, 2, 200);
    wire[200] = 0;
    memcpy(wire + 201, packet, 7);
    length = 208;
    assert(next() == 0);
    assert(next() == 255);
    assert(nst_frame() == 3);
    /* Empty delimiters and temporary idle never cause false loss. */
    start();
    wire[0] = wire[1] = 0;
    length = 2;
    assert(next() == 0);
    read_rc = FN_ERR_NOT_READY;
    assert(next() == 0 && !nst_lost());
    read_rc = FN_ERR_BUSY;
    assert(next() == 0 && !nst_lost());
    /* A final packet is delivered before EOF is reported. */
    start();
    memcpy(wire, packet, 7);
    length = 7;
    read_flags = FN_READ_EOF;
    assert(!nst_lost());
    assert(next() == 3);
    assert(nst_lost());
    /* Failed/partial writes do not replay an already accepted prefix. */
    start();
    prior = writes;
    short_write = 2;
    for (i = 0; i < 7; ++i)
        nst_put(packet[i]);
    assert(writes == prior + 1 && nst_lost());
    for (i = 0; i < 7; ++i)
        nst_put(packet[i]);
    assert(writes == prior + 1);
    start();
    for (i = 0; i < 7; ++i)
        nst_put(packet[i]);
    assert(!nst_lost() && txlen == 8);
    prior = closes;
    nst_close();
    nst_close();
    assert(closes == prior + 1);
    puts("transport passed");
}

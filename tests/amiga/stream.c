#include <assert.h>
#include <string.h>
#include <fujinet-nio.h>
#include "../../src/amiga/stream.h"
static unsigned calls, pauses, events, failures, mode;
static uint8_t response;
static fn_write_diagnostics_t diag;
void fb_stream_pause(unsigned ticks) { assert(ticks == (2u << pauses)); ++pauses; }
void fb_stream_event(const char *e, uint8_t rc, unsigned want, unsigned done, unsigned attempt)
{ (void)e; (void)rc; (void)want; (void)done; assert(attempt >= 1 && attempt <= 3); ++events; }
void fn_write_get_last_diagnostics(fn_write_diagnostics_t *d) { *d = diag; }
uint8_t fn_read(fn_handle_t h, uint32_t off, uint8_t *p, uint16_t cap, uint16_t *got, uint8_t *flags)
{
    assert(h == 7 && off == 123 && cap == 128);
    ++calls;
    *got = 0; *flags = 0;
    if (mode == 1) { *got = 129; return FN_OK; }
    if (mode == 2) { *got = 1; return FN_ERR_TIMEOUT; }
    if (mode == 4) { *flags = FN_READ_EOF; return FN_OK; }
    if (calls <= failures) return response;
    memcpy(p, "hello", 5); *got = 5; *flags = FN_READ_EOF;
    return FN_OK;
}
uint8_t fn_write(fn_handle_t h, uint32_t off, const uint8_t *p, uint16_t size, uint16_t *got)
{
    assert(h == 7 && off == 123 && size == 5 && !memcmp(p, "hello", 5));
    ++calls; *got = 0;
    if (calls <= failures) return response;
    *got = mode == 3 ? 2 : 5;
    return FN_OK;
}
static void reset(uint8_t rc, unsigned fail, unsigned m)
{ calls = pauses = events = 0; response = rc; failures = fail; mode = m; memset(&diag, 0, sizeof diag); }
int main(void)
{
    uint8_t data[128], flags;
    uint16_t got;
    reset(FN_ERR_TIMEOUT, 1, 0);
    assert(fb_stream_read(7, 123, data, &got, &flags) == FN_OK);
    assert(calls == 2 && pauses == 1 && got == 5 && flags == FN_READ_EOF && !memcmp(data, "hello", 5));
    reset(FN_ERR_IO, 2, 0);
    assert(fb_stream_read(7, 123, data, &got, &flags) == FN_OK && calls == 3);
    reset(FN_ERR_TRANSPORT, 99, 0);
    assert(fb_stream_read(7, 123, data, &got, &flags) == FN_ERR_TRANSPORT && calls == 3);
    reset(0, 0, 4);
    assert(fb_stream_read(7, 123, data, &got, &flags) == FN_OK && calls == 1 && !got && flags == FN_READ_EOF);
    reset(FN_ERR_NOT_READY, 99, 0);
    assert(fb_stream_read(7, 123, data, &got, &flags) == FN_ERR_NOT_READY && calls == 1 && !pauses);
    reset(FN_ERR_INVALID, 99, 0);
    assert(fb_stream_read(7, 123, data, &got, &flags) == FN_ERR_INVALID && calls == 1);
    reset(0, 0, 1);
    assert(fb_stream_read(7, 123, data, &got, &flags) == FN_ERR_IO && calls == 1);
    reset(0, 0, 2);
    assert(fb_stream_read(7, 123, data, &got, &flags) == FN_ERR_TIMEOUT && calls == 1);
    reset(FN_ERR_BUSY, 1, 0);
    diag.exchanges = diag.service_retry = 501;
    assert(fb_stream_write(7, 123, (uint8_t *)"hello", 5, &got) == FN_OK);
    assert(calls == 2 && pauses == 1 && got == 5);
    reset(FN_ERR_BUSY, 99, 0);
    diag.exchanges = diag.zero_accepted = 501;
    assert(fb_stream_write(7, 123, (uint8_t *)"hello", 5, &got) == FN_ERR_BUSY && calls == 3);
    /* A hidden accepted prefix in 0.2.0 returns written=0 on later failure. */
    reset(FN_ERR_BUSY, 99, 0);
    diag.exchanges = 502; diag.service_retry = 501;
    assert(fb_stream_write(7, 123, (uint8_t *)"hello", 5, &got) == FN_ERR_BUSY && calls == 1);
    reset(FN_ERR_TIMEOUT, 99, 0);
    diag.exchanges = 1;
    assert(fb_stream_write(7, 123, (uint8_t *)"hello", 5, &got) == FN_ERR_TIMEOUT && calls == 1);
    reset(0, 0, 3);
    assert(fb_stream_write(7, 123, (uint8_t *)"hello", 5, &got) == FN_ERR_IO && calls == 1);
}

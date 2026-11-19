#include <fujinet-nio.h>
#include "stream.h"
uint8_t fb_stream_read(uint16_t handle, uint32_t offset, uint8_t *data,
                       uint16_t *got, uint8_t *flags)
{
    unsigned attempt;
    uint8_t rc;
    for (attempt = 1; ; ++attempt) {
        *got = 0; *flags = 0;
        rc = fn_read(handle, offset, data, FB_READ_SIZE, got, flags);
        if (rc == FN_OK && *got <= FB_READ_SIZE) {
            if (attempt > 1) fb_stream_event("read-recovered", rc, FB_READ_SIZE, *got, attempt);
            return rc;
        }
        if (rc == FN_ERR_BUSY || rc == FN_ERR_NOT_READY) return rc;
        fb_stream_event("read-error", rc, FB_READ_SIZE, *got, attempt);
        /* The NIO TCP service caches its last read by offset AND size. Never
         * retry after partial progress or a malformed success response. */
        if (*got || attempt == FB_IO_ATTEMPTS ||
            (rc != FN_ERR_TIMEOUT && rc != FN_ERR_TRANSPORT && rc != FN_ERR_IO))
            return rc == FN_OK ? FN_ERR_IO : rc;
        fb_stream_pause(1u << attempt);
    }
}
uint8_t fb_stream_write(uint16_t handle, uint32_t offset, const uint8_t *data,
                        uint16_t size, uint16_t *written)
{
    unsigned attempt;
    uint8_t rc;
    for (attempt = 1; ; ++attempt) {
        fn_write_diagnostics_t d;
        *written = 0;
        rc = fn_write(handle, offset, data, size, written);
        if (rc == FN_OK && *written == size) {
            if (attempt > 1) fb_stream_event("write-recovered", rc, size, *written, attempt);
            return rc;
        }
        fn_write_get_last_diagnostics(&d);
        fb_stream_event("write-error", rc, size, *written, attempt);
        /* 0.2.0 only sets *written on success: zero alone does NOT prove that
         * nothing was sent. Retry only when EVERY exchange explicitly made
         * zero progress. An ambiguous transport/partial-write failure closes
         * the stream instead of replaying a bomb, credit, or frame prefix. */
        if (attempt == FB_IO_ATTEMPTS || *written || !d.exchanges ||
            (rc != FN_ERR_BUSY && rc != FN_ERR_NOT_READY) ||
            (uint32_t)d.transport_retry + d.service_retry + d.zero_accepted != d.exchanges)
            return rc == FN_OK ? FN_ERR_IO : rc;
        fb_stream_pause(1u << attempt);
    }
}

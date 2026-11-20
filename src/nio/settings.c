/* Native NIO persistence. Call from the platform's library-owning task.
 * Bounded chunks keep scratch requirements suitable for 8-bit NIO clients. */
#include <fujinet-nio.h>
#include "settings.h"
static uint8_t scratch[320];
static fn_appstore_io_t io = {scratch, sizeof scratch};
uint8_t fb_store_read(const char *key, uint8_t *data, uint16_t capacity, uint16_t *size)
{
    uint16_t offset = 0;
    *size = 0;
    while (offset < capacity) {
        fn_appstore_read_t result;
        uint16_t n = capacity - offset;
        if (n > 256) n = 256;
        if (fn_appstore_read(&io, "fujiblast", key, offset, data + offset, n, &result) != FN_OK ||
            result.offset != offset || result.bytes_read > n) return 0;
        offset += result.bytes_read;
        if (result.flags & FN_APPSTORE_READ_EOF) { *size = offset; return 1; }
        if (!result.bytes_read) return 0;
    }
    return 0; /* Oversized or incomplete value: never accept a truncated setting. */
}
uint8_t fb_store_write(const char *key, const uint8_t *data, uint16_t size)
{
    uint16_t offset = 0;
    do {
        fn_appstore_write_t result;
        uint16_t n = size - offset;
        if (n > 256) n = 256;
        if (fn_appstore_write(&io, "fujiblast", key, offset, data + offset, n, &result) != FN_OK ||
            result.offset != offset || result.bytes_written != n) return 0;
        offset += n;
    } while (offset < size);
    return 1;
}
static const char *setting(uint8_t key) { return key ? "server" : "player-name"; }
uint8_t fb_settings_read(uint8_t key, uint16_t *size, uint8_t *data)
{
    return fb_store_read(setting(key), data, FB_NIO_URL_BYTES - 1, size);
}
uint8_t fb_settings_write(uint8_t key, uint16_t size, const uint8_t *data)
{
    return size < FB_NIO_URL_BYTES && fb_store_write(setting(key), data, size);
}

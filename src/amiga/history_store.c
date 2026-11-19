#include "nio/servers.h"
#include "history_store.h"
#include "nio/settings.h"
uint8_t fb_history_read(uint8_t *data, uint16_t *size)
{
    return fb_store_read("servers", data, FB_SERVERS_BYTES, size);
}
uint8_t fb_history_write(const uint8_t *data, uint16_t size)
{
    return size == FB_SERVERS_BYTES && fb_store_write("servers", data, size);
}

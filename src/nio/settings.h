#ifndef FB_NIO_SETTINGS_H
#define FB_NIO_SETTINGS_H
#include <stdint.h>
/* Application buffer limits, independent of AppStore value sizes. */
/* Released NIO 0.2.0 fn_tcp_open reserves 16 of its 256 URL bytes. */
#define FB_NIO_HOST_MAX 240
#define FB_NIO_URL_BYTES 288
uint8_t fb_store_read(const char *key, uint8_t *data, uint16_t capacity, uint16_t *size);
uint8_t fb_store_write(const char *key, const uint8_t *data, uint16_t size);
uint8_t fb_settings_read(uint8_t key, uint16_t *size, uint8_t *data);
uint8_t fb_settings_write(uint8_t key, uint16_t size, const uint8_t *data);
#endif

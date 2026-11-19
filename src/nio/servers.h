#ifndef FB_SERVERS_H
#define FB_SERVERS_H
#include <stdint.h>
#define FB_SERVER_COUNT 5
#include "nio/settings.h"
/* Host, colon, five port digits, terminator. */
#define FB_SERVER_TEXT (FB_NIO_HOST_MAX + 7)
#define FB_SERVERS_BYTES (5 + FB_SERVER_COUNT * FB_SERVER_TEXT)
struct fb_servers {
    uint8_t count;
    char address[FB_SERVER_COUNT][FB_SERVER_TEXT];
};
uint8_t fb_server_normalize(const char *input, char *address);
uint8_t fb_servers_add(struct fb_servers *list, const char *address);
void fb_servers_delete(struct fb_servers *list, unsigned index);
uint8_t fb_servers_decode(struct fb_servers *list, const uint8_t *data, unsigned size);
void fb_servers_encode(const struct fb_servers *list, uint8_t *data);
#endif

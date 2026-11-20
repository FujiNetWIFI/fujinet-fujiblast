#include <stdio.h>
#include <string.h>
#include "servers.h"

uint8_t fb_server_normalize(const char *input, char *address)
{
    char host[FB_NIO_HOST_MAX + 1];
    const char *p = input;
    unsigned n = 0, port = 9910;
    if (!strncmp(p, "tcp://", 6) || !strncmp(p, "TCP://", 6)) p += 6;
    while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
           (*p >= '0' && *p <= '9') || *p == '.' || *p == '-') {
        char c = *p++;
        if (n == FB_NIO_HOST_MAX) return 0;
        host[n++] = c >= 'A' && c <= 'Z' ? c + 'a' - 'A' : c;
    }
    host[n] = 0;
    if (!n || host[0] == '.' || host[0] == '-' || host[n-1] == '.' || host[n-1] == '-') return 0;
    if (*p == ':') {
        ++p;
        port = 0;
        if (*p < '0' || *p > '9') return 0;
        while (*p >= '0' && *p <= '9') {
            if (port > 6553 || (port == 6553 && *p > '5')) return 0;
            port = port * 10 + (*p++ - '0');
        }
        if (!port) return 0;
    }
    if (*p == '/') ++p;
    if (*p) return 0;
    return snprintf(address, FB_SERVER_TEXT, "%s:%u", host, port) < FB_SERVER_TEXT;
}

void fb_servers_delete(struct fb_servers *list, unsigned index)
{
    if (index >= list->count) return;
    --list->count;
    memmove(list->address[index], list->address[index+1],
            (list->count - index) * FB_SERVER_TEXT);
    memset(list->address[list->count], 0, FB_SERVER_TEXT);
}

uint8_t fb_servers_add(struct fb_servers *list, const char *address)
{
    char normal[FB_SERVER_TEXT];
    unsigned i;
    if (!fb_server_normalize(address, normal)) return 0;
    for (i = 0; i < list->count; ++i)
        if (!strcmp(normal, list->address[i])) {
            fb_servers_delete(list, i);
            break;
        }
    if (list->count == FB_SERVER_COUNT) return 0;
    memmove(list->address[1], list->address[0], list->count * FB_SERVER_TEXT);
    memset(list->address[0], 0, FB_SERVER_TEXT);
    strcpy(list->address[0], normal);
    ++list->count;
    return 1;
}

uint8_t fb_servers_decode(struct fb_servers *list, const uint8_t *data, unsigned size)
{
    struct fb_servers decoded = {0};
    unsigned i, j, stride;
    if (!size) { *list = decoded; return 1; }
    /* Both versions are native AppStore history, not legacy AppKeys. */
    if (size == FB_SERVERS_BYTES && !memcmp(data, "FBS2", 4)) stride = FB_SERVER_TEXT;
    else if (size == 295 && !memcmp(data, "FBS1", 4)) stride = 58;
    else return 0;
    if (data[4] > FB_SERVER_COUNT) return 0;
    decoded.count = data[4];
    for (i = 0; i < decoded.count; ++i) {
        const char *entry = (const char *)data + 5 + i * stride;
        if (!memchr(entry, 0, stride) ||
            !fb_server_normalize(entry, decoded.address[i])) return 0;
        for (j = 0; j < i; ++j)
            if (!strcmp(decoded.address[j], decoded.address[i])) return 0;
    }
    *list = decoded;
    return 1;
}

void fb_servers_encode(const struct fb_servers *list, uint8_t *data)
{
    unsigned i;
    memset(data, 0, FB_SERVERS_BYTES);
    memcpy(data, "FBS2", 4);
    data[4] = list->count;
    for (i = 0; i < list->count; ++i)
        strcpy((char *)data + 5 + i * FB_SERVER_TEXT, list->address[i]);
}

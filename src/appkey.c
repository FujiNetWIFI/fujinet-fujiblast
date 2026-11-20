/*
 * appkey.c -- the lobby's handoff (from M.U.L.E.'s src/net.c). The FujiNet
 * lobby keeps its keys under creator 1, app 1:
 *   key 0     the player's name, shared by every FujiNet game
 *   key 0x10  FujiBlast's server: tcp://host:port/?table=id
 * Without them we ask for a name and list the default server's rooms; the
 * room picked goes back in key 0x10, so a reboot sits down there again.
 * FB_NIO builds instead use native fujiblast/player-name and server values;
 * the numeric IDs below are internal selectors, not legacy storage calls.
 */
#include <string.h>
#include "game.h"
#ifdef FB_NIO
#include "nio/settings.h"
#include "nio/servers.h"
#ifdef AMIGA_ASYNC
#include "amiga/worker.h"
#endif
#else
#include "fujinet-fuji.h"
#endif
#include "config.h"
#include "nstrans.h"

#define AK_CREATOR 1
#define AK_APP     1
#define AK_NAME    0
#define AK_SERVER  0x10

char my_name[NAME_LEN + 1];
char srv_host[FB_HOST_MAX + 1];
uint16_t srv_port;
char srv_table[TABLE_LEN + 1];

#ifdef FB_NIO
static uint8_t akbuf[FB_NIO_URL_BYTES];
#else
static uint8_t akbuf[68];
#endif

/* The buffer is zeroed first: an unset key can read back as zeros. */
#ifdef FB_NIO
static uint16_t ak_read(uint8_t key)
#else
static uint8_t ak_read(uint8_t key)
#endif
{
    uint16_t count = 0;
    uint8_t ok;
    memset(akbuf, 0, sizeof(akbuf));
#ifdef NO_APPKEYS                       /* a test build: no lib code (the 7800's bank 0 is small) */
    (void)count;
    (void)ok;
    return 0;
#else
    nst_pause();
#ifdef AMIGA_ASYNC
    ok = amiga_key_read(key, &count, akbuf);
#elif defined(FB_NIO)
    ok = fb_settings_read(key, &count, akbuf);
#else
    fuji_set_appkey_details(AK_CREATOR, AK_APP, DEFAULT);
    ok = fuji_read_appkey(key, &count, akbuf);
#endif
    nst_resume();
    if (!ok)
        return 0;
#ifdef FB_NIO
    if (count >= sizeof akbuf) return 0;
#else
    if (count > 64)
        count = 64;
#endif
    akbuf[count] = 0;
#ifdef FB_NIO
    return count;
#else
    return (uint8_t)count;
#endif
#endif
}

#ifdef FB_NIO
static uint8_t ak_write(uint8_t key, uint16_t n)
{
#ifdef NO_APPKEYS
    (void)key;
    (void)n;
    return 0;
#else
    uint8_t ok;
    nst_pause();
#ifdef AMIGA_ASYNC
    ok = amiga_key_write(key, n, akbuf);
#else
    ok = fb_settings_write(key, n, akbuf);
#endif
    nst_resume();
    return ok;
#endif
}

#else
static void ak_write(uint8_t key, uint8_t n)
{
#ifdef NO_APPKEYS
    (void)key;
    (void)n;
#else
    nst_pause();
    fuji_set_appkey_details(AK_CREATOR, AK_APP, DEFAULT);
    fuji_write_appkey(key, n, akbuf);
    nst_resume();
#endif
}

#endif

/* "tcp://host:port/?table=id" (or host:port): the host, the port, the table
 * (letters and digits). False if there is no host. */
#ifdef FB_NIO
static uint8_t parse(const char *u)
{
    const char *p = strstr(u, "://");
    uint16_t n = 0;
    char c, host[FB_HOST_MAX + 1], table[TABLE_LEN + 1];
    uint16_t port;
    p = p ? p + 3 : u;
    while (*p && *p != ':' && *p != '/' && *p != '?' && n < FB_HOST_MAX)
        host[n++] = *p++;
    host[n] = 0;
    if (*p && *p != ':' && *p != '/' && *p != '?')
        return 0;
    port = 0;
    if (*p == ':') {
        ++p;
        if (*p < '0' || *p > '9') return 0;
        while (*p >= '0' && *p <= '9') {
            if (port > 6553 || (port == 6553 && *p > '5')) return 0;
            port = port * 10 + (*p++ - '0');
        }
        if (!port || (*p && *p != '/' && *p != '?')) return 0;
    }
    if (!port)
        port = STREAM_PORT;
    table[0] = 0;
    p = strstr(p, "table=");
    if (p) {
        p += 6;
        for (n = 0; n < TABLE_LEN; ++n) {
            c = *p++;
            if (c >= 'A' && c <= 'Z')
                c += 32;
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
                break;
            table[n] = c;
        }
        table[n] = 0;
    }
    if (!host[0]) return 0;
    strcpy(srv_host, host);
    srv_port = port;
    strcpy(srv_table, table);
    return 1;
}

#else
static uint8_t parse(const char *u)
{
    const char *p = strstr(u, "://");
    uint8_t n = 0;
    char c;
    p = p ? p + 3 : u;
    while (*p && *p != ':' && *p != '/' && *p != '?' && n < 60)
        srv_host[n++] = *p++;
    srv_host[n] = 0;
    srv_port = 0;
    if (*p == ':')
        while (*++p >= '0' && *p <= '9')
            srv_port = srv_port * 10 + (*p - '0');
    if (!srv_port)
        srv_port = STREAM_PORT;
    srv_table[0] = 0;
    p = strstr(p, "table=");
    if (p) {
        p += 6;
        for (n = 0; n < TABLE_LEN; ++n) {
            c = *p++;
            if (c >= 'A' && c <= 'Z')
                c += 32;
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
                break;
            srv_table[n] = c;
        }
        srv_table[n] = 0;
    }
    return srv_host[0] != 0;
}

#endif

void appkey_load(void)
{
#ifdef FB_NIO
    uint16_t n;
    uint8_t k = 0;
#else
    uint8_t n, k = 0;
#endif
    char c;
    if (ak_read(AK_NAME)) {
        for (n = 0; akbuf[n] && k < NAME_LEN; ++n) {
            c = akbuf[n];
            if (c >= 'a' && c <= 'z')
                c -= 32;
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
                my_name[k++] = c;
        }
    }
    my_name[k] = 0;
    if (ak_read(AK_SERVER) > 4 && parse((char *)akbuf))
        return;
#ifdef FB_NIO
    /* A malformed build-time default must not leave a partial endpoint. */
    if (!parse(DEFAULT_SERVER))
        parse("fujinet.online:9910");
#else
    parse(DEFAULT_SERVER);
#endif
    srv_table[0] = 0;
}

void appkey_save_name(void)
{
    memset(akbuf, 0, sizeof(akbuf));
    strcpy((char *)akbuf, my_name);
    ak_write(AK_NAME, strlen(my_name));
}

static char *add(char *p, const char *s)
{
    while (*s)
        *p++ = *s++;
    return p;
}

void appkey_save_room(void)
{
    char port[6], *p, *q = port + 5;
    uint16_t v = srv_port;
    *q = 0;
    do {
        *--q = '0' + v % 10;
        v /= 10;
    } while (v);
#ifdef FB_NIO
    /* Bound the application buffer. Keep the previous key intact
     * when the full endpoint cannot be represented; never truncate it. */
    if (6 + strlen(srv_host) + 1 + strlen(q) + (srv_table[0] ? 8 + strlen(srv_table) : 1) >= sizeof akbuf)
        return;
#endif
    memset(akbuf, 0, sizeof(akbuf));
    p = add((char *)akbuf, "tcp://");
    p = add(p, srv_host);
    p = add(p, ":");
    p = add(p, q);
#ifdef FB_NIO
    p = add(p, srv_table[0] ? "/?table=" : "/");
#else
    p = add(p, "/?table=");
#endif
    p = add(p, srv_table);
#ifdef FB_NIO
    ak_write(AK_SERVER, (uint16_t)(p - (char *)akbuf));
#else
    ak_write(AK_SERVER, (uint8_t)(p - (char *)akbuf));
#endif
}

void appkey_clear_room(void)
{
#ifdef FB_NIO
    srv_table[0] = 0;
    /* Keep the selected endpoint after leaving a room. */
    appkey_save_room();
#else
    memset(akbuf, 0, sizeof(akbuf));
    ak_write(AK_SERVER, 0);
    srv_table[0] = 0;
#endif
}

#ifdef FB_NIO
/* Validate into local buffers: bad edits must not alter the active endpoint.
 * 1: saved, 0: invalid input, 2: persistence failure (allow retry). */
uint8_t fb_nio_save_setup(const char *name, const char *address)
{
    char address_normal[FB_SERVER_TEXT], url[FB_NIO_URL_BYTES];
    unsigned len = strlen(name);
    uint8_t i;
    if (len < 2 || len > NAME_LEN) return 0;
    for (i = 0; i < len; ++i)
        if (!((name[i] >= 'A' && name[i] <= 'Z') ||
              (name[i] >= '0' && name[i] <= '9'))) return 0;
    if (!fb_server_normalize(address, address_normal)) return 0;
    strcpy(url, "tcp://"); strcat(url, address_normal); strcat(url, "/");
    strcpy((char *)akbuf, url);
    if (!ak_write(AK_SERVER, strlen(url))) return 2;
    strcpy((char *)akbuf, name);
    if (!ak_write(AK_NAME, len)) return 2;
    strcpy(my_name, name);
    parse(url);
    return 1;
}
#endif

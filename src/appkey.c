/*
 * appkey.c -- the lobby's handoff (from M.U.L.E.'s src/net.c). The FujiNet
 * lobby keeps its keys under creator 1, app 1:
 *   key 0     the player's name, shared by every FujiNet game
 *   key 0x10  FujiBlast's server: tcp://host:port/?table=id
 * Without them we ask for a name and list the default server's rooms; the
 * room picked goes back in key 0x10, so a reboot sits down there again.
 */
#include <string.h>
#include "fujinet-fuji.h"
#include "game.h"
#include "config.h"
#include "nstrans.h"

#define AK_CREATOR 1
#define AK_APP     1
#define AK_NAME    0
#define AK_SERVER  0x10

char my_name[NAME_LEN + 1];
char srv_host[61];
uint16_t srv_port;
char srv_table[TABLE_LEN + 1];

static uint8_t akbuf[68];

/* The buffer is zeroed first: an unset key can read back as zeros. */
static uint8_t ak_read(uint8_t key)
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
    fuji_set_appkey_details(AK_CREATOR, AK_APP, DEFAULT);
    ok = fuji_read_appkey(key, &count, akbuf);
    nst_resume();
    if (!ok)
        return 0;
    if (count > 64)
        count = 64;
    akbuf[count] = 0;
    return (uint8_t)count;
#endif
}

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

/* "tcp://host:port/?table=id" (or host:port): the host, the port, the table
 * (letters and digits). False if there is no host. */
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

void appkey_load(void)
{
    uint8_t n, k = 0;
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
    parse(DEFAULT_SERVER);
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
    memset(akbuf, 0, sizeof(akbuf));
    p = add((char *)akbuf, "tcp://");
    p = add(p, srv_host);
    p = add(p, ":");
    p = add(p, q);
    p = add(p, "/?table=");
    p = add(p, srv_table);
    ak_write(AK_SERVER, (uint8_t)(p - (char *)akbuf));
}

void appkey_clear_room(void)
{
    memset(akbuf, 0, sizeof(akbuf));
    ak_write(AK_SERVER, 0);
    srv_table[0] = 0;
}

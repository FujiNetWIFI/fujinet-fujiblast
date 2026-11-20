#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "nio/settings.h"
#include "game.h"
#ifdef INVALID_DEFAULT
#define EXPECTED_HOST "fujinet.online"
#else
#define EXPECTED_HOST "localhost"
#endif
static char key[FB_NIO_URL_BYTES], name[FB_NIO_URL_BYTES];
static unsigned writes;
static int fail_write;
void nst_pause(void) {}
void nst_resume(void) {}
uint8_t fb_settings_read(uint8_t id, uint16_t *n, uint8_t *p)
{
    const char *s = id ? key : name;
    *n = strlen(s);
    memcpy(p, s, *n);
    return 1;
}
uint8_t fb_settings_write(uint8_t id, uint16_t n, const uint8_t *p)
{
    if (fail_write) return 0;
    assert(n < FB_NIO_URL_BYTES);
    memcpy(id ? key : name, p, n);
    (id ? key : name)[n] = 0;
    writes++;
    return 1;
}
int main(void)
{
    unsigned before;
    appkey_load();
    assert(!my_name[0] && !strcmp(srv_host, EXPECTED_HOST) && srv_port == 9910);
    strcpy(name, "a.n-n42");
    strcpy(key, "tcp://server:1234/?table=Blast1");
    appkey_load();
    assert(!strcmp(my_name, "ANN42") && !strcmp(srv_host, "server") && srv_port == 1234 &&
           !strcmp(srv_table, "blast1"));
    strcpy(key, "tcp://server:65536/");
    appkey_load();
    assert(!strcmp(srv_host, EXPECTED_HOST));
    strcpy(key, "tcp://server:0/");
    appkey_load();
    assert(srv_port == 9910);
    memset(srv_host, 'a', 60);
    srv_host[60] = 0;
    before = writes;
    appkey_save_room();
    assert(writes == before + 1 && strlen(key) > 64);
    strcpy(srv_host, "server");
    srv_port = 65535;
    strcpy(srv_table, "blast1");
    appkey_save_room();
    assert(!strcmp(key, "tcp://server:65535/?table=blast1"));
    appkey_clear_room();
    assert(!strcmp(key, "tcp://server:65535/") && !srv_table[0]);
    appkey_load();
    assert(!strcmp(srv_host, "server") && srv_port == 65535 && !srv_table[0]);
    assert(fb_nio_save_setup("MARK", "192.168.1.20:9911") == 1);
    appkey_load();
    assert(!strcmp(my_name, "MARK") && !strcmp(srv_host, "192.168.1.20") && srv_port == 9911);
    assert(fb_nio_save_setup("ALICE", "TCP://games.example/") == 1);
    assert(!strcmp(key, "tcp://games.example:9910/"));
    before = writes;
    assert(!fb_nio_save_setup("A", "server"));
    assert(!fb_nio_save_setup("A B", "server"));
    assert(!fb_nio_save_setup("ALICE", "http://server"));
    assert(!fb_nio_save_setup("ALICE", "server:0"));
    assert(!fb_nio_save_setup("ALICE", "server:65536"));
    assert(!fb_nio_save_setup("ALICE", "server:99999999999999999999"));
    assert(!fb_nio_save_setup("ALICE", "server:12x"));
    assert(!fb_nio_save_setup("ALICE", ":9910"));
    assert(!fb_nio_save_setup("ALICE", "server/?table=blast1"));
    assert(!fb_nio_save_setup("ALICE", "bad host"));
    assert(writes == before && !strcmp(srv_host, "games.example"));
    {
        char host[FB_HOST_MAX + 2];
        memset(host, 'a', FB_HOST_MAX); host[FB_HOST_MAX] = 0;
        assert(fb_nio_save_setup("ALICE", host) == 1);
        assert(strlen(key) > 64);
        appkey_load();
        assert(strlen(srv_host) == FB_HOST_MAX && srv_port == 9910);
        strcpy(srv_table, "blast1");
        appkey_save_room();
        assert(strlen(key) > 255);
        appkey_load();
        assert(strlen(srv_host) == FB_HOST_MAX && !strcmp(srv_table, "blast1"));
        host[FB_HOST_MAX] = 'a'; host[FB_HOST_MAX+1] = 0;
        assert(!fb_nio_save_setup("ALICE", host));
    }
    fail_write = 1;
    before = writes;
    assert(fb_nio_save_setup("BOB", "newhost:1234") == 2);
    assert(writes == before && !strcmp(my_name, "ALICE"));
    puts("appkeys passed");
}

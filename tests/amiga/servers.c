#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <fujinet-nio.h>
#include "nio/servers.h"
#include "amiga/history_store.h"
#include "nio/settings.h"
static uint8_t saved[FB_SERVERS_BYTES];
static unsigned calls;
static unsigned saved_size = sizeof saved;
static int fail, short_write, missing, no_eof;
uint8_t fn_appstore_read(fn_appstore_io_t *io, const char *ns, const char *key,
                        uint32_t offset, uint8_t *data, uint16_t max, fn_appstore_read_t *out)
{
    assert(io->capacity >= max+40 && !strcmp(ns,"fujiblast"));
    assert(max <= 256 && offset <= saved_size);
    ++calls;
    if (fail) return FN_ERR_IO;
    out->offset = offset;
    out->bytes_read = missing ? 0 : (saved_size-offset < max ? saved_size-offset : max);
    out->flags = (no_eof || (!missing && offset+out->bytes_read < saved_size) ? 0 : FN_APPSTORE_READ_EOF) | (missing ? 0 : FN_APPSTORE_READ_EXISTS);
    memcpy(data, saved+offset, out->bytes_read);
    return FN_OK;
}
uint8_t fn_appstore_write(fn_appstore_io_t *io, const char *ns, const char *key,
                         uint32_t offset, const uint8_t *data, uint16_t n, fn_appstore_write_t *out)
{
    assert(io->capacity >= n+40 && !strcmp(ns,"fujiblast"));
    assert(n <= 256 && offset+n <= sizeof saved);
    ++calls;
    if (fail) return FN_ERR_IO;
    out->offset = offset;
    out->bytes_written = short_write ? n-1 : n;
    memcpy(saved+offset, data, n);
    saved_size = offset+n;
    return FN_OK;
}
int main(void)
{
    struct fb_servers list = {0}, loaded, before;
    char normal[FB_SERVER_TEXT], host[FB_NIO_HOST_MAX+2];
    uint8_t data[FB_SERVERS_BYTES], broken[FB_SERVERS_BYTES];
    uint16_t n;
    unsigned i, old_calls;
    assert(fb_server_normalize("TCP://Games.Example/", normal));
    assert(!strcmp(normal,"games.example:9910"));
    assert(fb_server_normalize("192.168.1.10:65535", normal));
    assert(!fb_server_normalize("server:65536", normal));
    assert(!fb_server_normalize("server:999999999999999999999999", normal));
    assert(!fb_server_normalize("server:0", normal));
    assert(!fb_server_normalize("server:12x", normal));
    assert(!fb_server_normalize("http://server", normal));
    assert(!fb_server_normalize("server/?table=1", normal));
    assert(!fb_server_normalize("bad host", normal));
    memset(host,'a',FB_NIO_HOST_MAX); host[FB_NIO_HOST_MAX]=0;
    assert(fb_server_normalize(host,normal));
    host[FB_NIO_HOST_MAX]='a'; host[FB_NIO_HOST_MAX+1]=0;
    assert(!fb_server_normalize(host,normal));
    for(i=0;i<5;++i) { snprintf(host,sizeof host,"host%u",i); assert(fb_servers_add(&list,host)); }
    before=list;
    assert(!fb_servers_add(&list,"sixth"));
    assert(!memcmp(&list,&before,sizeof list));
    assert(fb_servers_add(&list,"TCP://HOST2:9910/"));
    assert(list.count==5 && !strcmp(list.address[0],"host2:9910"));
    fb_servers_delete(&list,4); assert(list.count==4);
    assert(fb_servers_add(&list,"new-host:1234"));
    fb_servers_encode(&list,data);
    assert(fb_history_write(data,sizeof data));
    assert(fb_history_read(broken,&n) && n==sizeof data);
    assert(fb_servers_decode(&loaded,broken,n));
    assert(!memcmp(&list,&loaded,sizeof list));
    before=loaded;
    assert(!fb_servers_decode(&loaded,data,sizeof data-1));
    memcpy(broken,data,sizeof data); broken[4]=6;
    assert(!fb_servers_decode(&loaded,broken,sizeof broken));
    memcpy(broken,data,sizeof data); memset(broken+5,'A',FB_SERVER_TEXT);
    assert(!fb_servers_decode(&loaded,broken,sizeof broken));
    memcpy(broken,data,sizeof data); memcpy(broken+5+FB_SERVER_TEXT,broken+5,FB_SERVER_TEXT);
    assert(!fb_servers_decode(&loaded,broken,sizeof broken));
    memcpy(broken,data,sizeof data); broken[3]='9';
    assert(!fb_servers_decode(&loaded,broken,sizeof broken));
    assert(!memcmp(&loaded,&before,sizeof loaded));
    while(list.count) fb_servers_delete(&list,0);
    fb_servers_encode(&list,data);
    assert(fb_history_write(data,sizeof data));
    assert(fb_history_read(broken,&n) && fb_servers_decode(&loaded,broken,n) && loaded.count==0);
    missing=1;
    assert(fb_history_read(broken,&n) && n==0 && fb_servers_decode(&loaded,broken,n));
    missing=0; fail=1;
    assert(!fb_history_read(broken,&n) && !n);
    assert(!fb_history_write(data,sizeof data));
    fail=0; no_eof=1; assert(!fb_history_read(broken,&n)); no_eof=0;
    short_write=1; assert(!fb_history_write(data,sizeof data)); short_write=0;
    old_calls=calls; assert(!fb_history_write(data,1)); assert(calls==old_calls);
    /* Native settings use the same chunked store, with no legacy API calls. */
    memset(data, 'a', FB_NIO_URL_BYTES-1);
    assert(fb_settings_write(16, FB_NIO_URL_BYTES-1, data));
    assert(fb_settings_read(16, &n, broken) && n == FB_NIO_URL_BYTES-1);
    assert(!memcmp(data, broken, n));
    assert(!fb_settings_write(16, FB_NIO_URL_BYTES, data));
    assert(fb_settings_write(0, 4, (const uint8_t *)"MARK"));
    assert(fb_settings_read(0, &n, broken) && n == 4 && !memcmp(broken, "MARK", 4));
    /* The earlier Amiga history already used AppStore, under this same key. */
    memset(data, 0, 295);
    memcpy(data, "FBS1", 4); data[4] = 2;
    strcpy((char *)data+5, "192.168.1.100:9910");
    strcpy((char *)data+5+58, "games.example:9910");
    assert(fb_store_write("servers", data, 295));
    assert(fb_history_read(broken, &n) && n == 295);
    assert(fb_servers_decode(&loaded, broken, n) && loaded.count == 2);
    assert(!strcmp(loaded.address[0], "192.168.1.100:9910"));
    before = loaded;
    broken[4] = 6;
    assert(!fb_servers_decode(&loaded, broken, n));
    broken[4] = 2;
    memset(broken+5, 'x', 58);
    assert(!fb_servers_decode(&loaded, broken, n));
    assert(!memcmp(&loaded, &before, sizeof loaded));
    fb_servers_encode(&loaded, data);
    assert(!memcmp(data, "FBS2", 4));
    assert(fb_history_write(data, sizeof data));
    assert(fb_history_read(broken, &n) && n == sizeof data);
    assert(fb_servers_decode(&loaded, broken, n) && loaded.count == 2);
    puts("server list validation, persistence and failures passed");
}

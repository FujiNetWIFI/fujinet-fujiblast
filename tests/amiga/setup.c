#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "input.h"
#include "nio/servers.h"
void amiga_setup(void);
char my_name[NAME_LEN+1]="MARK", srv_host[FB_HOST_MAX+1]="localhost", srv_table[TABLE_LEN+1];
uint16_t srv_port=9910;
uint8_t inp_dir,inp_btn,ev_btn,ev_key;
static uint8_t storage[FB_SERVERS_BYTES];
static uint16_t size;
static unsigned writes, saves, fail_writes, fail_reads;
static char saved_address[FB_SERVER_TEXT];
static uint8_t keys[512];
static unsigned used, pos;
static char screen[25][41];
static int saw_full, saw_five, saw_error, saw_lobby;
void frame_wait(void) {}
void input_poll(void) {}
void gfx_show(void);
void input_take(void) { gfx_show(); assert(pos<used); ev_key=keys[pos++]; inp_dir=ev_btn=0; }
void gfx_begin(void) { unsigned i; for(i=0;i<25;++i) { memset(screen[i],' ',40); screen[i][40]=0; } }
void gfx_clear_row(uint8_t row) { memset(screen[row],' ',40); }
void gfx_row(uint8_t row,uint8_t mode,uint8_t pal) { (void)row; (void)mode; (void)pal; }
void gfx_text(uint8_t row,uint8_t col,const char *s,uint8_t colour)
{ (void)colour; assert(row<25 && col+strlen(s)<=40); memcpy(screen[row]+col,s,strlen(s)); }
void centre(uint8_t row,const char *s,uint8_t colour,uint8_t width)
{ assert(strlen(s)<=width); gfx_text(row,(width-strlen(s))/2,s,colour); }
void gfx_show(void)
{
    unsigned i;
    if(strstr(screen[21],"LIST FULL")) saw_full=1;
    if(strstr(screen[21],"SAVE FAILED")) saw_error=1;
    if(strstr(screen[9],"lobby.example:9910")) saw_lobby=1;
    for(i=0;i<5;++i) if(screen[8+i*2][4]==' ') return;
    saw_five=1;
}
uint8_t amiga_history_read(uint8_t *data,uint16_t *n)
{ if(fail_reads) { --fail_reads; return 0; } memcpy(data,storage,size); *n=size; return 1; }
uint8_t amiga_history_write(uint8_t *data,uint16_t n)
{ ++writes; if(fail_writes) { --fail_writes; return 0; } assert(n==sizeof storage); memcpy(storage,data,n); size=n; return 1; }
uint8_t fb_nio_save_setup(const char *name,const char *address)
{ ++saves; strcpy(my_name,name); strcpy(saved_address,address); return 1; }
static void key(uint8_t k) { assert(used<sizeof keys); keys[used++]=k; }
static void tabs(unsigned n) { while(n--) key(9); }
static void text(const char *s) { while(*s) key(*s++); }
static void go(void) { pos=0; amiga_setup(); assert(pos==used); used=0; }
int main(void)
{
    struct fb_servers list={0};
    unsigned before, i;
    /* Import the old setting, add a new one, select it and save. */
    tabs(3); key(KEY_RETURN); text("EXAMPLE.COM:1234"); key(KEY_RETURN);
    tabs(3); key(KEY_RETURN); go();
    assert(saves==1 && writes==1 && !strcmp(saved_address,"example.com:1234"));
    assert(fb_servers_decode(&list,storage,size) && list.count==2);
    assert(!strcmp(list.address[1],"localhost:9910"));
    /* Cancelling deletion must not write anything. */
    before=writes; tabs(2); key(KEY_BACK); key(KEY_ESC); go(); assert(writes==before);
    /* Choose Lobby directly without typing, preserving custom entries. */
    tabs(1); key(KEY_RETURN); tabs(1); key(KEY_RETURN); go();
    assert(saw_lobby && !strcmp(saved_address,"lobby.example:9910"));
    assert(fb_servers_decode(&list,storage,size) && list.count==2);
    /* Fill all five slots, exercise the full-list feedback and wrap layout. */
    for(i=0;i<3;++i) { char address[60]; snprintf(address,sizeof address,"host%u.example",i); assert(fb_servers_add(&list,address)); }
    fb_servers_encode(&list,storage);
    tabs(7); key(KEY_RETURN); key(KEY_ESC); go(); assert(saw_full && saw_five);
    /* Delete a selected row, then save a different selection. */
    tabs(2); key(KEY_BACK); key(KEY_RETURN); key(KEY_RETURN); go();
    assert(fb_servers_decode(&list,storage,size) && list.count==4);
    /* A failed history write leaves the menu open for retry. */
    fail_writes=1; before=writes; tabs(7); key(KEY_RETURN); key(KEY_RETURN); go();
    assert(saw_error && writes==before+2);
    /* Read failure and malformed storage never overwrite the history. */
    fail_reads=1; before=writes; key(KEY_ESC); go(); assert(writes==before);
    storage[0]='?'; key(KEY_ESC); go(); assert(writes==before);
    /* Long entries remain bounded on screen and retain their full saved value. */
    memset(&list, 0, sizeof list);
    {
        char host[FB_NIO_HOST_MAX+1];
        memset(host, 'a', sizeof host-1); host[sizeof host-1]=0;
        assert(fb_servers_add(&list, host));
        fb_servers_encode(&list, storage);
        tabs(2); key(KEY_RETURN); key(KEY_RETURN); go();
        assert(!strcmp(saved_address, list.address[0]));
    }
    /* Entering F1 with old native history must display it, not a retry screen. */
    memset(storage, 0, sizeof storage);
    memcpy(storage, "FBS1", 4); storage[4]=1; size=295;
    strcpy((char *)storage+5, "192.168.1.100:9910");
    tabs(2); key(KEY_RETURN); key(KEY_RETURN); go();
    assert(!strcmp(saved_address, "192.168.1.100:9910"));
    assert(size == FB_SERVERS_BYTES && !memcmp(storage, "FBS2", 4));
    puts("setup add/delete/select/cancel/full-list/lobby/retry passed");
}

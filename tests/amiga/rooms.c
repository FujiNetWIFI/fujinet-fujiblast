#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "input.h"
#include "sound.h"
#include "nstrans.h"
uint16_t frame_count;
char my_name[NAME_LEN+1], srv_host[FB_HOST_MAX+1] = "test", srv_table[TABLE_LEN+1];
uint16_t srv_port = 9910;
uint8_t nst_buf[128], nst_taken, inp_dir, inp_btn, ev_btn, ev_key, prev_dir;
static char screen[25][41];
static unsigned steps, requests, record, wire_n, opens, closes, errors, saw_atomic, saw_empty;
static uint16_t requested;
static uint8_t wire[32];
static int mode;
enum { REFRESH, REMOVED, EMPTY, ESCAPE, JOIN_PENDING, TIMEOUT, DISCONNECT };
void frame_wait(void) { ++frame_count; assert(++steps < 1500); }
void input_poll(void) {}
uint8_t pressed(uint8_t bit) { return (inp_dir & bit) && !(prev_dir & bit); }
void input_take(void)
{
    inp_dir = ev_key = ev_btn = 0;
    if (steps == 10) inp_dir = IN_DOWN;
    if (requests == 2 && record == 1 && mode == REFRESH) {
        assert(screen[7][18] == '1');
        assert(!memcmp(tables[1], "humans", 6));
        saw_atomic = 1;
    }
    if (requests >= 2 && (mode == ESCAPE || mode == JOIN_PENDING))
        ev_key = mode == ESCAPE ? KEY_ESC : KEY_RETURN;
    if (requests == 2 && record == 2 && (mode == REFRESH || mode == REMOVED))
        ev_key = KEY_RETURN;
    if (mode == EMPTY && requests == 2 && record) {
        ev_key = KEY_RETURN; /* Must not join a stale row from the empty list. */
        if (strstr(screen[9], "NO ROOMS")) saw_empty = 1;
    }
    if (mode == EMPTY && requests == 3 && record == 2) ev_key = KEY_RETURN;
}
void gfx_begin(void) { memset(screen, ' ', sizeof screen); for(unsigned i=0;i<25;++i) screen[i][40]=0; }
void gfx_row(uint8_t r,uint8_t m,uint8_t p) { (void)r;(void)m;(void)p; }
void gfx_show(void) {}
void gfx_text(uint8_t r,uint8_t c,const char *s,uint8_t colour)
{ (void)colour; assert(r<25 && c+strlen(s)<=40); memcpy(screen[r]+c,s,strlen(s)); }
void centre(uint8_t r,const char *s,uint8_t c,uint8_t w) { gfx_text(r,(w-strlen(s))/2,s,c); }
void message(const char *a,const char *b,uint8_t f)
{ (void)b;(void)f; if (strcmp(a,"CONNECTING")) ++errors; }
void snd_play(uint8_t s) { (void)s; }
void snd_toggle_mute(void) {}
uint8_t nst_open(const char *h,uint16_t p) { (void)h;(void)p; ++opens; return 1; }
void nst_close(void) { ++closes; }
uint8_t nst_lost(void) { return mode == DISCONNECT && requests == 2; }
uint8_t nst_garbled(void) { return 0; }
void nst_put(uint8_t c)
{
    if (c) { assert(wire_n < sizeof wire); wire[wire_n++] = c; }
    else {
        if (wire_n > 1 && wire[1] == SC_TABLES) {
            ++requests; record=0; requested=frame_count;
        }
        wire_n=0;
    }
}
uint8_t nst_frame(void)
{
    unsigned count = 2;
    const char *id;
    if (!requests || record >= 2) return 0;
    if (requests > 1) {
        if (mode == TIMEOUT || mode == ESCAPE || mode == JOIN_PENDING) return 0;
        if ((uint16_t)(frame_count-requested) < (record ? 12 : 6)) return 0;
        if (mode == EMPTY && requests == 2) {
            if (record) return 0;
            memset(nst_buf,0,3); nst_buf[0]=CS_TABLE; ++record; return 3;
        }
        if (mode == REMOVED) count=1;
    }
    if (record >= count) { record=2; return 0; }
    memset(nst_buf,0,sizeof nst_buf);
    nst_buf[0]=CS_TABLE; nst_buf[1]=record; nst_buf[2]=count;
    id = requests == 1 ? (record ? "humans" : "bots") : (record ? "bots" : "humans");
    if (mode == REMOVED && requests > 1) id="bots";
    memcpy(nst_buf+3,id,strlen(id)); memcpy(nst_buf+11,id,strlen(id));
    nst_buf[23] = requests == 1 ? 1 : 3;
    nst_buf[24] = 4; nst_buf[25] = requests == 1 ? PH_WAIT : PH_PLAY;
    ++record;
    return 3+TABLE_REC;
}
int main(void)
{
    for (mode=REFRESH;mode<=DISCONNECT;++mode) {
        net_close();
        steps=requests=record=wire_n=opens=closes=errors=saw_atomic=saw_empty=0;
        inp_dir=prev_dir=0; frame_count=65500; srv_table[0]=0;
        if (mode == ESCAPE || mode == TIMEOUT || mode == DISCONNECT) {
            assert(!amiga_table_list()); assert(closes == 1);
            assert(errors == (mode != ESCAPE));
        } else {
            assert(amiga_table_list()); assert(opens == 1 && !errors);
            assert(!strcmp(srv_table, mode == REMOVED ? "bots" : "humans"));
            if (mode == REFRESH) {
                assert(saw_atomic && screen[5][18]=='3' && !memcmp(screen[5]+24,"PLAYING",7));
                assert(steps < 340);
            }
            if (mode == REMOVED) assert(screen[7][4]==' ');
            if (mode == EMPTY) assert(saw_empty && requests == 3);
            if (mode == JOIN_PENDING) assert(steps < 310);
        }
    }
    puts("room refresh: atomic list, selection, counts/status, removal, empty/recovery, input, timeout and clock wrap passed");
}

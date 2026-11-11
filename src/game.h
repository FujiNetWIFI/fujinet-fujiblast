/*
 * game.h -- FujiBlast's client, platform neutral: the state the server
 * pushes (proto.h's layout), the link, the player's own walking, the
 * screens.
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "proto.h"

#define NONE 0xFF

/* the last whole state */
extern uint8_t st[STATE_SIZE];
/* (by table, not s * PLAYER_SIZE or 1 << s: cc65 multiplies and shifts in
 * 16 bits, and these run for every seat every frame) */
extern const uint8_t seat_off[SEATS], seat_bit[SEATS];     /* net.c */
#define PL(s, f)  st[(uint8_t)(OFF_PLAYERS + seat_off[s] + (f))]
#define MAPT(t)   st[OFF_MAP + (t)]
#define NAME(s)   ((const char *)st + OFF_NAMES + (s) * NAME_LEN)
#define SEATED(s) (st[OFF_SEATED] & seat_bit[s])
#define ALIVE(s)  (st[OFF_ALIVE] & seat_bit[s])
#define INPLAY(s) (st[OFF_INPLAY] & seat_bit[s])

extern char my_name[NAME_LEN + 1];
extern char srv_host[61];
extern uint16_t srv_port;
extern char srv_table[TABLE_LEN + 1];
extern uint8_t my_seat;             /* NONE: watching */

/* net.c */
enum { NET_NONE, NET_STATE, NET_ERROR, NET_LOST };
#define MAX_TABLES 8
extern uint8_t tables[MAX_TABLES][TABLE_REC];
extern uint8_t net_err, net_heard;
extern uint8_t net_map;             /* the map changed since play_state looked */
uint8_t net_open(void);
void net_hello(void);
uint8_t net_poll(void);
uint8_t net_tables(void);           /* count, 0: none or no answer */
void net_move(uint8_t x, uint8_t y, uint8_t dir, uint8_t warp);
uint8_t net_act(uint8_t act, uint8_t a1, uint8_t a2);
void net_leave(void);
void net_close(void);

/* move.c */
extern uint8_t mv_x, mv_y, mv_dir, mv_acc, mv_bump, pend_tile;
void mv_reset(uint8_t x, uint8_t y, uint8_t dir);
uint8_t mv_step(uint8_t d, uint8_t lv);
uint8_t centre_tile(uint8_t x, uint8_t y);

/* play.c: the arena */
void play_enter(void);
void play_state(void);              /* a new state came */
void play_frame(void);

/* menus.c */
void title(void);
void name_entry(void);
uint8_t table_list(void);           /* 1: srv_table picked */

/* screens.c */
extern uint8_t prev_dir;            /* the stick, last frame */
void tick(void);                    /* a frame: wait, input, M for sound */
uint8_t pressed(uint8_t bit);       /* the stick pushed this frame (not held) */
void centre(uint8_t row, const char *s, uint8_t colour, uint8_t width);
void message(const char *a, const char *b, uint8_t frames);
void wait_enter(void);
void wait_state(void);
void wait_frame(void);
void results_enter(uint8_t final);
void results_frame(void);

/* appkey.c */
void appkey_load(void);
void appkey_save_name(void);
void appkey_save_room(void);
void appkey_clear_room(void);

#endif

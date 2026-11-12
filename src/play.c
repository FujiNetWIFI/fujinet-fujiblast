/*
 * play.c -- the arena: drawing the state, walking our player, bombs.
 *
 * We walk our own player (move.c) and tell the server where it is; the
 * server has the rest. Our bomb shows the moment we drop it (pend_tile)
 * until the server's state has it. Other players are drawn walking toward
 * where the server last said they were.
 */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "gfxcodes.h"
#include "input.h"
#include "plat.h"
#include "sound.h"

#define HALF_W (ARENA_W * 2)
#define HALF_H (ARENA_H * 2)

static uint8_t drawn[TILES];            /* the tile on screen (NONE: redraw) */
static uint8_t mov_on[MOVERS], mov_hx[MOVERS], mov_hy[MOVERS];
static uint8_t disp_x[SEATS], disp_y[SEATS], die_t[SEATS], was_alive;
static uint8_t my_warp, sent_x, sent_y, sent_d, pend_left;
static uint16_t sent_t, bump_t;
static uint8_t hud_timer, hud_alive, hud_wins[SEATS], stats[5], last_phase;
static uint8_t scan;                    /* drawn[] has holes: compare all the tiles */

#ifndef MOVE_EVERY
#define MOVE_EVERY 3            /* jiffies between our moves, at least (a slow
                                 * link sends fewer) */
#endif

static uint8_t is_flame(uint8_t t)
{
    return t >= T_FLAME_C && t <= T_FLAME_D;
}

static void hud(void)
{
    static const uint8_t col[SEATS] = { 0, 4, 12, 16 };
    static char b[21];
    static uint8_t c[20];
    uint8_t s, i, t = st[OFF_TIMER];
    memset(b, ' ', 20);
    for (s = 0; s < SEATS; ++s) {
        c[col[s]] = c[col[s] + 1] = c[col[s] + 2] = c[col[s] + 3] = s;
        if (!INPLAY(s))
            continue;
        for (i = 0; i < 3; ++i)
            b[col[s] + i] = ALIVE(s) || st[OFF_PHASE] != PH_PLAY ? (NAME(s)[i] ? NAME(s)[i] : ' ') : '-';
        b[col[s] + 3] = '0' + PL(s, P_WINS) % 10;
    }
    c[8] = c[9] = c[10] = c[11] = 0;
    if (st[OFF_PHASE] == PH_COUNTDOWN) {
        b[9] = '0' + t % 10;
    } else {
        b[8] = '0' + t / 60;
        b[9] = ':';
        b[10] = '0' + (t % 60) / 10;
        b[11] = '0' + t % 10;
    }
    gfx_big_line(0, b, c);
    hud_timer = t;
    hud_alive = st[OFF_ALIVE];
    for (s = 0; s < SEATS; ++s)
        hud_wins[s] = PL(s, P_WINS);
}

static void stats_draw(uint8_t force)
{
    uint8_t v[5], i;
    if (my_seat == NONE) {
        v[0] = v[1] = v[2] = v[3] = v[4] = 0;
    } else {
        v[0] = PL(my_seat, P_RANGE);
        v[1] = PL(my_seat, P_BOMBS);
        v[2] = PL(my_seat, P_SPEED) + 1;
        v[3] = PL(my_seat, P_FLAGS) & PF_KICK;
        v[4] = PL(my_seat, P_FLAGS) & PF_GLOVE;
    }
    for (i = 0; i < 5; ++i) {
        if (force || v[i] != stats[i]) {
            if (!force && v[i] > stats[i])
                snd_play(SND_PICKUP);
            stats[i] = v[i];
            gfx_stat(i, v[i]);
        }
    }
}

void play_enter(void)
{
    uint8_t s;
    gfx_begin();
    gfx_arena();
    memset(drawn, NONE, sizeof(drawn));
    scan = 1;
    memset(mov_on, 0, sizeof(mov_on));
    memset(die_t, 0, sizeof(die_t));
    pend_tile = NONE;
    for (s = 0; s < SEATS; ++s) {
        disp_x[s] = PL(s, P_X);
        disp_y[s] = PL(s, P_Y);
    }
    was_alive = st[OFF_ALIVE];
    last_phase = st[OFF_PHASE];
    if (my_seat != NONE) {
        my_warp = PL(my_seat, P_WARP);
        mv_reset(PL(my_seat, P_X), PL(my_seat, P_Y), DIR_DOWN);
    }
    stats_draw(1);
    play_state();
    gfx_round(st[OFF_ROUND]);
    hud();
    gfx_show();
    if (st[OFF_PHASE] == PH_COUNTDOWN)
        snd_music(MUS_ROUND);
}

/* The tiles a bomb drawn at half-tile hx, hy covers: redraw them later. */
static void dirty(uint8_t hx, uint8_t hy)
{
    uint8_t c = hx >> 1, r = hy >> 1;
    scan = 1;
    drawn[r * ARENA_W + c] = NONE;
    if ((hx & 1) && c + 1 < ARENA_W)
        drawn[r * ARENA_W + c + 1] = NONE;
    if ((hy & 1) && r + 1 < ARENA_H) {
        drawn[(r + 1) * ARENA_W + c] = NONE;
        if ((hx & 1) && c + 1 < ARENA_W)
            drawn[(r + 1) * ARENA_W + c + 1] = NONE;
    }
}

void play_state(void)
{
    uint8_t t, c, r, want, m, boom = 0, s;
    const uint8_t *mv;

    /* our own position, when the server says so */
    if (my_seat != NONE && PL(my_seat, P_WARP) != my_warp) {
        my_warp = PL(my_seat, P_WARP);
        mv_reset(PL(my_seat, P_X), PL(my_seat, P_Y), mv_dir);
    }
    if (pend_tile != NONE && MAPT(pend_tile) != T_FLOOR)
        pend_tile = NONE;               /* the server has it (or it's taken) */

    for (m = 0; m < MOVERS; ++m)
        if (mov_on[m]) {
            dirty(mov_hx[m], mov_hy[m]);
            mov_on[m] = 0;
        }
    /* (the scan is the slow part of a state: skipped when only players moved) */
#define TILE_CHECK \
            want = MAPT(t); \
            if (t == pend_tile) \
                want = T_BOMB; \
            if (drawn[t] != want) { \
                if (want == T_FLAME_C && drawn[t] != NONE && !is_flame(drawn[t])) \
                    boom = 1; \
                if (want == T_DUD && drawn[t] == T_BOMB) \
                    snd_play(SND_FIZZLE); \
                drawn[t] = want; \
                gfx_tile(c, r, want); \
            }
#ifdef PLAT_MAP_NEXT
    /* the platform finds the tiles that differ (an 8088's repe cmpsb) */
    if (scan || net_map || pend_tile != NONE)
        for (t = 0; (t = map_next(st + OFF_MAP, drawn, t)) < TILES; ++t) {
            r = t / ARENA_W;
            c = t - r * ARENA_W;
            TILE_CHECK
        }
#else
    t = 0;
    if (scan || net_map || pend_tile != NONE)
    for (r = 0; r < ARENA_H; ++r)
        for (c = 0; c < ARENA_W; ++c, ++t) {
            TILE_CHECK
        }
#endif
    scan = net_map = 0;
    if (boom)
        snd_play(SND_BOOM);
    mv = st + OFF_MOVERS;
    for (m = 0; m < MOVERS; ++m, mv += MOVER_SIZE) {
        if (!(mv[0] & 3))
            continue;
        mov_hx[m] = ((uint8_t)(mv[1] + 4) >> 3) % HALF_W;
        mov_hy[m] = ((uint8_t)(mv[2] + 4) >> 3) % HALF_H;
        if (mov_hx[m] == HALF_W - 1)
            mov_hx[m] = HALF_W - 2;     /* wrapping: shown at the edge */
        if (mov_hy[m] == HALF_H - 1)
            mov_hy[m] = HALF_H - 2;
        gfx_bomb_at(mov_hx[m], mov_hy[m], mv[0] & M_DUD);
        mov_on[m] = 1;
    }

    for (s = 0; s < SEATS; ++s) {
        if ((was_alive & seat_bit[s]) && !ALIVE(s) && st[OFF_PHASE] == PH_PLAY) {
            die_t[s] = 1;
            snd_play(SND_DIE);
        }
    }
    if (st[OFF_PHASE] == PH_COUNTDOWN && hud_timer != st[OFF_TIMER])
        snd_play(SND_BEEP);
    if (st[OFF_PHASE] == PH_PLAY && last_phase == PH_COUNTDOWN)
        snd_play(SND_GO);
    last_phase = st[OFF_PHASE];
    for (s = 0; s < SEATS && hud_wins[s] == PL(s, P_WINS); ++s)
        ;
    if (hud_timer != st[OFF_TIMER] || hud_alive != st[OFF_ALIVE] || s < SEATS)
        hud();
    was_alive = st[OFF_ALIVE];
    stats_draw(0);
}

/* The stick to a direction; on a diagonal, the turn first (the way we
 * aren't going), and if that's walled, on as we were. */
static uint8_t try_dirs(uint8_t lv)
{
    uint8_t s = inp_dir, h = NONE, v = NONE, a, b, x, y, acc;
    if (s & IN_UP)
        v = DIR_UP;
    else if (s & IN_DOWN)
        v = DIR_DOWN;
    if (s & IN_LEFT)
        h = DIR_LEFT;
    else if (s & IN_RIGHT)
        h = DIR_RIGHT;
    if (h == NONE || v == NONE)
        return mv_step(h == NONE ? v : h, lv);
    a = (mv_dir == DIR_LEFT || mv_dir == DIR_RIGHT) ? v : h;
    b = a == v ? h : v;
    x = mv_x;
    y = mv_y;
    acc = mv_acc;
    if (mv_step(a, lv))
        return 1;
    mv_x = x;
    mv_y = y;
    mv_acc = acc;
    return mv_step(b, lv);
}

/* (cc65 stacks every argument but the last, so these take one; play_frame
 * runs every jiffy, four seats each time) */
static const uint8_t fr_base[4] = { FR_UP0, FR_RIGHT0, FR_DOWN0, FR_LEFT0 };
static const uint8_t fr_walk[4] = { 0, 1, 0, 2 };
static uint8_t walk_f;                  /* this jiffy's walking frame, 0-2 */
#define FRAME_OF(dir, moving) (fr_base[(dir) & 3] + ((moving) ? walk_f : 0))

static uint8_t tw_at, tw_to;            /* toward(): from tw_at a step to tw_to */
static uint8_t toward(void)
{
    uint8_t at = tw_at, to = tw_to, d = at > to ? at - to : to - at;
    if (d > UNIT)
        return to;
    if (d > 3)
        d = 3;
    return at > to ? at - d : at + d;
}

void play_frame(void)
{
    uint8_t s, moved = 0, t, me, d;
    const uint8_t *pl;

    me = my_seat != NONE && ALIVE(my_seat) && st[OFF_PHASE] == PH_PLAY;
    if (me) {
        moved = try_dirs(PL(my_seat, P_SPEED));
        if (mv_bump != NONE && frame_count - bump_t >= 30 &&
            ((PL(my_seat, P_FLAGS) & PF_KICK) || MAPT(mv_bump) == T_DUD)) {
            bump_t = frame_count;
            net_act(ACT_BUMP, mv_bump, mv_dir);
            if (PL(my_seat, P_FLAGS) & PF_KICK)
                snd_play(SND_KICK);
        }
        if (ev_btn) {
            t = centre_tile(mv_x, mv_y);
            if ((PL(my_seat, P_FLAGS) & PF_GLOVE) && (MAPT(t) == T_BOMB || t == pend_tile)) {
                net_act(ACT_THROW, mv_dir, 0);
                snd_play(SND_THROW);
                pend_tile = NONE;
            } else if (MAPT(t) == T_FLOOR && t != pend_tile &&
                       PL(my_seat, P_OUT) + (pend_tile != NONE) < PL(my_seat, P_BOMBS)) {
                net_act(ACT_DROP, t, 0);
                pend_tile = t;
                pend_left = 90;
                drawn[t] = NONE;
                gfx_tile(t % ARENA_W, t / ARENA_W, T_BOMB);
                drawn[t] = T_BOMB;
                snd_play(SND_DROP);
            }
        }
        d = mv_dir | (moved ? P_DIR_MOVING : 0);
        if ((mv_x != sent_x || mv_y != sent_y || d != sent_d) && frame_count - sent_t >= MOVE_EVERY ||
            frame_count - sent_t >= 60) {
            sent_x = mv_x;
            sent_y = mv_y;
            sent_d = d;
            sent_t = frame_count;
            net_move(mv_x, mv_y, d, my_warp);
        }
    }
    if (pend_tile != NONE && pend_left && !--pend_left) {
        drawn[pend_tile] = NONE;        /* never came: off again at the next state */
        pend_tile = NONE;
        scan = 1;
    }

    walk_f = fr_walk[((uint8_t)frame_count >> 3) & 3];
    for (s = 0; s < SEATS; ++s) {
        if (!INPLAY(s)) {
            spr_hide(s);
            continue;
        }
        if (!ALIVE(s)) {
            if (die_t[s] && die_t[s] < 48) {
                spr_arena(s, disp_x[s], disp_y[s], FR_DIE0 + die_t[s] / 12, s);
                ++die_t[s];
            } else {
                spr_hide(s);
            }
            continue;
        }
        if (s == my_seat && me) {
            disp_x[s] = mv_x;
            disp_y[s] = mv_y;
            spr_arena(s, mv_x, mv_y, FRAME_OF(mv_dir, moved), s);
        } else {
            pl = &PL(s, 0);
            tw_at = disp_x[s];
            tw_to = pl[P_X];
            t = toward();
            disp_x[s] = t;
            tw_at = disp_y[s];
            tw_to = pl[P_Y];
            t = toward();
            disp_y[s] = t;
            d = pl[P_DIR];
            spr_arena(s, disp_x[s], disp_y[s], FRAME_OF(d, d & P_DIR_MOVING), s);
        }
    }
}

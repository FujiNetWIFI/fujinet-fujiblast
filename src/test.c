/*
 * test.c -- offline test scenes, for the emulator rigs (tools/atarisim):
 * the screens on canned states, no network. At start-up we mark
 * test_hook[1] $C3 and wait a third of a second for a rig to poke $5A into
 * test_hook[0], the scene into [2] and its argument into [3]:
 *   1  the arena, seat 0 walking (the stick through inp_hook), [3] its powers
 *   2  the waiting room       3  a round's end       4  the game's end
 * An explosion goes off in the arena every 4 seconds.
 */
#include <string.h>
#include "game.h"
#include "gfx.h"
#include "input.h"
#include "plat.h"

#ifndef TEST_ARG
#define TEST_ARG 0
#endif

uint8_t test_hook[4];

static void canned(void)
{
    static const char names[SEATS][NAME_LEN] = { "ANN", "BOB", "CAT", "DOT" };
    static const uint8_t corner[SEATS][2] = { { 0, 0 }, { MAX_X, MAX_Y }, { MAX_X, 0 }, { 0, MAX_Y } };
    uint8_t s, c, r, t;
    memset(st, 0, sizeof(st));
    st[OFF_PHASE] = PH_PLAY;
    st[OFF_ROUND] = 2;
    st[OFF_ROUNDS] = 5;
    st[OFF_TIMER] = 154;
    st[OFF_SEATED] = 0x0F;
    st[OFF_INPLAY] = 0x0F;
    st[OFF_ALIVE] = 0x07;
    st[OFF_CONN] = 0x0F;
    st[OFF_HOST] = 0;
    st[OFF_RESULT] = 2;
    st[OFF_SEQ] = 1;
    for (s = 0; s < SEATS; ++s) {
        memcpy((char *)st + OFF_NAMES + s * NAME_LEN, names[s], NAME_LEN);
        PL(s, P_X) = corner[s][0];
        PL(s, P_Y) = corner[s][1];
        PL(s, P_DIR) = DIR_DOWN;
        PL(s, P_RANGE) = 2;
        PL(s, P_BOMBS) = 1;
        PL(s, P_WINS) = s;
    }
    t = 0;
    for (r = 0; r < ARENA_H; ++r)
        for (c = 0; c < ARENA_W; ++c, ++t) {
            if ((c & 1) && (r & 1))
                MAPT(t) = T_PILLAR;
            else if (c > 2 && c < 10 && (r == 0 || r == 10 || c == 4 || c == 8))
                MAPT(t) = T_BLOCK;
        }
    /* one of everything, in the open middle */
    MAPT(2 * ARENA_W + 0) = T_BOMB;
    MAPT(4 * ARENA_W + 0) = T_DUD;
    MAPT(2 * ARENA_W + 12) = T_FIRE;
    MAPT(4 * ARENA_W + 12) = T_BOMBUP;
    MAPT(6 * ARENA_W + 12) = T_SPEED;
    MAPT(8 * ARENA_W + 12) = T_KICK;
    MAPT(6 * ARENA_W + 0) = T_GLOVE;
    MAPT(8 * ARENA_W + 0) = T_BURN;
    MAPT(5 * ARENA_W + 6) = T_FLAME_C;
    MAPT(5 * ARENA_W + 5) = T_FLAME_H;
    MAPT(5 * ARENA_W + 4) = T_BLOCK;
    MAPT(5 * ARENA_W + 7) = T_FLAME_H;
    MAPT(5 * ARENA_W + 8) = T_FLAME_R;
    MAPT(4 * ARENA_W + 6) = T_FLAME_V;
    MAPT(3 * ARENA_W + 6) = T_FLAME_U;
    MAPT(6 * ARENA_W + 6) = T_FLAME_D;
    st[OFF_MOVERS] = M_SLIDE;
    st[OFF_MOVERS + 1] = 2 * UNIT + 8;
    st[OFF_MOVERS + 2] = 2 * UNIT;
}

static void arena(uint8_t powers)
{
    uint16_t t = 0;
    uint8_t i;
    my_seat = 0;
    PL(0, P_FLAGS) = powers & (PF_KICK | PF_GLOVE);
    PL(0, P_SPEED) = powers >> 4;
    play_enter();
    for (;;) {
        frame_wait();
        input_poll();
        input_take();
        if (++t % 240 == 0) {          /* a blast in the top lane, and back */
            for (i = 2; i < 11; ++i)
                MAPT(i) = (t / 240) & 1 ? T_FLAME_H : (i == 2 || i == 10 ? T_FLOOR : T_BLOCK);
            st[OFF_TIMER]--;
            play_state();
        }
        play_frame();
    }
}

void test_check(void)
{
    uint8_t n;
    test_hook[1] = 0xC3;
#ifdef TEST_SCENE                        /* a rig that can't poke memory: built in */
    test_hook[0] = 0x5A;
    test_hook[2] = TEST_SCENE;
    test_hook[3] = TEST_ARG;
#endif
    for (n = 0; n < 20 && test_hook[0] != 0x5A; ++n)
        frame_wait();
    if (test_hook[0] != 0x5A)
        return;
    test_hook[0] = 0xA5;
    canned();
    strcpy(my_name, "ANN");
    strcpy(srv_table, "blast1");
    inp_hook = 1;
    switch (test_hook[2]) {
    case 1:
        arena(test_hook[3]);
    case 2:
        st[OFF_PHASE] = PH_WAIT;
        my_seat = 0;
        wait_enter();
        break;
    case 3:
    case 4:
        st[OFF_PHASE] = test_hook[2] == 3 ? PH_ROUNDEND : PH_GAMEOVER;
        st[OFF_TIMER] = 5;
        results_enter(test_hook[2] == 4);
        break;
    }
    for (;;) {
        frame_wait();
        input_poll();
        input_take();
        if (test_hook[2] == 2)
            wait_frame();
        else
            results_frame();
    }
}

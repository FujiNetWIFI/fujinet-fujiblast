' ===========================================================================
' test.bas -- offline test scenes, for the rigs (tools/intvsim/intvsim.py
' "test SCENE ARG"), as src/test.c: the screens on canned states, no
' network. A rig pokes $5A, $A5 into TEST_HOOK + 2 and + 3, the scene into
' + 4 and its argument into + 5 before the console starts:
'   1  the arena, seat 0 walking (the disc through input.bas's hook), the
'      argument its powers (bits 0-1 kick and glove, 4-7 speed)
'   2  the waiting room       3  a round's end       4  the game's end
' A blast goes off in the arena's top lane every 4 seconds.
' ===========================================================================

    DIM tst_s, tst_a, tst_k
    DIM #tst_t

test_check: PROCEDURE
    IF WITH_TEST = 0 THEN RETURN
    IF rb(TEST_HOOK + 2) <> $5A OR rb(TEST_HOOK + 3) <> $A5 THEN RETURN
    tst_s = rb(TEST_HOOK + 4) : tst_a = rb(TEST_HOOK + 5)
    POKE TEST_HOOK + 2, 0
    GOSUB canned
    IF tst_s = 1 THEN GOTO test_arena
    IF tst_s = 2 THEN
        POKE ST + OFF_PHASE, PH_WAIT
        my_seat = 0
        GOSUB wait_enter
    ELSE
        IF tst_s = 3 THEN POKE ST + OFF_PHASE, PH_ROUNDEND ELSE POKE ST + OFF_PHASE, PH_GAMEOVER
        POKE ST + OFF_TIMER, 5
        res_final = (tst_s = 4) AND 1
        GOSUB results_enter
    END IF
    DO
        GOSUB tick
        IF tst_s = 2 THEN GOSUB wait_frame ELSE GOSUB results_frame
    LOOP WHILE 1

test_arena:
    my_seat = 0
    POKE ST + OFF_PLAYERS + P_FLAGS, tst_a AND (PF_KICK + PF_GLOVE)
    POKE ST + OFF_PLAYERS + P_SPEED, tst_a / 16
    GOSUB play_enter
    #tst_t = 0
    DO
        GOSUB frame_wait
tp_a:
        GOSUB input_take
        FOR tst_k = 1 TO owed
            GOSUB jiffy_step
            #tst_t = #tst_t + 1
            IF #tst_t % 240 = 0 THEN
                FOR i = 2 TO 10
                    IF (#tst_t / 240) AND 1 THEN
                        c = T_FLAME_H
                    ELSEIF i = 2 OR i = 10 THEN
                        c = T_FLOOR
                    ELSE
                        c = T_BLOCK
                    END IF
                    POKE ST + OFF_MAP + i, c
                    ct = i : GOSUB tile_at
                NEXT i
                POKE ST + OFF_TIMER, rb(ST + OFF_TIMER) - 1
                GOSUB play_state
            END IF
            GOSUB play_logic
        NEXT tst_k
        GOSUB play_draw
tp_b:
    LOOP WHILE 1
END

    DIM cn_s
canned: PROCEDURE
    FOR #a = ST TO ST + STATE_SIZE - 1
        POKE #a, 0
    NEXT #a
    POKE ST + OFF_PHASE, PH_PLAY
    POKE ST + OFF_ROUND, 2
    POKE ST + OFF_ROUNDS, 5
    POKE ST + OFF_TIMER, 154
    POKE ST + OFF_SEATED, $0F
    POKE ST + OFF_INPLAY, $0F
    POKE ST + OFF_ALIVE, $07
    POKE ST + OFF_CONN, $0F
    POKE ST + OFF_HOST, 0
    POKE ST + OFF_RESULT, 2
    POKE ST + OFF_SEQ, 1
    FOR cn_s = 0 TO 3
        FOR i = 0 TO 2
            POKE ST + OFF_NAMES + cn_s * NAME_LEN + i, cn_names(cn_s * 3 + i)
        NEXT i
        #a = ST + OFF_PLAYERS + cn_s * PLAYER_SIZE
        POKE #a + P_X, cn_corner(cn_s * 2)
        POKE #a + P_Y, cn_corner(cn_s * 2 + 1)
        POKE #a + P_DIR, DIR_DOWN
        POKE #a + P_RANGE, 2
        POKE #a + P_BOMBS, 1
        POKE #a + P_WINS, cn_s
    NEXT cn_s
    t = 0
    FOR r = 0 TO ARENA_H - 1
        FOR c = 0 TO ARENA_W - 1
            IF (c AND 1) <> 0 AND (r AND 1) <> 0 THEN
                POKE ST + OFF_MAP + t, T_PILLAR
            ELSEIF c > 2 AND c < 10 AND (r = 0 OR r = 10 OR c = 4 OR c = 8) THEN
                POKE ST + OFF_MAP + t, T_BLOCK
            END IF
            t = t + 1
        NEXT c
    NEXT r
    ' one of everything, in the open middle
    FOR i = 0 TO 15
        POKE ST + OFF_MAP + cn_spot(i * 2), cn_spot(i * 2 + 1)
    NEXT i
    POKE ST + OFF_MOVERS, M_SLIDE
    POKE ST + OFF_MOVERS + 1, 2 * UNIT + 8
    POKE ST + OFF_MOVERS + 2, 2 * UNIT
    FOR i = 0 TO 3
        POKE MYNAME + i, cn_names(i)
    NEXT i
    POKE MYNAME + 3, 0
    FOR i = 0 TO 6
        POKE SRVTBL + i, cn_table(i)
    NEXT i
END

cn_names:
    DATA 65, 78, 78, 66, 79, 66, 67, 65, 84, 68, 79, 84      ' ANN BOB CAT DOT
cn_corner:
    DATA 0, 0, MAX_X, MAX_Y, MAX_X, 0, 0, MAX_Y
cn_spot:    ' tile (r * 13 + c), what
    DATA 26, T_BOMB, 52, T_DUD, 38, T_FIRE, 64, T_BOMBUP, 90, T_SPEED, 116, T_KICK, 78, T_GLOVE
    DATA 104, T_BURN, 71, T_FLAME_C, 70, T_FLAME_H, 69, T_BLOCK, 72, T_FLAME_H, 73, T_FLAME_R
    DATA 58, T_FLAME_V, 45, T_FLAME_U, 84, T_FLAME_D
cn_table:
    DATA 98, 108, 97, 115, 116, 49, 0     ' blast1

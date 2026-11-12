' ===========================================================================
' play.bas -- the arena, as src/play.c: drawing the state, walking our
' player, bombs.
'
' We walk our own player (move.bas) and tell the server where it is; the
' server has the rest. Our bomb shows the moment we drop it (pend_tile)
' until the server's state has it. Other players are drawn walking toward
' where the server last said they were.
'
' The map's tiles are drawn as their patches come (PATCH, netasm.asm), so a
' new state only redraws what lies over them: kicked and thrown bombs (as
' half cards between tiles), and our own bomb. play_logic runs once a
' jiffy, play_draw once a frame.
' ===========================================================================

    CONST HALF_W = ARENA_W * 2
    CONST HALF_H = ARENA_H * 2
    CONST MOVE_EVERY = 4        ' jiffies between our moves, at least

    DIM my_seat                 ' NONE: watching
    DIM disp_x(SEATS), disp_y(SEATS), die_t(SEATS)
    DIM mov_on(MOVERS), mov_hx(MOVERS), mov_hy(MOVERS)
    DIM stats(5)
    DIM was_alive, my_warp, sent_x, sent_y, sent_d, pend_left
    DIM hud_timer, hud_alive, last_phase, in_arena
    DIM #sent_t, #bump_t
    UNSIGNED #sent_t, #bump_t
    DIM ps, pt, pv, pm, pd, me, me_moved, mov_any, st_rr
    DIM sk_seat, sk_lim, sk_n, sd_walk   ' seats.asm's

    DEF FN pl(s, f) = rb(ST + OFF_PLAYERS + (s) * PLAYER_SIZE + (f))
    DEF FN mapt(t) = rb(ST + OFF_MAP + (t))

' ---- the HUD (row 0): each seat's 3 letters and trophies, the clock ----------

hud: PROCEDURE
    pt = rb(ST + OFF_TIMER)
    pv = rb(ST + OFF_ALIVE)
    pm = rb(ST + OFF_INPLAY)
    FOR ps = 0 TO 3
        #p = hud_col(ps)
        IF pm AND tbit(ps) THEN
            #cw = seat_hud(ps)
            #a = ST + OFF_NAMES + ps * NAME_LEN
            FOR i = 0 TO 2
                c = rb(#a + i)
                IF c = 0 THEN c = 32
                IF (pv AND tbit(ps)) = 0 AND rb(ST + OFF_PHASE) = PH_PLAY THEN c = 45
                #BACKTAB(#p + i) = (c - 32) * 8 + #cw
            NEXT i
            v = pl(ps, P_WINS)
            WHILE v > 9
                v = v - 10
            WEND
            #BACKTAB(#p + 3) = (16 + v) * 8 + #cw
        ELSE
            FOR i = 0 TO 3
                #BACKTAB(#p + i) = 0
            NEXT i
        END IF
    NEXT ps
    GOSUB hud_clock
    hud_alive = pv
END

' hud_clock: the clock (columns 8-11): the countdown, or minutes:seconds.
hud_clock: PROCEDURE
    pt = rb(ST + OFF_TIMER)
    FOR i = 8 TO 11
        #BACKTAB(i) = 0
    NEXT i
    IF rb(ST + OFF_PHASE) = PH_COUNTDOWN THEN
        v = pt : #p = 9 : #cw = CW_YELLOW : GOSUB gfx_num
    ELSE
        v = pt / 60
        #BACKTAB(8) = (16 + v) * 8 + CW_WHITE
        #BACKTAB(9) = (26 * 8) + CW_WHITE       ' :
        v = pt % 60
        #BACKTAB(10) = (16 + v / 10) * 8 + CW_WHITE
        #BACKTAB(11) = (16 + v % 10) * 8 + CW_WHITE
    END IF
    hud_timer = pt
END

' ---- the stats (columns 0-2): fire, bombs, speed, kick, glove ---------------

    DIM st_i, st_n
' gfx_stat: stat st_i is st_n (0: kick and glove hidden)
gfx_stat: PROCEDURE
    #p = (2 + st_i * 2) * 20
    IF st_i >= 3 AND st_n = 0 THEN
        #BACKTAB(#p) = 0
    ELSE
        #BACKTAB(#p) = tword(T_FIRE + st_i)
    END IF
    #BACKTAB(#p + 1) = 0 : #BACKTAB(#p + 2) = 0
    IF st_i < 3 THEN v = st_n : #p = #p + 1 : #cw = CW_WHITE : GOSUB gfx_num
END

' stat_one: stat st_i of ours, drawn if it changed (a pickup sounds);
' st_force draws it anyway.
    DIM st_force
stat_one: PROCEDURE
    st_n = 0
    IF my_seat <> NONE THEN
        IF st_i = 0 THEN
            st_n = pl(my_seat, P_RANGE)
        ELSEIF st_i = 1 THEN
            st_n = pl(my_seat, P_BOMBS)
        ELSEIF st_i = 2 THEN
            st_n = pl(my_seat, P_SPEED) + 1
        ELSEIF st_i = 3 THEN
            st_n = pl(my_seat, P_FLAGS) AND PF_KICK
        ELSE
            st_n = pl(my_seat, P_FLAGS) AND PF_GLOVE
        END IF
    END IF
    IF st_force = 0 AND st_n = stats(st_i) THEN RETURN
    IF st_force = 0 AND st_n > stats(st_i) THEN sid = SND_PICKUP : GOSUB snd_play
    stats(st_i) = st_n
    GOSUB gfx_stat
END

gfx_round: PROCEDURE
    PRINT AT 58 COLOR CW_WHITE, "RD"
    #BACKTAB(78) = 0 : #BACKTAB(79) = 0
    v = rb(ST + OFF_ROUND) : #p = 78 : #cw = CW_YELLOW : GOSUB gfx_num
END

' ---- kicked and thrown bombs: half cards between tiles ------------------------

' tile_at: the card at tile index t (ct) drawn from ST (and our bomb).
tile_at: PROCEDURE
    IF ct = pend_tile THEN
        #BACKTAB(tpos(ct) - $200) = tword(T_BOMB)
    ELSE
        #BACKTAB(tpos(ct) - $200) = tword(mapt(ct))
    END IF
END

' bomb_at: a bomb (pd: a dud) at half-tile pv, pt: over one tile, or as
' halves over two, the tiles' backgrounds behind.
bomb_at: PROCEDURE
    wc = pv / 2 : wr = pt / 2
    ct = wr * ARENA_W + wc
    IF pd THEN #w = CW_DUDHALF ELSE #w = CW_HALF
    IF pv AND 1 THEN
        #BACKTAB(tpos(ct) - $200) = #w + (tword(mapt(ct)) AND BG_MASK)
        IF wc + 1 < ARENA_W THEN
            ct = ct + 1
            #BACKTAB(tpos(ct) - $200) = #w + 8 + (tword(mapt(ct)) AND BG_MASK)
        END IF
    ELSEIF pt AND 1 THEN
        #BACKTAB(tpos(ct) - $200) = #w + 16 + (tword(mapt(ct)) AND BG_MASK)
        IF wr + 1 < ARENA_H THEN
            ct = ct + ARENA_W
            #BACKTAB(tpos(ct) - $200) = #w + 24 + (tword(mapt(ct)) AND BG_MASK)
        END IF
    ELSE
        IF pd THEN #BACKTAB(tpos(ct) - $200) = tword(T_DUD) ELSE #BACKTAB(tpos(ct) - $200) = tword(T_BOMB)
    END IF
END

' unbomb: the tiles under half-tile pv, pt drawn again
unbomb: PROCEDURE
    wc = pv / 2 : wr = pt / 2
    ct = wr * ARENA_W + wc
    GOSUB tile_at
    IF (pv AND 1) AND wc + 1 < ARENA_W THEN ct = ct + 1 : GOSUB tile_at
    IF (pt AND 1) AND wr + 1 < ARENA_H THEN ct = ct + ARENA_W : GOSUB tile_at
END

' ---- entering, and a new state --------------------------------------------------

play_enter: PROCEDURE
    arena_on = 0
    GOSUB gfx_begin
    GOSUB gfx_arena_gram
    FOR i = 1 TO 11
        #BACKTAB(i * 20 + 3) = CW_WALL
        #BACKTAB(i * 20 + 17) = CW_WALL
    NEXT i
    pend_tile = NONE
    CALL ARENA_DRAW
    arena_on = 1
    cm_boom = 0 : cm_fizz = 0
    FOR i = 0 TO MOVERS - 1
        mov_on(i) = 0
    NEXT i
    mov_any = 0
    FOR ps = 0 TO 3
        die_t(ps) = 0
        disp_x(ps) = pl(ps, P_X)
        disp_y(ps) = pl(ps, P_Y)
    NEXT ps
    was_alive = rb(ST + OFF_ALIVE)
    last_phase = rb(ST + OFF_PHASE)
    IF my_seat <> NONE THEN
        my_warp = pl(my_seat, P_WARP)
        mv_x = pl(my_seat, P_X) : mv_y = pl(my_seat, P_Y) : mv_dir = DIR_DOWN : mv_acc = 0
    END IF
    st_force = 1
    FOR st_i = 0 TO 4
        GOSUB stat_one
    NEXT st_i
    st_force = 0
    GOSUB play_state
    GOSUB gfx_round
    GOSUB hud
    in_arena = 1
    IF rb(ST + OFF_PHASE) = PH_COUNTDOWN THEN sid = MUS_ROUND : GOSUB snd_music
    #seen = #jiffy                      ' (the drawing's time isn't walked)
END

play_state: PROCEDURE
    ' our own position, when the server says so
    IF my_seat <> NONE THEN
        IF pl(my_seat, P_WARP) <> my_warp THEN
            my_warp = pl(my_seat, P_WARP)
            mv_x = pl(my_seat, P_X) : mv_y = pl(my_seat, P_Y) : mv_acc = 0
        END IF
    END IF
    IF pend_tile <> NONE THEN
        IF mapt(pend_tile) <> T_FLOOR THEN pend_tile = NONE     ' the server has it (or it's taken)
    END IF
    ' the bombs that were moving: their tiles again; the ones moving now
    #a = ST + OFF_MOVERS
    IF mov_any OR ((rb(#a) OR rb(#a + 4) OR rb(#a + 8) OR rb(#a + 12)) AND 3) <> 0 THEN
        FOR pm = 0 TO MOVERS - 1
            IF mov_on(pm) THEN pv = mov_hx(pm) : pt = mov_hy(pm) : GOSUB unbomb : mov_on(pm) = 0
        NEXT pm
        mov_any = 0
        IF pend_tile <> NONE THEN ct = pend_tile : GOSUB tile_at
        FOR pm = 0 TO MOVERS - 1
            IF rb(#a) AND 3 THEN
                pv = ((rb(#a + 1) + 4) AND 255) / 8
                WHILE pv >= HALF_W
                    pv = pv - HALF_W
                WEND
                pt = ((rb(#a + 2) + 4) AND 255) / 8
                WHILE pt >= HALF_H
                    pt = pt - HALF_H
                WEND
                IF pv = HALF_W - 1 THEN pv = HALF_W - 2     ' wrapping: shown at the edge
                IF pt = HALF_H - 1 THEN pt = HALF_H - 2
                pd = rb(#a) AND M_DUD
                GOSUB bomb_at
                mov_hx(pm) = pv : mov_hy(pm) = pt : mov_on(pm) = 1 : mov_any = 1
            END IF
            #a = #a + MOVER_SIZE
        NEXT pm
    ELSEIF pend_tile <> NONE THEN
        ct = pend_tile : GOSUB tile_at
    END IF
    IF cm_boom THEN cm_boom = 0 : sid = SND_BOOM : GOSUB snd_play
    IF cm_fizz THEN cm_fizz = 0 : sid = SND_FIZZLE : GOSUB snd_play

    pv = rb(ST + OFF_ALIVE)
    pt = rb(ST + OFF_PHASE)
    IF pv <> was_alive AND pt = PH_PLAY THEN
        FOR ps = 0 TO 3
            IF (was_alive AND tbit(ps)) <> 0 AND (pv AND tbit(ps)) = 0 THEN
                die_t(ps) = 1
                sid = SND_DIE : GOSUB snd_play
            END IF
        NEXT ps
    END IF
    pm = rb(ST + OFF_TIMER)
    IF pt = PH_COUNTDOWN AND hud_timer <> pm THEN sid = SND_BEEP : GOSUB snd_play
    IF pt = PH_PLAY AND last_phase = PH_COUNTDOWN THEN sid = SND_GO : GOSUB snd_play
    last_phase = pt
    ' the HUD: whole when someone fell, else the clock alone when it ticked
    ' (the trophies only change between rounds, when play_enter draws it)
    IF hud_alive <> pv THEN
        GOSUB hud
    ELSEIF hud_timer <> pm THEN
        GOSUB hud_clock
    END IF
    was_alive = pv
    ' a stat a state
    st_i = st_rr : GOSUB stat_one
    st_rr = st_rr + 1
    IF st_rr = 5 THEN st_rr = 0
END

' ---- a jiffy of play ----------------------------------------------------------

' play_logic: a jiffy (after jiffy_step).
play_logic: PROCEDURE
    me = 0
    IF my_seat <> NONE THEN
        IF (rb(ST + OFF_ALIVE) AND tbit(my_seat)) <> 0 AND rb(ST + OFF_PHASE) = PH_PLAY THEN me = 1
    END IF
    moved = 0
    IF me THEN
        CALL WALK(pl(my_seat, P_SPEED))
        IF mv_bump <> NONE AND #frame_count - #bump_t >= 30 THEN
            pv = pl(my_seat, P_FLAGS) AND PF_KICK
            IF pv OR mapt(mv_bump) = T_DUD THEN
                #bump_t = #frame_count
                na_act = ACT_BUMP : na_a1 = mv_bump : na_a2 = mv_dir : GOSUB net_act
                IF pv THEN sid = SND_KICK : GOSUB snd_play
            END IF
        END IF
        IF ev_btn THEN
            ev_btn = 0
            wc = mv_x : wr = mv_y : GOSUB centre_tile
            pt = mapt(ct)
            IF (pl(my_seat, P_FLAGS) AND PF_GLOVE) <> 0 AND (pt = T_BOMB OR ct = pend_tile) THEN
                na_act = ACT_THROW : na_a1 = mv_dir : na_a2 = 0 : GOSUB net_act
                sid = SND_THROW : GOSUB snd_play
                IF pend_tile <> NONE THEN pv = pend_tile : pend_tile = NONE : ct = pv : GOSUB tile_at
            ELSEIF pt = T_FLOOR AND ct <> pend_tile THEN
                pv = pl(my_seat, P_OUT)
                IF pend_tile <> NONE THEN pv = pv + 1
                IF pv < pl(my_seat, P_BOMBS) THEN
                    na_act = ACT_DROP : na_a1 = ct : na_a2 = 0 : GOSUB net_act
                    pend_tile = ct
                    pend_left = 90
                    GOSUB tile_at
                    sid = SND_DROP : GOSUB snd_play
                END IF
            END IF
        END IF
        pd = mv_dir
        IF moved THEN pd = pd + P_DIR_MOVING
        #w = #frame_count - #sent_t
        IF ((mv_x <> sent_x OR mv_y <> sent_y OR pd <> sent_d) AND #w >= MOVE_EVERY) OR #w >= 60 THEN
            sent_x = mv_x : sent_y = mv_y : sent_d = pd : #sent_t = #frame_count
            POKE RAWB + 1, mv_x : POKE RAWB + 2, mv_y : POKE RAWB + 3, pd : POKE RAWB + 4, my_warp
            GOSUB net_move
        END IF
    END IF
    me_moved = moved
    IF pend_tile <> NONE AND pend_left THEN
        pend_left = pend_left - 1
        IF pend_left = 0 THEN ct = pend_tile : pend_tile = NONE : GOSUB tile_at  ' never came
    END IF
END

' play_draw: once a pass, after its jiffies of play_logic (owed): the
' others walk toward where the server has them, the dead fall, and the
' bombers are drawn (seats.asm). A pass that is late (owed 2 or more)
' leaves the drawing to the next one, so it catches up.
play_draw: PROCEDURE
    IF me THEN
        CALL SEATS_STEP(my_seat, owed)
        disp_x(my_seat) = mv_x : disp_y(my_seat) = mv_y
    ELSE
        CALL SEATS_STEP(NONE, owed)
    END IF
    IF owed < 2 THEN CALL SEATS_DRAW
END

hud_col:
    DATA 0, 4, 12, 16
fr_base:
    DATA FR_UP0, FR_RIGHT0, FR_DOWN0, FR_LEFT0
fr_walk:
    DATA 0, 1, 0, 2

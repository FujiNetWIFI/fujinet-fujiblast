' ===========================================================================
' screens.bas -- the screens at a table besides the arena, as
' src/screens.c: the waiting room, the trophies after a round and a game;
' and the helpers every screen uses. Twenty columns: the texts are cut to
' fit.
' ===========================================================================

' tick: a frame of a screen with no walking: wait, the presses, 0 mutes.
tick: PROCEDURE
    GOSUB frame_wait
    #seen = #jiffy
    #frame_count = #seen
    GOSUB input_take
    IF ev_key = KEY_MUTE THEN GOSUB snd_toggle_mute
END

' pressed: v = 1 if the disc went bit v (IN_*) this frame (not held).
pressed: PROCEDURE
    IF (inp_dir AND v) <> 0 AND (prev_dir AND v) = 0 THEN v = 1 ELSE v = 0
END

' gfx_ccentre: the 0-ended text at #a (cartridge RAM, n at most) centred
' on row r, colour #cw.
gfx_ccentre: PROCEDURE
    GOSUB gfx_clen
    #p = r * 20 + (20 - n) / 2
    GOSUB gfx_cstr
END

' message_wait: the screen as the caller drew it, for msg_t frames, or
' (msg_t 0) until fire or ENTER, with PRESS FIRE on row 9.
    DIM msg_t
message_wait: PROCEDURE
    IF msg_t = 0 THEN PRINT AT 185 COLOR CW_WHITE, "PRESS FIRE"
    DO
        GOSUB tick
        IF msg_t THEN
            msg_t = msg_t - 1
            IF msg_t = 0 THEN EXIT DO
        ELSE
            IF ev_btn OR ev_key = KEY_RETURN THEN EXIT DO
        END IF
    LOOP WHILE 1
END

' msg_host: "NO ANSWER FROM" and the host, until fire.
msg_host: PROCEDURE
    GOSUB gfx_begin
    PRINT AT 83 COLOR CW_YELLOW, "NO ANSWER FROM"
    #a = SRVHOST : n = 20 : r = 6 : #cw = CW_WHITE : GOSUB gfx_ccentre
    msg_t = 0 : GOSUB message_wait
END

' ---- the waiting room ------------------------------------------------------------

wait_enter: PROCEDURE
    in_arena = 0
    arena_on = 0
    GOSUB gfx_begin
    GOSUB wait_state
END

    DIM wt_n
wait_state: PROCEDURE
    r = 0 : GOSUB gfx_clear_row
    POKE STXT, 82 : POKE STXT + 1, 79 : POKE STXT + 2, 79 : POKE STXT + 3, 77 : POKE STXT + 4, 32
    FOR i = 0 TO TABLE_LEN
        c = rb(SRVTBL + i)
        IF c >= 97 AND c <= 122 THEN c = c - 32
        POKE STXT + 5 + i, c
        IF c = 0 THEN EXIT FOR
    NEXT i
    #a = STXT : n = 20 : r = 0 : #cw = CW_YELLOW : GOSUB gfx_ccentre
    wt_n = 0
    pv = rb(ST + OFF_SEATED)
    FOR ps = 0 TO 3
        r = 1 + ps * 2 : GOSUB gfx_clear_row
        #p = r * 20 + 3
        sp_n = ps : sp_s = ps
        IF pv AND tbit(ps) THEN
            wt_n = wt_n + 1
            #a = ST + OFF_NAMES + ps * NAME_LEN : n = NAME_LEN : #cw = CW_WHITE : GOSUB gfx_cstr
            IF ps = rb(ST + OFF_HOST) THEN PRINT AT r * 20 + 13 COLOR CW_TAN, "HOST"
            sp_x = 8 : sp_y = r * 8 : sp_f = FR_DOWN0 : sp_big = 0 : GOSUB spr_put
        ELSE
            PRINT AT #p COLOR CW_TAN, "--------"
            GOSUB spr_hide
        END IF
    NEXT ps
    r = 9 : GOSUB gfx_clear_row
    PRINT AT 183 COLOR CW_WHITE, "ROUNDS:"
    v = rb(ST + OFF_ROUNDS) : #p = 190 : #cw = CW_YELLOW : GOSUB gfx_num
    IF my_seat <> NONE AND my_seat = rb(ST + OFF_HOST) THEN PRINT AT 192 COLOR CW_TAN, "<DISC>"
    r = 10 : GOSUB gfx_clear_row
    IF my_seat = NONE THEN
        PRINT AT 200 COLOR CW_WHITE, "WATCHING: ROOM FULL"
    ELSEIF my_seat <> rb(ST + OFF_HOST) THEN
        PRINT AT 202 COLOR CW_WHITE, "WAITING FOR HOST"
    ELSEIF wt_n < 2 THEN
        PRINT AT 200 COLOR CW_WHITE, "WAITING FOR PLAYERS"
    ELSE
        PRINT AT 204 COLOR CW_YELLOW, "FIRE: START"
    END IF
    PRINT AT 220 COLOR CW_TAN, "CLEAR:LEAVE 0:SOUND"
END

wait_frame: PROCEDURE
    r = rb(ST + OFF_ROUNDS)
    IF my_seat <> NONE AND my_seat = rb(ST + OFF_HOST) THEN
        v = IN_LEFT : GOSUB pressed
        IF v AND r > 1 THEN na_act = ACT_ROUNDS : na_a1 = r - 1 : na_a2 = 0 : GOSUB net_act
        v = IN_RIGHT : GOSUB pressed
        IF v AND r < MAX_ROUNDS THEN na_act = ACT_ROUNDS : na_a1 = r + 1 : na_a2 = 0 : GOSUB net_act
        IF ev_btn THEN na_act = ACT_START : na_a1 = 0 : na_a2 = 0 : GOSUB net_act
    END IF
    prev_dir = inp_dir
END

' ---- trophies ------------------------------------------------------------------

    DIM res_final, res_seat, res_timer, res_k, res_row(SEATS)
results_enter: PROCEDURE
    in_arena = 0
    arena_on = 0
    res_seat = rb(ST + OFF_RESULT)
    res_timer = NONE
    GOSUB gfx_begin
    GOSUB gfx_arena_gram                ' (the trophy)
    anim_on = 0
    ' the winner's line, black on the seat's colour
    IF res_seat < SEATS THEN
        CALL COPYB(ST + OFF_NAMES + res_seat * NAME_LEN, STXT, NAME_LEN)
        POKE STXT + NAME_LEN, 0
        #a = STXT : n = NAME_LEN : GOSUB gfx_clen
        RESTORE res_wins
        FOR i = 0 TO 5
            READ c
            POKE STXT + n + i, c
        NEXT i
        IF res_final = 0 THEN POKE STXT + n + 5, 0
        #a = STXT : n = 20 : r = 0 : #cw = seat_hud(res_seat) : GOSUB gfx_ccentre
    ELSEIF res_final THEN
        PRINT AT 5 COLOR CW_YELLOW, "NO WINNER"
    ELSE
        PRINT AT 8 COLOR CW_YELLOW, "DRAW"
    END IF
    IF res_final = 0 THEN
        IF rb(ST + OFF_SUDDEN) THEN
            PRINT AT 24 COLOR CW_WHITE, "SUDDEN DEATH"
        ELSE
            PRINT AT 26 COLOR CW_WHITE, "ROUND"
            v = rb(ST + OFF_ROUND) : #p = 32 : #cw = CW_WHITE : GOSUB gfx_num
        END IF
    END IF
    res_k = 0
    pv = rb(ST + OFF_SEATED)
    FOR ps = 0 TO 3
        res_row(ps) = 0
        sp_n = ps : sp_s = ps
        IF pv AND tbit(ps) THEN
            r = 3 + res_k * 2
            res_k = res_k + 1
            res_row(ps) = r
            #p = r * 20 + 3
            #a = ST + OFF_NAMES + ps * NAME_LEN : n = NAME_LEN : #cw = CW_WHITE : GOSUB gfx_cstr
            v = pl(ps, P_WINS) : #p = r * 20 + 12 : #cw = CW_YELLOW : GOSUB gfx_num
            IF v > 12 THEN v = 12
            i = 0
            WHILE i < v
                #BACKTAB(r * 20 + 23 + i) = CW_TROPHY
                i = i + 1
            WEND
            sp_x = 8 : sp_y = r * 8 : sp_f = FR_DOWN0 : sp_big = 0 : GOSUB spr_put
        ELSE
            GOSUB spr_hide
        END IF
    NEXT ps
    IF res_final AND res_seat < SEATS THEN
        sp_n = res_seat : sp_s = res_seat : sp_x = 128 : sp_y = 28 : sp_f = FR_WIN0 : sp_big = 1 : GOSUB spr_put
    END IF
    IF res_final THEN
        sid = MUS_GAME : GOSUB snd_music
    ELSEIF res_seat < SEATS THEN
        sid = MUS_WIN : GOSUB snd_music
    END IF
END

results_frame: PROCEDURE
    IF res_seat < SEATS THEN
        sp_n = res_seat : sp_s = res_seat : sp_f = FR_WIN0 + ((#frame_count / 16) AND 1)
        IF res_final THEN
            sp_x = 128 : sp_y = 28 : sp_big = 1 : GOSUB spr_put
        ELSEIF res_row(res_seat) THEN
            sp_x = 8 : sp_y = res_row(res_seat) * 8 : sp_big = 0 : GOSUB spr_put
        END IF
    END IF
    IF rb(ST + OFF_TIMER) <> res_timer THEN
        res_timer = rb(ST + OFF_TIMER)
        r = 11 : GOSUB gfx_clear_row
        IF res_final THEN
            PRINT AT 220 COLOR CW_WHITE, "BACK TO THE ROOM"
            #p = 237
        ELSE
            PRINT AT 221 COLOR CW_WHITE, "NEXT ROUND IN"
            #p = 235
        END IF
        v = res_timer : #cw = CW_YELLOW : GOSUB gfx_num
    END IF
END

res_wins:
    DATA 32, 87, 73, 78, 83, 33         '  WINS!

' ===========================================================================
' menus.bas -- the screens before a table, as src/menus.c: the title, the
' name (picked a letter at a time with the disc: the Intellivision has no
' keyboard), the rooms.
' ===========================================================================

    DIM #mt

title: PROCEDURE
    in_arena = 0
    arena_on = 0
    GOSUB gfx_begin
    GOSUB gfx_logo
    PRINT AT 84 COLOR CW_YELLOW, "FOR FUJINET"
    PRINT AT 165 COLOR CW_WHITE, "PRESS FIRE"
    PRINT AT 201 COLOR CW_TAN, "BOMBS, BLOCKS AND"
    PRINT AT 223 COLOR CW_TAN, "BLASTS FOR 2-4"
    sid = MUS_TITLE : GOSUB snd_music
    #mt = 0
    DO
        GOSUB tick
        #mt = #mt + 1
        FOR ps = 0 TO 3
            #v = (#mt / 2 + ps * 40)
            WHILE #v >= 168
                #v = #v - 168
            WEND
            sp_n = ps : sp_s = ps : sp_x = #v : sp_y = 48 : sp_big = 0
            IF #mt AND 8 THEN sp_f = FR_RIGHT1 + (ps AND 1) ELSE sp_f = FR_RIGHT0
            GOSUB spr_put
        NEXT ps
        IF ev_btn OR ev_key = KEY_RETURN THEN EXIT DO
    LOOP WHILE 1
    GOSUB snd_stop
    GOSUB spr_hide_all
END

' name_entry: MYNAME, 2-8 letters and digits. The disc's up and down pick
' the letter, right or fire takes it, left or CLEAR takes one back; a
' keypad digit is taken at once; ENTER ends it.
    DIM ne_n, ne_c, ne_rep
name_entry: PROCEDURE
    GOSUB gfx_begin
    PRINT AT 41 COLOR CW_YELLOW, "WHAT IS YOUR NAME?"
    PRINT AT 162 COLOR CW_TAN, "DISC UP/DOWN: PICK"
    PRINT AT 182 COLOR CW_TAN, "RIGHT/FIRE: TAKE"
    PRINT AT 200 COLOR CW_TAN, "LEFT/CLEAR: BACK"
    PRINT AT 222 COLOR CW_TAN, "ENTER: DONE"
    ne_n = 0 : ne_c = 65 : ne_rep = 0
    POKE MYNAME, 0
    prev_dir = 0
    DO
        ' the name, and the letter to take next
        r = 5 : GOSUB gfx_clear_row
        #a = MYNAME : n = NAME_LEN : #p = 106 : #cw = CW_WHITE : GOSUB gfx_cstr
        IF ne_n < NAME_LEN THEN
            IF #frame_count AND 16 THEN #BACKTAB(106 + ne_n) = (ne_c - 32) * 8 + CW_YELLOW ELSE #BACKTAB(106 + ne_n) = (45 - 32) * 8 + CW_YELLOW
        END IF
        GOSUB tick
        ' up and down step the letter, and repeat while held
        v = 0
        IF inp_dir AND (IN_UP + IN_DOWN) THEN
            IF (prev_dir AND (IN_UP + IN_DOWN)) = 0 THEN
                v = 1 : ne_rep = 18
            ELSE
                ne_rep = ne_rep - 1
                IF ne_rep = 0 THEN v = 1 : ne_rep = 5
            END IF
        END IF
        IF v THEN
            IF inp_dir AND IN_UP THEN GOSUB ne_next ELSE GOSUB ne_prev
            sid = SND_SELECT : GOSUB snd_play
        END IF
        ' take it
        v = IN_RIGHT : GOSUB pressed
        IF (v OR ev_btn) AND ne_n < NAME_LEN THEN
            POKE MYNAME + ne_n, ne_c : ne_n = ne_n + 1 : POKE MYNAME + ne_n, 0
            sid = SND_SELECT : GOSUB snd_play
        END IF
        IF ev_key >= 48 AND ev_key <= 57 AND ne_n < NAME_LEN THEN
            POKE MYNAME + ne_n, ev_key : ne_n = ne_n + 1 : POKE MYNAME + ne_n, 0
            sid = SND_SELECT : GOSUB snd_play
        END IF
        ' back
        v = IN_LEFT : GOSUB pressed
        IF (v OR ev_key = KEY_ESC) AND ne_n > 0 THEN
            ne_n = ne_n - 1 : POKE MYNAME + ne_n, 0
        END IF
        prev_dir = inp_dir
        IF ev_key = KEY_RETURN AND ne_n >= 2 THEN EXIT DO
    LOOP WHILE 1
END

' the letters run A-Z then 0-9
ne_next: PROCEDURE
    ne_c = ne_c + 1
    IF ne_c = 91 THEN ne_c = 48
    IF ne_c = 58 THEN ne_c = 65
END
ne_prev: PROCEDURE
    ne_c = ne_c - 1
    IF ne_c = 64 THEN ne_c = 57
    IF ne_c = 47 THEN ne_c = 90
END

' table_list: the rooms on the server, one picked into SRVTBL (v = 1), or
' v = 0 (no answer).
    DIM tl_sel
table_list: PROCEDURE
    in_arena = 0
    arena_on = 0
    GOSUB gfx_begin
    PRINT AT 85 COLOR CW_YELLOW, "CONNECTING"
    #a = SRVHOST : n = 20 : r = 6 : #cw = CW_WHITE : GOSUB gfx_ccentre
    GOSUB net_tables
    IF table_n = 0 THEN
        GOSUB net_close
        GOSUB msg_host
        v = 0
        RETURN
    END IF
    GOSUB gfx_begin
    PRINT AT 4 COLOR CW_YELLOW, "PICK A ROOM"
    FOR i = 0 TO table_n - 1
        #a = TABLES + i * TABLE_REC
        #p = (2 + i) * 20 + 1
        n = 10 : #cw = CW_WHITE
        GOSUB tl_name
        #p = (2 + i) * 20 + 12
        #BACKTAB(#p) = (16 + rb(#a + 20)) * 8 + CW_WHITE
        #BACKTAB(#p + 1) = (47 - 32) * 8 + CW_WHITE
        #BACKTAB(#p + 2) = (16 + rb(#a + 21)) * 8 + CW_WHITE
        IF rb(#a + 22) = PH_WAIT THEN PRINT AT #p + 4 COLOR CW_TAN, "WAIT" ELSE PRINT AT #p + 4 COLOR CW_RED, "PLAY"
    NEXT i
    PRINT AT 202 COLOR CW_TAN, "DISC: CHOOSE"
    PRINT AT 222 COLOR CW_TAN, "FIRE: JOIN"
    tl_sel = 0
    prev_dir = 0
    DO
        FOR i = 0 TO table_n - 1
            IF i = tl_sel THEN #BACKTAB((2 + i) * 20) = (62 - 32) * 8 + CW_YELLOW ELSE #BACKTAB((2 + i) * 20) = 0
        NEXT i
        GOSUB tick
        GOSUB net_poll
        v = IN_UP : GOSUB pressed
        IF v AND tl_sel > 0 THEN tl_sel = tl_sel - 1 : sid = SND_SELECT : GOSUB snd_play
        v = IN_DOWN : GOSUB pressed
        IF v AND tl_sel + 1 < table_n THEN tl_sel = tl_sel + 1 : sid = SND_SELECT : GOSUB snd_play
        prev_dir = inp_dir
        IF ev_btn OR ev_key = KEY_RETURN THEN EXIT DO
    LOOP WHILE 1
    CALL COPYB(TABLES + tl_sel * TABLE_REC, SRVTBL, TABLE_LEN)
    POKE SRVTBL + TABLE_LEN, 0
    v = 1
END

' tl_name: a room's name (at #a + 8, up to 12 bytes, n shown) at #p
tl_name: PROCEDURE
    FOR k = 0 TO n - 1
        c = rb(#a + 8 + k)
        IF c >= 96 THEN c = c - 32
        IF c < 32 OR c > 95 THEN c = 32
        #BACKTAB(#p + k) = (c - 32) * 8 + #cw
    NEXT k
END

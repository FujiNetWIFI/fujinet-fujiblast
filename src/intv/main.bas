' ===========================================================================
' main.bas -- FujiBlast for the Intellivision and the FujiNet cartridge: a
' bomb-maze game for two to four players, as src/main.c.
'
' The lobby's appkeys say who we are and where to play (appkey.bas); else
' we ask, and list the rooms. At a table, the server's state says which
' screen to show: the waiting room, the arena (countdown and play), the
' trophies.
'
' There is no C compiler for the CP1610, so this is the client again in
' IntyBASIC 1.5 (src/intv/Makefile builds it), with the link's hot loops
' in netasm.asm. Memory: memory.bas; the screen: draw.bas.
' ===========================================================================

    OPTION MAP 2
    OPTION EXPLICIT ON
    ASM CFGVAR "name" = "FujiBlast"
    ASM CFGVAR "short_name" = "FujiBlast"
    ASM CFGVAR "year" = 2026

    GOTO boot

    INCLUDE "memory.bas"
    INCLUDE "proto.bas"
    INCLUDE "opts.bas"
    ' $4810-$4FFF: the generated tables and cards
    SEGMENT 5
    INCLUDE "gfx.bas"
    INCLUDE "snd.bas"
    INCLUDE "config.bas"
    ' $5000-$6FFF
    SEGMENT 0
    INCLUDE "fujinet.bas"
    INCLUDE "nstrans.bas"
    INCLUDE "net.bas"
    INCLUDE "isr.bas"
    INCLUDE "input.bas"
    INCLUDE "sound.bas"
    INCLUDE "appkey.bas"
    ASM INCLUDE "netasm.asm"
    ASM INCLUDE "io.asm"
    ' $C040-$DFFF ($E000-$EFFF is the ECS's ROM: tools/intv/checkcfg.py)
    SEGMENT 2
    INCLUDE "draw.bas"
    INCLUDE "move.bas"
    INCLUDE "play.bas"
    INCLUDE "screens.bas"
    ASM INCLUDE "seats.asm"
    ASM INCLUDE "walk.asm"
    ' $A000-$B7FF ($B800 up shows GRAM on some consoles)
    SEGMENT 1
    INCLUDE "menus.bas"
    INCLUDE "test.bas"
    SEGMENT 0

    CONST JOIN_WAIT = 600       ' frames for the first state before giving up
    CONST S_NONE  = 0
    CONST S_WAIT  = 1
    CONST S_ARENA = 2
    CONST S_ROUND = 3
    CONST S_GAME  = 4
    DIM pt_ret, shown, shown_seq, want, lg_k, fs_seated, ps_due
    DIM #pt_t

' find_seat: my_seat = the seat with our name, NONE if we watch. (The
' seat we had is looked at first: it is nearly always still ours.)
find_seat: PROCEDURE
    IF my_seat <> NONE THEN
        ps = my_seat : GOSUB seat_is_me
        IF i THEN RETURN
    END IF
    my_seat = NONE
    FOR ps = 0 TO 3
        GOSUB seat_is_me
        IF i THEN my_seat = ps : RETURN
    NEXT ps
END

' seat_is_me: i = 1 if seat ps is seated, with our name.
seat_is_me: PROCEDURE
    IF (rb(ST + OFF_SEATED) AND tbit(ps)) = 0 THEN i = 0 : RETURN
    #a = ST + OFF_NAMES + ps * NAME_LEN
    FOR k = 0 TO NAME_LEN - 1
        c = rb(#a + k)
        IF c <> rb(MYNAME + k) THEN i = 0 : RETURN
        IF c = 0 THEN i = 1 : RETURN
    NEXT k
    i = 1
END

screen_for: PROCEDURE
    c = rb(ST + OFF_PHASE)
    IF c = PH_COUNTDOWN OR c = PH_PLAY THEN
        want = S_ARENA
    ELSEIF c = PH_ROUNDEND THEN
        want = S_ROUND
    ELSEIF c = PH_GAMEOVER THEN
        want = S_GAME
    ELSE
        want = S_WAIT
    END IF
END

' table_upper: SRVTBL in capitals at STXT (the GROM has no small letters).
table_upper: PROCEDURE
    FOR i = 0 TO TABLE_LEN
        c = rb(SRVTBL + i)
        IF c >= 97 AND c <= 122 THEN c = c - 32
        POKE STXT + i, c
        IF c = 0 THEN EXIT FOR
    NEXT i
END

' play_table: play at SRVTBL. pt_ret = 1: come back to it (the link
' dropped); 0: we left, or it would not have us.
play_table: PROCEDURE
    GOSUB appkey_save_room
    GOSUB gfx_begin
    PRINT AT 86 COLOR CW_YELLOW, "JOINING"
    GOSUB table_upper
    #a = STXT : n = 20 : r = 6 : #cw = CW_WHITE : GOSUB gfx_ccentre
    GOSUB net_open
    IF link = 0 THEN GOSUB msg_host : pt_ret = 0 : RETURN
    GOSUB net_hello
    shown = S_NONE : shown_seq = 0 : #pt_t = 0 : fs_seated = 0 : my_seat = NONE
    DO
        GOSUB frame_wait
pt_a:                                   ' (the pass, for intvsim's prof: pt_a-pt_b; pt_n after the link)
        GOSUB input_take
        IF ev_key = KEY_MUTE THEN GOSUB snd_toggle_mute
        GOSUB net_poll
pt_n:
        IF net_r = NET_ERROR THEN
            GOSUB net_close
            GOSUB gfx_begin
            PRINT AT 81 COLOR CW_YELLOW, "THE SERVER SAID NO"
            IF net_err = ERR_VERSION THEN PRINT AT 122 COLOR CW_WHITE, "UPDATE FUJIBLAST" ELSE PRINT AT 124 COLOR CW_WHITE, "NO SUCH ROOM"
            msg_t = 0 : GOSUB message_wait
            pt_ret = 0 : RETURN
        END IF
        IF net_r = NET_LOST THEN
            GOSUB gfx_begin
            PRINT AT 82 COLOR CW_YELLOW, "CONNECTION LOST"
            PRINT AT 124 COLOR CW_WHITE, "TRYING AGAIN"
            msg_t = 120 : GOSUB message_wait
            pt_ret = 1 : RETURN
        END IF
        IF ev_key = KEY_ESC THEN
            GOSUB net_leave
            GOSUB net_close
            pt_ret = 0 : RETURN
        END IF
        IF net_heard = 0 THEN
            #seen = #jiffy
            #pt_t = #pt_t + 1
            IF #pt_t > JOIN_WAIT THEN
                GOSUB net_close
                GOSUB msg_host
                pt_ret = 0 : RETURN
            END IF
        ELSE
            ' A state is whole only at its commit (the patches go straight
            ' into ST as they come): the screen is picked then.
            IF net_r = NET_STATE THEN
                ' (only a state can move us; and only when the seats change)
                IF rb(ST + OFF_SEATED) <> fs_seated OR my_seat = NONE THEN fs_seated = rb(ST + OFF_SEATED) : GOSUB find_seat
                GOSUB screen_for
                IF want <> shown OR (want = S_ARENA AND rb(ST + OFF_SEQ) <> shown_seq) THEN
                    shown = want
                    shown_seq = rb(ST + OFF_SEQ)
                    IF want = S_WAIT THEN
                        GOSUB wait_enter
                    ELSEIF want = S_ARENA THEN
                        GOSUB play_enter
                        ps_due = 0
                    ELSE
                        res_final = (want = S_GAME) AND 1
                        GOSUB results_enter
                    END IF
                ELSEIF shown = S_WAIT THEN
                    GOSUB wait_state
                ELSEIF shown = S_ARENA THEN
                    ps_due = 1                  ' (next pass: this one decoded it)
                END IF
            END IF
            IF shown = S_ARENA THEN
                IF ps_due AND net_r <> NET_STATE THEN ps_due = 0 : GOSUB play_state
                FOR lg_k = 1 TO owed
                    GOSUB jiffy_step
                    GOSUB play_logic
                NEXT lg_k
                GOSUB play_draw
            ELSE
                #seen = #jiffy
                #frame_count = #seen
                IF shown = S_WAIT THEN GOSUB wait_frame ELSE GOSUB results_frame
            END IF
        END IF
pt_b:
    LOOP WHILE 1
END

' ---------------------------------------------------------------------------
boot:
    POKE TEST_HOOK, 0           ' (cartridge RAM: a reset leaves it as it was)
    WAIT
    ON FRAME GOSUB frame_isr
    MODE 1
    BORDER 0
    CLS
    SOUND 4, 1, $38
    GOSUB gfx_init
    my_seat = NONE
    pend_tile = NONE
    ready = 1
    GOSUB test_check
    GOSUB fn_wait_mailbox
    IF fn_ok = 0 THEN
        PRINT AT 82 COLOR CW_YELLOW, "NO FUJINET FOUND"
        PRINT AT 120 COLOR CW_WHITE, "IS THE CARTRIDGE IN?"
        DO
            WAIT
        LOOP WHILE 1
    END IF
    GOSUB appkey_load
    GOSUB title
    IF rb(MYNAME) = 0 THEN
        GOSUB name_entry
        GOSUB appkey_save_name
    END IF
main_loop:
    IF rb(SRVTBL) = 0 THEN
        GOSUB table_list
        IF v = 0 THEN GOTO main_loop
    END IF
    GOSUB play_table
    IF pt_ret = 0 THEN GOSUB appkey_clear_room
    GOTO main_loop

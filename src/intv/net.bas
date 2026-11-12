' ===========================================================================
' net.bas -- the stream protocol (docs/PROTOCOL.md) over the link
' (nstrans.bas), as src/net.c.
'
' The server pushes the state as patches, then a commit, when the state is
' whole. The patches go straight into ST (PATCH, netasm.asm, which draws a
' map tile that changed): src/net.c stages them, but a state's patches and
' its commit come together, and copying 247 bytes costs an Intellivision a
' frame. We send where our player is (SC_MOVE) and what it does (SC_ACT);
' we drain the link every pass and tell the server what we took
' (SC_CREDIT), so it never sends more than we can hold.
' ===========================================================================

    CONST PING_EVERY = 300      ' passes between keepalives (each is answered)
    CONST DEAD_AFTER = 900      ' passes without a frame: the link is gone
    CONST ASK_AGAIN  = 60       ' HELLO unanswered: again
    CONST RESYNC_GAP = 30       ' passes between RESYNCs
    CONST DECODE_PASS = 16      ' bytes decoded a pass, at most (a state's spread over passes)
    CONST NET_NONE  = 0
    CONST NET_STATE = 1
    CONST NET_ERROR = 2
    CONST NET_LOST  = 3
    CONST MAX_TABLES = 8

    DIM link, joined, seq, credited, resync_hold, resync_owed
    DIM table_n, table_have
    DIM net_err, net_heard, net_r
    DIM arena_on, cm_boom, cm_fizz      ' PATCH's
    DIM #quiet, #q_ping, q_ask
    DIM #srv_port
    DIM ns_n, ns_t              ' a frame's payload length and type (net_send)
    DIM #n_commit, #n_bad       ' counts, for the rigs

' net_send: frame ns_t with the ns_n bytes at RAWB + 1 into TXQ.
net_send: PROCEDURE
    ns_n = USR SEND(ns_t, ns_n)
END

send_credit: PROCEDURE
    credited = taken
    POKE RAWB + 1, credited
    ns_t = SC_CREDIT : ns_n = 1 : GOSUB net_send
END

net_resync: PROCEDURE
    resync_owed = 1
    IF resync_hold = 0 THEN
        ns_t = SC_RESYNC : ns_n = 0 : GOSUB net_send
        resync_hold = RESYNC_GAP
        resync_owed = 0
    END IF
END

' net_open: link = 1 if the server answered.
net_open: PROCEDURE
    IF link THEN RETURN
    joined = 0
    credited = 0
    #quiet = 0 : #q_ping = 0 : q_ask = 0
    #nst_port = #srv_port
    GOSUB nst_open
    link = 1 - nst_dead
END

hello_frame: PROCEDURE
    FOR i = 1 TO 2 + TABLE_LEN + NAME_LEN
        POKE RAWB + i, 0
    NEXT i
    POKE RAWB + 1, PROTO_VERSION
    i = 0
    WHILE i < TABLE_LEN
        c = rb(SRVTBL + i)
        IF c = 0 THEN EXIT WHILE
        POKE RAWB + 3 + i, c
        i = i + 1
    WEND
    i = 0
    WHILE i < NAME_LEN
        c = rb(MYNAME + i)
        IF c = 0 THEN EXIT WHILE
        POKE RAWB + 3 + TABLE_LEN + i, c
        i = i + 1
    WEND
    ns_t = SC_HELLO : ns_n = 2 + TABLE_LEN + NAME_LEN : GOSUB net_send
END

net_hello: PROCEDURE
    joined = 1
    net_heard = 0
    net_err = 0
    #quiet = 0 : #q_ping = 0 : q_ask = 0
    resync_owed = 0
    FOR #a = ST TO ST + STATE_SIZE - 1
        POKE #a, 0
    NEXT #a
    GOSUB hello_frame
END

net_drop: PROCEDURE
    GOSUB nst_close
    link = 0 : joined = 0
END

' net_poll: a pass of the link. net_r = NET_NONE, NET_STATE (a commit
' came: ST is whole), NET_ERROR (net_err) or NET_LOST.
net_poll: PROCEDURE
    net_r = NET_NONE
    IF link = 0 THEN net_r = NET_LOST : RETURN
    GOSUB nst_step
    IF nst_dead THEN GOSUB net_drop : net_r = NET_LOST : RETURN
    IF resync_hold THEN
        resync_hold = resync_hold - 1
    ELSEIF resync_owed THEN
        GOSUB net_resync
    END IF
    dec_left = DECODE_PASS
    WHILE 1
        n = USR DECODE
        IF n = 0 THEN EXIT WHILE
        #quiet = 0 : #q_ping = 0 : q_ask = 0
        IF n = 255 THEN
            #n_bad = #n_bad + 1
            GOSUB net_resync
        ELSE
            c = rb(NBUF)
            IF c = CS_PATCH THEN
                CALL PATCH(n)
            ELSEIF c = CS_COMMIT THEN
                IF joined THEN
                    net_heard = 1
                    net_r = NET_STATE
                    #n_commit = #n_commit + 1
                END IF
            ELSEIF c = CS_TABLE THEN
                GOSUB net_table
            ELSEIF c = CS_ERROR THEN
                net_err = rb(NBUF + 1)
                net_r = NET_ERROR
                RETURN
            END IF
        END IF
    WEND
    IF ((taken - credited) AND 255) >= 48 THEN GOSUB send_credit
    #quiet = #quiet + 1
    #q_ping = #q_ping + 1
    IF #q_ping = PING_EVERY THEN
        #q_ping = 0
        GOSUB send_credit
        ns_t = SC_PING : ns_n = 0 : GOSUB net_send
    END IF
    q_ask = q_ask + 1
    IF q_ask = ASK_AGAIN THEN
        q_ask = 0
        IF joined AND net_heard = 0 THEN GOSUB hello_frame
    END IF
    IF #quiet > DEAD_AFTER THEN GOSUB net_drop : net_r = NET_LOST
END

' A room's record (CS_TABLE: index, count, record) into TABLES.
net_table: PROCEDURE
    IF n >= 3 + TABLE_REC THEN
        c = rb(NBUF + 1)
        IF c < MAX_TABLES THEN
            CALL COPYB(NBUF + 3, TABLES + c * TABLE_REC, TABLE_REC)
            table_have = table_have OR tbit(c)
        END IF
    END IF
    IF n >= 3 THEN
        table_n = rb(NBUF + 2)
        IF table_n > MAX_TABLES THEN table_n = MAX_TABLES
    END IF
END

' net_tables: the room list (waited for, asked again until it comes):
' table_n rooms, 0 if none or no answer.
net_tables: PROCEDURE
    GOSUB net_open
    IF link = 0 THEN table_n = 0 : RETURN
    table_n = NONE
    table_have = 0
    FOR #a = 0 TO 599
        IF table_n = NONE AND (#a % ASK_AGAIN) = 0 THEN
            POKE RAWB + 1, 0
            ns_t = SC_TABLES : ns_n = 0 : GOSUB net_send
        END IF
        WAIT
        GOSUB net_poll
        IF net_r = NET_LOST THEN table_n = 0 : RETURN
        IF table_n <> NONE THEN
            v = tbit(table_n) - 1
            IF (table_have AND v) = v THEN RETURN
        END IF
    NEXT #a
    table_n = 0
END

' net_move(mv_x, mv_y, d, my_warp) is play.bas's: x, y, dir, warp at RAWB + 1.
net_move: PROCEDURE
    ns_t = SC_MOVE : ns_n = 4 : GOSUB net_send
END

' net_act: act, a1, a2 (na_act, na_a1, na_a2); seq the action's.
    DIM na_act, na_a1, na_a2
net_act: PROCEDURE
    seq = seq + 1
    IF seq = 0 THEN seq = 1
    POKE RAWB + 1, seq
    POKE RAWB + 2, na_act
    POKE RAWB + 3, na_a1
    POKE RAWB + 4, na_a2
    ns_t = SC_ACT : ns_n = 4 : GOSUB net_send
END

net_leave: PROCEDURE
    IF joined THEN na_act = ACT_LEAVE : na_a1 = 0 : na_a2 = 0 : GOSUB net_act
    joined = 0
END

' net_close: goodbye, sent before the link is closed.
net_close: PROCEDURE
    IF link THEN
        IF joined THEN
            ns_t = SC_BYE : ns_n = 0 : GOSUB net_send
            #a = FRAME
            WHILE txq_len AND FRAME - #a < 30
                WAIT
                GOSUB nst_step
            WEND
        END IF
        GOSUB net_drop
    END IF
END

tbit:
    DATA 1, 2, 4, 8, 16, 32, 64, 128, 256

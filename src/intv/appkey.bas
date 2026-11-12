' ===========================================================================
' appkey.bas -- the lobby's handoff, as src/appkey.c. The FujiNet lobby
' keeps its keys under creator 1, app 1:
'   key 0     the player's name, shared by every FujiNet game
'   key 0x10  FujiBlast's server: tcp://host:port/?table=id
' Without them we ask for a name and list the default server's rooms; the
' room picked goes back in key 0x10, so a reset sits down there again.
' ===========================================================================

    CONST AK_NAME   = 0
    CONST AK_SERVER = $10

' ak_get: key ak_key into AKBUF (ak_len bytes, 0 ended). The first read can
' come too soon after power-up: it gets a second try.
ak_get: PROCEDURE
    GOSUB nst_quiet
    ak_mode = 0
    GOSUB ak_open
    IF fn_ok = 0 THEN GOSUB ak_open
    ak_len = 0
    POKE AKBUF, 0
    IF fn_ok = 0 THEN RETURN
    GOSUB ak_read
    GOSUB ak_close
END

' ak_put: AKBUF's ak_len bytes into key ak_key.
ak_put: PROCEDURE
    GOSUB nst_quiet
    v = ak_len
    ak_mode = 1
    GOSUB ak_open
    IF fn_ok = 0 THEN RETURN
    ak_len = v
    GOSUB ak_write
    GOSUB ak_close
END

' parse_url: AKBUF's "tcp://host:port/?table=id" (or host:port) into
' SRVHOST, #srv_port and SRVTBL (letters and digits, lower case). v = 0 if
' there is no host.
parse_url: PROCEDURE
    t = 0                               ' after "://", if there is one
    k = 0
    WHILE rb(AKBUF + k) <> 0 AND k < 64
        IF rb(AKBUF + k) = 58 AND rb(AKBUF + k + 1) = 47 AND rb(AKBUF + k + 2) = 47 THEN t = k + 3 : EXIT WHILE
        k = k + 1
    WEND
    n = 0
    WHILE n < 60
        c = rb(AKBUF + t)
        IF c = 0 OR c = 58 OR c = 47 OR c = 63 THEN EXIT WHILE
        POKE SRVHOST + n, c
        n = n + 1 : t = t + 1
    WEND
    POKE SRVHOST + n, 0
    v = n
    #srv_port = 0
    IF rb(AKBUF + t) = 58 THEN
        t = t + 1
        c = rb(AKBUF + t)
        WHILE c >= 48 AND c <= 57
            #srv_port = #srv_port * 10 + c - 48
            t = t + 1
            c = rb(AKBUF + t)
        WEND
    END IF
    IF #srv_port = 0 THEN #srv_port = STREAM_PORT
    POKE SRVTBL, 0
    ' "table="
    WHILE rb(AKBUF + t) <> 0 AND t < 64
        FOR k = 0 TO 5
            IF rb(AKBUF + t + k) <> table_eq(k) THEN EXIT FOR
        NEXT k
        IF k = 6 THEN
            t = t + 6
            n = 0
            WHILE n < TABLE_LEN
                c = rb(AKBUF + t + n)
                IF c >= 65 AND c <= 90 THEN c = c + 32
                IF (c < 97 OR c > 122) AND (c < 48 OR c > 57) THEN EXIT WHILE
                POKE SRVTBL + n, c
                n = n + 1
            WEND
            POKE SRVTBL + n, 0
            RETURN
        END IF
        t = t + 1
    WEND
END

table_eq:
    DATA 116, 97, 98, 108, 101, 61      ' table=

appkey_load: PROCEDURE
    ak_key = AK_NAME
    GOSUB ak_get
    n = 0
    k = 0
    WHILE n < ak_len AND k < NAME_LEN
        c = rb(AKBUF + n)
        IF c >= 97 AND c <= 122 THEN c = c - 32
        IF (c >= 65 AND c <= 90) OR (c >= 48 AND c <= 57) THEN POKE MYNAME + k, c : k = k + 1
        n = n + 1
    WEND
    POKE MYNAME + k, 0
    ak_key = AK_SERVER
    GOSUB ak_get
    IF ak_len > 4 THEN
        GOSUB parse_url
        IF v THEN RETURN
    END IF
    ' the default server, and no room
    i = 0
    DO
        c = default_srv(i)
        POKE AKBUF + i, c
        i = i + 1
    LOOP WHILE c <> 0
    GOSUB parse_url
    POKE SRVTBL, 0
END

appkey_save_name: PROCEDURE
    ak_len = 0
    WHILE rb(MYNAME + ak_len) <> 0
        POKE AKBUF + ak_len, rb(MYNAME + ak_len)
        ak_len = ak_len + 1
    WEND
    ak_key = AK_NAME
    GOSUB ak_put
END

' ak_add: the 0-ended text at #a onto AKBUF at ak_len (cartridge RAM).
ak_add: PROCEDURE
    WHILE rb(#a) <> 0
        POKE AKBUF + ak_len, rb(#a)
        ak_len = ak_len + 1
        #a = #a + 1
    WEND
END

appkey_save_room: PROCEDURE
    ak_len = 0
    FOR i = 0 TO 5
        POKE AKBUF + ak_len, url_tcp(i) : ak_len = ak_len + 1
    NEXT i
    #a = SRVHOST : GOSUB ak_add
    POKE AKBUF + ak_len, 58 : ak_len = ak_len + 1
    ' the port, in decimal (put_num writes onto FN_TX: borrow its end)
    #fn_txlen = 200
    #v = #srv_port : GOSUB put_num
    FOR i = 200 TO #fn_txlen - 1
        POKE AKBUF + ak_len, rb(FN_TX + i) : ak_len = ak_len + 1
    NEXT i
    FOR i = 0 TO 7
        POKE AKBUF + ak_len, url_table(i) : ak_len = ak_len + 1
    NEXT i
    #a = SRVTBL : GOSUB ak_add
    ak_key = AK_SERVER
    GOSUB ak_put
END

appkey_clear_room: PROCEDURE
    ak_len = 0
    ak_key = AK_SERVER
    GOSUB ak_put
    POKE SRVTBL, 0
END

url_tcp:
    DATA 116, 99, 112, 58, 47, 47       ' tcp://
url_table:
    DATA 47, 63, 116, 97, 98, 108, 101, 61      ' /?table=

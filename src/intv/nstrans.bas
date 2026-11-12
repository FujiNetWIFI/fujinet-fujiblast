' ===========================================================================
' nstrans.bas -- the byte link to the server (nstrans.h): one TCP
' connection through the FujiNet cartridge's mailbox (fujinet.bas), never
' waited on while the game plays. From syn-ack-wire's net.bas.
'
' One mailbox transaction is out at a time. nst_step looks for its answer
' and posts the next: the frames queued in TXQ first, else, once the last
' READ's bytes are all decoded, a STATUS (at once while bytes come, else
' every POLL_FRAMES), and after a STATUS that says bytes wait, a READ of
' them. The bytes are decoded where the READ left them, in the mailbox's
' receive buffer (DECODE in netasm.asm), between #rx_p and #rx_e: a WRITE
' doesn't touch that buffer, so frames go out while they are decoded.
'
' nst_dead: the link is gone (the cartridge didn't answer or refused, or
' FujiNet says the connection is closed). It stays set until the next open.
' ===========================================================================

    CONST NX_STATUS   = 1
    CONST NX_READ     = 2
    CONST NX_WRITE    = 3
    CONST POLL_FRAMES = 2

    DIM nx_st                   ' the transaction out: 0 none, NX_*
    DIM nst_dead
    DIM flowing                 ' bytes came last time, or an answer is due: ask at once
    DIM #asked                  ' FRAME of the last STATUS
    DIM #rx_p, #rx_e            ' the bytes read and not decoded yet
    UNSIGNED #rx_p, #rx_e
    DIM dec_left, dec_code, dec_bad, taken             ' DECODE's
    DIM #dec_p, #dec_e, #dec_s1, #dec_s2
    DIM txq_len                 ' SEND's
    DIM #nst_port
    DIM #n_status, #n_read, #n_write, #n_bytes          ' counts, for the rigs

' nst_step: a step of the link: the answer to the transaction out, and
' the next posted.
nst_step: PROCEDURE
    IF nx_st THEN
        fn_ok = USR MB_CHECK
        IF fn_ok = 255 THEN RETURN
        IF fn_ok = 0 THEN nx_st = 0 : nst_dead = 1 : RETURN
        IF nx_st = NX_STATUS THEN
            nx_st = 0
            #v = rb(FN_RX) + rb(FN_RX + 1) * 256
            IF #v = 0 THEN
                flowing = 0
                IF rb(FN_RX + 2) = 0 THEN nst_dead = 1
                RETURN
            END IF
            flowing = 1
            IF #v > 512 THEN #v = 512
            CALL POST_READ(#v)
            nx_st = NX_READ
            #n_read = #n_read + 1
            RETURN
        END IF
        IF nx_st = NX_READ THEN
            #v = rb(FN_RXLEN_LO) + rb(FN_RXLEN_HI) * 256
            IF #v = 0 THEN nx_st = 0 : nst_dead = 1 : RETURN
            #rx_p = FN_RX
            #rx_e = FN_RX + #v
            #n_bytes = #n_bytes + #v
        END IF
        nx_st = 0
    END IF
    IF txq_len THEN
        CALL POST_WRITE(txq_len)
        txq_len = 0
        nx_st = NX_WRITE
        flowing = 1                     ' the answer is on its way: look at once
        #n_write = #n_write + 1
        RETURN
    END IF
    IF #rx_p < #rx_e THEN RETURN        ' bytes still to decode
    IF flowing = 0 AND FRAME - #asked < POLL_FRAMES THEN RETURN
    #asked = FRAME
    CALL POST_STATUS
    nx_st = NX_STATUS
    #n_status = #n_status + 1
END

' nst_quiet: let the transaction out end (its answer is dropped).
nst_quiet: PROCEDURE
    WHILE nx_st
        WAIT
        GOSUB fn_check
        IF fn_ok <> 255 THEN nx_st = 0
    WEND
END

' nst_open: dial N:TCP://SRVHOST:#nst_port/. nst_dead = 0 if it answered:
' a failed open can still be ACKed; the failure shows in the status after
' it (not connected), as on the other platforms.
nst_open: PROCEDURE
    GOSUB nst_quiet
    #rx_p = 0 : #rx_e = 0 : txq_len = 0 : flowing = 0
    #dec_p = NBUF : dec_code = 0 : dec_bad = 0 : taken = 0 : #dec_s1 = 0 : #dec_s2 = 0
    nst_dead = 1
    ' the devicespec
    #fn_txlen = 0
    RESTORE nst_spec
    FOR i = 0 TO 7
        READ c
        POKE FN_TX + #fn_txlen, c : #fn_txlen = #fn_txlen + 1
    NEXT i
    i = 0
    WHILE rb(SRVHOST + i) <> 0 AND i < 60
        POKE FN_TX + #fn_txlen, rb(SRVHOST + i) : #fn_txlen = #fn_txlen + 1
        i = i + 1
    WEND
    POKE FN_TX + #fn_txlen, 58 : #fn_txlen = #fn_txlen + 1
    #v = #nst_port : GOSUB put_num
    POKE FN_TX + #fn_txlen, 47 : #fn_txlen = #fn_txlen + 1
    GOSUB fn_open
    IF fn_ok = 0 THEN RETURN
    GOSUB fn_status_post
    DO
        WAIT
        GOSUB fn_check
    LOOP WHILE fn_ok = 255
    IF fn_ok = 0 THEN RETURN
    IF rb(FN_RX + 2) = 0 THEN RETURN
    nst_dead = 0
    #asked = FRAME
END

nst_close: PROCEDURE
    GOSUB nst_quiet
    GOSUB fn_close
    txq_len = 0
    nst_dead = 1
END

' put_num: #v in decimal onto FN_TX at #fn_txlen.
put_num: PROCEDURE
    k = 0                               ' a digit put yet
    FOR j = 0 TO 4
        #w = pow10(j)
        c = 48
        WHILE #v >= #w
            #v = #v - #w : c = c + 1
        WEND
        IF c > 48 OR k OR j = 4 THEN
            POKE FN_TX + #fn_txlen, c : #fn_txlen = #fn_txlen + 1
            k = 1
        END IF
    NEXT j
END

nst_spec:
    DATA 78, 58, 84, 67, 80, 58, 47, 47     ' N:TCP://
pow10:
    DATA 10000, 1000, 100, 10, 1

; p11.inc -- page 11: the offline test scenes (src/test.c's). At boot a rig
; pokes 0A0h + a scene into internal RAM's TESTHK (o2sim `ipoke 36 A1`); the
; boot sees it within half a second and comes here instead of the cartridge
; check. The state is test.c's canned one (gen/o2/canned.inc); the link is
; quiet (FL_TEST: p5's step returns nothing). Then:
;   1  the arena, seat 0 (ANN) walked by the stick
;   2  the waiting room       3  a round's end       4  the game's end

    PAGE_HEAD
    db e_run&0FFh
e_run:
    jmp t_run

t_run:
    mov r2,#ST>>8               ; the canned state
    mov a,#ST&0FFh
    FCALL poke_at
    mov r4,#canned_st&0FFh
    mov r6,#STATE_SIZE
    mov r0,#FN_W_REGDATA
    call tc_l
    mov r0,#SRVTBL              ; ANN at blast1, seat 0
    mov r1,#tnames&0FFh
    mov r2,#16
t_n:
    mov a,r1
    movp a,@a
    movx @r0,a
    inc r0
    inc r1
    djnz r2,t_n
    clr a
    xset MYSEAT
    mov a,#NL_LINK|NL_JOINED|NL_HEARD
    xset NLINK
    xorb FLAGS,FL_TEST
    mov r1,#TESTHK              ; the scene
    mov a,@r1
    anl a,#0Fh
    xrl a,#1
    jnz t_scr
    GOTO PG_PLAY,E_ENTER
t_scr:                          ; 2-4 (A = scene ^ 1): the phase, then the
    xrl a,#1!2                  ; table's screens
    mov r5,#PH_WAIT
    jz t_ph
    xrl a,#2!3
    mov r5,#PH_ROUNDEND
    jz t_ph
    mov r5,#PH_GAMEOVER
t_ph:
    mov r2,#ST>>8
    mov a,#(ST+OFF_PHASE)&0FFh
    FCALL poke_at
    mov a,r5
    FCALL poke_b
    mov r2,#ST>>8
    mov a,#(ST+OFF_TIMER)&0FFh
    FCALL poke_at
    mov a,#5
    FCALL poke_b
    GOTO PG_MAIN,E_BACK

; tnames: SRVTBL ("blast1") then MYNAME ("ANN"), as ram.inc lays them out
tnames:
    if MYNAME <> SRVTBL+TABLE_LEN
    error "p11: SRVTBL and MYNAME must be adjacent"
    endif
    db "blast1",0,0, "ANN",0,0,0,0,0

    if $ > 900h
    error "page 11's page $8 has run out"
    endif

; ===========================================================================
    org 900h
tc_l:                           ; (the table's reader: in its page)
    mov a,r4
    movp a,@a
    movx @r0,a
    inc r4
    djnz r6,tc_l
    ret
    include "canned.inc"
    if $ > 0A00h
    error "page 11's page $9 has run out"
    endif

    PAGE_TAIL

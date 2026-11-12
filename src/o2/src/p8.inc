; p8.inc -- page 8: the title, the name, the rooms (src/menus.c). Entered one
; way (far_go):
;   E_TITLE  the title; then the name if there is none; then the rooms if
;            there is no table, else the table (p7 E_JOIN)
;   E_ROOMS  the build's server's rooms, one picked

    PAGE_HEAD
    db e_title&0FFh
    db e_rooms&0FFh
e_title:
    jmp m_title
e_rooms:
    jmp m_rooms

; m_title: "FUJIBLAST", the four bombers walking across, until fire.
m_title:
    FARC PG_TLIB,L_CLEAR
    call lb_clr
    mov a,#s_title&0FFh
    call ls_8
    mov a,#C_TAN
    xset TXCOL
    FARC PG_TLIB,L_T0
    mov a,#C_WHITE
    xset TXCOL
    call lb_clr
    mov a,#s_for&0FFh
    call ls_8
    mov r4,#1
    call row_out
    call lb_clr
    mov a,#s_tag&0FFh
    call ls_8
    mov r4,#2
    call row_out
    call lb_clr
    mov a,#s_fire&0FFh
    call ls_8
    mov a,#C_CYAN
    xset TXCOL
    mov r4,#5
    call row_out
    mov a,#SND_TITLE
    call beep
mt_l:
    INFRAME
    call walkers
    xget JOYEV
    jb4 mt_go
    xget KEY
    xrl a,#K_ENT
    jnz mt_l
mt_go:
    mov a,#SND_SELECT
    call beep
    xget MYNAME                 ; no name: ask
    jnz mt_t
    call name_entry
    FARC PG_NCTL,K_SAVENAME
mt_t:
    xget SRVTBL                 ; no table: the rooms
    jnz mt_j
    jmp m_rooms
mt_j:
    GOTO PG_MAIN,E_JOIN

; ls_8: the NUL-ended string at offset A of page $8 onto LINEBUF at r1.
ls_8:
    mov r0,a
ls8_l:
    mov a,r0
    movp a,@a
    jz ls8_x
    movx @r1,a
    inc r1
    inc r0
    jmp ls8_l
ls8_x:
    ret

s_title: db "   FUJIBLAST",0
s_for:   db "FOR FUJINET",0
s_tag:   db "BOMBS 2-4",0
s_fire:  db " PRESS FIRE",0

    if $ > 900h
    error "page 8's page $8 has run out"
    endif

; ===========================================================================
;  the name
; ===========================================================================
    org 900h

; name_entry: letters and digits on the keyboard, up to 8; CLEAR rubs out;
; ENTER takes it (2 or more). MYNAME, NUL-padded.
name_entry:
    FARC PG_TLIB,L_CLEAR
    call lb_clr
    mov a,#s_name&0FFh
    call ls_9
    mov a,#C_WHITE
    xset TXCOL
    FARC PG_TLIB,L_T0
    call lb_clr
    mov a,#s_keys&0FFh
    call ls_9
    mov a,#C_DGREY
    xset TXCOL
    mov r4,#5
    call row_out
    mov r0,#MYNAME
    mov r2,#NAME_LEN
    clr a
ne_z:
    movx @r0,a
    inc r0
    djnz r2,ne_z
ne_draw:
    call lb_clr                 ; the name, a block after it
    mov a,#' '
    call lb_ch
    mov r2,#MYNAME
    mov r3,#NAME_LEN
ne_n:
    mov a,r2
    mov r0,a
    movx a,@r0
    jz ne_c
    call lb_ch
    inc r2
    djnz r3,ne_n
    jmp ne_o
ne_c:
    mov a,#CH_BLOCK
    call lb_ch
ne_o:
    mov a,#C_CYAN
    xset TXCOL
    mov r4,#2
    call row_out
ne_l:
    INFRAME
    xget KEY
    mov r5,a
    inc a
    jz ne_l
    mov a,r5
    xrl a,#K_ENT
    jz ne_ent
    mov a,r5
    xrl a,#K_CLR
    jz ne_clr
    mov a,r5                    ; a letter or a digit?
    add a,#256-48
    jc ne_l
    mov a,r5
    add a,#keychr&0FFh
    movp a,@a
    jz ne_l
    mov r5,a
    call ne_len
    xrl a,#NAME_LEN
    jz ne_l
    xrl a,#NAME_LEN
    add a,#MYNAME
    mov r0,a
    mov a,r5
    movx @r0,a
    mov a,#SND_SELECT
    call beep
    jmp ne_draw
ne_clr:
    call ne_len
    jz ne_l
    add a,#MYNAME-1
    mov r0,a
    clr a
    movx @r0,a
    jmp ne_draw
ne_ent:
    call ne_len
    add a,#256-2
    jnc ne_l
    ret

; keychr: BIOS readkey's code -> a letter or digit (0: neither)
keychr: db "0123456789",0,0,0,0,"LP",0,"WERTUIOQSDFGHJKAZXCVBM",0,0,0,0,0,"YN",0,0

; ls_9: the NUL-ended string at offset A of page $9 onto LINEBUF at r1.
ls_9:
    mov r0,a
ls9_l:
    mov a,r0
    movp a,@a
    jz ls9_x
    movx @r1,a
    inc r1
    inc r0
    jmp ls9_l
ls9_x:
    ret

s_name: db "   YOUR NAME?",0
s_keys: db "THEN ENTER",0

    if $ > 0A00h
    error "page 8's page $9 has run out"
    endif

; ===========================================================================
;  the rooms
; ===========================================================================
    org 0A00h

; m_rooms: the build's server: the socket, the table list asked for (again
; every 60 frames until it comes), its rooms on rows 0-6, one picked with the
; stick and fire. Nothing for 10 s: a message, then the title.
m_rooms:
    FARC PG_TLIB,L_CLEAR
    call lb_clr
    mov a,#s_rooms&0FFh
    call ls_a
    mov a,#C_WHITE
    xset TXCOL
    FARC PG_TLIB,L_T0
    FARC PG_NCTL,C_OPEN
    mov a,r2
    jz mr_none
    clr a
    xset SEL
    xset JOINT
    xset JOINTH
mr_ask:
    FARC PG_NET,N_TABLES
    mov r6,#60
mr_l:
    mov a,r6
    xset RCOUNT
    INFRAME
    FARC PG_NET,N_STEP
    xget NEVT
    jb2 mr_none
    xget TBLN                   ; all in?
    mov r2,a
    inc a
    jz mr_w
    mov a,r2
    jz mr_none
    add a,#bita8&0FFh
    movp a,@a                   ; (1 << n) - 1
    mov r2,a
    xget TBLHAVE
    anl a,r2
    xrl a,r2
    jz mr_have
mr_w:
    xinc JOINT
    jnz mr_t
    xinc JOINTH
mr_t:
    xget JOINTH
    add a,#256-3                ; ~10 s
    jc mr_none
    xget RCOUNT
    mov r6,a
    djnz r6,mr_l
    jmp mr_ask
mr_none:
    FARC PG_NCTL,C_CLOSE
    FARC PG_TLIB,L_CLEAR
    call lb_clr
    mov a,#s_noansw&0FFh
    call ls_a
    mov a,#C_WHITE
    xset TXCOL
    FARC PG_TLIB,L_T0
    mov r6,#180
mn_w:
    mov a,r6
    xset RCOUNT
    INFRAME
    xget RCOUNT
    mov r6,a
    djnz r6,mn_w
    jmp m_title
mr_have:
    jmp mr_have2
ls_a:
    mov r0,a
lsa_l:
    mov a,r0
    movp a,@a
    jz lsa_x
    movx @r1,a
    inc r1
    inc r0
    jmp lsa_l
lsa_x:
    ret

bita8:  db 0, 1, 3, 7, 15, 31, 63, 127, 255
rec24a:
K   set 0
    rept MAX_TABLES
    db (ST&0FFh)+TABLE_REC*K
K   set K+1
    endm
s_rooms:  db "     ROOMS",0
s_noansw: db "   NO ANSWER",0

    if $ > 0B00h
    error "page 8's page $A has run out"
    endif

; ===========================================================================
    org 0B00h

; lb_clr: LINEBUF 16 spaces; r1 at its start. Clobbers A, r0.
lb_clr:
    mov r1,#LINEBUF
    mov r0,#16
    mov a,#' '
lc_l:
    movx @r1,a
    inc r1
    djnz r0,lc_l
    mov r1,#LINEBUF
    ret

; lb_ch: A onto LINEBUF at r1.
lb_ch:
    movx @r1,a
    inc r1
    ret

; row_out: LINEBUF onto text row r4. Clobbers A, r0-r7.
row_out:
    FARC PG_TLIB,L_ROW
    ret


    include "beep.inc"

; ne_len: A = MYNAME's length. Clobbers r0, r2.
ne_len:
    mov r0,#MYNAME
    mov r2,#0
nl_l:
    movx a,@r0
    jz nl_x
    inc r0
    inc r2
    mov a,r2
    xrl a,#NAME_LEN
    jnz nl_l
nl_x:
    mov a,r2
    ret


    if $ > 0C00h
    error "page 8's page $B has run out"
    endif

; ===========================================================================
    org 0C00h
; rooms_draw: rows 0-6: each room's name (7) and "s/m" -- the picked one in
; cyan. Clobbers A, r0-r7.
rooms_draw:
    mov r4,#0
rd_l:
    mov a,r4
    xset TMP0
    call lb_clr
    xget TBLN
    mov r2,a
    xget TMP0
    cpl a
    add a,r2                    ; C: a room on this row
    jnc rd_o
    xget TMP0
    add a,#rec24c&0FFh
    movp a,@a
    add a,#TABLE_LEN            ; its name[12]
    mov r3,a
    mov r2,#7
rd_n:
    mov a,r3
    FCALL st_get
    jz rd_s
    call lb_ch
    inc r3
    djnz r2,rd_n
rd_s:
    mov r1,#LINEBUF+8           ; seated / max
    xget TMP0
    add a,#rec24c&0FFh
    movp a,@a
    add a,#TABLE_LEN+12
    mov r3,a
    FCALL st_get
    add a,#'0'
    call lb_ch
    mov a,#'/'
    call lb_ch
    mov a,r3
    inc a
    FCALL st_get
    add a,#'0'
    call lb_ch
rd_o:
    mov a,#C_WHITE
    xset TXCOL
    xget SEL
    mov r2,a
    xget TMP0
    xrl a,r2
    jnz rd_c
    mov a,#C_CYAN
    xset TXCOL
rd_c:
    xget TMP0
    mov r4,a
    call row_out
    xget TMP0
    inc a
    mov r4,a
    xrl a,#NTROWS
    jnz rd_l
    ret

rec24c:
K   set 0
    rept MAX_TABLES
    db (ST&0FFh)+TABLE_REC*K
K   set K+1
    endm

    if $ > 0D00h
    error "page 8's page $C has run out"
    endif

; ===========================================================================
    org 0D00h
; walkers: the four bombers walking right along row 6, 40 apart, in their
; colours, the walk's two frames.
walkers:
    mov r4,#0
wk_l:
    mov r1,#FRAMES              ; x = (frames / 2 + 40n) mod 168
    mov a,@r1
    clr c
    rrc a
    mov r2,a
    mov a,r4
    add a,#w40&0FFh
    movp a,@a
    add a,r2
wk_m:
    mov r2,a
    add a,#256-168
    jnc wk_x
    jmp wk_m
wk_x:
    mov a,r2
    add a,#8
    mov r5,a                    ; X
    mov a,r4
    rl a
    add a,r4
    add a,#SPRTAB&0FFh
    mov r2,#SPRTAB>>8
    FCALL poke_at
    mov a,#Y_TROW0+6*TPITCH-2
    FCALL poke_b
    mov a,r5
    FCALL poke_b
    mov a,r4
    add a,#ssp8&0FFh
    movp a,@a
    FCALL poke_b
    mov r1,#FRAMES
    mov a,@r1
    anl a,#8
    jnz wk_f1
    mov a,#FR_RIGHT0
    jmp wk_f
wk_f1:
    mov a,#FR_RIGHT1
wk_f:
    mov r2,a
    mov a,#SPF
    add a,r4
    mov r0,a
    mov a,r2
    movx @r0,a
    inc r4
    mov a,r4
    xrl a,#4
    jnz wk_l
    FARC PG_ALIB,A_SHAPES
    ret

w40:    db 0, 40, 80, 120
ssp8:   db 7*S_COL, 1*S_COL, 6*S_COL, 3*S_COL


    if $ > 0E00h
    error "page 8's page $D has run out"
    endif

; ===========================================================================
;  picking a room
; ===========================================================================
    org 0E00h
mr_have2:
    call rooms_draw
mr_pick:
    INFRAME
    FARC PG_NET,N_STEP          ; (the link kept alive)
    xget JOYEV
    mov r5,a
    jb0 mr_up
    jb1 mr_dn
    jb4 mr_go
    xget KEY
    xrl a,#K_ENT
    jz mr_go
    jmp mr_pick
mr_up:
    xget SEL
    jz mr_pick
    dec a
    jmp mr_mv
mr_dn:
    xget SEL
    inc a
    mov r2,a
    xget TBLN
    xrl a,r2
    jz mr_pick
    mov a,r2
mr_mv:
    xset SEL
    mov a,#SND_SELECT
    call beep
    call rooms_draw
    jmp mr_pick
mr_go:
    xget SEL                    ; its id into SRVTBL
    add a,#rec24e&0FFh
    movp a,@a
    mov r3,a
    mov r1,#SRVTBL
    mov r2,#TABLE_LEN
mg_l:
    mov a,r3
    FCALL st_get
    movx @r1,a
    inc r1
    inc r3
    djnz r2,mg_l
    mov a,#SND_SELECT
    call beep
    FARC PG_NCTL,C_CLOSE
    GOTO PG_MAIN,E_JOIN

rec24e:
K   set 0
    rept MAX_TABLES
    db (ST&0FFh)+TABLE_REC*K
K   set K+1
    endm

    if $ > 0F00h
    error "page 8's page $E has run out"
    endif

    PAGE_TAIL

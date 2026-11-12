; p4.inc -- page 4: the text screens' drawing, by farcall (L_*): LINEBUF
; (ASCII; bit 7: TXCOL2's colour, else TXCOL's) into a row of the text
; overlay's TROWS or into T0 (core.inc OBJQ). Codes $00-$1F are the font's
; pictures: glyph code+32 (CH_BALL, CH_MAN...). Arguments in r2-r5.

    PAGE_HEAD
    db e_clear&0FFh             ; L_CLEAR: a blank screen
    db e_row&0FFh               ; L_ROW: row r4 from LINEBUF's 12
    db e_t0&0FFh                ; L_T0: T0 from LINEBUF's 16
e_clear:
    jmp tl_clear
e_row:
    jmp tl_row
e_t0:
    jmp tl_t0

; tl_row: row r4 (0-6) from LINEBUF's first 12 characters. Clobbers A,
; r0-r7.
tl_row:
    mov a,r4
    add a,#trowoff&0FFh
    movp a,@a
    mov r2,#TROWS>>8
    FCALL poke_at
    mov a,r4
    add a,#ty2&0FFh
    movp a,@a
    mov r3,a                    ; Y / 2
    mov r1,#LINEBUF
    mov r4,#NTCOLS
tr_l:
    movx a,@r1
    call gl_put
    inc r1
    djnz r4,tr_l
    ret

; tl_t0: T0 from LINEBUF's 16 characters: quad A gets 0, 2, 4, 6, B 1, 3,
; 5, 7, C 8, 10, 12, 14, D 9, 11, 13, 15. Then ODIRTY's quads. Clobbers A,
; r0-r7.
tl_t0:
    mov r2,#OBJQ>>8
    mov a,#OBJQ&0FFh
    FCALL poke_at
    mov r3,#Y_T0/2
    mov r4,#0                   ; the quad
t0_q:
    mov a,#Y_T0
    call pk
    mov a,r4
    add a,#qx&0FFh
    movp a,@a
    call pk                     ; X
    mov a,r4
    add a,#qs&0FFh
    movp a,@a
    add a,#LINEBUF
    mov r1,a
    mov r5,#4
t0_c:
    movx a,@r1
    call gl_put
    inc r1
    inc r1
    djnz r5,t0_c
    inc r4
    mov a,r4
    xrl a,#4
    jnz t0_q
    mov r1,#ODIRTY
    mov a,@r1
    orl a,#OD_QA|OD_QH
    mov @r1,a
    ret

qx:     db X_TEXT, X_TEXT+8, X_TEXT+64, X_TEXT+72
qs:     db 0, 1, 8, 9

; tl_clear: a blank screen: every row and T0 spaces, the singles' X for the
; rows, the sprites hidden; ODIRTY all. Clobbers A, r0-r7.
tl_clear:
    call lb_space
    mov r4,#0
tc_r:
    mov a,r4
    mov r0,#TMP0
    movx @r0,a
    call tl_row
    mov r0,#TMP0
    movx a,@r0
    inc a
    mov r4,a
    xrl a,#NTROWS
    jnz tc_r
    call tl_t0
    mov r2,#OBJTAB>>8           ; the singles: Y parked, X the rows', blank
    mov a,#OBJTAB&0FFh
    FCALL poke_at
    mov r5,#X_TROW
    mov r6,#NTCOLS
tc_s:
    mov a,#0F8h
    call pk
    mov a,r5
    call pk
    clr a
    call pk
    call pk
    mov a,r5
    add a,#8
    mov r5,a
    djnz r6,tc_s
    mov r2,#SPRTAB>>8           ; the sprites hidden
    mov a,#SPRTAB&0FFh
    FCALL poke_at
    mov r6,#4
tc_p:
    mov a,#SPR_OFF
    call pk
    clr a
    call pk
    call pk
    djnz r6,tc_p
    mov r1,#ODIRTY
    mov a,#OD_ALL
    mov @r1,a
    ret

; lb_space: LINEBUF 16 spaces. Clobbers A, r0, r2.
lb_space:
    mov r0,#LINEBUF
    mov r2,#16
    mov a,#' '
lbs_l:
    movx @r0,a
    inc r0
    djnz r2,lbs_l
    ret

; trowoff: each row's table in TROWS. ty2: each row's Y / 2.
trowoff:
K   set 0
    rept NTROWS
    db (TROWS&0FFh)+K*TROW
K   set K+1
    endm
ty2:
K   set 0
    rept NTROWS
    db (Y_TROW0+K*TPITCH)/2
K   set K+1
    endm

    if $ > 900h
    error "page 4's page $8 has run out"
    endif

; ===========================================================================
    org 900h
; gl_put: the character A (LINEBUF's: bit 7 TXCOL2) poked as (pointer,
; control) for a line at Y = 2 x r3. Clobbers A, r0, r2, r6, r7.
gl_put:
    mov r7,a
    anl a,#7Fh
    add a,#256-20h
    jnc gp_pic                  ; $00-$1F: a picture, glyph code + 32
    add a,#charmap&0FFh
    movp a,@a
    anl a,#3Fh
    jmp gp_g
gp_pic:
    add a,#20h+32
gp_g:
    mov r2,a                    ; glyph
    rl a
    rl a
    rl a
    mov r6,a                    ; glyph x 8: low in bits 3-7, bit 8 in bit 0
    anl a,#0F8h
    cpl a
    add a,r3
    cpl a                       ; - Y/2; C: a borrow
    call pk
    mov a,r6
    anl a,#1
    jnc gp_nb
    xrl a,#1
gp_nb:
    mov r6,a
    mov a,r7                    ; the colour
    mov r0,#TXCOL
    jb7 gp_c2
    jmp gp_c
gp_c2:
    mov r0,#TXCOL2
gp_c:
    movx a,@r0
    rl a
    orl a,r6
    jmp pk

; pk: POKE A. Keeps A. Clobbers r0.
pk:
    mov r0,#FN_W_REGDATA
    movx @r0,a
    ret

    include "charmap.inc"

    if $ > 0A00h
    error "page 4's page $9 has run out"
    endif

    PAGE_TAIL

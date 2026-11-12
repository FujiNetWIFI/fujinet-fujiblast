; ov_text.inc -- the text screens' overlay, $600-$7FF: the title, the name,
; the rooms, the waiting room, the results. The image boots with it in the
; fixed half; ov.inc swaps it with the arena's.
;
; A screen is T0 -- a line of 16 characters at the top, the four quads
; interleaved (core.inc OBJQ, written in the vblank when dirty) -- and seven
; rows of twelve singles below it (TROWS), re-pointed by a band in the gap
; before each row: the foreground off, the twelve singles' Y, pointer and
; control written straight from TROWS (MOVP: no staging), the foreground on.
; The singles' X are the screen's: OBJTAB's singles, written once (OD_SING).
; Row 0 is written in the vblank. The sprites (SPRTAB) are the bombers
; beside the rows; a sprite in a band's gap blanks while it writes, so a
; bomber stands inside its row's 14 lines.
;
;   vblank  the sprites, a dirty object group (T0, the singles' X), row 0
;   h_row   rows 1-6, in the gap above each

; ===========================================================================
;  page $6: the rows and their writer
; ===========================================================================
    org 600h

; TROWS: NTROWS rows x NTCOLS x (pointer, control). A pointer is made for
; its row's Y (disp: glyph x 8 - Y / 2).
TROWS:
    db NTROWS*TROW dup (0)

; t_rowwrite: the row at offset A of this page into the singles at Y r4:
; each one's Y, then (X skipped) its pointer and control. VDC selected,
; foreground off. About 290 cycles. Clobbers A, r0-r2.
t_rowwrite:
    mov r1,a
    mov r0,#vdc_char0
    mov r2,#NTCOLS
trw_l:
    mov a,r4
    movx @r0,a                  ; Y
    inc r0
    inc r0                      ; (X)
    mov a,r1
    movp a,@a
    movx @r0,a                  ; pointer
    inc r0
    inc r1
    mov a,r1
    movp a,@a
    movx @r0,a                  ; control
    inc r0
    inc r1
    djnz r2,trw_l
    ret

    if $ > 700h-(100h-OV_N6)
    error "ov_text: page $6 has run into the swap's limit"
    endif

; ===========================================================================
;  page $7: the interrupts, the bands, the sprites
; ===========================================================================
    org 700h
    jmp vbl_isr                   ; OV_VBL
    jmp tim_isr                   ; OV_TIM

TSCRCOL:                        ; the colour register (ov.inc pokes it)
    db TEXT_COL

; vbl_isr: once a frame, in the blank: the first band armed, the sound, the
; clock, the sprites, a dirty object group, row 0, then the objects on.
vbl_isr:
    ISR_IN
    ISR_VDC
    mov r0,#vdc_status
    movx a,@r0                  ; reading the status drops the IRQ
    mov a,#256-TBL_FIRST
    mov t,a
    mov r5,#TBANDS&0FFh
    mov r1,#SNDCNT
    mov a,@r1
    jz tv_q
    call snd_vbl
tv_q:
    mov r1,#FRAMES
    inc @r1
    ISR_CTL CTL_OFF
    mov a,#TSCRCOL&0FFh
    movp a,@a
    mov r0,#vdc_color
    movx @r0,a
    call t_sprw
    mov r1,#ODIRTY
    mov a,@r1
    jz tv_row
    call obj_write
tv_row:
    mov r4,#Y_TROW0             ; (r4 runs down the rows: t_hrow)
    mov a,#TROWS&0FFh
    call t_rowwrite
    ISR_CTL CTL_FG
    ISR_OUT

; tim_isr: a band is due: re-arm first (a late band doesn't make the next
; late), the last ticks QUIET, then the handler.
tim_isr:
    ISR_IN
    mov a,r5
    add a,#2
    movp a,@a
    mov r3,a
    mov a,t
    add a,r3
    mov t,a
    mov a,r3
    jnz tt_go
    mov r1,#QUIET
    inc @r1
tt_go:
    ISR_VDC
    mov a,r5
    movp a,@a
    jmpp @a
jtab:
TJ_ROW  equ $&0FFh
    db t_hrow&0FFh

; t_hrow: the row at the band's argument (an offset in page $6), its Y a
; row below the last one written (r4: the vblank wrote row 0 at Y_TROW0).
t_hrow:
    call t_barg
    mov r1,a
    mov a,r4
    add a,#TPITCH
    mov r4,a
    ISR_CTL CTL_OFF
    mov a,r1
    call t_rowwrite
    ISR_CTL CTL_FG
    ISR_OUT

; t_barg: A = this band's argument, and r5 on to the next band. Clobbers r3.
t_barg:
    mov a,r5
    inc a
    movp a,@a
    mov r3,a
    mov a,r5
    add a,#3
    mov r5,a
    mov a,r3
    ret

; t_sprw: the four sprites' (Y, X, attribute) from SPRTAB. VDC selected,
; foreground off. Clobbers A, r0-r2.
t_sprw:
    mov r1,#SPRTAB&0FFh
    mov r0,#vdc_spr0_ctrl
    mov r2,#4
tsw_l:
    mov a,r1
    movp a,@a
    movx @r0,a
    inc r0
    inc r1
    mov a,r1
    movp a,@a
    movx @r0,a
    inc r0
    inc r1
    mov a,r1
    movp a,@a
    movx @r0,a
    inc r0
    inc r0
    inc r1
    djnz r2,tsw_l
    ret

; The bands: row r (1-6) is written in the gap above it, from LEAD lines
; before row r-1's characters end.
TBL_FIRST equ Y_TROW0+14-TLEAD
TBANDS:
K   set 1
    rept NTROWS-1
    if K<NTROWS-1
    db TJ_ROW, (TROWS&0FFh)+K*TROW, 256-TPITCH
    else
    db TJ_ROW, (TROWS&0FFh)+K*TROW, 0
    endif
K   set K+1
    endm

    if $ > SPRTAB
    error "ov_text: page $7 has run into SPRTAB"
    endif
    org SPRTAB
    db 12 dup (0)

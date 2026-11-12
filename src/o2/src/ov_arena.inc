; ov_arena.inc -- the arena's overlay, $600-$7FF: the display engine while
; the game is played.
;
; The arena's walls are the grid in fill mode. A grid column byte's bit for
; the grid row the beam is in fills that 16 x 24 cell; the grid's rows don't
; match ours (16 lines), so a band at every tile row writes all ten columns
; afresh -- $FF filled, 0 empty, whatever the grid row -- and the grid shows
; any rows we like, each a tile's 16 lines. The grid's registers take writes
; only with the grid off: a band turns it off, writes the row's ten bytes
; straight from GRIDROWS (MOVP, 7 cycles a byte: no staging, so a band costs
; about 6 lines of the CPU), and turns it on again 4 lines later, both
; writes in the horizontal blank (GPREN, GNOPN) -- the seam at the top of
; every tile row.
;
; The pieces (bombs, duds, powerups) are characters (OBJTAB, core.inc),
; written in the vblank only when the main program has changed them
; (ODIRTY, a group a frame), so the foreground stays on from the HUD to the
; bottom: the bombers -- the sprites -- never blink. LOOK 1 adds a band
; partway down each row (MIDL): the row's band writes only the pillars and
; the mid band the whole row, so a pillar has a cap above its body and a
; block is the body alone.
;
;   vblank  the sprites, a dirty object group; objects on
;   h_grid  Y0 + 16r: row r (LOOK 1: the pillars; h_grid at +MIDL: row r)
;   h_end   after row 10: the grid off

; ===========================================================================
;  page $6: the grid's rows and their writer
; ===========================================================================
    org 600h

; GRIDROWS: the 11 rows, a byte a view column: $FF filled, 0 empty.
GRIDROWS:
    db ARENA_H*VIEW_W dup (0)
; LOOK 1: the rows' caps -- none (even rows) or the pillars (odd rows; ar_mid
; pokes them when VOFF changes parity).
CAPNONE:
    db VIEW_W dup (0)
CAPPIL:
    db VIEW_W dup (0)

; g_write: the grid off, the row at offset A of this page into the grid's
; ten columns, the grid on: the seam, 4 lines. Two delays of 2 + 2n cycles
; put the first write and the last in the horizontal blank: n is GPREN and
; GNOPN on NTSC, and the boot pokes the PAL ones into GW_PRE, GW_NOP and
; GO_PRE (a PAL line is longer in cycles). VDC selected. Clobbers A, r0-r2.
g_write:
    mov r1,a
    mov r2,#GPREN               ; (the operand: GW_PRE, poked for PAL)
GW_PRE  equ $-1
    djnz r2,$
    mov r0,#vdc_control
    mov a,#CTL_FG
    movx @r0,a                  ; the grid off: its registers open
    mov r0,#vdc_gridv0
    rept VIEW_W-1
    mov a,r1
    movp a,@a
    movx @r0,a
    inc r0
    inc r1
    endm
    mov a,r1
    movp a,@a
    movx @r0,a
    mov r2,#GNOPN               ; (GW_NOP, poked for PAL)
GW_NOP  equ $-1
    djnz r2,$
    mov r0,#vdc_control
    mov a,#CTL_ARENA
    movx @r0,a                  ; the grid on
    ret

; g_off: the grid off, with g_write's timing (the arena's last band).
; Clobbers A, r0, r2.
g_off:
    nop
    mov r2,#GPREN               ; (GO_PRE, poked for PAL)
GO_PRE  equ $-1
    djnz r2,$
    mov r0,#vdc_control
    mov a,#CTL_FG
    movx @r0,a
    ret

    if $ > 700h-(100h-OV_N6)
    error "ov_arena: page $6 has run into the swap's limit"
    endif

; ===========================================================================
;  page $7: the interrupts, the bands, the sprites
; ===========================================================================
    org 700h
    jmp vbl_isr                 ; OV_VBL
    jmp tim_isr                 ; OV_TIM

ASCRCOL:                        ; the colour register: floor and walls
    db ARENA_COL

; vbl_isr: once a frame, in the blank: the first band armed (the counter
; counts display lines only, so it is exact anywhere in the blank), the
; sound, the clock, the sprites, a dirty object group, then the objects on;
; the grid waits for row 0's band.
vbl_isr:
    ISR_IN
    ISR_VDC
    mov r0,#vdc_status
    movx a,@r0                  ; reading the status drops the IRQ
    mov a,#256-BL_FIRST         ; (VB_FIRST, poked for PAL)
VB_FIRST equ $-1
    mov t,a
    mov r5,#BANDS&0FFh
    mov r1,#SNDCNT
    mov a,@r1
    jz vb_q
    call snd_vbl
vb_q:
    mov r1,#FRAMES
    inc @r1
    ISR_CTL CTL_OFF
    mov a,#ASCRCOL&0FFh
    movp a,@a
    mov r0,#vdc_color
    movx @r0,a
    call spr_write
    mov r1,#ODIRTY
    mov a,@r1
    jz vb_on
    call obj_write
vb_on:
    ISR_CTL CTL_FG
    ISR_OUT

; tim_isr: a band is due. The counter is re-armed first, plus what it has
; counted since the overflow, so a late band doesn't make the next late. The
; last band ticks QUIET (input.inc frame_sync). Then the handler.
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
    jnz ti_go
    mov r1,#QUIET
    inc @r1
ti_go:
    ISR_VDC
    mov a,r5
    movp a,@a
    jmpp @a
jtab:
J_GRID  equ $&0FFh
    db h_grid&0FFh
J_END   equ $&0FFh
    db h_end&0FFh

; h_grid: the row at the band's argument (an offset in page $6) into the
; grid.
h_grid:
    call band_arg
    call g_write
    ISR_OUT

; h_end: the arena has ended: the grid off.
h_end:
    call band_arg
    call g_off
    ISR_OUT

; band_arg: A = this band's argument, and r5 on to the next band. Clobbers
; r3.
band_arg:
    mov a,r5
    inc a
    movp a,@a
    mov r3,a
    mov a,r5
    add a,#3
    mov r5,a
    mov a,r3
    ret

; spr_write: the four sprites' (Y, X, attribute) from SPRTAB. Their shapes
; the main program writes itself. VDC selected, foreground off.
; Clobbers A, r0-r2.
spr_write:
    mov r1,#SPRTAB&0FFh
    mov r0,#vdc_spr0_ctrl
    mov r2,#4
sw_l:
    mov a,r1
    movp a,@a
    movx @r0,a                  ; Y
    inc r0
    inc r1
    mov a,r1
    movp a,@a
    movx @r0,a                  ; X
    inc r0
    inc r1
    mov a,r1
    movp a,@a
    movx @r0,a                  ; attribute
    inc r0
    inc r0
    inc r1
    djnz r2,sw_l
    ret

; The bands: (handler, argument, 256 - lines to the next band, 0: the last).
; Row r's band starts GLEAD lines before its line (the interrupt takes a
; line or two to reach its writes).
BL_FIRST equ Y0-GLEAD
BL_FIRSTP equ Y0-GLEADP          ; PAL's (the boot pokes VB_FIRST)
    ifndef MIDL
MIDL    equ 7                   ; LOOK 1: the row's band, lines into a row
    endif
BANDS:
K   set 0
    rept ARENA_H
    if LOOK=0
    db J_GRID, (GRIDROWS&0FFh)+VIEW_W*K, 256-TILE_H
    else
    if K&1
    db J_GRID, CAPPIL&0FFh, 256-MIDL
    else
    db J_GRID, CAPNONE&0FFh, 256-MIDL
    endif
    db J_GRID, (GRIDROWS&0FFh)+VIEW_W*K, 256-(TILE_H-MIDL)
    endif
K   set K+1
    endm
    db J_END, 0, 0

    if $ > SPRTAB
    error "ov_arena: page $7 has run into SPRTAB"
    endif
    org SPRTAB
    db 12 dup (0)

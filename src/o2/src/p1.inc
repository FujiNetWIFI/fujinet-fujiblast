; p1.inc -- page 1: the arena's drawing, by farcall (entries A_*): the main
; program's side of ov_arena.inc. It reads the state through st_get and
; writes the overlay's tables through POKE; the vblank and the bands do the
; rest. Arguments in r2-r5 (farcall keeps them).

    PAGE_HEAD
    db e_rows&0FFh              ; A_ROWS: every grid row
    db e_row&0FFh               ; A_ROW: grid row r4
    db e_pieces&0FFh            ; A_PIECES: the characters on the map
    db e_hud&0FFh               ; A_HUD: the clock and the trophies
    db e_bombers&0FFh           ; A_BOMBERS: the sprites from SPX/SPY/SPF
    db e_mid&0FFh               ; A_MID: the pillars' band for VOFF
    db e_shapes&0FFh            ; A_SHAPES: each sprite's shape for its SPF
e_rows:
    jmp ar_rows
e_row:
    jmp ar_row
e_pieces:
    jmp ar_pieces
e_hud:
    jmp ar_hud
e_bombers:
    jmp ar_bombers
e_mid:
    jmp ar_mid
e_shapes:
    jmp ab_shapes

; ===========================================================================
;  the grid's rows
; ===========================================================================

; ar_rows: GRIDROWS from the map, all 11. About 3500 cycles. Clobbers A,
; r0-r7.
ar_rows:
    clr a
    xset LITL
    xset LITH
    mov r4,#0
arr_l:
    mov a,r4
    mov r0,#ROWT
    movx @r0,a
    call ar_row
    mov r0,#ROWT
    movx a,@r0
    inc a
    mov r4,a
    xrl a,#ARENA_H
    jnz arr_l
    ret

; ar_row: GRIDROWS row r4 from the map: view columns 0-9 = arena columns
; VOFF on; a pillar or block filled, a flame or a burning block filled while
; BLINK. About 330 cycles. Clobbers A, r0-r3, r5, r6.
ar_row:
    mov a,r4                    ; GRIDROWS + 10r
    add a,#rowx10&0FFh
    movp a,@a
    add a,#GRIDROWS&0FFh
    mov r2,#GRIDROWS>>8
    FCALL poke_at
    mov a,r4
    add a,#rowbase&0FFh
    movp a,@a                   ; 13r
    add a,#OFF_MAP
    mov r5,a
    mov r0,#VOFF
    movx a,@r0
    add a,r5
    mov r5,a                    ; st offset of view column 0's tile
    mov r0,#BLINK
    movx a,@r0
    mov r3,a
    mov r6,#VIEW_W
    mov r0,#FN_W_REGDATA
arw_l:
    mov a,r5
    FCALL st_get
    add a,#gclass&0FFh
    movp a,@a                   ; 0 floor, 0FFh wall, 1 lit
    jb1 arw_put                 ; a wall ($FF)
    jb0 arw_lit
arw_put:
    movx @r0,a
    inc r5
    djnz r6,arw_l
    ret
arw_lit:
    call row_bit                ; lit: the row marked (the blink redraws
    mov r0,#FN_W_REGDATA        ; it), BLINK's (row_bit spent r0)
    mov a,r3
    jmp arw_put

; row_bit: row r4 marked in LITL/LITH. Clobbers A, r0, r1.
row_bit:
    mov a,r4
    anl a,#7
    add a,#bit8&0FFh
    movp a,@a
    mov r1,a
    mov a,r4
    mov r0,#LITL
    jb3 rb_h
    jmp rb_s
rb_h:
    mov r0,#LITH
rb_s:
    movx a,@r0
    orl a,r1
    movx @r0,a
    ret
bit8:   db 1, 2, 4, 8, 10h, 20h, 40h, 80h

; gclass: a tile's kind for the grid (proto.inc T_*): 0 floor, 0FFh a wall,
; 1 lit (a flame, a burning block: BLINK).
gclass:
    db 0, 0FFh, 0FFh, 1         ; floor, pillar, block, burn
    db 0, 0                     ; bomb, dud
    db 1, 1, 1, 1, 1, 1, 1      ; the flames
    db 0, 0, 0, 0, 0            ; the powerups

; rowbase: 13 x row. rowx10: 10 x row.
rowbase:
K   set 0
    rept ARENA_H
    db ARENA_W*K
K   set K+1
    endm
rowx10:
K   set 0
    rept ARENA_H
    db VIEW_W*K
K   set K+1
    endm

; ar_mid: CAPPIL, the pillars' caps (LOOK 1): pillars on the odd arena
; columns, so on view columns of VOFF's other parity. Clobbers A, r0-r2, r6.
ar_mid:
    mov r2,#CAPPIL>>8
    mov a,#CAPPIL&0FFh
    FCALL poke_at
    mov r0,#VOFF
    movx a,@r0
    jb0 am_odd
    clr a                       ; VOFF even: view column 0 empty, 1 a pillar
    jmp am_go
am_odd:
    mov a,#0FFh
am_go:
    mov r6,#VIEW_W
    mov r0,#FN_W_REGDATA
am_l:
    movx @r0,a
    cpl a
    djnz r6,am_l
    ret

    if $ > 900h
    error "page 1's page $8 has run out"
    endif

; ===========================================================================
;  the pieces: characters on the map
; ===========================================================================
    org 900h

; ar_pieces: the singles from the map's pieces in view (bombs, duds,
; powerups) and the movers; the rest parked. Then ODIRTY's singles. About
; 3000 cycles. Clobbers A, r0-r7.
ar_pieces:
    mov r2,#OBJTAB>>8           ; the singles from the top
    mov a,#OBJTAB&0FFh
    FCALL poke_at
    mov r0,#NSING
    clr a
    movx @r0,a
    mov r4,#0                   ; row
ap_row:
    mov a,r4
    mov r0,#ROWT
    movx @r0,a
    add a,#rowbase9&0FFh
    movp a,@a
    add a,#OFF_MAP
    mov r5,a
    mov r0,#VOFF
    movx a,@r0
    add a,r5
    mov r5,a                    ; st offset of view column 0
    mov r6,#0                   ; view column
ap_col:
    mov a,r5
    FCALL st_get
    add a,#pglyph&0FFh
    movp a,@a                   ; the tile's glyph, 0FFh: no piece
    inc a
    jz ap_next
    dec a
    mov r7,a                    ; glyph
    mov a,r5                    ; its colour: pcol[tile]
    FCALL st_get
    add a,#pcol&0FFh
    movp a,@a
    mov r3,a                    ; colour << 1
    call ap_put
ap_next:
    inc r5
    inc r6
    mov a,r6
    xrl a,#VIEW_W
    jnz ap_col
    mov r0,#ROWT
    movx a,@r0
    inc a
    mov r4,a
    xrl a,#ARENA_H
    jnz ap_row
    ; the rest parked below the screen
ap_park:
    mov r0,#NSING
    movx a,@r0
    xrl a,#12
    jz ap_done
    mov a,#0F8h
    call ap_b
    clr a
    call ap_b
    call ap_b
    call ap_b
    mov r0,#NSING
    movx a,@r0
    inc a
    movx @r0,a
    jmp ap_park
ap_done:
    mov r2,#OBJQ>>8             ; the arena's quads parked (PAL shows Y 0)
    mov a,#OBJQ&0FFh
    FCALL poke_at
    mov r6,#2
apq_l:
    mov a,#0F8h
    call ap_b
    clr a
    mov r7,#9
apq_z:
    call ap_b
    djnz r7,apq_z
    djnz r6,apq_l
    mov r1,#ODIRTY
    mov a,@r1
    orl a,#OD_SING|OD_QA
    mov @r1,a
    ret

; ap_put: a single for glyph r7 in colour r3 (<< 1) at row r4, view column
; r6, if one is left. Clobbers A, r0-r2.
ap_put:
    mov r0,#NSING
    movx a,@r0
    xrl a,#12
    jz app_x                    ; none left
    xrl a,#12
    inc a
    movx @r0,a
    mov a,r4                    ; Y = Y0 + 16r + Y_CHAR
    swap a
    add a,#Y0+Y_CHAR
    mov r2,a
    call ap_b
    mov a,r6                    ; X = X_MID + 16v
    swap a
    add a,#X_MID
    call ap_b
    mov a,r2                    ; the pointer: glyph x 8 - Y / 2
    rr a
    anl a,#7Fh
    mov r1,a                    ; Y / 2
    mov a,r7
    rl a
    rl a
    rl a
    anl a,#0F8h                 ; (glyph x 8) & $FF
    cpl a
    add a,r1
    cpl a                       ; - Y/2; C: a borrow
    call ap_b
    mov a,r7                    ; bit 8: (glyph >> 5) - borrow, its bit 0
    swap a
    rr a
    anl a,#1
    jnc ap_nb
    xrl a,#1
ap_nb:
    orl a,r3
    call ap_b
app_x:
    ret

; ap_b: POKE A (the pointer was aimed at OBJTAB's next byte). Keeps A.
ap_b:
    mov r0,#FN_W_REGDATA
    movx @r0,a
    ret

; pglyph, pcol: a tile's piece -- its glyph (0FFh: none) and colour << 1.
pglyph:
    db 0FFh, 0FFh, 0FFh, 0FFh   ; floor, pillar, block, burn
    db G_BALL, G_BALL           ; bomb, dud
    db 7 dup (0FFh)             ; the flames
    db 27, 37, 25, 31, 28       ; F(ire), B(omb up), S(peed), K(ick), G(love)
rowbase9:                       ; (rowbase, for this page's readers)
K   set 0
    rept ARENA_H
    db ARENA_W*K
K   set K+1
    endm
pcol:
    db 0, 0, 0, 0
    db C_DGREY<<1, C_VIOLET<<1
    db 7 dup (0)
    db C_RED<<1, C_WHITE<<1, C_CYAN<<1, C_TAN<<1, C_VIOLET<<1

    if $ > 0A00h
    error "page 1's page $9 has run out"
    endif

; ===========================================================================
;  the HUD and the bombers
; ===========================================================================
    org 0A00h

; ar_hud: the HUD's quads (2 and 3, interleaved: 8 characters at 8 pixels):
; seat 0's and 1's trophies, the clock m:ss, seat 2's and 3's, each seat's in
; its colour. Clobbers A, r0-r7.
X_HUD   equ 56
ar_hud:
    mov a,#OFF_TIMER            ; the clock: minutes, tens, units
    FCALL st_get
    mov r2,#0                   ; minutes
ah_m:
    add a,#256-60
    jnc ah_mx
    inc r2
    jmp ah_m
ah_mx:
    add a,#60
    mov r3,#0                   ; tens
ah_t:
    add a,#256-10
    jnc ah_tx
    inc r3
    jmp ah_t
ah_tx:
    add a,#10
    mov r4,a                    ; units
    ; LINEBUF: 8 (glyph, colour << 1)
    mov r0,#LINEBUF
    mov r5,#0
    call ah_win
    mov r5,#1
    call ah_win
    mov a,r2
    mov r6,#C_TAN<<1
    call ah_put
    mov a,#10                   ; ':'
    call ah_put
    mov a,r3
    call ah_put
    mov a,r4
    call ah_put
    mov r5,#2
    call ah_win
    mov r5,#3
    call ah_win
    ; quad 2: characters 0, 2, 4, 6; quad 3: 1, 3, 5, 7
    mov r2,#OBJQ>>8
    mov a,#(OBJQ+20)&0FFh
    FCALL poke_at
    mov r1,#LINEBUF
    mov r5,#X_HUD
    call ah_quad
    mov r1,#LINEBUF+2
    mov r5,#X_HUD+8
    call ah_quad
    mov r1,#ODIRTY
    mov a,@r1
    orl a,#OD_QH
    mov @r1,a
    ret

; ah_win: seat r5's trophies (a digit, '-' if not playing) into LINEBUF at
; r0, in its colour. Clobbers A, r6.
ah_win:
    mov a,r5
    add a,#scol&0FFh
    movp a,@a
    mov r6,a
    mov a,#OFF_INPLAY
    FCALL st_get
    mov r7,a
    mov a,r5
    add a,#sbit&0FFh
    movp a,@a
    anl a,r7
    jz ahw_none
    mov a,r5
    add a,#soff&0FFh
    movp a,@a
    add a,#P_WINS
    FCALL st_get
    jmp ah_put
ahw_none:
    mov a,#40                   ; '-'
; ah_put: glyph A, colour r6, into LINEBUF at r0. Clobbers A.
ah_put:
    movx @r0,a
    inc r0
    mov a,r6
    movx @r0,a
    inc r0
    ret

; ah_quad: a quad at X r5 from LINEBUF r1, every other character: Y, X, then
; 4 x (pointer, control) poked. Clobbers A, r0, r1, r6, r7.
ah_quad:
    mov a,#Y_HUD
    call ap_b2
    mov a,r5
    call ap_b2
    mov r7,#4
ahq_l:
    movx a,@r1                  ; glyph
    inc r1
    rl a
    rl a
    rl a
    mov r6,a                    ; (glyph x 8) & $FF, high bit in bits 0-2
    anl a,#0F8h
    add a,#256-Y_HUD/2
    call ap_b2                  ; C: no borrow
    mov a,r6
    anl a,#1                    ; glyph x 8, bit 8 (glyph bit 5, rotated)
    jc ahq_nb
    xrl a,#1
ahq_nb:
    mov r6,a
    movx a,@r1                  ; colour << 1
    inc r1
    orl a,r6
    call ap_b2
    inc r1                      ; the next character but one
    inc r1
    djnz r7,ahq_l
    ret

ap_b2:
    mov r0,#FN_W_REGDATA
    movx @r0,a
    ret

; seat tables
scol:   db C_WHITE<<1, C_RED<<1, C_CYAN<<1, C_TAN<<1
sbit:   db 1, 2, 4, 8
soff:   db OFF_PLAYERS, OFF_PLAYERS+PLAYER_SIZE, OFF_PLAYERS+2*PLAYER_SIZE, OFF_PLAYERS+3*PLAYER_SIZE

    if $ > 0B00h
    error "page 1's page $A has run out"
    endif

    org 0B00h

; ar_bombers: SPRTAB from SPX/SPY/SPF -- a bomber outside the view hidden --
; and each sprite's shape when its frame changed (SHAPED). Clobbers A,
; r0-r7.
ar_bombers:
    mov r2,#SPRTAB>>8
    mov a,#SPRTAB&0FFh
    FCALL poke_at
    mov r4,#0
ab_l:
    mov a,#SPF                  ; hidden?
    add a,r4
    mov r0,a
    movx a,@r0
    inc a
    jz ab_hide
    mov r0,#VOFF                ; x - 16 VOFF, in view: 0-144
    movx a,@r0
    swap a
    mov r1,a
    mov a,#SPX
    add a,r4
    mov r0,a
    movx a,@r0
    cpl a
    add a,r1
    cpl a                       ; C: a borrow (x < 16 VOFF)
    jc ab_hide
    mov r1,a
    add a,#256-145
    jc ab_hide
    mov a,#SPY
    add a,r4
    mov r0,a
    movx a,@r0
    add a,#Y0
    call ab_b                   ; Y
    mov a,r1
    add a,#X_MID
    call ab_b                   ; X
    mov a,r4
    add a,#ssprc&0FFh
    movp a,@a
    call ab_b                   ; attribute: the seat's colour
    jmp ab_n
ab_hide:
    mov a,#SPR_OFF
    call ab_b
    clr a
    call ab_b
    call ab_b
ab_n:
    inc r4
    mov a,r4
    xrl a,#4
    jnz ab_l
    jmp ab_shapes

ab_b:
    mov r0,#FN_W_REGDATA
    movx @r0,a
    ret

; the seats' sprite colours (attribute bits 5-3)
ssprc:  db 7*S_COL, 1*S_COL, 6*S_COL, 3*S_COL

; ab_shapes: each sprite whose frame is not the one in its shape (SHAPED):
; its 8 bytes from frames, written straight to the VDC (the shapes take
; writes with the display on). Clobbers A, r0-r7.
ab_shapes:
    mov r4,#0
abs_l:
    mov a,#SPF
    add a,r4
    mov r0,a
    movx a,@r0
    mov r5,a                    ; frame
    inc a
    jz abs_n                    ; hidden: whatever it had
    mov a,#SHAPED
    add a,r4
    mov r1,a
    movx a,@r1
    xrl a,r5
    jz abs_n
    mov a,r5
    movx @r1,a
    rl a
    rl a
    rl a                        ; frame x 8
    add a,#frames&0FFh
    mov r1,a
    mov a,r4
    rl a
    rl a
    rl a
    add a,#vdc_spr0_shape
    mov r0,a
    mov a,#P1_VDC
    outl p1,a
    mov r2,#8
abs_c:
    mov a,r1
    movp a,@a
    movx @r0,a
    inc r0
    inc r1
    djnz r2,abs_c
    mov a,#P1_MAIN
    outl p1,a
abs_n:
    inc r4
    mov a,r4
    xrl a,#4
    jnz abs_l
    ret

    include "sprites.inc"

    if $ > 0C00h
    error "page 1's page $B has run out"
    endif

    PAGE_TAIL

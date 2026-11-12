; p10.inc -- page 10: our own walking, by farcall (W_*), exactly as
; src/move.c (and the server's internal/game/move.go Walker): the server
; checks it.
;
; The bomber is a tile-sized box at MVX, MVY (16 units a tile); one of the
; two is always on a lane (a multiple of 16). Pushing into a cross lane from
; up to LANE units off it slides onto it.

    PAGE_HEAD
    db e_walk&0FFh              ; W_WALK: the stick; r2 = 1 if we moved
    db e_centre&0FFh            ; W_CENTRE: r2 = the tile under MVX, MVY
    db e_seats&0FFh             ; W_SEATS: the bombers drawn (p2's)
e_walk:
    jmp walk
e_centre:
    jmp w_centre
e_seats:
    jmp seats_draw

; w_centre: r2 = centre_tile(MVX, MVY). Clobbers A, r0, r3.
w_centre:
    xget MVX
    mov r2,a
    xget MVY
    mov r3,a
    call centre_tile
    mov r2,a
    ret

; my_spd: r5 = our player's offset (MYSEAT seated). Clobbers A.
my_spd:
    xget MYSEAT
    add a,#sofw&0FFh
    movp a,@a
    mov r5,a
    ret
sofw:   db OFF_PLAYERS, OFF_PLAYERS+PLAYER_SIZE, OFF_PLAYERS+2*PLAYER_SIZE, OFF_PLAYERS+3*PLAYER_SIZE

; walk: the stick to a direction (try_dirs): on a diagonal, the turn first
; (the way we aren't going), and if that's walled, on as we were. r2 = 1 if
; we moved. Clobbers A, r0-r7.
walk:
    call my_spd                 ; our speed level
    mov a,r5
    add a,#P_SPEED
    FCALL st_get
    iset WLV
    xget JOY
    mov r2,a
    mov r3,#NONE                ; v
    mov r4,#NONE                ; h
    jb0 wk_u
    jb1 wk_d
    jmp wk_h
wk_u:
    mov r3,#DIR_UP
    jmp wk_h
wk_d:
    mov r3,#DIR_DOWN
wk_h:
    mov a,r2
    jb2 wk_l
    jb3 wk_r
    jmp wk_1
wk_l:
    mov r4,#DIR_LEFT
    jmp wk_1
wk_r:
    mov r4,#DIR_RIGHT
wk_1:
    mov a,r4                    ; one axis (or none): just that
    inc a
    jnz wk_2
    mov a,r3
    jmp mv_step
wk_2:
    mov a,r3
    inc a
    jnz wk_diag
    mov a,r4
    jmp mv_step
wk_diag:                        ; a = the turn: v if going left/right, else h
    xget MVDIR
    jb0 wk_lr                   ; (RIGHT 1, LEFT 3: bit 0)
    mov a,r4
    mov r5,a                    ; a
    mov a,r3
    mov r6,a                    ; b
    jmp wk_try
wk_lr:
    mov a,r3
    mov r5,a
    mov a,r4
    mov r6,a
wk_try:
    xget MVX                    ; saved, in case a is walled
    iset WSX
    xget MVY
    iset WSY
    xget MVACC
    iset WSA
    mov a,r6
    iset WB
    mov a,r5
    call mv_step
    mov a,r2
    jnz wk_x
    iget WSX
    xset MVX
    iget WSY
    xset MVY
    iget WSA
    xset MVACC
    iget WB
    jmp mv_step
wk_x:
    ret

; mv_step: one jiffy toward A (NONE: still) at speed level WLV: r2 = 1 if
; it moved. MVBUMP: a bomb or dud walked into (else NONE). Clobbers A,
; r0-r7.
mv_step:
    mov r7,a
    mov a,#NONE
    xset MVBUMP
    mov r2,#0
    mov a,r7
    inc a
    jnz ms_go
    clr a
    xset MVACC
    ret
ms_go:
    mov a,r7
    xset MVDIR
    iget WLV                    ; acc += 16 + 4 lv
    rl a
    rl a
    add a,#SPEED_BASE
    mov r6,a
    xget MVACC
    add a,r6
    mov r6,a
    anl a,#0Fh
    movx @r0,a                  ; acc &= 15
    mov a,r6
    swap a
    anl a,#0Fh                  ; n = acc >> 4
    jz ms_x
    mov r5,a
ms_l:
    mov a,r5
    iset WN
    mov a,r2
    iset WMOVED
    mov a,r7
    call unit_step
    jz ms_stop
    iget WMOVED
    mov a,#1
    mov r2,a
    xget MVDIR
    mov r7,a
    iget WN
    mov r5,a
    djnz r5,ms_l
ms_x:
    ret
ms_stop:
    clr a
    xset MVACC
    iget WMOVED
    mov r2,a
    ret

    if $ > 900h
    error "page 10's page $8 has run out"
    endif

; ===========================================================================
    org 900h

; unit_step: a unit toward direction A: A nonzero if it moved. Clobbers A,
; r0-r6.
unit_step:
    jb0 us_h                    ; RIGHT 1, LEFT 3
    xrl a,#DIR_DOWN             ; up / down: dy = +1 / -1
    mov r6,#1
    jz us_v
    mov r6,#0FFh
us_v:
    xget MVX                    ; off = x & 15; c = x >> 4; r = (y >> 4) + dy
    anl a,#0Fh
    mov r5,a
    xget MVX
    swap a
    anl a,#0Fh
    mov r2,a
    xget MVY
    swap a
    anl a,#0Fh
    add a,r6
    mov r3,a
    mov a,r5
    jz us_vl                    ; on the lane
    add a,#256-LANE-1           ; off <= LANE: slide left onto it
    jc us_v2
    call walkable
    jz us_v2
    xget MVX
    dec a
    movx @r0,a
    jmp us_yes
us_v2:
    mov a,r5                    ; off >= 16 - LANE: slide right
    add a,#256-(UNIT-LANE)
    jnc us_no
    inc r2
    call walkable
    jz us_no
    xget MVX
    inc a
    movx @r0,a
    jmp us_yes
us_vl:
    xget MVY                    ; on the lane: a wall ahead at a tile's edge
    anl a,#0Fh
    jnz us_vgo
    call walkable
    jnz us_vgo
    call bumped
    jmp us_no
us_vgo:
    xget MVY
    add a,r6
    movx @r0,a
    jmp us_yes
us_h:                           ; left / right: dx = -1 / +1
    xrl a,#DIR_RIGHT
    mov r6,#1
    jz us_hh
    mov r6,#0FFh
us_hh:
    xget MVY                    ; off = y & 15; c = (x >> 4) + dx; r = y >> 4
    anl a,#0Fh
    mov r5,a
    xget MVX
    swap a
    anl a,#0Fh
    add a,r6
    mov r2,a
    xget MVY
    swap a
    anl a,#0Fh
    mov r3,a
    mov a,r5
    jz us_hl
    add a,#256-LANE-1           ; off <= LANE: slide up onto it
    jc us_h2
    call walkable
    jz us_h2
    xget MVY
    dec a
    movx @r0,a
    jmp us_yes
us_h2:
    mov a,r5                    ; off >= 16 - LANE: slide down
    add a,#256-(UNIT-LANE)
    jnc us_no
    inc r3
    call walkable
    jz us_no
    xget MVY
    inc a
    movx @r0,a
    jmp us_yes
us_hl:
    xget MVX
    anl a,#0Fh
    jnz us_hgo
    call walkable
    jnz us_hgo
    call bumped
    jmp us_no
us_hgo:
    xget MVX
    add a,r6
    movx @r0,a
us_yes:
    mov a,#1
    ret
us_no:
    clr a
    ret

    if $ > 0A00h
    error "page 10's page $9 has run out"
    endif

; ===========================================================================
    org 0A00h

; walkable: A nonzero if tile (r2, r3) can be walked onto: in the arena, not
; our pending bomb, not a pillar, block, burning block, bomb or dud. (r2, r3
; may be -1: as unsigned it is out of the arena too.) Clobbers A, r0, r1.
walkable:
    mov a,r2
    add a,#256-ARENA_W
    jc wa_no
    mov a,r3
    add a,#256-ARENA_H
    jc wa_no
    call tile_of
    mov r1,a
    xget PEND
    xrl a,r1
    jz wa_no
    mov a,r1
    add a,#OFF_MAP
    FCALL st_get
    add a,#wtab&0FFh
    movp a,@a
    ret
wa_no:
    clr a
    ret

; bumped: tile (r2, r3), in the arena, a bomb or a dud: MVBUMP = it.
; Clobbers A, r0, r1.
bumped:
    mov a,r2
    add a,#256-ARENA_W
    jc bu_x
    mov a,r3
    add a,#256-ARENA_H
    jc bu_x
    call tile_of
    mov r1,a
    add a,#OFF_MAP
    FCALL st_get
    xrl a,#T_BOMB
    jz bu_b
    xrl a,#T_BOMB!T_DUD
    jnz bu_x
bu_b:
    mov a,r1
    xset MVBUMP
bu_x:
    ret

; tile_of: A = r3 x 13 + r2. Clobbers nothing else.
tile_of:
    mov a,r3
    add a,#rowsc&0FFh
    movp a,@a
    add a,r2
    ret

; centre_tile: A = the tile under the box at (r2, r3) units: ((x + 8) >> 4,
; (y + 8) >> 4), held in the arena. Clobbers r2, r3.
centre_tile:
    mov a,r2
    add a,#UNIT/2
    swap a
    anl a,#0Fh
    mov r2,a
    add a,#256-ARENA_W
    jnc ct_y
    mov r2,#ARENA_W-1
ct_y:
    mov a,r3
    add a,#UNIT/2
    swap a
    anl a,#0Fh
    mov r3,a
    add a,#256-ARENA_H
    jnc tile_of
    mov r3,#ARENA_H-1
    jmp tile_of

; wtab: a tile walkable? (T_*)
wtab:   db 1, 0, 0, 0, 0, 0           ; floor; pillar, block, burn, bomb, dud
        db 1, 1, 1, 1, 1, 1, 1        ; the flames
        db 1, 1, 1, 1, 1              ; the powerups
rowsc:
K   set 0
    rept ARENA_H
    db ARENA_W*K
K   set K+1
    endm

; toward: A (at) a step toward r2 (to): 3 units, or there at once if more
; than a tile off or 3 or less. Clobbers r0, r7.
toward:
    mov r7,a
    cpl a
    add a,r2                    ; to - at - 1; C: to > at
    jc tw_up
    mov a,r2
    cpl a
    add a,r7                    ; at - to - 1; C: at > to
    jnc tw_eq
    inc a                       ; d = at - to
    mov r0,a
    add a,#256-UNIT-1
    jc tw_to
    mov a,r0
    add a,#256-4
    jnc tw_to
    mov a,r7
    add a,#256-3
    ret
tw_up:
    inc a                       ; d = to - at
    mov r0,a
    add a,#256-UNIT-1
    jc tw_to
    mov a,r0
    add a,#256-4
    jnc tw_to
    mov a,r7
    add a,#3
    ret
tw_to:
    mov a,r2
    ret
tw_eq:
    mov a,r7
    ret

; frame_of: r2 = the frame for direction r5 (DIR_*) and moving r6: right
; and left the man stands or walks; up and down he faces us, mirrored on
; every other step. Clobbers A, r1.
frame_of:
    mov a,r5
    add a,#frbase&0FFh
    movp a,@a
    mov r2,a
    mov a,r6
    jz fo_x
    mov r1,#FRAMES              ; walking: the second frame every 8 frames
    mov a,@r1
    anl a,#8
    jz fo_x
    inc r2
fo_x:
    ret

frbase: db FR_FWD0, FR_RIGHT0, FR_FWD0, FR_LEFT0      ; up, right, down, left

; pk2: POKE A (keeps A). Clobbers r0.
pk2:
    mov r0,#FN_W_REGDATA
    movx @r0,a
    ret

    if $ > 0B00h
    error "page 10's page $A has run out"
    endif

; ===========================================================================
;  the bombers (p2's, by farcall: W_SEATS)
; ===========================================================================
    org 0B00h

; seats_draw: each bomber: hidden if not playing or out of the view;
; knocked out: its falling frames for 36 frames, then hidden; ours where we
; walk; the others toward the server's place. Poked straight into SPRTAB
; (the vblank writes it); the shapes (p1) only when a frame changed.
; Clobbers A, r0-r7.
seats_draw:
    mov r2,#SPRTAB>>8
    mov a,#SPRTAB&0FFh
    FCALL poke_at
    mov r4,#0
sd_l:
    mov a,r4
    xset LTMP
    mov a,#OFF_INPLAY
    FCALL st_get
    mov r2,a
    mov a,r4
    add a,#bitd&0FFh
    movp a,@a
    mov r3,a
    anl a,r2
    jz sd_hide
    mov a,#OFF_ALIVE
    FCALL st_get
    anl a,r3
    jnz sd_alive
    mov a,#DIET                 ; knocked out
    add a,r4
    mov r1,a
    movx a,@r1
    jz sd_hide
    add a,#256-36
    jc sd_hide
    movx a,@r1
    inc a
    movx @r1,a
    dec a
    mov r2,#FR_DIE0             ; a frame every 12
sd_df:
    add a,#256-12
    jnc sd_put
    inc r2
    jmp sd_df
sd_hide:
    mov r2,#0FFh
    jmp sd_set
sd_alive:
    xget MYSEAT
    xrl a,r4
    jnz sd_other
    xget MOVED                  ; ours: where we walk
    mov r6,a
    xget MVDIR
    mov r5,a
    mov a,#DISPX
    add a,r4
    mov r1,a
    xget MVX
    movx @r1,a
    mov a,#DISPY
    add a,r4
    mov r1,a
    xget MVY
    movx @r1,a
    jmp sd_fr
sd_other:
    mov a,r4
    add a,#soffd&0FFh
    movp a,@a
    mov r5,a
    add a,#P_DIR
    FCALL st_get
    mov r6,a                    ; dir | moving
    mov a,r5
    FCALL st_get
    mov r2,a
    mov a,#DISPX
    add a,r4
    mov r1,a
    movx a,@r1
    call toward
    movx @r1,a
    mov a,r5
    inc a
    FCALL st_get
    mov r2,a
    mov a,#DISPY
    add a,r4
    mov r1,a
    movx a,@r1
    call toward
    movx @r1,a
    mov a,r6
    anl a,#3
    mov r5,a                    ; dir
    mov a,r6
    anl a,#P_DIR_MOVING
    mov r6,a
sd_fr:
    call frame_of               ; r2 = the frame for dir r5, moving r6
sd_put:                         ; at DISPX/Y, if in the view
    xget VOFF
    swap a
    mov r3,a                    ; 16 VOFF
    xget LTMP
    add a,#DISPX
    mov r0,a
    movx a,@r0
    cpl a
    add a,r3
    cpl a                       ; x - 16 VOFF; C: a borrow
    jc sd_hide
    mov r3,a
    add a,#256-145
    jc sd_hide
    xget LTMP
    add a,#DISPY
    mov r0,a
    movx a,@r0
    add a,#Y0
    call pk2                    ; Y
    mov a,r3
    add a,#X_MID
    call pk2                    ; X
    xget LTMP
    add a,#ssprd&0FFh
    movp a,@a
    call pk2                    ; the seat's colour
    jmp sd_f
sd_set:                         ; hidden
    mov a,#SPR_OFF
    call pk2
    clr a
    call pk2
    call pk2
sd_f:
    xget LTMP
    add a,#SPF
    mov r0,a
    mov a,r2
    movx @r0,a
    xget LTMP
    inc a
    mov r4,a
    xrl a,#SEATS
    jnz sd_l
    jmp shp_chk                 ; a shape to change?


bitd:   db 1, 2, 4, 8
ssprd:  db 7*S_COL, 1*S_COL, 6*S_COL, 3*S_COL
soffd:  db OFF_PLAYERS, OFF_PLAYERS+PLAYER_SIZE, OFF_PLAYERS+2*PLAYER_SIZE, OFF_PLAYERS+3*PLAYER_SIZE

    if $ > 0C00h
    error "page 10's page $B has run out"
    endif

; ===========================================================================
;  the sprites' shapes (p1's, again: an entry may not farcall)
; ===========================================================================
    org 0C00h
shp_chk:                        ; a shape to change: the shapes
    mov r4,#SEATS
    mov r0,#SPF
    mov r1,#SHAPED
sd_c:
    movx a,@r0
    mov r2,a
    inc a
    jz sd_cn                    ; (hidden: whatever it had)
    movx a,@r1
    xrl a,r2
    jnz sd_sh
sd_cn:
    inc r0
    inc r1
    djnz r4,sd_c
    ret
sd_sh:
    jmp shapes
; shapes: each sprite whose frame is not the one in its shape (SHAPED):
; its 8 bytes from frames, written straight to the VDC (the shapes take
; writes with the display on). Clobbers A, r0-r7.
shapes:
    mov r4,#0
shp_l:
    mov a,#SPF
    add a,r4
    mov r0,a
    movx a,@r0
    mov r5,a                    ; frame
    inc a
    jz shp_n                    ; hidden: whatever it had
    mov a,#SHAPED
    add a,r4
    mov r1,a
    movx a,@r1
    xrl a,r5
    jz shp_n
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
shp_c:
    mov a,r1
    movp a,@a
    movx @r0,a
    inc r0
    inc r1
    djnz r2,shp_c
    mov a,#P1_MAIN
    outl p1,a
shp_n:
    inc r4
    mov a,r4
    xrl a,#4
    jnz shp_l
    ret

    include "sprites.inc"

    if $ > 0D00h
    error "page 10's page $C has run out"
    endif

    PAGE_TAIL

; p2.inc -- page 2: the arena (src/play.c, src/move.c). Entered by far_go
; (E_ENTER) when the phase is the countdown or play; leaves by far_go to
; page 7 when it isn't, or the link goes.
;
; We walk our own bomber (walk.inc: move.c exactly, which the server checks)
; and tell the server where it is; the server has the rest. Our bomb shows
; the moment we drop it (PEND) until the state has it. The others are drawn
; walking toward where the server last said they were. The game's clock is
; 60 jiffies a second: on PAL every fifth frame runs two.

MOVE_EVERY equ 4                ; jiffies between our moves, at least
PEND_WAIT  equ 90               ; jiffies our bomb may wait for the server

    PAGE_HEAD
    db e_enter&0FFh
e_enter:
    jmp p_enter

; p_enter: the arena's overlay, the bombers and our walker from the state,
; everything drawn; the countdown's tune.
p_enter:
    FARC PG_OV,O_ARENA
    mov a,#S_ARENA
    xset SCREEN
    call play_enter
    call hud_check
    FARC PG_WALK,W_SEATS
    mov a,#OFF_PHASE
    FCALL st_get
    xrl a,#PH_COUNTDOWN
    jnz p_loop
    mov a,#SND_ROUND
    call beep

; ---- the loop ----
p_loop:
    INFRAME
    FARC PG_NET,N_STEP
    xget NEVT
    jb1 p_err
    jb2 p_lost
    xget KEY
    xrl a,#K_CLR
    jz p_leave
    xget NEVT
    jb0 p_state
    jmp p_frame
p_state:
    call find_seat
    mov a,#OFF_PHASE            ; still the arena's?
    FCALL st_get
    xrl a,#PH_COUNTDOWN
    jz ps_seq
    xrl a,#PH_COUNTDOWN!PH_PLAY
    jz ps_seq
    FARC PG_OV,O_TEXT
    GOTO PG_MAIN,E_BACK
ps_seq:
    mov a,#OFF_SEQ              ; a new round: in afresh
    FCALL st_get
    mov r2,a
    xget SHOWNQ
    xrl a,r2
    jz ps_go
    call play_enter
    call hud_check
    jmp p_frame
ps_go:
    call play_state
p_frame:
    call jiffies                ; the jiffies owed (1, PAL: 2 every fifth)
pj_l:
    call jiffy_pre              ; the clocks; are we playing?
    jz pj_n
    FARC PG_WALK,W_WALK         ; the stick: r2 = 1 if we moved (a farcall
    mov a,r2                    ; from the top: its depth)
    xset MOVED
    call jiffy_post             ; bumps, fire, our move
pj_n:
    mov r1,#JCNT
    mov a,@r1
    dec a
    mov @r1,a
    jnz pj_l
    xget MFLAGS                 ; our bomb shown, or a pending one gone
    jb4 pj_pc
    jmp pj_d
pj_pc:
    anl a,#(~MF_PIECES)&0FFh
    movx @r0,a
    FARC PG_ALIB,A_PIECES
pj_d:
    FARC PG_WALK,W_SEATS        ; the bombers
    call scroll
    call blink
    jmp p_loop

p_err:
    FARC PG_OV,O_TEXT
    GOTO PG_MAIN,E_ERR
p_lost:
    FARC PG_OV,O_TEXT
    GOTO PG_MAIN,E_LOST
p_leave:
    FARC PG_OV,O_TEXT
    GOTO PG_MAIN,E_LEAVE



; pe_clear: the knocked-out clocks zeroed, the sprites' shapes unknown.
; Clobbers A, r0, r2.
pe_clear:
    mov r0,#DIET                ; the knocked-out clocks, the shapes unknown
    mov r2,#4
    clr a
pec_d:
    movx @r0,a
    inc r0
    djnz r2,pec_d
    mov r0,#SHAPED
    mov r2,#4
    mov a,#0FEh
pec_h:
    movx @r0,a
    inc r0
    djnz r2,pec_h
    ret

    if $ > 900h
    error "page 2's page $8 has run out"
    endif

; ===========================================================================
;  a new state
; ===========================================================================
    org 900h

; play_state: our position when the server moves it (WARP), our bomb once it
; is there, the map's changed rows and the pieces, a blast's or a dud's
; sound, the bombers knocked out, the HUD. Clobbers A, r0-r7.
play_state:
    xget MYSEAT                 ; a warp: snap to the server's position
    inc a
    jz pst_p
    dec a
    add a,#soff9&0FFh
    movp a,@a
    mov r5,a
    add a,#P_WARP
    FCALL st_get
    mov r2,a
    xget MYWARP
    xrl a,r2
    jz pst_p
    mov a,r2
    movx @r0,a
    mov a,r5
    FCALL st_get
    xset MVX
    mov a,r5
    inc a
    FCALL st_get
    xset MVY
    clr a
    xset MVACC
pst_p:
    xget PEND                   ; our bomb: the server's now (or something)
    inc a
    jz pst_m
    dec a
    add a,#OFF_MAP
    FCALL st_get
    jz pst_m
    mov a,#NONE
    xset PEND
pst_m:
    xget MFLAGS                 ; the map: its changed rows, the pieces
    mov r6,a
    jb1 pst_boom
    jmp pst_f
pst_boom:
    mov a,#SND_BOOM
    call beep
pst_f:
    mov a,r6
    jb2 pst_fz
    jmp pst_r
pst_fz:
    mov a,#SND_FIZZLE
    call beep
pst_r:
    mov a,r6
    anl a,#MF_MAP|MF_MOVERS
    jz pst_d
    call dirty_rows
    FARC PG_ALIB,A_PIECES
    clr a
    xset MFLAGS
pst_d:
    call stat_sum               ; a power up: the pickup's sound
    mov r2,a
    mov r1,#STATSUM
    mov a,@r1
    cpl a
    add a,r2                    ; C: new > old
    mov a,r2
    mov @r1,a
    jnc pst_dd
    mov a,#SND_PICKUP
    call beep
pst_dd:
    mov a,#OFF_ALIVE            ; knocked out: the clock starts, the sound
    FCALL st_get
    mov r2,a
    xget WASA
    mov r3,a
    mov a,r2
    movx @r0,a
    mov a,r2
    cpl a
    anl a,r3                    ; alive before, not now
    jz pst_h
    mov r3,a
    mov r4,#0
pst_k:
    mov a,r3
    rrc a
    mov r3,a
    jnc pst_kn
    mov a,#DIET
    add a,r4
    mov r0,a
    mov a,#1
    movx @r0,a
pst_kn:
    inc r4
    mov a,r4
    xrl a,#SEATS
    jnz pst_k
    mov a,#SND_DIE
    call beep
pst_h:
    jmp hud_check

; dirty_rows: the grid rows MAPDL/H mark redrawn, then unmarked. Clobbers
; A, r0-r7.
dirty_rows:
    mov r4,#0
dr_l:
    mov a,r4
    xset LTMP
    mov a,r4
    anl a,#7
    add a,#bit9&0FFh
    movp a,@a
    mov r2,a
    mov a,r4
    mov r0,#MAPDL
    jb3 dr_h
    jmp dr_t
dr_h:
    mov r0,#MAPDH
dr_t:
    movx a,@r0
    anl a,r2
    jz dr_n
    FARC PG_ALIB,A_ROW
dr_n:
    xget LTMP
    inc a
    mov r4,a
    xrl a,#ARENA_H
    jnz dr_l
    clr a
    xset MAPDL
    xset MAPDH
    ret

soff9:  db OFF_PLAYERS, OFF_PLAYERS+PLAYER_SIZE, OFF_PLAYERS+2*PLAYER_SIZE, OFF_PLAYERS+3*PLAYER_SIZE
bit9:   db 1, 2, 4, 8, 10h, 20h, 40h, 80h


    if $ > 0A00h
    error "page 2's page $9 has run out"
    endif

; ===========================================================================
;  a jiffy of the game
; ===========================================================================
    org 0A00h

; jiffies: JCNT = the jiffies owed: the frames since the last pass (a stall
; of more than 30 -- a swap, a wait -- counts 1), at most 4; PAL: one more
; every fifth frame. Clobbers A, r0-r3.
jiffies:
    mov r1,#LASTF
    mov a,@r1
    cpl a
    inc a
    mov r1,#FRAMES
    add a,@r1                   ; frames since
    mov r2,a
    mov a,@r1
    mov r1,#LASTF
    mov @r1,a
    mov a,r2
    jz jj_1
    add a,#256-31
    jnc jj_n
jj_1:
    mov r2,#1
jj_n:
    mov a,r2
    mov r3,a                    ; jiffies
    mov r1,#MODE
    mov a,@r1
    jb7 jj_pal
    jmp jj_cap
jj_pal:
    mov r1,#OWED                ; PAL: 6 a 5 frames
jj_p:
    mov a,@r1
    inc a
    mov @r1,a
    xrl a,#5
    jnz jj_pn
    mov @r1,a
    inc r3
jj_pn:
    djnz r2,jj_p
jj_cap:
    mov a,r3
    add a,#256-5
    jnc jj_s
    mov r3,#4
jj_s:
    mov r0,#JCNT
    mov a,r3
    mov @r0,a
    ret

; jiffy_pre: a jiffy's clocks; our bomb never come gone. A nonzero if we
; play (my_off: r5 our offset). Clobbers A, r0-r2.
jiffy_pre:
    xinc SENTT                  ; the clocks (they stop at 255)
    jnz jf_b
    dec a
    movx @r0,a
jf_b:
    xinc BUMPT
    jnz jf_p
    dec a
    movx @r0,a
jf_p:
    xget PEND                   ; our bomb never came: gone
    inc a
    jz jf_me
    xget PENDL
    jz jf_me
    dec a
    movx @r0,a
    jnz jf_me
    mov a,#NONE
    xset PEND
    xorb MFLAGS,MF_PIECES
jf_me:
    call my_off
    jnz jf_x
    clr a
    xset MOVED
jf_x:
    ret

; jiffy_post: after the walk: a bump kicks or relights; fire throws or
; drops; our move goes out. Clobbers A, r0-r7.
jiffy_post:
    call bump
    call fire
    jmp send_move

; my_off: A nonzero if we sit, are alive, and the phase is play; r5 = our
; player's offset in ST. Clobbers A, r0, r2.
my_off:
    mov a,#OFF_PHASE
    FCALL st_get
    xrl a,#PH_PLAY
    jnz mo_no
    xget MYSEAT
    inc a
    jz mo_x
    dec a
    mov r2,a
    add a,#sofa&0FFh
    movp a,@a
    mov r5,a
    mov a,#OFF_ALIVE
    FCALL st_get
    mov r0,a
    mov a,r2
    add a,#bita&0FFh
    movp a,@a
    anl a,r0
    ret
mo_no:
    clr a
mo_x:
    ret

; bump: walked into a bomb or a dud (MVBUMP), 30 jiffies since the last, and
; we kick or it's a dud: ACT_BUMP. Clobbers A, r0-r7.
bump:
    xget MVBUMP
    inc a
    jz bm_x
    xget BUMPT
    add a,#256-30
    jnc bm_x
    call my_off
    mov a,r5
    add a,#P_FLAGS
    FCALL st_get
    anl a,#PF_KICK
    mov r6,a
    jnz bm_go
    xget MVBUMP
    add a,#OFF_MAP
    FCALL st_get
    xrl a,#T_DUD
    jnz bm_x
bm_go:
    clr a
    xset BUMPT
    xget MVBUMP
    mov r4,a
    xget MVDIR
    mov r5,a
    mov r3,#ACT_BUMP
    FARC PG_NET,N_ACT
    mov a,r6
    jz bm_x
    mov a,#SND_KICK
    call beep
bm_x:
    ret

sofa:   db OFF_PLAYERS, OFF_PLAYERS+PLAYER_SIZE, OFF_PLAYERS+2*PLAYER_SIZE, OFF_PLAYERS+3*PLAYER_SIZE
bita:   db 1, 2, 4, 8

    if $ > 0B00h
    error "page 2's page $A has run out"
    endif


; ===========================================================================
;  fire, our move
; ===========================================================================
    org 0B00h
; fire: on our bomb (or the one pending) with the glove: THROW; else on a
; floor tile, with a bomb to spare: DROP (shown at once, pending).
; Clobbers A, r0-r7.
fire:
    xget JOYEV
    jb4 fi_go
    ret
fi_go:
    xget MVX
    mov r2,a
    xget MVY
    mov r3,a
    FARC PG_WALK,W_CENTRE        ; r2 = the tile under us
    mov a,r2
    xset FTILE
    call my_off
    mov a,r5
    add a,#P_FLAGS
    FCALL st_get
    anl a,#PF_GLOVE
    jz fi_drop
    xget FTILE                  ; the glove: on our bomb, or the pending one
    add a,#OFF_MAP
    FCALL st_get
    xrl a,#T_BOMB
    jz fi_throw
    xget PEND
    mov r2,a
    xget FTILE
    xrl a,r2
    jnz fi_drop
fi_throw:
    xget MVDIR
    mov r4,a
    mov r3,#ACT_THROW
    mov r5,#0
    FARC PG_NET,N_ACT
    mov a,#NONE
    xset PEND
    mov a,#SND_THROW
    jmp beep
fi_drop:
    xget FTILE                  ; on floor, not our pending bomb
    add a,#OFF_MAP
    FCALL st_get
    jnz fi_x
    xget PEND
    mov r2,a
    xget FTILE
    xrl a,r2
    jz fi_x
    mov a,r5                    ; out + pending < bombs
    add a,#P_OUT
    FCALL st_get
    mov r3,a
    xget PEND
    inc a
    jz fi_c
    inc r3
fi_c:
    mov a,r5
    add a,#P_BOMBS
    FCALL st_get
    dec a
    cpl a
    add a,r3                    ; C: out + pending > bombs - 1
    jc fi_x
    xget FTILE
    mov r4,a
    xset PEND
    mov a,#PEND_WAIT
    xset PENDL
    mov r3,#ACT_DROP
    mov r5,#0
    FARC PG_NET,N_ACT
    xorb MFLAGS,MF_PIECES       ; shown at once (the loop draws it)
    mov a,#SND_DROP
    jmp beep
fi_x:
    ret

; send_move: where we are, if it changed and MOVE_EVERY jiffies have gone,
; or every 60 regardless: SENTX/Y/D, TX_MOVE (p5 encodes it).
send_move:
    xget MVDIR
    mov r3,a
    xget MOVED
    jz sm_d
    mov a,r3
    orl a,#P_DIR_MOVING
    mov r3,a
sm_d:
    xget SENTT
    add a,#256-60
    jc sm_go
    xget SENTT
    add a,#256-MOVE_EVERY
    jnc sm_x
    xget SENTX
    mov r2,a
    xget MVX
    xrl a,r2
    jnz sm_go
    xget SENTY
    mov r2,a
    xget MVY
    xrl a,r2
    jnz sm_go
    xget SENTD
    xrl a,r3
    jz sm_x
sm_go:
    xget MVX
    xset SENTX
    xget MVY
    xset SENTY
    mov a,r3
    xset SENTD
    clr a
    xset SENTT
    xorb TXF,TX_MOVE
sm_x:
    ret


    if $ > 0C00h
    error "page 2's page $B has run out"
    endif

; ===========================================================================
;  entering, the HUD
; ===========================================================================
    org 0C00h
; play_enter: the bombers where the server has them, our walker reset, no
; bomb pending, the view on us, everything drawn. Clobbers A, r0-r7.
play_enter:
    mov a,#OFF_SEQ
    FCALL st_get
    xset SHOWNQ
    mov a,#NONE
    xset PEND
    clr a
    xset BLINK
    xset MAPDL
    xset MAPDH
    xset MFLAGS
    xset MOVED
    call pe_clear
    mov r4,#0                   ; the bombers at the server's places
pe_s:
    mov a,r4
    add a,#soff2&0FFh
    movp a,@a
    mov r5,a
    FCALL st_get
    mov r1,a
    mov a,#DISPX
    add a,r4
    mov r0,a
    mov a,r1
    movx @r0,a
    mov a,r5
    inc a
    FCALL st_get
    mov r1,a
    mov a,#DISPY
    add a,r4
    mov r0,a
    mov a,r1
    movx @r0,a
    inc r4
    mov a,r4
    xrl a,#SEATS
    jnz pe_s
    xget MYSEAT                 ; ours: the walker from the state
    inc a
    jz pe_v
    dec a
    add a,#soff2&0FFh
    movp a,@a
    mov r5,a
    add a,#P_WARP
    FCALL st_get
    xset MYWARP
    mov a,r5
    FCALL st_get
    xset MVX
    mov a,r5
    inc a
    FCALL st_get
    xset MVY
    mov a,#DIR_DOWN
    xset MVDIR
    clr a
    xset MVACC
    xset SENTT
pe_v:
    call stat_sum
    mov r1,#STATSUM
    mov @r1,a
    call view_on
    mov a,#0FFh
    xset HUDT
    xset HUDA
    mov a,#OFF_ALIVE
    FCALL st_get
    xset WASA
    FARC PG_ALIB,A_MID
    FARC PG_ALIB,A_ROWS
    FARC PG_ALIB,A_PIECES
    ret

soff2:  db OFF_PLAYERS, OFF_PLAYERS+PLAYER_SIZE, OFF_PLAYERS+2*PLAYER_SIZE, OFF_PLAYERS+3*PLAYER_SIZE


; hud_check: the HUD redrawn if the clock or who's alive changed; the
; countdown's beeps, and GO when the clock jumps to the round's. Clobbers
; A, r0-r7.
hud_check:
    mov a,#OFF_TIMER
    FCALL st_get
    mov r2,a
    xget HUDT
    xrl a,r2
    jnz hc_t
    mov a,#OFF_ALIVE
    FCALL st_get
    mov r2,a
    xget HUDA
    xrl a,r2
    jz hc_x
    mov a,r2
    movx @r0,a
    jmp hc_d
hc_t:
    xget HUDT                   ; (0FFh: just entered, no sound)
    inc a
    jz hc_t2
    mov a,#OFF_PHASE
    FCALL st_get
    xrl a,#PH_COUNTDOWN
    jnz hc_go
    mov a,#SND_BEEP             ; the countdown: a beep a second
    call beep
    jmp hc_t2
hc_go:
    xget HUDT                   ; play: from a countdown's few, GO
    add a,#256-6
    jc hc_t2
    mov a,#SND_GO
    call beep
hc_t2:
    mov a,r2
    xset HUDT
hc_d:
    FARC PG_ALIB,A_HUD
hc_x:
    ret


    if $ > 0D00h
    error "page 2's page $C has run out"
    endif

; ===========================================================================
    org 0D00h
; stat_sum: A = our flags + range + bombs + speed (0 watching). Clobbers
; r0, r1, r3.
stat_sum:
    xget MYSEAT
    inc a
    jz ss_x
    dec a
    add a,#sofe&0FFh
    movp a,@a
    add a,#P_FLAGS
    mov r1,a
    mov r3,#0
    mov r0,#4                   ; flags, range, bombs, speed: 3-6
ss_l:
    mov a,r1
    FCALL st_get
    add a,r3
    mov r3,a
    inc r1
    djnz r0,ss_l
    mov a,r3
ss_x:
    ret
sofe:   db OFF_PLAYERS, OFF_PLAYERS+PLAYER_SIZE, OFF_PLAYERS+2*PLAYER_SIZE, OFF_PLAYERS+3*PLAYER_SIZE


    if $ > 0E00h
    error "page 2's page $D has run out"
    endif

; ===========================================================================
;  the view, the blink, the frame
; ===========================================================================
    org 0E00h

; view_on: VOFF so our column (watching: 1) sits 3-6 columns in.
; Clobbers A, r0, r2.
view_on:
    xget MYSEAT
    inc a
    jnz vo_me
    mov a,#1
    jmp vo_set
vo_me:
    xget MVX
    add a,#UNIT/2
    swap a
    anl a,#0Fh
    add a,#256-4                ; column - 4, in 0-3
    jc vo_c
    clr a
vo_c:
    mov r2,a
    add a,#256-OFF_MAX-1
    jnc vo_k
    mov r2,#OFF_MAX
vo_k:
    mov a,r2
vo_set:
    xset VOFF
    ret

; scroll: the view follows us: our column kept 3-6 view columns in (a column
; at a time); watching, the stick's left and right move it. On a change the
; grid and the pieces are redrawn. Clobbers A, r0-r7.
scroll:
    xget VOFF
    mov r3,a
    xget MYSEAT
    inc a
    jnz sc_me
    xget JOYEV                  ; watching
    jb2 sc_left
    jb3 sc_right2
    ret
sc_me:
    xget MVX
    add a,#UNIT/2
    swap a
    anl a,#0Fh
    mov r2,a                    ; our column
    mov a,r3
    add a,#2                    ; column >= VOFF + 3: not left
    cpl a
    add a,r2
    jc sc_right
sc_left:
    mov a,r3
    jz sc_x
    dec r3
    jmp sc_set
sc_right:
    mov a,r3
    add a,#6                    ; column > VOFF + 6: right
    cpl a
    add a,r2
    jnc sc_x
sc_right2:
    mov a,r3
    xrl a,#OFF_MAX
    jz sc_x
    inc r3
sc_set:
    mov a,r3
    xset VOFF
    FARC PG_ALIB,A_MID
    FARC PG_ALIB,A_ROWS
    FARC PG_ALIB,A_PIECES
sc_x:
    ret

; blink: every 8 frames the flames and burning blocks on or off: only the
; rows that have them (LITL/H) redrawn.
blink:
    mov r1,#FRAMES
    mov a,@r1
    anl a,#7
    jnz bk_x
    xget LITL
    mov r2,a
    xget LITH
    orl a,r2
    jz bk_x
    xget BLINK
    cpl a
    movx @r0,a
    xget LITL
    xset MAPDL
    xget LITH
    xset MAPDH
    jmp dirty_rows
bk_x:
    ret

; find_seat: MYSEAT = the seated seat named MYNAME (p7's). Clobbers A,
; r0-r5.
find_seat:
    mov r4,#0
fs_s:
    mov a,#OFF_SEATED
    FCALL st_get
    mov r5,a
    mov a,r4
    add a,#bite&0FFh
    movp a,@a
    anl a,r5
    jz fs_n
    mov a,r4
    rl a
    rl a
    rl a
    add a,#OFF_NAMES
    mov r3,a
    mov r1,#MYNAME
    mov r2,#NAME_LEN
fs_c:
    mov a,r3
    FCALL st_get
    mov r5,a
    movx a,@r1
    xrl a,r5
    jnz fs_n
    inc r3
    inc r1
    djnz r2,fs_c
    mov a,r4
    jmp fs_x
fs_n:
    inc r4
    mov a,r4
    xrl a,#SEATS
    jnz fs_s
    mov a,#NONE
fs_x:
    xset MYSEAT
    ret
bite:   db 1, 2, 4, 8


    include "beep.inc"

    if $ > 0F00h
    error "page 2's page $E has run out"
    endif

    PAGE_TAIL

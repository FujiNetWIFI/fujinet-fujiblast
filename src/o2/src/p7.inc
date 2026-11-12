; p7.inc -- page 7: at a table (src/main.c's play_table, src/screens.c's
; waiting room and trophies). Entered one way (far_go):
;   E_JOIN   sit down at SRVTBL: the room key saved, the socket, HELLO
;   E_BACK   the arena (p2) is over for now: the text overlay is in
;   E_LOST   the link went: say so, then join again
;   E_ERR    the server said no
;   E_LEAVE  the player left (CLEAR)
; Every frame: the input, the link's step, then the screen the state's phase
; wants -- the waiting room, a round's trophies, the game's -- or the arena,
; which p2 runs (it comes back by E_BACK). A screen is drawn on entry and
; again on each new state; a frame only moves its bombers.

JOIN_WAIT equ 600               ; frames for the first state before giving up

    PAGE_HEAD
    db e_join&0FFh
    db e_back&0FFh
    db e_lost&0FFh
    db e_err&0FFh
    db e_leave&0FFh
e_join:
    jmp m_join
e_back:
    jmp m_back
e_lost:
    jmp m_lost
e_err:
    jmp m_err
e_leave:
    jmp m_leave

; m_join: sit down: the room key saved, "JOINING", the socket, HELLO.
m_join:
    FARC PG_NCTL,K_SAVEROOM
    SCRNEW
    mov a,#s_joining&0FFh
    call t0_str
    call lb_clr
    call lb_table
    mov r4,#3
    call row_out
    FARC PG_NCTL,C_OPEN
    mov a,r2
    jnz mj_ok
    mov a,#s_noansw&0FFh        ; no socket: back to the rooms
    call msg_fire
    jmp to_menu
mj_ok:
    FARC PG_NET,N_HELLO
    clr a
    xset SCREEN
    xset JOINT
    xset JOINTH
    jmp m_loop

; m_back: the arena is over for now (p2 put the text overlay back): the
; screen the phase wants is drawn afresh.
m_back:
    clr a
    xset SCREEN
    jmp m_loop

m_lost:
    SCRNEW
    mov a,#s_lost&0FFh
    call t0_str
    mov a,#s_again&0FFh
    call row3_str
    mov a,#120                  ; (a farcall spends every register)
    xset RCOUNT
ml_w:
    INFRAME
    mov r0,#RCOUNT
    movx a,@r0
    dec a
    movx @r0,a
    jnz ml_w
    jmp m_join

m_err:
    FARC PG_NCTL,C_CLOSE
    SCRNEW
    mov a,#s_saidno&0FFh
    call t0_str
    xget NERR
    xrl a,#ERR_VERSION
    jz me_v
    mov a,#s_noroom&0FFh
    call row3_str
    jmp me_s
me_v:
    mov a,#s_update&0FFh
    call row3_str
me_s:
    call wait_fire
    FARC PG_NCTL,K_CLEARROOM
    jmp to_menu

; m_leave: give the seat up, BYE (a few frames to get it out), close.
m_leave:
    FARC PG_NET,N_LEAVE
    xorb TXF,TX_BYE
    mov a,#30
    xset RCOUNT
mlv_l:
    INFRAME
    FARC PG_NET,N_STEP
    xget TXF
    jnz mlv_n
    xget NKIND
    jz mlv_x
mlv_n:
    mov r0,#RCOUNT
    movx a,@r0
    dec a
    movx @r0,a
    jnz mlv_l
mlv_x:
    FARC PG_NCTL,C_CLOSE
    FARC PG_NCTL,K_CLEARROOM
to_menu:
    GOTO PG_MENU,E_ROOMS



    if $ > 900h
    error "page 7's page $8 has run out"
    endif

; ===========================================================================
;  the waiting room
; ===========================================================================
    org 900h

; wait_draw: T0 "ROOM <table>"; rows 0-3 the seats (a bomber beside each
; seated one, in its colour; "H" the host's); row 4 the rounds; row 5 what
; happens next; row 6 the keys. Clobbers A, r0-r7.
wait_draw:
    SCRNEW
    call lb_clr
    mov a,#s_room&0FFh
    call ls_9
    call lb_table
    mov a,#C_WHITE
    xset TXCOL
    FARC PG_TLIB,L_T0
    mov r4,#0                   ; the seats
wd_s:
    mov a,r4
    xset TMP0
    call lb_clr
    call seat_on
    jz wd_e
    mov a,#' '                  ; the name a column in from the bomber
    call lb_ch
    xget TMP0
    mov r2,a
    call lb_name
    mov a,#OFF_HOST
    FCALL st_get
    mov r2,a
    xget TMP0
    xrl a,r2
    jnz wd_c
    mov r1,#LINEBUF+11
    mov a,#'H'
    movx @r1,a
wd_c:
    xget TMP0
    mov r4,a
    call seat_col
    call spr_row                ; its bomber, facing us
    jmp wd_o
wd_e:
    mov a,#C_DGREY
    xset TXCOL
    mov a,#s_empty&0FFh
    call ls_9
    xget TMP0
    mov r4,a
    call spr_off
wd_o:
    xget TMP0
    mov r4,a
    call row_out
    xget TMP0
    inc a
    mov r4,a
    xrl a,#SEATS
    jnz wd_s
    call lb_clr                 ; row 4: "ROUNDS n"
    mov a,#s_rounds&0FFh
    call ls_9
    mov a,#OFF_ROUNDS
    FCALL st_get
    call lb_dig
    mov a,#C_TAN
    xset TXCOL
    mov r4,#4
    call row_out
    call lb_clr                 ; row 5: what happens next
    call wait_status
    mov a,#C_CYAN
    xset TXCOL
    mov r4,#5
    call row_out
    jmp wd_keys

; ls_9: the NUL-ended string at offset A of page $9 onto LINEBUF at r1.
; Clobbers A, r0.
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

s_room:   db "ROOM ",0
s_empty:  db " --------",0
s_rounds: db " ROUNDS ",0
s_watch:  db "  WATCHING",0
s_whost:  db " WAIT HOST",0
s_wplay:  db "WAIT PLAYERS",0
s_fire:   db " PRESS FIRE",0
s_keys:   db "CLEAR: LEAVE",0
s_hkeys:  db "L/R: ROUNDS",0

    if $ > 0A00h
    error "page 7's page $9 has run out"
    endif

; ===========================================================================
;  trophies
; ===========================================================================
    org 0A00h

; results_enter: T0 the winner ("ANN WINS"; "DRAW", "NO WINNER") in its
; colour; row 0 the round (or "SUDDEN DEATH", "GAME OVER"); rows 1-4 the
; seats, each with its trophies; row 6 the timer (results_frame).
results_enter:
    SCRNEW
    mov a,#0FFh
    xset HUDT                   ; (the timer: drawn by results_frame)
    call lb_clr
    mov a,#OFF_RESULT
    FCALL st_get
    mov r2,a
    add a,#256-SEATS
    jc re_none
    mov a,r2
    mov r4,a
    call seat_col
    call lb_name
    mov a,#s_wins&0FFh
    call ls_a
    jmp re_t0
re_none:
    xget SCREEN
    xrl a,#S_GAME
    jnz re_dr
    mov a,#s_nowin&0FFh
    jmp re_d
re_dr:
    mov a,#s_draw&0FFh
re_d:
    call ls_a
    mov a,#C_WHITE
    xset TXCOL
re_t0:
    FARC PG_TLIB,L_T0
    call lb_clr                 ; row 0: the round
    xget SCREEN
    xrl a,#S_GAME
    jnz re_r
    mov a,#s_over&0FFh
    call ls_a
    jmp re_ro
re_r:
    mov a,#OFF_SUDDEN
    FCALL st_get
    jz re_rn
    mov a,#s_sudden&0FFh
    call ls_a
    jmp re_ro
re_rn:
    mov a,#s_round&0FFh
    call ls_a
    mov a,#OFF_ROUND
    FCALL st_get
    call lb_dig
re_ro:
    mov a,#C_TAN
    xset TXCOL
    mov r4,#0
    call row_out
    mov r4,#0                   ; rows 1-4: each seat's name, its trophies
re_s:
    mov a,r4
    xset TMP0
    call lb_clr
    call seat_on
    jz re_e
    mov a,#' '
    call lb_ch
    xget TMP0
    mov r2,a
    call lb_name
    mov r1,#LINEBUF+10
    xget TMP0
    add a,#soffa&0FFh
    movp a,@a
    add a,#P_WINS
    FCALL st_get
    call lb_dig
    xget TMP0
    mov r4,a
    call seat_col
    inc r4                      ; (row s + 1)
    call spr_row
    jmp re_o
re_e:
    xget TMP0
    mov r4,a
    call spr_off
re_o:
    xget TMP0
    inc a
    mov r4,a
    call row_out
    xget TMP0
    inc a
    mov r4,a
    xrl a,#SEATS
    jnz re_s
    jmp res_tune

; ls_a: the NUL-ended string at offset A of page $A onto LINEBUF at r1.
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

soffa:  db OFF_PLAYERS, OFF_PLAYERS+PLAYER_SIZE, OFF_PLAYERS+2*PLAYER_SIZE, OFF_PLAYERS+3*PLAYER_SIZE
s_wins:   db " WINS",0
s_draw:   db "DRAW",0
s_nowin:  db "NO WINNER",0
s_over:   db "GAME OVER",0
s_sudden: db "SUDDEN DEATH",0
s_round:  db " ROUND ",0

    if $ > 0B00h
    error "page 7's page $A has run out"
    endif

; ===========================================================================
;  lines, seats, bombers, messages
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

; lb_dig: A's units digit (A < 100: the tens too, if any) onto LINEBUF.
lb_dig:
lb_num:
    mov r2,#0
ln_t:
    add a,#256-10
    jnc ln_u
    inc r2
    jmp ln_t
ln_u:
    add a,#10+'0'
    mov r3,a
    mov a,r2
    jz ln_1
    add a,#'0'
    call lb_ch
ln_1:
    mov a,r3
    jmp lb_ch

; lb_name: seat r2's name (up to 8, NUL ends it) onto LINEBUF. Clobbers A,
; r0, r2, r3.
lb_name:
    mov a,r2
    rl a
    rl a
    rl a
    add a,#OFF_NAMES
    mov r2,a
    mov r3,#NAME_LEN
lnm_l:
    mov a,r2
    FCALL st_get
    jz lnm_x
    call lb_ch
    inc r2
    djnz r3,lnm_l
lnm_x:
    ret

; lb_table: SRVTBL upper-cased onto LINEBUF. Clobbers A, r0, r2.
lb_table:
    mov r0,#SRVTBL
    mov r2,#TABLE_LEN
lt_l:
    movx a,@r0
    jz lt_x
    add a,#256-'a'
    jnc lt_d
    add a,#'A'
    jmp lt_p
lt_d:
    add a,#'a'
lt_p:
    call lb_ch
    inc r0
    djnz r2,lt_l
lt_x:
    ret

; seat_on: A nonzero if seat TMP0 is seated. Clobbers A, r0, r2.
seat_on:
    mov a,#OFF_SEATED
    FCALL st_get
    mov r2,a
    xget TMP0
    add a,#sbitb&0FFh
    movp a,@a
    anl a,r2
    ret

; seat_col: TXCOL = seat r4's colour. Clobbers A, r0.
seat_col:
    mov a,r4
    add a,#scolb&0FFh
    movp a,@a
    xset TXCOL
    ret

; row_out: LINEBUF onto text row r4 (p4's L_ROW). Clobbers A, r0-r7.
row_out:
    FARC PG_TLIB,L_ROW
    ret

; spr_row: sprite r4 (a seat) beside text row r4... the bomber of seat
; TMP0 at the left of row r4, facing us. spr_off: sprite TMP0 hidden.
; spr_frame: sprite r4's frame r5 (its shape). Clobbers A, r0-r7.
spr_row:
    mov a,r4
    add a,#syrow&0FFh
    movp a,@a
    mov r3,a                    ; Y
    xget TMP0
    mov r4,a
    mov a,r3
    mov r5,#X_TROW-12           ; X
    call spr_put
    mov r5,#FR_FWD0
    jmp spr_frame
spr_off:
    xget TMP0
    mov r4,a
    mov a,#SPR_OFF
    mov r5,#0
spr_put:                        ; sprite r4 at Y A, X r5, its seat's colour
    mov r3,a
    mov a,r4
    rl a
    add a,r4
    add a,#SPRTAB&0FFh
    mov r2,#SPRTAB>>8
    FCALL poke_at
    mov a,r3
    FCALL poke_b
    mov a,r5
    FCALL poke_b
    mov a,r4
    add a,#ssprb&0FFh
    movp a,@a
    FCALL poke_b
    ret
spr_frame:                      ; sprite r4's shape: frame r5 (p1 draws it)
    mov a,#SPF
    add a,r4
    mov r0,a
    mov a,r5
    movx @r0,a
    FARC PG_ALIB,A_SHAPES
    ret

; the text rows' Y, less 2 (the figure's top row is blank): a bomber stands
; in its row's 14 lines
syrow:
K   set 0
    rept NTROWS
    db Y_TROW0+K*TPITCH-2
K   set K+1
    endm
sbitb:  db 1, 2, 4, 8
scolb:  db C_WHITE, C_RED, C_CYAN, C_TAN
ssprb:  db 7*S_COL, 1*S_COL, 6*S_COL, 3*S_COL

    include "beep.inc"

    if $ > 0C00h
    error "page 7's page $B has run out"
    endif

; ===========================================================================
    org 0C00h


; t0_str: T0 = the string at offset A of page $C (white). row3_str: the
; same onto row 3.
t0_str:
    mov r2,a
    call lb_clr
    mov a,r2
    call ls_c
    mov a,#C_WHITE
    xset TXCOL
    FARC PG_TLIB,L_T0
    ret
row3_str:
    mov r2,a
    call lb_clr
    mov a,r2
    call ls_c
    mov a,#C_WHITE
    xset TXCOL
    mov r4,#3
    jmp row_out

; msg_fire: T0 the string at offset A of page $C, "PRESS FIRE" on row 5,
; until fire (or ENTER).
msg_fire:
    mov r2,a
    SCRNEW
    mov a,r2
    call t0_str
    call lb_clr
    mov a,#s_pfire&0FFh
    call ls_c
    mov r4,#5
    call row_out
wait_fire:
    INFRAME
    xget JOYEV
    jb4 wfr_x
    xget KEY
    xrl a,#K_ENT
    jnz wait_fire
wfr_x:
    ret


ls_c:
    mov r0,a
lsc_l:
    mov a,r0
    movp a,@a
    jz lsc_x
    movx @r1,a
    inc r1
    inc r0
    jmp lsc_l
lsc_x:
    ret

s_joining: db "    JOINING",0
s_noansw:  db "   NO ANSWER",0
s_lost:    db "CONNECTION LOST",0
s_again:   db "TRYING AGAIN",0
s_saidno:  db "THE SERVER SAID",0
s_noroom:  db " NO SUCH ROOM",0
s_update:  db "UPDATE FUJIBLAST",0
s_pfire:   db " PRESS FIRE",0

; wd_keys: the waiting room's row 6: the keys (the host's own).
wd_keys:
    call lb_clr
    xget MYSEAT
    mov r2,a
    mov a,#OFF_HOST
    FCALL st_get
    xrl a,r2
    jnz wdk_p
    mov a,#s_hkeys&0FFh
    call ls_9
    jmp wdk_o
wdk_p:
    mov a,#s_keys&0FFh
    call ls_9
wdk_o:
    mov a,#C_DGREY
    xset TXCOL
    mov r4,#6
    jmp row_out


    if $ > 0D00h
    error "page 7's page $C has run out"
    endif

; ===========================================================================
;  the loop
; ===========================================================================
    org 0D00h
; ---- the loop ----
m_loop:
    INFRAME
    FARC PG_NET,N_STEP
    xget NEVT
    jb1 ml_err
    jb2 ml_lost
    xget KEY
    xrl a,#K_CLR
    jz ml_leave
    xget NLINK                  ; nothing heard yet: wait (not for ever)
    anl a,#NL_HEARD
    jnz mlp_h
    xinc JOINT
    jnz mlp_t
    xinc JOINTH
mlp_t:
    xget JOINT
    add a,#(10000h-JOIN_WAIT-1)&0FFh
    xget JOINTH
    addc a,#(10000h-JOIN_WAIT-1)>>8
    jnc m_loop
    FARC PG_NCTL,C_CLOSE
    mov a,#s_noansw&0FFh
    call msg_fire
    jmp to_menu
mlp_h:
    xget NEVT                   ; a state: which seat is ours
    jb0 mlp_s
    jmp mlp_w
mlp_s:
    call find_seat
mlp_w:
    call want_screen            ; r2: the screen the phase wants
    mov a,r2
    xrl a,#S_ARENA
    jnz mlp_t2
    GOTO PG_PLAY,E_ENTER        ; the arena: p2's (it swaps the overlay)
mlp_t2:
    xget SCREEN
    xrl a,r2
    jz mlp_same
    mov a,r2
    xset SCREEN
    xrl a,#S_WAIT
    jz mlp_wait
    call results_enter
    jmp m_loop
mlp_wait:
    call wait_draw
    jmp m_loop
mlp_same:
    xget SCREEN
    xrl a,#S_WAIT
    jnz mlp_res
    xget NEVT                   ; the waiting room: redrawn on a state,
    jb0 mlp_wd                  ; the host's keys every frame
    jmp mlp_wf
mlp_wd:
    call wait_draw
mlp_wf:
    call wait_frame
    jmp m_loop
mlp_res:
    call results_frame
    jmp m_loop

ml_err:
    jmp m_err
ml_lost:
    jmp m_lost
ml_leave:
    jmp m_leave

; want_screen: r2 = S_* for the state's phase. Clobbers A, r0.
want_screen:
    mov a,#OFF_PHASE
    FCALL st_get
    mov r2,#S_ARENA
    xrl a,#PH_COUNTDOWN
    jz ws_x
    xrl a,#PH_COUNTDOWN!PH_PLAY
    jz ws_x
    mov r2,#S_ROUND
    xrl a,#PH_PLAY!PH_ROUNDEND
    jz ws_x
    mov r2,#S_GAME
    xrl a,#PH_ROUNDEND!PH_GAMEOVER
    jz ws_x
    mov r2,#S_WAIT
ws_x:
    ret

; find_seat: MYSEAT = the seated seat named MYNAME (8 bytes, NUL padded),
; else NONE. Clobbers A, r0-r5.
find_seat:
    mov r4,#0
fs_s:
    mov a,#OFF_SEATED
    FCALL st_get
    mov r5,a
    mov a,r4
    add a,#sbit7&0FFh
    movp a,@a
    anl a,r5
    jz fs_n
    mov a,r4                    ; its name: ST + OFF_NAMES + 8s
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
    mov a,r4                    ; all 8 alike
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

sbit7:  db 1, 2, 4, 8

    if $ > 0E00h
    error "page 7's page $D has run out"
    endif

; ===========================================================================
;  the waiting room's and the trophies' frames
; ===========================================================================
    org 0E00h
; wait_status: the status line onto LINEBUF (page $9's strings).
wait_status:
    xget MYSEAT
    inc a
    jnz wst_s
    mov a,#s_watch&0FFh
    jmp ls_9
wst_s:
    dec a
    mov r2,a
    mov a,#OFF_HOST
    FCALL st_get
    xrl a,r2
    jz wst_h
    mov a,#s_whost&0FFh
    jmp ls_9
wst_h:
    mov a,#OFF_SEATED           ; 2 or more seated
    FCALL st_get
    mov r2,#0
    mov r3,#4
wst_c:
    rrc a
    jnc wst_n
    inc r2
wst_n:
    djnz r3,wst_c
    mov a,r2
    add a,#256-2
    jnc wst_w
    mov a,#s_fire&0FFh
    jmp ls_9
wst_w:
    mov a,#s_wplay&0FFh
    jmp ls_9

; wait_frame: the host's keys: left/right the rounds (1-9), fire starts.
; Clobbers A, r0-r7.
wait_frame:
    xget MYSEAT
    mov r2,a
    mov a,#OFF_HOST
    FCALL st_get
    xrl a,r2
    jnz wf_x
    mov a,#OFF_ROUNDS
    FCALL st_get
    mov r5,a
    xget JOYEV
    mov r6,a
    jb2 wf_l
    jb3 wf_r
    jb4 wf_f
wf_x:
    ret
wf_l:
    mov a,r5
    add a,#256-2
    jnc wf_x                    ; (1: no fewer)
    dec r5
    jmp wf_set
wf_r:
    mov a,r5
    add a,#256-MAX_ROUNDS
    jc wf_x
    inc r5
wf_set:
    mov a,r5
    mov r4,a
    mov r3,#ACT_ROUNDS
    mov r5,#0
    FARC PG_NET,N_ACT
    ret
wf_f:
    mov r3,#ACT_START
    mov r4,#0
    mov r5,#0
    FARC PG_NET,N_ACT
    ret

; results_frame: the winner's bomber cheers; row 6 the time to what's next.
results_frame:
    mov a,#OFF_RESULT
    FCALL st_get
    mov r4,a
    add a,#256-SEATS
    jc rf_t
    mov r1,#FRAMES              ; cheer: forward and mirrored, 16 frames each
    mov a,@r1
    anl a,#10h
    jnz rf_f1
    mov a,#FR_FWD0
    jmp rf_f
rf_f1:
    mov a,#FR_FWD1
rf_f:
    mov r5,a
    call spr_frame
rf_t:
    mov a,#OFF_TIMER
    FCALL st_get
    mov r2,a
    xget HUDT
    xrl a,r2
    jz rf_x
    mov a,r2
    movx @r0,a
    call lb_clr
    xget SCREEN
    xrl a,#S_GAME
    jnz rf_sn
    mov a,#s_back&0FFh
    jmp rf_s
rf_sn:
    mov a,#s_next&0FFh
rf_s:
    call ls_e
    xget HUDT
    call lb_num
    mov a,#C_WHITE
    xset TXCOL
    mov r4,#6
    jmp row_out
rf_x:
    ret

ls_e:
    mov r0,a
lse_l:
    mov a,r0
    movp a,@a
    jz lse_x
    movx @r1,a
    inc r1
    inc r0
    jmp lse_l
lse_x:
    ret
s_next:   db "  NEXT ",0
s_back:   db "  ROOM ",0

; res_tune: the game's tune, or the round winner's.
res_tune:
    xget SCREEN
    xrl a,#S_GAME
    jnz re_w
    mov a,#SND_GAME
    jmp beep
re_w:
    mov a,#OFF_RESULT
    FCALL st_get
    add a,#256-SEATS
    jc re_x
    mov a,#SND_WIN
    jmp beep
re_x:
    ret



    if $ > 0F00h
    error "page 7's page $E has run out"
    endif

    PAGE_TAIL

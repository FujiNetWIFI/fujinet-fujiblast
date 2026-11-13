; p_room.inc -- the screens at a table besides the arena (src/screens.c):
; the waiting room, the trophies after a round and a game. Each seat is its
; own bomber (sprite slot s, its standing frame), so you know who is who
; in the arena.

WR_Y0   EQU     14              ; the first seat's line
WR_DY   EQU     17

; ---------------------------------------------------------------------------
; wait_enter / wait_state / wait_tick -- the waiting room: the room, the
;   seats (bomber, name, HOST), the rounds; the host's 4 and 6 set them, 5
;   starts.
; ---------------------------------------------------------------------------
        RFIT    34
wait_enter:
        OPN     TOP_CLS
        SETV    SSYT, 0
        FCALL   seats_show              ; the bombers: their standing frames
        SETV    KEV, NONE
        SETV    KNEW, 0
        FJMP    wait_state

        RFIT    255
wait_state:
        HOLD1
        PARS                            ; cleared (the bombers are sprites)
        P       0
        P       0
        P       64
        P       120
        P       TM_CLEAR|TP_SOLID
        GOP     TOP_RECT
        SETV    TXM, TM_SET
        LDR     R9, FBUF                ; "ROOM " id
        LDR     R8, s_room
        FCALL   strcat
        LDR     R8, SRVTBL
        FCALL   strcat
        LDR     R8, FBUF
        LDI     0
        PHI     R9
        FCALL   txt_ctr
        SETV    GS, 0
ws1:    GETV    GS                      ; its line
        PLO     R9
        ADI     LO(WRY)
        PLO     R8
        LDI     HI(WRY)
        PHI     R8
        LDN     R8
        PHI     R9
        GLO     R9
        ADI     LO(BITS)
        PLO     R8
        LDI     HI(BITS)
        PHI     R8
        LDN     R8
        STR     R2
        GETS    OFF_SEATED
        AND
        BNZ     ws2
        LDR     R8, s_dashes            ; empty
        BR      ws3
ws2:    GETV    GS                      ; its name
        SHL
        SHL
        SHL
        ADI     LO(ST+OFF_NAMES)
        PLO     R8
        LDI     HI(ST)
        PHI     R8
ws3:    LDI     8
        PLO     R9
        LDI     NAME_LEN
        PLO     RC
        FCALL   txt_at
        GETS    OFF_HOST                ; HOST
        STR     R2
        GETV    GS
        XOR
        BNZ     ws4
        GETV    GS
        ADI     LO(WRY)
        PLO     R8
        LDI     HI(WRY)
        PHI     R8
        LDN     R8
        PHI     R9
        LDI     44
        PLO     R9
        LDR     R8, s_host
        FCALL   txt_at16
ws4:    INCV    GS
        XRI     SEATS
        BNZ     ws1
        LDR     R8, s_rounds            ; ROUNDS n
        LDI     4
        PLO     R9
        LDI     84
        PHI     R9
        FCALL   txt_at16
        GETS    OFF_ROUNDS
        PLO     RC
        LDI     32
        PLO     R9
        LDI     84
        PHI     R9
        GLO     RC
        FCALL   txt_num
        FCALL   ws_status
        HOLD0
        FRET

; ws_status: the line under the rounds, and the keys.
        RFIT    119
ws_status:
        SETV    TXM, TM_SET|TM_SHORT
        GETV    MYSEAT
        XRI     NONE
        BNZ     wst1
        LDR     R8, s_watch
        BR      wst8
wst1:   GETS    OFF_HOST
        STR     R2
        GETV    MYSEAT
        XOR
        BZ      wst2
        LDR     R8, s_whost
        BR      wst8
wst2:   LDI     0                       ; the host: players?
        PLO     R9
        GETS    OFF_SEATED
wst3:   SHR
        BNF     wst4
        INC     R9
wst4:   BNZ     wst3
        GLO     R9
        SMI     2
        BDF     wst5
        LDR     R8, s_need
        BR      wst8
wst5:   LDR     R8, s_start
wst8:   LDI     100
        PHI     R9
        FCALL   txt_ctr
        GETS    OFF_HOST
        STR     R2
        GETV    MYSEAT
        XOR
        BZ      wst9
        LDR     R8, s_leave
        BR      wst10
wst9:   LDR     R8, s_hkeys
wst10:  LDI     110
        PHI     R9
        FCALL   txt_ctr
        SETV    TXM, TM_SET
        FRET

        RFIT    92
wait_tick:
        GETV    MYSEAT                  ; the host's keys
        XRI     NONE
        BZ      wt9
        GETS    OFF_HOST
        STR     R2
        GETV    MYSEAT
        XOR
        BNZ     wt9
        GETV    KNEW
        ANI     KB_FIRE
        BZ      wt1
        LDN     RA
        XRI     KB_FIRE
        STR     RA
        LDI     ACT_START
        FJMP    act_q
wt1:    GETV    KEV
        XRI     NONE
        BZ      wt9
        LDN     RA
        PLO     R9
        SETV    KEV, NONE
        GETS    OFF_ROUNDS
        PLO     R8
        GLO     R9
        XRI     4
        BNZ     wt2
        GLO     R8                      ; 4: one fewer
        SMI     2
        BNF     wt9
        ADI     1
        BR      wt3
wt2:    GLO     R9
        XRI     6
        BNZ     wt9
        GLO     R8                      ; 6: one more
        XRI     MAX_ROUNDS
        BZ      wt9
        GLO     R8
        ADI     1
wt3:    PLO     R8
        LDI     ACT_ROUNDS
        FJMP    act_q
wt9:    FRET

; seats_show: each seated seat's bomber standing (FR_DOWN0) in slot s at
; x 2 on its line; the rest hidden. (Its image uploaded.)
        RFIT    133
seats_show:
        SETV    GS, 0
ss1:    GETV    GS
        PLO     R9
        LDI     FR_DOWN0
        PHI     R9
        LCALL   seat_up
        VARX    SFS
        LDI     FR_DOWN0
        STR     RA
        GETV    GS
        PLO     R9
        ADI     LO(BITS)
        PLO     R8
        LDI     HI(BITS)
        PHI     R8
        LDN     R8
        STR     R2
        GETS    OFF_SEATED
        AND
        BZ      ss2
        PARS
        GLO     R9
        PD
        GLO     R9
        ADI     IM_SEAT
        PD
        P       2
        GETV    SSYT
        STR     R2
        GLO     R9
        ADD
        ADI     LO(WRY)
        PLO     R8
        LDI     HI(WRY)
        PHI     R8
        LDN     R8
        SMI     3
        PD
        P       TM_SET
        GOP     TOP_SPRITE
        BR      ss3
ss2:    PARS
        GLO     R9
        PD
        P       HIDDEN
        P       0
        GOP     TOP_SPRMOVE
ss3:    INCV    GS
        XRI     SEATS
        BNZ     ss1
        FRET

        PFIT    8
WRY:    DB      WR_Y0, WR_Y0+WR_DY, WR_Y0+2*WR_DY, WR_Y0+3*WR_DY
RSY:    DB      22, 44, 66, 88          ; the trophies' screen

s_room:   DB    "ROOM ", 0
s_dashes: DB    "--------", 0
s_host:   DB    "HOST", 0
s_rounds: DB    "ROUNDS", 0
s_watch:  DB    "WATCHING: FULL", 0
s_whost:  DB    "WAIT FOR HOST", 0
s_need:   DB    "NEED 2 PLAYERS", 0
s_start:  DB    "5 STARTS", 0
s_leave:  DB    "0 LEAVES", 0
s_hkeys:  DB    "4 6 ROUNDS 0 OFF", 0

; ---------------------------------------------------------------------------
; res_enter -- a round's end (D = 0) or the game's (D = 1): the arena
;   frozen and inverted a moment, then who won, each seat's trophies, the
;   winner cheering, the clock to the next.
; ---------------------------------------------------------------------------
RS_FREEZE EQU   30              ; jiffies the arena stays, inverted

        RFIT    54
res_enter:
        PUTV    RFINAL
        GETS    OFF_RESULT
        PUTV    RSEAT
        SETV    RTIM, NONE
        SETV    RFRZ, RS_FREEZE
        PARS                            ; the arena, inverted (if it is up)
        P       0
        P       0
        P       TV_INVERT
        GOP     TOP_VIEW
        FRET

; res_draw: the trophies' screen.
        RFIT    199
res_draw:
        OPN     TOP_CLS
        HOLD1
        SETV    SSYT, RSY-WRY
        FCALL   seats_show
        LDR     R9, FBUF                ; the header: "NAME WINS", or not
        GETV    RSEAT
        SMI     SEATS
        BDF     rd1
        GETV    RSEAT
        SHL
        SHL
        SHL
        ADI     LO(ST+OFF_NAMES)
        PLO     R8
        LDI     HI(ST)
        PHI     R8
        LDI     NAME_LEN
        PLO     RC
rd0:    LDA     R8
        BZ      rd0a
        STR     R9
        INC     R9
        DEC     RC
        GLO     RC
        BNZ     rd0
rd0a:   LDR     R8, s_wins
        GETV    RFINAL
        BZ      rd2
        LDR     R8, s_winsx
        BR      rd2
rd1:    LDR     R8, s_draw
        GETV    RFINAL
        BZ      rd2
        LDR     R8, s_nowin
rd2:    FCALL   strcat
        SETV    TXM, TM_SET
        LDR     R8, FBUF
        LDI     0
        PHI     R9
        FCALL   txt_ctr
        SETV    TXM, TM_SET|TM_SHORT    ; ROUND n, SUDDEN DEATH
        GETV    RFINAL
        BNZ     rd5
        GETS    OFF_SUDDEN
        BZ      rd3
        LDR     R8, s_sudden
        LDI     12
        PHI     R9
        FCALL   txt_ctr
        BR      rd5
rd3:    LDR     R8, s_round
        LDI     20
        PLO     R9
        LDI     12
        PHI     R9
        FCALL   txt_at16
        GETS    OFF_ROUND
        PLO     RC
        LDI     44
        PLO     R9
        LDI     12
        PHI     R9
        GLO     RC
        FCALL   txt_num
rd5:    FJMP    res_seats

; res_draw, the seats: each seated seat's name and trophies; the music.
        RFIT    89
res_seats:
        SETV    TXM, TM_SET
        SETV    GS, 0                   ; each seated seat: name, trophies
rd6:    GETV    GS
        PLO     R9
        ADI     LO(BITS)
        PLO     R8
        LDI     HI(BITS)
        PHI     R8
        LDN     R8
        STR     R2
        GETS    OFF_SEATED
        AND
        BZ      rd8
        FCALL   rd_seat
rd8:    INCV    GS
        XRI     SEATS
        BNZ     rd6
        HOLD0
        GETV    RFINAL
        BZ      rd9
        SND     MUS_GAME
        FRET
rd9:    GETV    RSEAT
        SMI     SEATS
        BDF     rd10
        SND     MUS_WIN
rd10:   FRET

; rd_seat: seat GS's line: its name, its trophies under it.
        RFIT    111
rd_seat:
        GETV    GS
        SHL
        SHL
        SHL
        ADI     LO(ST+OFF_NAMES)
        PLO     R8
        LDI     HI(ST)
        PHI     R8
        GETV    GS
        ADI     LO(RSY)
        PLO     RC
        LDI     HI(RSY)
        PHI     RC
        LDN     RC
        PHI     R9
        LDI     8
        PLO     R9
        LDI     NAME_LEN
        PLO     RC
        FCALL   txt_at
        GETV    GS                      ; its wins: a trophy each (9 at most)
        ADI     LO(PLOFF)
        PLO     R8
        LDI     HI(PLOFF)
        PHI     R8
        LDN     R8
        ADI     P_WINS
        PLO     RB
        LDN     RB
        SMI     10
        BNF     rds1
        LDI     0FFH
rds1:   ADI     10
        BZ      rds9
        PLO     R8                      ; R8.0 trophies left
        LDI     8
        PHI     R8                      ; R8.1 x
rds2:   PARS
        P       IM_TROPHY
        GHI     R8
        PD
        GETV    GS
        ADI     LO(RSY)
        PLO     RC
        LDI     HI(RSY)
        PHI     RC
        LDN     RC
        ADI     13
        PD
        P       TM_SET
        GOP     TOP_STAMP
        GHI     R8
        ADI     5
        PHI     R8
        DEC     R8
        GLO     R8
        BNZ     rds2
rds9:   FRET

; res_tick: the frozen arena's moment, then the screen; the winner cheering
; (16 jiffies a frame); the clock.
        RFIT    92
res_tick:
        GETV    RFRZ
        BZ      rt2
        GETV    DT
        STR     R2
        GETV    RFRZ
        SM
        BZ      rt1
        BNF     rt1
        STR     RA
        FRET
rt1:    SETV    RFRZ, 0
        PARS
        GOP     TOP_VIEW
        FJMP    res_draw
rt2:    GETV    RSEAT                   ; the winner: WIN0, WIN1
        SMI     SEATS
        BDF     rt4
        GETV    JIFFY
        SHR
        SHR
        SHR
        SHR
        ANI     1
        ADI     16
        PHI     R9                      ; (16, 17: SFS's tag for the win frames)
        GETV    RSEAT
        PLO     R9
        VARX    SFS
        LDN     RA
        STR     R2
        GHI     R9
        XOR
        BZ      rt4
        GHI     R9
        STR     RA
        LCALL   seat_upw
rt4:    FJMP    res_clock

; res_tick, the clock: NEXT ROUND / TO THE ROOM and the seconds.
        RFIT    177
res_clock:
        GETS    OFF_TIMER               ; the clock
        STR     R2
        GETV    RTIM
        XOR
        BZ      rt9
        GETS    OFF_TIMER
        PUTV    RTIM
        SETV    TXM, TM_SET|TM_SHORT
        PARS
        P       0
        P       110
        P       64
        P       5
        P       TM_CLEAR|TP_SOLID
        GOP     TOP_RECT
        LDR     R8, s_next
        GETV    RFINAL
        BZ      rt5
        LDR     R8, s_back
rt5:    LDI     4
        PLO     R9
        LDI     110
        PHI     R9
        FCALL   txt_at16
        GETV    RTIM
        PLO     RC
        LDI     52
        PLO     R9
        LDI     110
        PHI     R9
        GLO     RC
        FCALL   txt_num
        SETV    TXM, TM_SET
rt9:    FRET

; res_state: (the clock is the tick's)
res_state:
        FRET

s_wins:   DB    " WINS", 0
s_winsx:  DB    " WINS!", 0
s_draw:   DB    "DRAW", 0
s_nowin:  DB    "NO WINNER", 0
s_sudden: DB    "SUDDEN DEATH", 0
s_round:  DB    "ROUND", 0
s_next:   DB    "NEXT ROUND", 0
s_back:   DB    "TO THE ROOM", 0

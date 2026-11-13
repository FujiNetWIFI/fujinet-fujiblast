; p_arena.inc -- the arena drawn (src/play.c play_enter, play_state,
; play_frame's drawing), on the cart's graphics layer:
;   arena_enter  the arena afresh: the walls, every tile, the score band and
;                the panel, the bombers' and the movers' sprites
;   arena_state  a new state: deaths, the countdown's beeps, the HUD
;   arena_tick   a pass: the tiles that changed (at most ATILES), the
;                bombers (frames uploaded, at most AUPS), the movers, the
;                blast's shake -- all inside one HOLD, so a pass shows whole
; The tiles that changed come from the link (MAPMARK pushes them on DQ) and
; from the two-frame tiles' flip (ANIMB); a sweep (REDRAW) when DQ ran over.

ATILES  EQU     4               ; tiles stamped a pass, at most
AUPS    EQU     2               ; bomber frames uploaded a pass, at most
ANIM_EVERY EQU  16              ; jiffies a two-frame tile shows each frame
TOWARD  EQU     3               ; units a jiffy another seat is drawn moving
HIDDEN  EQU     100             ; a sprite's x off the screen (shown x >= 64: skipped)

; the score band: where each seat's head and wins go; the clock
HB_CLOCK EQU    24
; the panel: power-ups' icons and counts
PN_ICON EQU     55
PN_NUM  EQU     60
PN_Y0   EQU     12
PN_DY   EQU     22

; ---------------------------------------------------------------------------
; arena_enter -- the arena afresh (play_enter).
; ---------------------------------------------------------------------------
        RFIT    237
arena_enter:
        OPN     TOP_CLS
        HOLD1
        PARS                            ; the side walls
        P       0
        P       ARENA_Y
        P       1
        P       ARENA_H*TILE_H
        P       TM_SET|TP_SOLID
        GOP     TOP_RECT
        PARS
        P       PANEL_X
        P       ARENA_Y
        P       1
        P       ARENA_H*TILE_H
        P       TM_SET|TP_SOLID
        GOP     TOP_RECT
        SETV    PEND, NONE
        SETV    PENDL, 0
        SETV    DQH, 0
        SETV    DQT, 0
        SETV    REDRAW, NONE
        SETV    ANIMPH, 0
        SETV    ANIMT, ANIM_EVERY
        SETV    SHAKE, 0
        SETV    FLASH, 0
        LDR     R8, ANIMB               ; no two-frame tiles yet
        LDI     18
        PLO     R9
ae0:    LDI     0
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     ae0
        SETV    GI, 0                   ; every tile
ae1:    GETV    GI
        PLO     R9
        LCALL   tstamp
        INCV    GI
        XRI     TILES
        BNZ     ae1
        FCALL   a_reset
        GETS    OFF_ALIVE
        PUTV    WASALV
        GETS    OFF_PHASE
        PUTV    LASTPH
        FCALL   hud_all
        HOLD0
        FRET

; a_reset: the seats where the server has them, nothing shown, no falls;
; no movers shown.
        RFIT    170
a_reset:
        LDR     R8, SDX                 ; the seats: where the server has them,
        LDI     0                       ;  nothing shown, no falls
        PLO     R9
ae2:    GLO     R9
        ADI     LO(PLOFF)
        PLO     RB
        LDI     HI(PLOFF)
        PHI     RB
        LDN     RB                      ; (PLOFF is ROM: RB.1 put back below)
        ADI     P_X
        PLO     RB
        LDI     HI(ST)
        PHI     RB
        LDA     RB                      ; P_X, P_Y
        STR     R8                      ; SDX
        INC     R8
        INC     R8
        INC     R8
        INC     R8
        LDN     RB
        STR     R8                      ; SDY
        DEC     R8
        DEC     R8
        DEC     R8
        INC     R9
        GLO     R9
        XRI     SEATS
        BNZ     ae2
        LDR     R8, SPX                 ; SPX, SPY, SFW, SFS: NONE; SRK: s;
        LDI     16                      ;  DIET 0; MVK, MVPX, MVPY: NONE
        PLO     R9
ae3:    LDI     NONE
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     ae3
        LDI     0
        STR     R8
        INC     R8
        LDI     1
        STR     R8
        INC     R8
        LDI     2
        STR     R8
        INC     R8
        LDI     3
        STR     R8
        INC     R8
        LDI     16
        PLO     R9
ae4:    LDI     0                       ; DIET (4) then MVK.. (12): DIET 0,
        STR     R8                      ;  the movers NONE
        INC     R8
        DEC     R9
        GLO     R9
        XRI     12
        BNZ     ae4
ae5:    LDI     NONE
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     ae5
        LDR     R8, SLD                 ; nothing drawn yet
        LDI     NONE
        STR     R8
        INC     R8
        STR     R8
        INC     R8
        STR     R8
        INC     R8
        STR     R8
        LDR     R8, ORDER               ; slot n: seat n, hidden; keys 255
        LDI     0
        PLO     R9
ae6:    GLO     R9
        STR     R8
        INC     R8
        PARS
        GLO     R9
        PD
        GLO     R9
        ADI     IM_SEAT
        PD
        P       HIDDEN
        P       0
        P       TM_SET
        GOP     TOP_SPRITE
        INC     R9
        GLO     R9
        XRI     SEATS
        BNZ     ae6
        LDI     255
        STR     R8
        INC     R8
        STR     R8
        INC     R8
        STR     R8
        INC     R8
        STR     R8
        FRET

; seat s's record in ST: ST + PLOFF[s]
        PFIT    4
PLOFF:  DB      OFF_PLAYERS, OFF_PLAYERS+PLAYER_SIZE, OFF_PLAYERS+2*PLAYER_SIZE
        DB      OFF_PLAYERS+3*PLAYER_SIZE

; ---------------------------------------------------------------------------
; arena_tick -- a pass's drawing, inside one HOLD.
; ---------------------------------------------------------------------------
        RFIT    38
arena_tick:
        HOLD1
        FJMP    a_tiles                 ; (a chain: each jumps to the next,
                                        ;  a_end returns)
; a_end: the pass drawn: the picture let go.
a_end:  HOLD0
        FRET

; a_tiles: the two-frame tiles' flip, then up to ATILES tiles stamped: the
; queue first, then the sweep.
        RFIT    153
a_tiles:
        GETV    DT                      ; ANIMT -= DT: run out, flip
        STR     R2
        GETV    ANIMT
        SM
        BZ      at0
        BDF     at2
at0:    LDI     ANIM_EVERY
        STR     RA
        GETV    ANIMPH
        XRI     1
        STR     RA
        LDR     R8, ANIMB               ; every two-frame tile, queued
        LDI     0
        PLO     R9                      ; R9.0: the tile at the byte's bit 0
at1:    LDN     R8
        BZ      at1n
        PHI     R9                      ; R9.1: the byte's bits left
at1b:   GHI     R9
        SHR
        PHI     R9
        BNF     at1c
        GLO     R9                      ; this bit: queue the tile
        FCALL   dq_push
at1c:   INC     R9
        GHI     R9
        BNZ     at1b
        GLO     R9                      ; back to the byte's next 8
        ADI     7
        ANI     0F8H
        PLO     R9
        BR      at1d
at1n:   GLO     R9
        ADI     8
        PLO     R9
at1d:   INC     R8
        GLO     R8
        XRI     LO(ANIMB+18)
        BNZ     at1
        BR      at3
at2:    STR     RA
at3:    SETV    GI, ATILES
at4:    GETV    DQH                     ; the queue
        STR     R2
        GETV    DQT
        XOR
        BZ      at6
        GETV    DQH
        ADI     1
        STR     RA
        SMI     1
        ANI     31
        ADI     LO(DQ)
        PLO     R8
        LDI     HI(DQ)
        PHI     R8
        LDN     R8
at5:    PLO     R9
        LCALL   tstamp
        VAR     GI
        LDN     RA
        SMI     1
        STR     RA
        BNZ     at4
        FJMP    a_seats
at6:    GETV    REDRAW                  ; the sweep
        XRI     NONE
        BZ      at9
        LDN     RA                      ; this tile
        PLO     R9
        ADI     1
        STR     RA
        XRI     TILES
        BNZ     at8
        LDI     NONE                    ; (the last: the sweep is over)
        STR     RA
at8:    GLO     R9
        BR      at5
at9:    FJMP    a_seats

; ---------------------------------------------------------------------------
; dq_push -- tile D queued to be stamped again (the link's MAPMARK, the
;   flip). A full queue: the sweep over everything instead.  Keeps R8, R9;
;   clobbers RC (no leaf runs meanwhile), RD.1.
; ---------------------------------------------------------------------------
        RFIT    38
dq_push:
        PHI     RD
        GETV    DQH                     ; DQT - DQH waiting: 31 is full
        STR     R2
        GETV    DQT
        SM
        SMI     31
        BDF     dqp9
        LDN     RA                      ; DQ[DQT & 31] = the tile
        ANI     31
        ADI     LO(DQ)
        PLO     RC
        LDI     HI(DQ)
        PHI     RC
        GHI     RD
        STR     RC
        LDN     RA                      ; DQT += 1
        ADI     1
        STR     RA
        FRET
dqp9:   SETV    REDRAW, 0               ; run over: sweep everything
        FRET


; ---------------------------------------------------------------------------
; a_seats -- each seat's sprite, in one loop (src/play.c play_frame's
;   seats): not playing or blinking (ours, the countdown), hidden; fallen,
;   its fall's frames (DIET, 12 jiffies each), then hidden; ours while we
;   walk, where our walker is; anyone else's, drawn toward where the server
;   has it, 3 units a jiffy (more than a tile away: there at once). A new
;   frame is uploaded (AUPS a pass, at most: the rest next pass), a new
;   place sent (SPRMOVE); a seat whose y changed has the slots' order
;   checked (a_order: the lower bomber in front).
;   In the loop: R9.0 the seat, R9.1 its bit, R8.1/R8.0 its x/y, RB its
;   record, RC tables (RC.1 the step while drawing toward).
; ---------------------------------------------------------------------------
        RFIT    119
a_seats:
        SETV    UPC, AUPS
        SETV    T6, 0                   ; T6: a y changed
        GETS    OFF_INPLAY
        PUTV    T0
        GETS    OFF_ALIVE
        PUTV    T1
        SETV    T2, NONE                ; T2: the seat our walker draws
        SETV    T3, NONE                ; T3: the seat hidden (ours, blinking)
        GETS    OFF_PHASE
        XRI     PH_PLAY
        BNZ     asa
        GETV    MYSEAT
        PUTV    T2
        BR      asb
asa:    XRI     PH_PLAY!PH_COUNTDOWN
        BNZ     asb
        GETV    JIFFY
        ANI     8
        BZ      asb
        GETV    MYSEAT
        PUTV    T3
asb:    GETV    JIFFY
        PUTV    T5                      ; T5: the walk's clock
        GETV    DT                      ; T4: the step, 3 DT
        SHL
        STR     R2
        LDN     RA
        ADD
        PUTV    T4
        LDI     0
        PLO     R9
        LDI     1
        PHI     R9
        FJMP    asl

; a_seats, part 1: the seat R9.0: playing, fallen, ours; x toward P_X.
        RFIT    215
asl:    GHI     R9                      ; playing?
        STR     R2
        GETV    T0
        AND
        BZ      as_h1
        GETV    T1                      ; alive?
        AND
        BNZ     asal
        VARX    DIET                    ; fallen: its fall
        LDN     RA
        BZ      as_h1
        SMI     48
        BDF     as_h1
        LDN     RA
        SHR
        SHR
        ADI     LO(DIV3)
        PLO     RC
        LDI     HI(DIV3)
        PHI     RC
        LDN     RC
        ADI     FR_DIE0
        FJMP    asfr
as_h1:  FJMP    as_hid
asal:   GETV    T2                      ; ours, walking?
        STR     R2
        GLO     R9
        XOR
        BNZ     asot
        GETV    WX
        PHI     R8
        VARX    SDX
        GHI     R8
        STR     RA
        GETV    WY
        PHI     R8
        VARX    SDY
        GHI     R8
        STR     RA
        GETV    WMOVED
        PHI     R8
        GETV    WDIR
        BR      asdir
asot:   GLO     R9                      ; the server's: RB -> its record
        ADI     LO(PLOFF)
        PLO     RC
        LDI     HI(PLOFF)
        PHI     RC
        LDN     RC
        PLO     RB
        LDN     RB                      ; idle? drawn where it is, not moving,
        STR     R2                      ;  the same way as drawn, shown: next
        VARX    SDX
        LDN     RA
        XOR
        BNZ     asgo
        INC     RB
        LDN     RB
        STR     R2
        VARX    SDY
        LDN     RA
        XOR
        DEC     RB
        BNZ     asgo
        INC     RB
        INC     RB
        LDN     RB
        DEC     RB
        DEC     RB
        STR     R2
        ANI     P_DIR_MOVING
        BNZ     asgo
        VARX    SLD
        LDN     RA
        XOR
        BNZ     asgo
        VARX    SPX
        LDN     RA
        XRI     NONE
        BZ      asgo
        FJMP    asnx
asgo:   GETV    T4                      ; RC.1: the step
        PHI     RC
        LDN     RB                      ; x: SDX toward P_X
        STR     R2
        VARX    SDX
        LDN     RA
        SM
        BZ      tx9
        BNF     txn
        SMI     UNIT+1
        BDF     txto
        ADI     UNIT+1
        STR     R2
        GHI     RC
        SD
        BZ      txto
        BNF     txto
        GHI     RC
        STR     R2
        LDN     RA
        SM
        STR     RA
        BR      tx9
txn:    SDI     0
        SMI     UNIT+1
        BDF     txto
        ADI     UNIT+1
        STR     R2
        GHI     RC
        SD
        BZ      txto
        BNF     txto
        GHI     RC
        STR     R2
        LDN     RA
        ADD
        STR     RA
        BR      tx9
txto:   LDN     RB
        STR     RA
tx9:    FJMP    as_ty
asdir:  FJMP    as_dir

; a_seats, part 2: y toward P_Y, then the way and moving from P_DIR.
        RFIT    76
as_ty:  GETV    T4                      ; RC.1: the step
        PHI     RC
        INC     RB                      ; y: SDY toward P_Y
        LDN     RB
        STR     R2
        VARX    SDY
        LDN     RA
        SM
        BZ      ty9
        BNF     tyn
        SMI     UNIT+1
        BDF     tyto
        ADI     UNIT+1
        STR     R2
        GHI     RC
        SD
        BZ      tyto
        BNF     tyto
        GHI     RC
        STR     R2
        LDN     RA
        SM
        STR     RA
        BR      ty9
tyn:    SDI     0
        SMI     UNIT+1
        BDF     tyto
        ADI     UNIT+1
        STR     R2
        GHI     RC
        SD
        BZ      tyto
        BNF     tyto
        GHI     RC
        STR     R2
        LDN     RA
        ADD
        STR     RA
        BR      ty9
tyto:   LDN     RB
        STR     RA
ty9:    INC     RB                      ; P_DIR: the way, moving (kept: SLD)
        VARX    SLD
        LDN     RB
        STR     RA
        ANI     P_DIR_MOVING
        PHI     R8
        LDN     RB
        FJMP    as_dir

; a_seats, part 3: the frame (R8.1 moving, D the way), its upload, the place.
        RFIT    106
as_dir: ANI     3                       ; the way's first frame: 3 way
        PLO     R8
        SHL
        STR     R2
        GLO     R8
        ADD
        PLO     R8
        GHI     R8                      ; moving: + {0, 1, 0, 2} by (T5 + 2 s) / 8
        BZ      asf0
        GETV    T5
        STR     R2
        GLO     R9
        SHL
        ADD
        SHR
        SHR
        SHR
        ANI     3
        ADI     LO(WALKT)
        PLO     RC
        LDI     HI(WALKT)
        PHI     RC
        LDN     RC
        STR     R2
        GLO     R8
        ADD
        BR      asfr
asf0:   GLO     R8
asfr:   PHI     R8                      ; R8.1 the frame
        GETV    T3                      ; ours, blinking: hidden
        STR     R2
        GLO     R9
        XOR
        BZ      ash3
        VARX    SFS                     ; a new frame: upload it
        LDN     RA
        STR     R2
        GHI     R8
        XOR
        BZ      aspos
        GETV    UPC
        BZ      asup0                   ; (none left: the next pass)
        SMI     1
        STR     RA
        VARX    SFS
        GHI     R8
        STR     RA
        VAR     T7                      ; (R9.1, the bit, kept in T7)
        GHI     R9
        STR     RA
        GHI     R8
        PHI     R9
        LCALL   seat_up
        GETV    T7
        PHI     R9
aspos:  FJMP    as_pos
asup0:  VARX    SLD                     ; (not idle until it is uploaded)
        LDI     NONE
        STR     RA
        BR      aspos
ash3:   FJMP    as_hid

; a_seats, part 4: the place (R8.1 the frame done with).
        RFIT    174
as_pos: VARX    SDX                     ; x = 1 + SDX / 4
        LDN     RA
        SHR
        SHR
        ADI     1
        PHI     R8
        VARX    SDY                     ; y = YLINE[SDY], up SPR_UP
        LDN     RA
        ADI     LO(YLINE)
        PLO     RC
        LDI     HI(YLINE)
        PHI     RC
        LDN     RC
        ADI     ARENA_Y-SPR_UP
        PLO     R8
        VARX    SPY                     ; moved?
        LDN     RA
        STR     R2
        GLO     R8
        XOR
        BZ      asx
        GLO     R8
        STR     RA                      ; a new y: the order checked
        VARX    SKEY
        GLO     R8
        STR     RA
        VAR     T6
        LDI     1
        STR     RA
        BR      asmv
asx:    VARX    SPX
        LDN     RA
        STR     R2
        GHI     R8
        XOR
        BZ      asnx
asmv:   VARX    SPX
        GHI     R8
        STR     RA
        VARX    SRK                     ; SPRMOVE its slot
        LDN     RA
        PLO     RC
        PARS
        GLO     RC
        PD
        GHI     R8
        PD
        GLO     R8
        PD
        GOP     TOP_SPRMOVE
asnx:   GHI     R9                      ; the next seat
        SHL
        PHI     R9
        INC     R9
        GLO     R9
        XRI     SEATS
        BZ      asend
        FJMP    asl
asend:  GETV    T6
        BZ      as9
        FJMP    a_order
as9:    FJMP    a_movers
as_hid: VARX    SPX                     ; hidden (once)
        LDN     RA
        XRI     NONE
        BZ      asnx
        LDI     NONE
        STR     RA
        VARX    SKEY
        LDI     255
        STR     RA
        VAR     T6
        STR     RA
        VARX    SRK
        LDN     RA
        PLO     RC
        PARS
        GLO     RC
        PD
        P       HIDDEN
        P       0
        GOP     TOP_SPRMOVE
        BR      asnx

        PFIT    16
WALKT:  DB      0, 1, 0, 2
DIV3:   DB      0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3

; ---------------------------------------------------------------------------
; a_order -- the slots in y order (a hidden seat last, SKEY): if ORDER is
;   not sorted any more, each seat ranked again (o_sort).
; ---------------------------------------------------------------------------
        RFIT    37
a_order:
        LDI     0
        PLO     R9
ao1:    GLO     R9                      ; key(ORDER[n]) in M(R2)
        ADI     LO(ORDER)
        PLO     RA
        LDN     RA
        ADI     LO(SKEY)
        PLO     RA
        LDN     RA
        STR     R2
        GLO     R9                      ; key(ORDER[n+1])
        ADI     LO(ORDER)+1
        PLO     RA
        LDN     RA
        ADI     LO(SKEY)
        PLO     RA
        LDN     RA
        SM                              ; a borrow: out of order
        BNF     ao9
        INC     R9
        GLO     R9
        XRI     SEATS-1
        BNZ     ao1
        FJMP    a_movers
ao9:    FJMP    o_sort

; o_sort -- each seat's slot: the seats with a smaller key (or the same,
;   and a smaller number) before it. A seat whose slot changed: its sprite
;   sent whole to the slot (SPRITE: its image too).
        RFIT    140
o_sort: SETV    GS, 0
os1:    GETV    GS
        PLO     R9
        VARX    SKEY
        LDN     RA
        PHI     R8                      ; key(s)
        LDI     0
        PLO     R8                      ; its slot
        PHI     R9                      ; t
os2:    GHI     R9
        STR     R2
        GLO     R9
        XOR
        BZ      os4
        GHI     R9
        ADI     LO(SKEY)
        PLO     RA
        LDN     RA
        STR     R2                      ; key(t)
        GHI     R8
        SM                              ; key(s) - key(t)
        BNF     os4                     ; t after s
        BNZ     os3                     ; t before s
        GHI     R9
        STR     R2
        GLO     R9
        SM                              ; the same: s - t
        BNF     os4
os3:    INC     R8
os4:    GHI     R9
        ADI     1
        PHI     R9
        XRI     SEATS
        BNZ     os2
        GLO     R8                      ; ORDER[slot] = s
        ADI     LO(ORDER)
        PLO     RA
        GLO     R9
        STR     RA
        GLO     R8                      ; SRK[s]: changed?
        STR     R2
        VARX    SRK
        LDN     RA
        XOR
        BZ      os5
        GLO     R8
        STR     RA
        PARS                            ; the slot gets its image and place
        GLO     R8
        PD
        GLO     R9
        ADI     IM_SEAT
        PD
        VARX    SPX
        LDN     RA
        XRI     NONE
        BZ      os4a
        LDN     RA
        PD
        VARX    SPY
        LDN     RA
        PD
        BR      os4b
os4a:   P       HIDDEN
        P       0
os4b:   P       TM_SET
        GOP     TOP_SPRITE
os5:    INCV    GS
        XRI     SEATS
        BNZ     os1
        FJMP    a_movers

; ---------------------------------------------------------------------------
; a_movers -- the bombs in flight (kicked, thrown): sprites 4-7, a bomb or
;   a dud where the server has them (clamped to the arena: a throw wraps).
; ---------------------------------------------------------------------------
        RFIT    243
a_movers:
        GETS    OFF_MOVERS              ; none in flight and none shown: done
        STR     R2
        GETS    OFF_MOVERS+MOVER_SIZE
        OR
        STR     R2
        GETS    OFF_MOVERS+2*MOVER_SIZE
        OR
        STR     R2
        GETS    OFF_MOVERS+3*MOVER_SIZE
        OR
        ANI     3
        BNZ     am0
        GETV    MVK                     ; (all NONE: none shown)
        STR     R2
        INC     RA
        LDN     RA
        AND
        STR     R2
        INC     RA
        LDN     RA
        AND
        STR     R2
        INC     RA
        LDN     RA
        AND
        XRI     NONE
        BNZ     am0
        FJMP    a_fx
am0:    LDI     0
        PLO     R9

am1:    GLO     R9                      ; RB -> its record
        SHL
        SHL
        ADI     LO(ST+OFF_MOVERS)
        PLO     RB
        LDA     RB                      ; its kind
        PLO     R8
        ANI     3
        BZ      amh
        GLO     R8
        ANI     M_DUD
        BZ      am2
        LDI     IM_DUD
        BR      am3
am2:    LDI     IM_BOMB0
am3:    PHI     R8                      ; R8.1 the image
        LDA     RB                      ; x, at most MAX_X
        STR     R2
        LDI     MAX_X
        SM
        BDF     am4
        LDI     MAX_X
        BR      am5
am4:    LDN     R2
am5:    SHR
        SHR
        ADI     1
        PLO     R8                      ; R8.0 its x
        LDN     RB                      ; y, at most MAX_Y
        STR     R2
        LDI     MAX_Y
        SM
        BDF     am6
        LDI     MAX_Y
        BR      am7
am6:    LDN     R2
am7:    ADI     LO(YLINE)
        PLO     RC
        LDI     HI(YLINE)
        PHI     RC
        LDN     RC
        ADI     ARENA_Y
        PHI     RC                      ; RC.1 its y
        VARX    MVK                     ; changed?
        LDN     RA
        STR     R2
        GHI     R8
        XOR
        BNZ     am8
        VARX    MVPX
        LDN     RA
        STR     R2
        GLO     R8
        XOR
        BNZ     am8
        VARX    MVPY
        LDN     RA
        STR     R2
        GHI     RC
        XOR
        BZ      am9
am8:    VARX    MVK
        GHI     R8
        STR     RA
        VARX    MVPX
        GLO     R8
        STR     RA
        VARX    MVPY
        GHI     RC
        STR     RA
        PARS
        GLO     R9
        ADI     4
        PD
        GHI     R8
        PD
        GLO     R8
        PD
        GHI     RC
        PD
        P       TM_SET
        GOP     TOP_SPRITE
        BR      am9
amh:    VARX    MVK                     ; none: hidden, if it was shown
        LDN     RA
        XRI     NONE
        BZ      am9
        LDI     NONE
        STR     RA
        PARS
        GLO     R9
        ADI     4
        PD
        P       HIDDEN
        P       0
        GOP     TOP_SPRMOVE
am9:    INC     R9
        GLO     R9
        XRI     MOVERS
        BNZ     am1
        FJMP    a_fx

; ---------------------------------------------------------------------------
; a_fx -- the VIEW: a blast's shake (SHAKE: a line up and down), the
;   round's end (FLASH: inverted); the falls' clocks (DIET).
; ---------------------------------------------------------------------------
        RFIT    165
a_fx:   LDI     0
        PHI     R8                      ; dy
        PLO     R8                      ; flags
        GETV    SHAKE
        BZ      af1
        GETV    DT
        STR     R2
        GETV    SHAKE
        SM
        BDF     af0
        LDI     0
af0:    STR     RA
        BZ      af1
        ANI     2
        BZ      afm
        LDI     1
        BR      afv
afm:    LDI     0FFH
afv:    PHI     R8
af1:    GETV    FLASH
        BZ      af2
        GETV    DT
        STR     R2
        GETV    FLASH
        SM
        BDF     af3
        LDI     0
af3:    STR     RA
        BZ      af2
        LDI     TV_INVERT
        PLO     R8
af2:    GHI     R8                      ; sent while an effect runs, and once
        STR     R2                      ;  more when it ends
        GLO     R8
        OR
        BNZ     af4
        GETV    VIEWS
        BZ      af5
af4:    GHI     R8
        STR     R2
        GLO     R8
        OR
        PUTV    VIEWS
        PARS
        P       0
        GHI     R8
        PD
        GLO     R8
        PD
        GOP     TOP_VIEW
af5:    GETV    DIET                    ; the falls: DIET += DT, to 48
        STR     R2
        INC     RA
        LDN     RA
        OR
        STR     R2
        INC     RA
        LDN     RA
        OR
        STR     R2
        INC     RA
        LDN     RA
        OR
        BNZ     af5a
        FJMP    a_end
af5a:   LDI     0
        PLO     R9
af6:    VARX    DIET
        LDN     RA
        BZ      af7
        SMI     48
        BDF     af7
        GETV    DT
        STR     R2
        VARX    DIET
        LDN     RA
        ADD
        STR     RA
af7:    INC     R9
        GLO     R9
        XRI     SEATS
        BNZ     af6
        FJMP    a_end

; ---------------------------------------------------------------------------
; the HUD: the score band (each seat's head, its wins, the clock; ours
; inverted) and the panel (our power-ups). hud_all draws it all; hud_check,
; at a state, what changed.
; ---------------------------------------------------------------------------
        RFIT    44
hud_all:
        SETV    GS, 0
ha1:    FCALL   hud_seat
        INCV    GS
        XRI     SEATS
        BNZ     ha1
        FCALL   hud_clock
        SETV    GS, 0
ha2:    FCALL   hud_stat
        INCV    GS
        XRI     5
        BNZ     ha2
        FRET

; hud_check: the HUD's parts whose numbers changed; a power-up gained
; sounds.
        RFIT    165
hud_check:
        GETS    OFF_TIMER
        STR     R2
        GETV    HTIM
        XOR
        BZ      hc1
        FCALL   hud_clock
hc1:    GETV    MYSEAT                  ; ours moved, or the living changed:
        STR     R2                      ;  every seat
        GETV    HSEAT
        XOR
        BNZ     hc2
        GETS    OFF_ALIVE
        STR     R2
        GETV    HALV
        XOR
        BZ      hc3
hc2:    GETS    OFF_ALIVE
        PUTV    HALV
        SETV    GS, 0
hc2a:   FCALL   hud_seat
        INCV    GS
        XRI     SEATS
        BNZ     hc2a
        BR      hc5
hc3:    SETV    GS, 0                   ; else a seat whose wins changed
hc4:    GETV    GS
        PLO     R9
        ADI     LO(PLOFF)
        PLO     R8
        LDI     HI(PLOFF)
        PHI     R8
        LDN     R8
        ADI     P_WINS
        PLO     RB
        LDN     RB
        STR     R2
        VARX    HWIN
        LDN     RA
        XOR
        BZ      hc4a
        FCALL   hud_seat
hc4a:   INCV    GS
        XRI     SEATS
        BNZ     hc4
hc5:    FCALL   stat_same               ; our power-ups: the same? done
        BNZ     hc5a
        FRET
hc5a:   SETV    GS, 0
hc6:    FCALL   stat_of                 ; D: the value
        STR     R2
        GETV    GS
        ADI     LO(HSTAT)
        PLO     RA
        LDN     RA
        SD                              ; new - old: up, a pick-up
        BZ      hc7
        BNF     hc6a
        SND     SND_PICKUP
hc6a:   FCALL   hud_stat
hc7:    INCV    GS
        XRI     5
        BNZ     hc6
        FRET

; stat_same: D = 0 if our five power-ups are those shown (HSTAT).
        RFIT    76
stat_same:
        GETV    MYSEAT
        XRI     NONE
        BZ      ssm9                    ; (watching: all 0, never changes)
        LDN     RA
        ADI     LO(PLOFF)
        PLO     R8
        LDI     HI(PLOFF)
        PHI     R8
        LDN     R8
        ADI     P_RANGE
        PLO     RB
        LDA     RB                      ; range, bombs, speed (+ 1)
        STR     R2
        GETV    HSTAT
        XOR
        BNZ     ssm8
        LDA     RB
        STR     R2
        GETV    HSTAT+1
        XOR
        BNZ     ssm8
        LDN     RB
        ADI     1
        STR     R2
        GETV    HSTAT+2
        XOR
        BNZ     ssm8
        GLO     RB                      ; kick, glove
        ADI     P_FLAGS-P_SPEED
        PLO     RB
        LDN     RB
        ANI     PF_KICK
        STR     R2
        GETV    HSTAT+3
        XOR
        BNZ     ssm8
        LDN     RB
        ANI     PF_GLOVE
        STR     R2
        GETV    HSTAT+4
        XOR
ssm8:   FRET
ssm9:   LDI     0
        FRET

; hud_seat: seat GS's place in the band: its head (playing; alive, or not
; in play), its wins; ours inverted.
        RFIT    214
hud_seat:
        GETV    GS
        PLO     R9
        ADI     LO(HBX)
        PLO     R8
        LDI     HI(HBX)
        PHI     R8
        LDN     R8
        PHI     R9                      ; R9.1: its x
        PARS                            ; cleared
        GHI     R9
        PD
        P       0
        P       8
        P       5
        P       TM_CLEAR|TP_SOLID
        GOP     TOP_RECT
        GLO     R9                      ; its record: RB -> P_WINS
        ADI     LO(PLOFF)
        PLO     R8
        LDI     HI(PLOFF)
        PHI     R8
        LDN     R8
        ADI     P_WINS
        PLO     RB
        LDN     RB
        PHI     R8                      ; R8.1 its wins
        VARX    HWIN
        GHI     R8
        STR     RA
        GLO     R9
        ADI     LO(BITS)
        PLO     R8
        LDI     HI(BITS)
        PHI     R8
        LDN     R8
        STR     R2
        GETS    OFF_INPLAY
        AND
        BZ      hs9                     ; not playing: nothing
        GETS    OFF_ALIVE
        AND
        BNZ     hs1
        GETS    OFF_PHASE
        XRI     PH_PLAY
        BZ      hs2                     ; fallen in play: no head
hs1:    PARS
        GLO     R9
        ADI     IM_HEAD0
        PD
        GHI     R9
        PD
        P       0
        P       TM_SET
        GOP     TOP_STAMP
hs2:    PARS                            ; its wins, the short font
        GHI     R9
        ADI     4
        PD
        P       0
        P       TM_SET|TM_SHORT
        GLO     R9
        ADI     LO(PLOFF)
        PLO     R8
        LDI     HI(PLOFF)
        PHI     R8
        LDN     R8
        ADI     P_WINS
        PLO     RB
        LDN     RB
        ADI     '0'
        PD
        GOP     TOP_PTEXT
hs9:    GETV    MYSEAT                  ; ours: inverted
        PUTV    HSEAT
        STR     R2
        GLO     R9
        XOR
        BNZ     hs10
        PARS
        GHI     R9
        PD
        P       0
        P       8
        P       5
        P       TM_XOR|TP_SOLID
        GOP     TOP_RECT
hs10:   FRET

        PFIT    4
HBX:    DB      0, 12, 44, 56

; hud_clock: the round's clock (M:SS), the countdown's number alone.
        RFIT    122
hud_clock:
        GETS    OFF_TIMER
        PUTV    HTIM
        PLO     R9                      ; seconds
        LDI     0
        PHI     R9                      ; minutes
hk1:    GLO     R9
        SMI     60
        BNF     hk2
        PLO     R9
        GHI     R9
        ADI     1
        PHI     R9
        BR      hk1
hk2:    LDI     0                       ; R8.1 tens, R8.0 ones
        PHI     R8
hk3:    GLO     R9
        SMI     10
        BNF     hk4
        PLO     R9
        GHI     R8
        ADI     1
        PHI     R8
        BR      hk3
hk4:    PARS
        P       HB_CLOCK
        P       0
        P       TM_SET|TM_SHORT
        GETS    OFF_PHASE
        XRI     PH_COUNTDOWN
        BZ      hk5
        GHI     R9
        ADI     '0'
        PD
        P       ':'
        GHI     R8
        ADI     '0'
        PD
        GLO     R9
        ADI     '0'
        PD
        GOP     TOP_PTEXT
        FRET
hk5:    P       ' '
        P       ' '
        GLO     R9
        ADI     '0'
        PD
        P       ' '
        GOP     TOP_PTEXT
        FRET

; stat_of: D = our power-up GS: 0 range, 1 bombs, 2 speed + 1, 3 kick, 4
; glove (watching: 0).
        RFIT    74
stat_of:
        GETV    MYSEAT
        XRI     NONE
        BZ      so0
        LDN     RA
        ADI     LO(PLOFF)
        PLO     R8
        LDI     HI(PLOFF)
        PHI     R8
        LDN     R8
        PLO     RB                      ; RB -> our record
        GETV    GS
        BZ      so_r
        SMI     1
        BZ      so_b
        SMI     1
        BZ      so_s
        SMI     1
        BZ      so_k
        GLO     RB                      ; glove
        ADI     P_FLAGS
        PLO     RB
        LDN     RB
        ANI     PF_GLOVE
        FRET
so_k:   GLO     RB
        ADI     P_FLAGS
        PLO     RB
        LDN     RB
        ANI     PF_KICK
        FRET
so_s:   GLO     RB
        ADI     P_SPEED
        PLO     RB
        LDN     RB
        ADI     1
        FRET
so_b:   GLO     RB
        ADI     P_BOMBS
        PLO     RB
        LDN     RB
        FRET
so_r:   GLO     RB
        ADI     P_RANGE
        PLO     RB
        LDN     RB
        FRET
so0:    LDI     0
        FRET

; hud_stat: the panel's row GS: its icon and count (kick, glove: the icon
; while we have it).
        RFIT    145
hud_stat:
        FCALL   stat_of
        PLO     R9                      ; R9.0 the value
        GETV    GS
        ADI     LO(HSTAT)
        PLO     RA
        GLO     R9
        STR     RA
        GETV    GS
        ADI     LO(PNY)
        PLO     R8
        LDI     HI(PNY)
        PHI     R8
        LDN     R8
        PHI     R9                      ; R9.1 its y
        GETV    GS
        SMI     3
        BNF     hst1
        GLO     R9                      ; kick, glove: the icon, or nothing
        BNZ     hst1
        PARS
        P       PN_ICON
        GHI     R9
        PD
        P       TILE_W
        P       TILE_H
        P       TM_CLEAR|TP_SOLID
        GOP     TOP_RECT
        FRET
hst1:   GETV    GS                      ; the icon
        ADI     LO(PNICON)
        PLO     R8
        LDN     R8
        PHI     RC
        PARS
        GHI     RC
        PD
        P       PN_ICON
        GHI     R9
        PD
        P       TM_SET
        GOP     TOP_STAMP
        GETV    GS
        SMI     3
        BDF     hst9
        PARS                            ; the count
        P       PN_NUM
        GHI     R9
        ADI     3
        PD
        P       TM_SET|TM_SHORT
        GLO     R9
        ADI     '0'
        PD
        GOP     TOP_PTEXT
hst9:   FRET

        PFIT    10
PNY:    DB      PN_Y0, PN_Y0+PN_DY, PN_Y0+2*PN_DY, PN_Y0+3*PN_DY, PN_Y0+4*PN_DY
PNICON: DB      IM_FIRE0, IM_BOMBUP0, IM_SPEED0, IM_KICK0, IM_GLOVE0

; ---------------------------------------------------------------------------
; arena_state -- a new state (play_state): the deaths, the countdown's
;   beeps and GO, a blast's shake and sound, a dud's fizzle, the HUD; our
;   pending bomb let go once the server has the tile.
; ---------------------------------------------------------------------------
        RFIT    178
arena_state:
        GETV    PEND                    ; our bomb: the server's now?
        XRI     NONE
        BZ      ast1
        LDN     RA
        ADI     LO(ST+OFF_MAP)
        PLO     RB
        LDN     RB
        BZ      ast1
        SETV    PEND, NONE
ast1:   GETS    OFF_PHASE               ; deaths, in play
        XRI     PH_PLAY
        BNZ     ast3
        GETS    OFF_ALIVE
        XRI     0FFH
        STR     R2
        GETV    WASALV
        AND                             ; alive before, not now
        PHI     R9
        BZ      ast3
        LDI     0
        PLO     R9
ast2:   GHI     R9
        SHR
        PHI     R9
        BNF     ast2a
        VARX    DIET
        LDI     1
        STR     RA
        SND     SND_DIE
ast2a:  INC     R9
        GHI     R9
        BNZ     ast2
ast3:   GETS    OFF_PHASE               ; the countdown's beeps
        XRI     PH_COUNTDOWN
        BNZ     ast4
        GETS    OFF_TIMER
        STR     R2
        GETV    HTIM
        XOR
        BZ      ast5
        SND     SND_BEEP
        BR      ast5
ast4:   GETS    OFF_PHASE               ; and GO
        XRI     PH_PLAY
        BNZ     ast5
        GETV    LASTPH
        XRI     PH_COUNTDOWN
        BNZ     ast5
        SND     SND_GO
ast5:   GETV    MFLAGS                  ; what the patches did
        ANI     MF_BOOM
        BZ      ast6
        SETV    SHAKE, 8
        SND     SND_BOOM
ast6:   GETV    MFLAGS
        ANI     MF_FIZZ
        BZ      ast7
        SND     SND_FIZZLE
ast7:   SETV    MFLAGS, 0
        FCALL   hud_check
        GETS    OFF_ALIVE
        PUTV    WASALV
        GETS    OFF_PHASE
        PUTV    LASTPH
        FRET

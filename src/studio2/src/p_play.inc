; p_play.inc -- our own walking and bombs (src/move.c, line for line;
; src/play.c play_frame's own part and play_state's warp). The walker runs
; once a jiffy, DT times a pass; fire and the move sent once a pass.

MOVE_EVERY EQU  4               ; jiffies between our moves, at least
HEARTBEAT  EQU  60              ;  and at most
BUMP_GAP   EQU  30              ; jiffies between bumps
PEND_WAIT  EQU  90              ; jiffies our bomb waits for the server

; play_reset: the walker where the server has us (play_enter).
        RFIT    60
play_reset:
        SETV    PEND, NONE
        SETV    PENDL, 0
        SETV    WACC, 0
        SETV    WDIR, DIR_DOWN
        SETV    SENTW, 0
        SETV    SENTH, 0
        SETV    BUMPW, 0
        SETV    SENTD, NONE
        GETV    MYSEAT
        XRI     NONE
        BZ      prs9
        FJMP    p_warp
prs9:   FRET

; play_state: a new state: the server moved us (P_WARP): there.
        RFIT    32
play_state:
        GETV    MYSEAT
        XRI     NONE
        BZ      pst9
        LDN     RA
        ADI     LO(PLOFF)
        PLO     R8
        LDI     HI(PLOFF)
        PHI     R8
        LDN     R8
        ADI     P_WARP
        PLO     RB
        LDN     RB
        STR     R2
        GETV    MYWARP
        XOR
        BZ      pst9
        FJMP    p_warp
pst9:   FRET

; p_warp: WX, WY, MYWARP from our record (mv_reset).
        RFIT    44
p_warp: GETV    MYSEAT
        ADI     LO(PLOFF)
        PLO     R8
        LDI     HI(PLOFF)
        PHI     R8
        LDN     R8
        PLO     RB
        LDA     RB
        PUTV    WX
        LDN     RB
        PUTV    WY
        GLO     RB
        ADI     P_WARP-P_Y
        PLO     RB
        LDN     RB
        PUTV    MYWARP
        SETV    WACC, 0
        FRET

; ---------------------------------------------------------------------------
; play_pass -- the pass's own part: DT jiffies of walking (and bumps, and
;   our pending bomb's wait), then fire and our move.
; ---------------------------------------------------------------------------
        RFIT    143
play_pass:
        SETV    WMOVED, 0
        SETV    WME, 0
        GETV    MYSEAT                  ; we walk: seated, alive, in play
        XRI     NONE
        BZ      pp1
        LDN     RA
        ADI     LO(BITS)
        PLO     R8
        LDI     HI(BITS)
        PHI     R8
        LDN     R8
        STR     R2
        GETS    OFF_ALIVE
        AND
        BZ      pp1
        GETS    OFF_PHASE
        XRI     PH_PLAY
        BNZ     pp1
        GETV    MYSEAT                  ; our speed
        ADI     LO(PLOFF)
        PLO     R8
        LDI     HI(PLOFF)
        PHI     R8
        LDN     R8
        ADI     P_SPEED
        PLO     RB
        LDN     RB
        PUTV    WSPD
        SETV    WME, 1
pp1:    GETV    DT
        PUTV    WCNT
pp2:    GETV    WME
        BZ      pp3
        GETV    KHELD                   ; no way held: still (mv_step(NONE))
        ANI     KB_DIRS
        BNZ     pp2b
        SETV    WACC, 0
        SETV    WBUMP, NONE
        BR      pp3
pp2b:   FCALL   try_dirs
        BZ      pp2a
        SETV    WMOVED, 1
pp2a:   GETV    WBUMP
        XRI     NONE
        BZ      pp3
        FCALL   p_bump
pp3:
        IF      DEBUG
        FCALL   p_trace
        ENDIF
pp7:    VAR     WCNT
        LDN     RA
        SMI     1
        STR     RA
        BNZ     pp2
        FJMP    p_clocks

; p_clocks: the pass's clocks, by DT: our pending bomb's wait (never come:
; off again), the move's and the bump's gaps; then fire and our move.
        RFIT    93
p_clocks:
        GETV    DT
        STR     R2
        GETV    PEND                    ; our bomb, never come: off again
        XRI     NONE
        BZ      pc1
        GETV    PENDL
        BZ      pc1
        SM
        BZ      pc0
        BDF     pc0a
pc0:    LDI     0
        STR     RA
        GETV    PEND
        PHI     R9
        SETV    PEND, NONE
        GHI     R9
        FCALL   dq_push
        GETV    DT
        STR     R2
        BR      pc1
pc0a:   STR     RA
pc1:    GETV    SENTW                   ; the gaps, to 0
        SM
        BDF     pc2
        LDI     0
pc2:    STR     RA
        GETV    SENTH
        SM
        BDF     pc3
        LDI     0
pc3:    STR     RA
        GETV    BUMPW
        SM
        BDF     pc4
        LDI     0
pc4:    STR     RA
        GETV    WME
        BZ      pc9
        FCALL   p_fire
        FJMP    p_move
pc9:    FRET

; ---------------------------------------------------------------------------
; try_dirs -- the keys to a way; on a diagonal, the turn first (the way we
;   aren't going), and if that's walled, on as we were. D = 1 if we moved.
; ---------------------------------------------------------------------------
        RFIT    163
try_dirs:
        LDI     NONE
        PHI     R9                      ; up or down
        PLO     R9                      ; across
        GETV    KHELD
        PLO     R8
        ANI     KB_UP
        BZ      td1
        LDI     DIR_UP
        PHI     R9
        BR      td2
td1:    GLO     R8
        ANI     KB_DOWN
        BZ      td2
        LDI     DIR_DOWN
        PHI     R9
td2:    GLO     R8
        ANI     KB_LEFT
        BZ      td3
        LDI     DIR_LEFT
        PLO     R9
        BR      td4
td3:    GLO     R8
        ANI     KB_RIGHT
        BZ      td4
        LDI     DIR_RIGHT
        PLO     R9
td4:    GHI     R9
        XRI     NONE
        BNZ     td5
        GLO     R9                      ; across, or still
        FJMP    mv_step
td5:    GLO     R9
        XRI     NONE
        BNZ     td6
        GHI     R9                      ; up or down
        FJMP    mv_step
td6:    GETV    WDIR                    ; both: going across (odd), turn up or
        ANI     1                       ;  down first
        BZ      td7
        GLO     R9
        PUTV    WTB
        GHI     R9
        BR      td8
td7:    GHI     R9                      ; going up or down: across first
        PUTV    WTB
        GLO     R9
td8:    PHI     R8
        GETV    WX
        PUTV    WSX
        GETV    WY
        PUTV    WSY
        GETV    WACC
        PUTV    WSA
        GHI     R8
        FCALL   mv_step
        BNZ     td9
        GETV    WSX                     ; walled: back, and on as we were
        PUTV    WX
        GETV    WSY
        PUTV    WY
        GETV    WSA
        PUTV    WACC
        GETV    WTB
        FJMP    mv_step
td9:    FRET

; mv_step -- one jiffy toward way D (NONE: still) at speed WSPD: D = 1 if
;   it moved (move.c mv_step).
        RFIT    109
mv_step:
        PUTV    WD
        SETV    WBUMP, NONE
        SETV    WMS, 0
        GETV    WD
        XRI     NONE
        BNZ     ms1
        SETV    WACC, 0
        LDI     0
        FRET
ms1:    GETV    WD
        PUTV    WDIR
        GETV    WSPD                    ; acc += 16 + 4 lv: acc >> 4 steps
        SHL
        SHL
        ADI     SPEED_BASE
        STR     R2
        GETV    WACC
        ADD
        PLO     R8
        ANI     15
        STR     RA
        GLO     R8
        SHR
        SHR
        SHR
        SHR
        PUTV    WN
ms2:    GETV    WN
        BZ      ms9
        SMI     1
        STR     RA
        FCALL   unit_step
        BNZ     ms3
        SETV    WACC, 0                 ; walled: no more
        BR      ms9
ms3:    SETV    WMS, 1
        BR      ms2
ms9:    GETV    WMS
        FRET

; ---------------------------------------------------------------------------
; unit_step -- a unit toward WD: D = 1 if it went (move.c unit_step). Off a
;   lane by LANE or less, pushing into a cross lane slides onto it.
; ---------------------------------------------------------------------------
        RFIT    168
unit_step:
        GETV    WD
        ANI     1
        BNZ     ush
        FJMP    usv
ush:    GETV    WX                      ; across: c = x / 16 + dx, r = y / 16
        SHR
        SHR
        SHR
        SHR
        PLO     R9
        GETV    WD
        XRI     DIR_RIGHT
        BZ      ush1
        DEC     R9
        DEC     R9
ush1:   INC     R9
        GETV    WY
        SHR
        SHR
        SHR
        SHR
        PHI     R9
        GETV    WY                      ; off the lane?
        ANI     15
        BZ      ush4
        SMI     LANE+1
        BDF     ush2
        LCALL   walkable                ; up to LANE below a lane: up onto it
        BZ      us0
        GETV    WY
        SMI     1
        STR     RA
        LDI     1
        FRET
ush2:   GETV    WY                      ; up to LANE above the next: down
        ANI     15
        SMI     UNIT-LANE
        BNF     us0
        GHI     R9
        ADI     1
        PHI     R9
        LCALL   walkable
        BZ      us0
        GETV    WY
        ADI     1
        STR     RA
        LDI     1
        FRET
ush4:   GETV    WX                      ; on the lane: into a wall?
        ANI     15
        BNZ     ush5
        LCALL   walkable
        BNZ     ush5
        LCALL   bumpchk
        PUTV    WBUMP
        LDI     0
        FRET
ush5:   GETV    WD
        XRI     DIR_RIGHT
        BZ      ush6
        GETV    WX
        SMI     1
        STR     RA
        LDI     1
        FRET
ush6:   GETV    WX
        ADI     1
        STR     RA
        LDI     1
        FRET
us0:    LDI     0
        FRET

; unit_step, up or down: c = x / 16, r = y / 16 + dy.
        RFIT    159
usv:    GETV    WX
        SHR
        SHR
        SHR
        SHR
        PLO     R9
        GETV    WY
        SHR
        SHR
        SHR
        SHR
        PHI     R9
        GETV    WD
        XRI     DIR_DOWN
        BZ      usv1
        GHI     R9
        SMI     2
        PHI     R9
usv1:   GHI     R9
        ADI     1
        PHI     R9
        GETV    WX                      ; off the lane?
        ANI     15
        BZ      usv4
        SMI     LANE+1
        BDF     usv2
        LCALL   walkable
        BZ      usv0
        GETV    WX
        SMI     1
        STR     RA
        LDI     1
        FRET
usv2:   GETV    WX
        ANI     15
        SMI     UNIT-LANE
        BNF     usv0
        INC     R9
        LCALL   walkable
        BZ      usv0
        GETV    WX
        ADI     1
        STR     RA
        LDI     1
        FRET
usv4:   GETV    WY                      ; on the lane: into a wall?
        ANI     15
        BNZ     usv5
        LCALL   walkable
        BNZ     usv5
        LCALL   bumpchk
        PUTV    WBUMP
        LDI     0
        FRET
usv5:   GETV    WD
        XRI     DIR_DOWN
        BZ      usv6
        GETV    WY
        SMI     1
        STR     RA
        LDI     1
        FRET
usv6:   GETV    WY
        ADI     1
        STR     RA
        LDI     1
        FRET
usv0:   LDI     0
        FRET

; ---------------------------------------------------------------------------
; walkable -- D = 1 if tile (R9.0 c, R9.1 r) can be walked on: in the
;   arena, not our pending bomb, not a pillar, block, burning block, bomb
;   or dud. Leaf. Keeps R9; clobbers R8, RB.0, M(R2).
; ---------------------------------------------------------------------------
        RFIT    47
walkable:
        GLO     R9                      ; (-1 is 255: out too)
        SMI     ARENA_W
        BDF     wk0
        GHI     R9
        SMI     ARENA_H
        BDF     wk0
        GHI     R9                      ; t = 13 r + c
        ADI     LO(TROW)
        PLO     R8
        LDI     HI(TROW)
        PHI     R8
        LDN     R8
        STR     R2
        GLO     R9
        ADD
        PLO     R8
        STR     R2
        GETV    PEND
        XOR
        BZ      wk0
        GLO     R8
        ADI     LO(ST+OFF_MAP)
        PLO     RB
        LDN     RB                      ; kinds 1-5 are solid
        SMI     1
        SMI     5
        BNF     wk0
        LDI     1
        SEP     R3
wk0:    LDI     0
        SEP     R3

; bumpchk -- D = tile (R9.0 c, R9.1 r) if it is in the arena and a bomb or
;   a dud (move.c bumped), else NONE. Leaf. Keeps R9.
        RFIT    39
bumpchk:
        GLO     R9
        SMI     ARENA_W
        BDF     bc0
        GHI     R9
        SMI     ARENA_H
        BDF     bc0
        GHI     R9
        ADI     LO(TROW)
        PLO     R8
        LDI     HI(TROW)
        PHI     R8
        LDN     R8
        STR     R2
        GLO     R9
        ADD
        PLO     R8
        ADI     LO(ST+OFF_MAP)
        PLO     RB
        LDN     RB
        XRI     T_BOMB
        BZ      bc1
        XRI     T_BOMB!T_DUD
        BNZ     bc0
bc1:    GLO     R8
        SEP     R3
bc0:    LDI     NONE
        SEP     R3

; ---------------------------------------------------------------------------
; p_bump -- walked into a bomb (WBUMP): with the kick, or into a dud, tell
;   the server (BUMP_GAP jiffies apart).
; ---------------------------------------------------------------------------
        RFIT    67
p_bump: GETV    WBUMP
        XRI     NONE
        BZ      pb9
        GETV    BUMPW
        BNZ     pb9
        FCALL   my_flags
        ANI     PF_KICK
        BNZ     pb1
        GETV    WBUMP                   ; no kick: only a dud
        ADI     LO(ST+OFF_MAP)
        PLO     RB
        LDN     RB
        XRI     T_DUD
        BNZ     pb9
pb1:    SETV    BUMPW, BUMP_GAP
        GETV    WBUMP
        PLO     R8
        GETV    WDIR
        PHI     R8
        LDI     ACT_BUMP
        FCALL   act_q
        FCALL   my_flags
        ANI     PF_KICK
        BZ      pb9
        SND     SND_KICK
pb9:    FRET

; my_flags: D = our P_FLAGS (we have a seat). RB -> our record + P_FLAGS.
        RFIT    16
my_flags:
        GETV    MYSEAT
        ADI     LO(PLOFF)
        PLO     R8
        LDI     HI(PLOFF)
        PHI     R8
        LDN     R8
        ADI     P_FLAGS
        PLO     RB
        LDN     RB
        FRET

; ---------------------------------------------------------------------------
; p_fire -- 5 pressed: standing on our bomb with the glove, throw it; else
;   on a floor, with a bomb to spare, drop one: drawn at once (PEND) until
;   the server's state has it.
; ---------------------------------------------------------------------------
        RFIT    221
p_fire: GETV    KNEW
        ANI     KB_FIRE
        BNZ     pf0
        FRET
pf0:    LDN     RA                      ; taken
        XRI     KB_FIRE
        STR     RA
        GETV    WX                      ; the tile we are on: centre_tile
        ADI     UNIT/2
        SHR
        SHR
        SHR
        SHR
        SMI     ARENA_W
        BNF     pf1
        LDI     0FFH                    ; (past the edge: the last column)
pf1:    ADI     ARENA_W
        PLO     R9
        GETV    WY
        ADI     UNIT/2
        SHR
        SHR
        SHR
        SHR
        SMI     ARENA_H
        BNF     pf2
        LDI     0FFH
pf2:    ADI     ARENA_H
        ADI     LO(TROW)
        PLO     R8
        LDI     HI(TROW)
        PHI     R8
        LDN     R8
        STR     R2
        GLO     R9
        ADD
        PUTV    T0                      ; T0: t
        ADI     LO(ST+OFF_MAP)
        PLO     RB
        LDN     RB
        PUTV    T1                      ; T1: its kind
        FCALL   my_flags                ; the glove, on a bomb (ours, pending too)?
        ANI     PF_GLOVE
        BZ      pf5
        GETV    T1
        XRI     T_BOMB
        BZ      pf3
        GETV    T0
        STR     R2
        GETV    PEND
        XOR
        BNZ     pf5
pf3:    GETV    WDIR                    ; throw it
        PLO     R8
        LDI     0
        PHI     R8
        LDI     ACT_THROW
        FCALL   act_q
        SETV    PEND, NONE
        SND     SND_THROW
        FRET
pf5:    GETV    T1                      ; drop: a floor, not ours already,
        BNZ     pf9                     ;  out + pending < bombs
        GETV    T0
        STR     R2
        GETV    PEND
        XOR
        BZ      pf9
        GETV    MYSEAT
        ADI     LO(PLOFF)
        PLO     R8
        LDI     HI(PLOFF)
        PHI     R8
        LDN     R8
        ADI     P_OUT
        PLO     RB
        LDN     RB                      ; out
        PLO     R8
        GETV    PEND
        XRI     NONE
        BZ      pf6
        INC     R8                      ; + 1 pending
pf6:    GLO     RB
        ADI     P_BOMBS-P_OUT
        PLO     RB
        GLO     R8
        STR     R2
        LDN     RB                      ; bombs - (out + pending)
        SM
        BZ      pf9
        BNF     pf9
        GETV    T0                      ; drop it: drawn now
        PLO     R8
        PUTV    PEND
        SETV    PENDL, PEND_WAIT
        LDI     0
        PHI     R8
        LDI     ACT_DROP
        FCALL   act_q
        GETV    T0
        FCALL   dq_push
        SND     SND_DROP
pf9:    FRET

; ---------------------------------------------------------------------------
; p_move -- our move sent when it changed (MOVE_EVERY jiffies apart at
;   most often), and every HEARTBEAT jiffies anyway.
; ---------------------------------------------------------------------------
        RFIT    107
p_move: GETV    WMOVED                  ; d = the way | moving
        BZ      pm1
        LDI     P_DIR_MOVING
pm1:    STR     R2
        GETV    WDIR
        OR
        PLO     R9
        GETV    SENTH                   ; the heartbeat: anyway
        BZ      pm3
        GETV    SENTW
        BNZ     pm9
        GETV    SENTX
        STR     R2
        GETV    WX
        XOR
        BNZ     pm3
        GETV    SENTY
        STR     R2
        GETV    WY
        XOR
        BNZ     pm3
        GETV    SENTD
        STR     R2
        GLO     R9
        XOR
        BZ      pm9
pm3:    GETV    WX
        PUTV    SENTX
        GETV    WY
        PUTV    SENTY
        GLO     R9
        PUTV    SENTD
        SETV    SENTW, MOVE_EVERY
        SETV    SENTH, HEARTBEAT
        GETV    TXF
        ORI     TX_MOVE
        STR     RA
pm9:    FRET

        IF      DEBUG
; p_trace: the jiffy's walk into the cart's TRACE page, 5 bytes an entry
; (the ways held, x, y, dir, acc), 50 entries round: tools/s2sim/walkref.c
; replays them through src/move.c.
        RFIT    95
p_trace:
        GETV    TRP
        PLO     R9
        POKESTP TRACE
        GETV    KHELD
        ANI     KB_DIRS
        PKD
        GETV    WX
        PKD
        GETV    WY
        PKD
        GETV    WDIR
        PKD
        GETV    WACC
        PKD
        PKEND
        GLO     R9
        ADI     5
        SMI     250
        BNF     ptr1
        LDI     0FFH-249
ptr1:   ADI     250
        PUTV    TRP
        FRET
        ENDIF

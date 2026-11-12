; p_play.inc -- the arena's frame (src/play.c play_frame; src/move.c, line
; for line: the server walks us with the same rules, internal/game/move.go).
;
; Each pass runs the jiffies the tick says went by (DT, at most 4): our
; walking, the others drawn toward where the server has them, the
; knocked-out falls, our bomb's wait; then once: a bump, FIRE, our move
; sent, and the sprites (their shadow, which the vblank writes, and their
; frames, which it copies into their user glyphs).

        RETC,UN                 ; E_ENTER (p_arena's)
        NOP
        NOP
        RETC,UN                 ; E_STATE (p_arena's)
        NOP
        NOP
        BCTA,UN PFRAME          ; E_TICK

PFRAME: LODA,R0 DT              ; the jiffies owed
        COMI,R0 1
        BCTR,EQ PF00
        LODA,R0 NLATE           ; (a pass that took more than a frame: counted)
        ADDI,R0 1
        BCTR,EQ PF00
        STRA,R0 NLATE
PF00:   LODA,R0 NPASL           ; (passes, for the rigs)
        ADDI,R0 1
        STRA,R0 NPASL
        PPSL    PSLWC
        LODA,R0 NPASH
        ADDI,R0 0
        STRA,R0 NPASH
        CPSL    PSLWC
        LODA,R0 DT
        COMI,R0 5
        BCTR,LT PF0
        LODI,R0 4
PF0:    STRA,R0 RCOUNT
        STRA,R0 PJIF            ; (the others' smoothing: once, for them all)
        STRZ    R1
        ADDZ    R1
        ADDZ    R1
        STRA,R0 TWMAX           ;  3 units a jiffy
        EORZ    R0
        STRA,R0 MOVED
        STRA,R0 PME             ; we walk: a seat, alive, in play
        LODA,R1 MYSEAT
        COMI,R1 NONE
        BCTR,EQ PFJ
        LODA,R0 ST+OFF_ALIVE
        ANDA,R0 SEATBIT,R1
        BCTR,EQ PFJ
        LODA,R0 ST+OFF_PHASE
        COMI,R0 PH_PLAY
        BCFR,EQ PFJ
        LODI,R0 1
        STRA,R0 PME
PFJ:    LODA,R0 PME             ; a jiffy
        BCTA,EQ PFJ2
        LODA,R0 INDIR           ; (the rigs' record of it, written after:
        STRA,R0 PDIN            ;  the disc, where it took us, its number last)
        LODA,R1 MYSEAT
        LODA,R0 SEATOFF,R1
        STRZ    R2
        LODA,R0 ST+OFF_PLAYERS+P_SPEED,R2
        STRA,R0 MSPEED
        BSTA,UN TRYDIRS
        COMI,R0 0
        BCTR,EQ PFJ1
        STRA,R0 MOVED
PFJ1:   BSTA,UN PBUMP
        LODA,R0 PDIN
        STRA,R0 PDBG+1
        LODA,R0 MVX
        STRA,R0 PDBG+2
        LODA,R0 MVY
        STRA,R0 PDBG+3
        LODA,R0 MVDIR
        STRA,R0 PDBG+4
        LODA,R0 MVACC
        STRA,R0 PDBG+5
        LODA,R0 PDBG
        ADDI,R0 1
        STRA,R0 PDBG
PFJ2:   LODA,R1 PEND            ; our bomb, never come: off again
        COMI,R1 NONE
        BCTR,EQ PFJ3
        LODA,R0 PENDL
        BCTR,EQ PFJ3
        SUBI,R0 1
        STRA,R0 PENDL
        BCFR,EQ PFJ3
        LODI,R0 NONE
        STRA,R0 PEND
        ZBSR    *ZTDRAW
PFJ3:   LODA,R0 RCOUNT
        SUBI,R0 1
        STRA,R0 RCOUNT
        BCFA,EQ PFJ
; once a pass
        BSTA,UN SEATSTEP
        LODA,R0 PME
        BCTR,EQ PF9
        BSTA,UN PFIRE
        BSTA,UN PMOVE
PF9:    LODA,R0 FRML            ; the arena's glyphs: a frame every 8 jiffies
        ADDI,R0 4
        RRR,R0
        RRR,R0
        RRR,R0
        ANDI,R0 1
        STRA,R0 ANWANT
        BCTA,UN SPRITES         ; tail call

; ---- move.c ------------------------------------------------------------------------
; TRYDIRS: the disc to a direction; on a diagonal, the turn first (the way
; we aren't going), and if that's walled, on as we were (play.c try_dirs).
; R0 = 1 if we moved. Calls three down. Clobbers R0-R3.
TRYDIRS: LODI,R1 NONE           ; R1 the vertical, R2 the horizontal
        LODI,R2 NONE
        LODA,R0 INDIR
        TMI,R0  D_UP
        BCFR,EQ TD1
        LODI,R1 DIR_UP
        BCTR,UN TD2
TD1:    TMI,R0  D_DOWN
        BCFR,EQ TD2
        LODI,R1 DIR_DOWN
TD2:    TMI,R0  D_LEFT
        BCFR,EQ TD3
        LODI,R2 DIR_LEFT
        BCTR,UN TD4
TD3:    TMI,R0  D_RIGHT
        BCFR,EQ TD4
        LODI,R2 DIR_RIGHT
TD4:    COMI,R1 NONE
        BCFR,EQ TD5
        LODZ    R2              ; across, or still
        BCTA,UN MVSTEP
TD5:    COMI,R2 NONE
        BCFR,EQ TD6
        LODZ    R1              ; up or down
        BCTA,UN MVSTEP
TD6:    LODA,R0 MVDIR           ; both: a = the turn, b = on
        COMI,R0 DIR_LEFT
        BCTR,EQ TD7
        COMI,R0 DIR_RIGHT
        BCTR,EQ TD7
        STRA,R2 TDA             ; going up or down: across first
        STRA,R1 TDB
        BCTR,UN TD8
TD7:    STRA,R1 TDA             ; going across: up or down first
        STRA,R2 TDB
TD8:    LODA,R0 MVX
        STRA,R0 TSX
        LODA,R0 MVY
        STRA,R0 TSY
        LODA,R0 MVACC
        STRA,R0 TSA
        LODA,R0 TDA
        BSTA,UN MVSTEP
        COMI,R0 0
        RETC,GT                 ; (R0 = 1)
        LODA,R0 TSX
        STRA,R0 MVX
        LODA,R0 TSY
        STRA,R0 MVY
        LODA,R0 TSA
        STRA,R0 MVACC
        LODA,R0 TDB
        BCTA,UN MVSTEP          ; tail call

; MVSTEP: one jiffy toward direction R0 (NONE: still) at speed MSPEED.
; R0 = 1 if it moved (mv_step). Calls two down. Clobbers R0-R3.
MVSTEP: STRA,R0 MSD
        LODI,R1 NONE
        STRA,R1 MVBUMP
        COMI,R0 NONE
        BCFR,EQ MS1
        EORZ    R0
        STRA,R0 MVACC
        RETC,UN
MS1:    STRA,R0 MVDIR
        LODA,R0 MSPEED          ; acc += 16 + 4 lv: acc >> 4 steps
        RRL,R0
        RRL,R0
        ADDI,R0 SPEED_BASE
        ADDA,R0 MVACC
        STRZ    R1
        ANDI,R0 15
        STRA,R0 MVACC
        LODZ    R1
        RRR,R0
        RRR,R0
        RRR,R0
        RRR,R0
        ANDI,R0 $0F
        STRA,R0 MSN
        EORZ    R0
        STRA,R0 MSMOV
MS2:    LODA,R0 MSN
        BCTR,EQ MS9
        SUBI,R0 1
        STRA,R0 MSN
        BSTA,UN USTEP
        COMI,R0 0
        BCFR,EQ MS3
        STRA,R0 MVACC           ; walled: no more
        BCTR,UN MS9
MS3:    LODI,R0 1
        STRA,R0 MSMOV
        BCTR,UN MS2
MS9:    LODA,R0 MSMOV
        RETC,UN

; USTEP: a unit toward MSD (unit_step): R0 = 1 if it went. Off a lane by
; up to LANE, it slides onto the lane toward an open tile. Calls two down.
; Clobbers R0-R3.
USTEP:  LODA,R0 MSD
        COMI,R0 DIR_LEFT
        BCTA,EQ USH
        COMI,R0 DIR_RIGHT
        BCTA,EQ USH
        LODI,R3 1               ; up or down: dy
        COMI,R0 DIR_DOWN
        BCTR,EQ USV1
        LODI,R3 $FF
USV1:   STRA,R3 UDD
        LODA,R0 MVX             ; c, r + dy
        BSTA,UN HI4
        STRZ    R1
        LODA,R0 MVY
        BSTA,UN HI4
        ADDA,R0 UDD
        STRZ    R2
        LODA,R0 MVX             ; off the column's lane?
        ANDI,R0 15
        BCTR,EQ USV4
        STRA,R0 UOFF
        COMI,R0 LANE+1
        BCFR,LT USV2
        BSTA,UN WALKABLE
        COMI,R0 0
        BCTR,EQ USV2
        LODA,R0 MVX
        SUBI,R0 1
        STRA,R0 MVX
        LODI,R0 1
        RETC,UN
USV2:   LODA,R0 UOFF
        COMI,R0 UNIT-LANE
        BCTR,LT USV3
        ADDI,R1 1
        BSTA,UN WALKABLE
        COMI,R0 0
        BCTR,EQ USV3
        LODA,R0 MVX
        ADDI,R0 1
        STRA,R0 MVX
        LODI,R0 1
        RETC,UN
USV3:   EORZ    R0
        RETC,UN
USV4:   LODA,R0 MVY             ; on the lane: a tile's edge, walled?
        ANDI,R0 15
        BCFR,EQ USV5
        BSTA,UN WALKABLE
        COMI,R0 0
        BCFR,EQ USV5
        BSTA,UN BUMPED
        EORZ    R0
        RETC,UN
USV5:   LODA,R0 MVY
        ADDA,R0 UDD
        STRA,R0 MVY
        LODI,R0 1
        RETC,UN
USH:    LODI,R3 1               ; left or right: dx
        COMI,R0 DIR_RIGHT
        BCTR,EQ USH1
        LODI,R3 $FF
USH1:   STRA,R3 UDD
        LODA,R0 MVX             ; c + dx, r
        BSTA,UN HI4
        ADDA,R0 UDD
        STRZ    R1
        LODA,R0 MVY
        BSTA,UN HI4
        STRZ    R2
        LODA,R0 MVY             ; off the row's lane?
        ANDI,R0 15
        BCTR,EQ USH4
        STRA,R0 UOFF
        COMI,R0 LANE+1
        BCFR,LT USH2
        BSTA,UN WALKABLE
        COMI,R0 0
        BCTR,EQ USH2
        LODA,R0 MVY
        SUBI,R0 1
        STRA,R0 MVY
        LODI,R0 1
        RETC,UN
USH2:   LODA,R0 UOFF
        COMI,R0 UNIT-LANE
        BCTA,LT USV3
        ADDI,R2 1
        BSTA,UN WALKABLE
        COMI,R0 0
        BCTA,EQ USV3
        LODA,R0 MVY
        ADDI,R0 1
        STRA,R0 MVY
        LODI,R0 1
        RETC,UN
USH4:   LODA,R0 MVX
        ANDI,R0 15
        BCFR,EQ USH5
        BSTA,UN WALKABLE
        COMI,R0 0
        BCFR,EQ USH5
        BSTA,UN BUMPED
        EORZ    R0
        RETC,UN
USH5:   LODA,R0 MVX
        ADDA,R0 UDD
        STRA,R0 MVX
        LODI,R0 1
        RETC,UN

; HI4: R0 >> 4. Leaf.
HI4:    RRR,R0
        RRR,R0
        RRR,R0
        RRR,R0
        ANDI,R0 $0F
        RETC,UN

; WALKABLE: R0 = 1 if tile (R1, R2) can be walked on: in the arena, not
; our pending bomb's, not a pillar, block, burning block, bomb or dud
; (walkable). Leaf. Clobbers R0, R3.
WALKABLE:
        COMI,R1 ARENA_W         ; (-1 is 255: out too)
        BCFR,LT WK0
        COMI,R2 ARENA_H
        BCFR,LT WK0
        LODA,R0 ROW13,R2
        ADDZ    R1
        COMA,R0 PEND
        BCTR,EQ WK0
        STRZ    R3
        LODA,R0 ST+OFF_MAP,R3
        COMI,R0 T_PILLAR
        BCTR,LT WK1
        COMI,R0 T_DUD
        BCFR,GT WK0
WK1:    LODI,R0 1
        RETC,UN
WK0:    EORZ    R0
        RETC,UN

; BUMPED: tile (R1, R2) a bomb or a dud: MVBUMP (bumped). Leaf. Clobbers R0, R3.
BUMPED: COMI,R1 ARENA_W
        RETC,GT
        RETC,EQ
        COMI,R2 ARENA_H
        RETC,GT
        RETC,EQ
        LODA,R0 ROW13,R2
        ADDZ    R1
        STRZ    R3
        LODA,R0 ST+OFF_MAP,R3
        COMI,R0 T_BOMB
        BCTR,EQ BU1
        COMI,R0 T_DUD
        RETC,LT
        RETC,GT
BU1:    STRA,R3 MVBUMP
        RETC,UN

; CTILE: R0 = the tile under our centre (centre_tile). Leaf. Clobbers R0, R1.
CTILE:  LODA,R0 MVX
        ADDI,R0 UNIT/2
        BSTA,UN HI4
        COMI,R0 ARENA_W
        BCTR,LT CT1
        LODI,R0 ARENA_W-1
CT1:    STRA,R0 PFT
        LODA,R0 MVY
        ADDI,R0 UNIT/2
        BSTA,UN HI4
        COMI,R0 ARENA_H
        BCTR,LT CT2
        LODI,R0 ARENA_H-1
CT2:    STRZ    R1
        LODA,R0 ROW13,R1
        ADDA,R0 PFT
        RETC,UN

; ---- play_frame's actions -----------------------------------------------------------
; PBUMP: bumped into a bomb, 30 frames since the last: with a kick (or a
; dud), BUMP it. Calls two down. Clobbers R0-R3.
PBUMP:  LODA,R1 MVBUMP
        COMI,R1 NONE
        RETC,EQ
        LODA,R1 BUMPTH
        LODA,R2 BUMPTL
        BSTA,UN SINCE
        COMI,R0 30
        RETC,LT
        LODA,R1 MYSEAT
        LODA,R0 SEATOFF,R1
        STRZ    R2
        LODA,R0 ST+OFF_PLAYERS+P_FLAGS,R2
        ANDI,R0 PF_KICK
        STRA,R0 PKK
        BCFR,EQ PB1
        LODA,R1 MVBUMP
        LODA,R0 ST+OFF_MAP,R1
        COMI,R0 T_DUD
        RETC,LT
        RETC,GT
PB1:    LODA,R0 FRMH
        STRA,R0 BUMPTH
        LODA,R0 FRML
        STRA,R0 BUMPTL
        LODI,R1 ACT_BUMP
        LODA,R2 MVBUMP
        LODA,R3 MVDIR
        ZBSR    *ZACTQ
        LODA,R0 PKK
        RETC,EQ
        SND     SND_KICK
        RETC,UN

; PFIRE: FIRE: with the glove, on a bomb (ours too), THROW it; else on
; the floor with a bomb to spare, DROP one -- shown at once (PEND) until
; the server's state has it. Calls two down. Clobbers R0-R3.
PFIRE:  LODA,R0 EVBTN
        RETC,EQ
        BSTA,UN CTILE
        STRA,R0 PFT
        LODA,R1 MYSEAT
        LODA,R0 SEATOFF,R1
        STRZ    R2              ; (R2: our record, all along)
        LODA,R0 ST+OFF_PLAYERS+P_FLAGS,R2
        ANDI,R0 PF_GLOVE
        BCTR,EQ PDROP
        LODA,R1 PFT
        LODA,R0 ST+OFF_MAP,R1
        COMI,R0 T_BOMB
        BCTR,EQ PTHROW
        COMA,R1 PEND
        BCFR,EQ PDROP
PTHROW: LODI,R1 ACT_THROW
        LODA,R2 MVDIR
        LODI,R3 0
        ZBSR    *ZACTQ
        SND     SND_THROW
        LODI,R0 NONE
        STRA,R0 PEND
        RETC,UN
PDROP:  LODA,R1 PFT
        LODA,R0 ST+OFF_MAP,R1
        RETC,GT                 ; (not the floor)
        RETC,LT
        COMA,R1 PEND
        RETC,EQ
        LODA,R0 ST+OFF_PLAYERS+P_OUT,R2         ; out + pending < bombs
        LODA,R3 PEND
        COMI,R3 NONE
        BCTR,EQ PD1
        ADDI,R0 1
PD1:    COMA,R0 ST+OFF_PLAYERS+P_BOMBS,R2
        RETC,GT
        RETC,EQ
        LODI,R1 ACT_DROP
        LODA,R2 PFT
        LODI,R3 0
        ZBSR    *ZACTQ
        LODA,R1 PFT
        STRA,R1 PEND
        LODI,R0 90
        STRA,R0 PENDL
        ZBSR    *ZTDRAW
        SND     SND_DROP
        RETC,UN

; PMOVE: our move sent, when it changed (at most every MOVE_EVERY jiffies),
; or every 60 anyway. Calls one down. Clobbers R0-R3.
PMOVE:  LODA,R0 MVDIR
        LODA,R1 MOVED
        BCTR,EQ PM1
        IORI,R0 P_DIR_MOVING
PM1:    STRA,R0 PMD
        LODA,R1 SENTTH
        LODA,R2 SENTTL
        BSTA,UN SINCE
        COMI,R0 60
        BCFR,LT PM3
        COMI,R0 MOVE_EVERY
        RETC,LT
        LODA,R0 MVX
        COMA,R0 SENTX
        BCFR,EQ PM3
        LODA,R0 MVY
        COMA,R0 SENTY
        BCFR,EQ PM3
        LODA,R0 PMD
        COMA,R0 SENTD
        RETC,EQ
PM3:    LODA,R0 MVX
        STRA,R0 SENTX
        LODA,R0 MVY
        STRA,R0 SENTY
        LODA,R0 PMD
        STRA,R0 SENTD
        LODA,R0 FRMH
        STRA,R0 SENTTH
        LODA,R0 FRML
        STRA,R0 SENTTL
        LODA,R0 TXF
        IORI,R0 TX_MOVE
        STRA,R0 TXF
        RETC,UN

; SINCE: R0 = frames since the frame R1:R2 (255 if more). Leaf.
SINCE:  LODA,R0 FRML
        SUBZ    R2              ; (C: no borrow)
        STRZ    R2
        PPSL    PSLWC
        LODA,R0 FRMH
        SUBZ    R1
        CPSL    PSLWC
        BCFR,EQ SIN1
        LODZ    R2
        RETC,UN
SIN1:   LODI,R0 255
        RETC,UN

; ---- the bombers ---------------------------------------------------------------------
; SEATSTEP: the pass's jiffies (PJIF) of the others: each drawn toward
; where the server has it (3 units a jiffy: TWMAX; more than a tile off,
; there); a knocked-out one's fall counted. Calls one down. Clobbers R0-R3.
SEATSTEP:
        LODI,R3 SEATS-1
SS1:    LODA,R0 ST+OFF_INPLAY
        ANDA,R0 SEATBIT,R3
        BCTA,EQ SS9
        LODA,R0 ST+OFF_ALIVE
        ANDA,R0 SEATBIT,R3
        BCFR,EQ SS3
        LODA,R0 DIET,R3         ; out: its fall
        BCTR,EQ SS9
        COMI,R0 48
        BCFR,LT SS9
        ADDA,R0 PJIF
        COMI,R0 48
        BCTR,LT SS2
        LODI,R0 48
SS2:    STRA,R0 DIET,R3
        BCTR,UN SS9
SS3:    COMA,R3 MYSEAT          ; ours, walking: drawn where we are
        BCFR,EQ SS4
        LODA,R0 PME
        BCFR,EQ SS9
SS4:    LODA,R0 SEATOFF,R3
        STRZ    R2
        LODA,R0 ST+OFF_PLAYERS+P_X,R2
        STRZ    R1
        LODA,R0 DISPX,R3
        BSTR,UN TOWARD
        STRA,R0 DISPX,R3
        LODA,R0 ST+OFF_PLAYERS+P_Y,R2
        STRZ    R1
        LODA,R0 DISPY,R3
        BSTR,UN TOWARD
        STRA,R0 DISPY,R3
SS9:    SUBI,R3 1
        BCFA,LT SS1
        RETC,UN

; TOWARD: R0 (where it is drawn) moved toward R1 (where it is): TWMAX
; units at most; more than a tile off, there (play.c toward). Leaf.
; Clobbers R0, R1.
TOWARD: COMZ    R1
        RETC,EQ
        STRA,R0 TWA
        BCTR,GT TW2
        LODZ    R1              ; behind: d = to - at
        SUBA,R0 TWA
        COMI,R0 UNIT+1
        BCFR,LT TWTO
        COMA,R0 TWMAX
        BCFR,GT TW1
        LODA,R0 TWMAX
TW1:    ADDA,R0 TWA
        RETC,UN
TW2:    SUBZ    R1              ; ahead: d = at - to
        COMI,R0 UNIT+1
        BCFR,LT TWTO
        COMA,R0 TWMAX
        BCFR,GT TW3
        LODA,R0 TWMAX
TW3:    STRA,R0 TWD
        LODA,R0 TWA
        SUBA,R0 TWD
        RETC,UN
TWTO:   LODZ    R1
        RETC,UN

; SPRITES: each seat's sprite: where it is drawn (Y 256 - line, X the
; pixel + 43; a unit is a line down, half a pixel across) and its frame
; (play.c's spr_arena calls); hidden when out of play. Calls one down.
; Clobbers R0-R3, T0.
SPRITES:
        LODI,R3 SEATS-1
SP1:    STRA,R3 T0
        LODA,R0 ST+OFF_INPLAY
        ANDA,R0 SEATBIT,R3
        BCTA,EQ SPHIDE
        LODA,R0 ST+OFF_ALIVE
        ANDA,R0 SEATBIT,R3
        BCFR,EQ SP3
        LODA,R0 DIET,R3         ; out: its fall, 4 frames of 12 jiffies
        BCTA,EQ SPHIDE
        COMI,R0 48
        BCFA,LT SPHIDE
        LODI,R1 FR_DIE0-1
SP2:    ADDI,R1 1
        SUBI,R0 12
        BCFR,LT SP2
        LODZ    R1
        BCTR,UN SP6
SP3:    COMA,R3 MYSEAT          ; ours, walking: where we are
        BCFR,EQ SP4
        LODA,R0 PME
        BCTR,EQ SP4
        LODA,R0 MVX
        STRA,R0 DISPX,R3
        LODA,R0 MVY
        STRA,R0 DISPY,R3
        LODA,R1 MVDIR
        LODA,R2 MOVED
        BCTR,UN SP5
SP4:    LODA,R0 SEATOFF,R3      ; another: the way the server has it facing
        STRZ    R2
        LODA,R0 ST+OFF_PLAYERS+P_DIR,R2
        STRZ    R1
        ANDI,R0 P_DIR_MOVING
        STRZ    R2
SP5:    BSTR,UN FRAMEOF
SP6:    LODA,R3 T0
        STRA,R0 FRWANT,R3
        LODZ    R3              ; its registers: 2 s
        ADDZ    R3
        STRZ    R2
        LODI,R0 SPRY0
        SUBA,R0 DISPY,R3
        STRA,R0 SPRSH,R2
        LODA,R0 DISPX,R3
        RRR,R0
        ANDI,R0 $7F
        ADDI,R0 SPRX0
        STRA,R0 SPRSH+1,R2
        BCTR,UN SP9
SPHIDE: LODZ    R3
        ADDZ    R3
        STRZ    R2
        EORZ    R0
        STRA,R0 SPRSH,R2
SP9:    LODA,R3 T0
        SUBI,R3 1
        BCFA,LT SP1
        RETC,UN

; FRAMEOF: R0 = the frame facing R1 (DIR_*), walking if R2 (frame_of:
; the steps 0, 1, 0, 2, 8 jiffies each). Leaf. Clobbers R0, R1.
FRAMEOF: LODZ   R1
        ANDI,R0 3
        STRZ    R1
        LODA,R0 FRBASE,R1
        COMI,R2 0
        RETC,EQ
        STRA,R0 TFR
        LODA,R0 FRML
        RRR,R0
        RRR,R0
        RRR,R0
        ANDI,R0 3
        STRZ    R1
        LODA,R0 WALKF,R1
        ADDA,R0 TFR
        RETC,UN

FRBASE: DB      FR_UP0,FR_RIGHT0,FR_DOWN0,FR_LEFT0
WALKF:  DB      0,1,0,2
ROW13:  DB      0,13,26,39,52,65,78,91,104,117,130

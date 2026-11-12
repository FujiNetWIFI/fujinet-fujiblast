; p_arena.inc -- the arena, as a new state draws it (src/play.c play_enter,
; play_state, hud, stats_draw). Its frame (walking, bombs, the bombers) is
; p_play.inc's.
;
; The screen: row 0 the HUD (each seat's initial and wins, the clock),
; rows 1-11 the arena (tile (c, r) is cell (ACOL + c, AROW + r)) between
; walls in columns 0 and 14, row 12 the wall under it; column 15 our
; powers. A tile is one cell (equ.inc's user glyphs, ROM glyphs and
; letters: TCELLS); the screen itself is what is shown: a row the patches
; touched (the link's MAPDL/MAPDH) is compared cell by cell and redrawn.

        BCTA,UN AENTER          ; E_ENTER
        BCTA,UN ASTATE          ; E_STATE
        RETC,UN                 ; E_TICK (p_play's)

; ---- play_enter --------------------------------------------------------------
AENTER: ZBSR    *ZSCRCLR        ; cleared, the sprites off
        LODI,R0 A_BRICK>>8      ; the arena's glyphs: the brick now, the rest
        LODI,R1 A_BRICK&$FF     ;  animated from the next vblank
        LODI,R2 GBRICK-GUDC
        ZBSR    *ZUDCSET
        EORZ    R0
        STRA,R0 ANWANT
        LODI,R0 $FD
        STRA,R0 ANSHOW
        LODI,R1 0               ; the seats' colours
        LODI,R2 SPRC0
        ZBSR    *ZSPRCOL
        LODI,R1 1
        LODI,R2 SPRC1
        ZBSR    *ZSPRCOL
        LODI,R1 2
        LODI,R2 SPRC2
        ZBSR    *ZSPRCOL
        LODI,R1 3
        LODI,R2 SPRC3
        ZBSR    *ZSPRCOL
        LODI,R2 16*AROW         ; the walls beside it
AEN1:   LODI,R0 WALLC
        STRA,R0 SCR+ACOL-1,R2
        STRA,R0 SCR+ACOL+ARENA_W,R2
        ADDI,R2 16
        COMI,R2 16*(AROW+ARENA_H)
        BCFR,EQ AEN1
        BSTA,UN WALL12          ; and under it
        DCOL    TXTI            ; the powers' labels
        DAT     1,SCOL
        LODI,R0 'F'
        ZBSR    *ZDPUTC
        DAT     3,SCOL
        LODI,R0 'B'
        ZBSR    *ZDPUTC
        DAT     5,SCOL
        LODI,R0 'S'
        ZBSR    *ZDPUTC
        LODI,R0 NONE            ; nothing ours or moving shown
        STRA,R0 PEND
        STRA,R0 HUDTMR
        LODI,R3 4
AEN2:   LODI,R0 NONE
        STRA,R0 MOVT-1,R3
        EORZ    R0
        STRA,R0 DIET-1,R3
        LODA,R0 SEATOFF-1,R3    ; each drawn where the server has it
        STRZ    R2
        LODA,R0 ST+OFF_PLAYERS+P_X,R2
        STRA,R0 DISPX-1,R3
        LODA,R0 ST+OFF_PLAYERS+P_Y,R2
        STRA,R0 DISPY-1,R3
        BDRR,R3 AEN2
        LODA,R0 ST+OFF_ALIVE
        STRA,R0 WASALV
        LODA,R0 ST+OFF_PHASE
        STRA,R0 LASTPH
        LODA,R1 MYSEAT          ; we walk from where it has us
        COMI,R1 NONE
        BCTR,EQ AEN3
        LODA,R0 SEATOFF,R1
        STRZ    R2
        LODA,R0 ST+OFF_PLAYERS+P_WARP,R2
        STRA,R0 MYWARP
        LODA,R0 ST+OFF_PLAYERS+P_X,R2
        STRA,R0 MVX
        LODA,R0 ST+OFF_PLAYERS+P_Y,R2
        STRA,R0 MVY
        LODI,R0 DIR_DOWN
        STRA,R0 MVDIR
        EORZ    R0
        STRA,R0 MVACC
AEN3:   LODI,R0 1               ; our powers, all drawn (no sound)
        BSTA,UN STATDRAW
        LODI,R0 $FF             ; every row
        STRA,R0 MAPDL
        STRA,R0 MAPDH
        LODI,R0 MF_MAP
        STRA,R0 MFLAGS
        BSTR,UN ASTATE
        LODA,R0 ST+OFF_PHASE
        COMI,R0 PH_COUNTDOWN
        RETC,LT
        RETC,GT
        DAT     12,4            ; "ROUND n" in the wall under it
        DCOL    TXTI
        TXTAT   SROUND
        LODA,R0 ST+OFF_ROUND
        LODI,R1 1
        ZBSR    *ZDNUM
        EORZ    R0
        ZBSR    *ZDPUTC
        SND     MUS_ROUND
        RETC,UN
SROUND: DB      " ROUND ",$FF

; WALL12: the wall under the arena. Clobbers R0-R3.
WALL12: DAT     12,0
        LODI,R0 WALLC
        LODI,R3 ACOL+ARENA_W+1
        ZBRR    *ZDFILL         ; tail call

; ---- play_state ------------------------------------------------------------------
ASTATE: LODA,R1 MYSEAT          ; our own position, when the server says so
        COMI,R1 NONE
        BCTR,EQ AS1
        LODA,R0 SEATOFF,R1
        STRZ    R2
        LODA,R0 ST+OFF_PLAYERS+P_WARP,R2
        COMA,R0 MYWARP
        BCTR,EQ AS1
        STRA,R0 MYWARP
        LODA,R0 ST+OFF_PLAYERS+P_X,R2
        STRA,R0 MVX
        LODA,R0 ST+OFF_PLAYERS+P_Y,R2
        STRA,R0 MVY
        EORZ    R0
        STRA,R0 MVACC
AS1:    LODA,R1 PEND            ; our bomb: the server has it (or the tile is taken)
        COMI,R1 NONE
        BCTR,EQ AS2
        LODA,R0 ST+OFF_MAP,R1
        BCTR,EQ AS2
        LODI,R0 NONE
        STRA,R0 PEND
AS2:    LODI,R3 MOVERS          ; the movers shown: their rows redrawn
AS3:    LODA,R0 MOVT-1,R3
        COMI,R0 NONE
        BCTR,EQ AS4
        STRA,R3 T0
        BSTA,UN MARKROW
        LODA,R3 T0
        LODI,R0 NONE
        STRA,R0 MOVT-1,R3
AS4:    BDRR,R3 AS3
; the rows touched, cell by cell
        LODA,R0 MAPDL
        IORA,R0 MAPDH
        BCTA,EQ AR5
        LODI,R3 0
AR0:    STRA,R3 T0
        COMI,R3 8
        BCFR,LT AR0H
        LODA,R0 MAPDL
        ANDA,R0 BITS,R3
        BCTR,EQ AR4
        BCTR,UN AR0D
AR0H:   LODA,R0 MAPDH
        ANDA,R0 BITS-8,R3
        BCTR,EQ AR4
AR0D:   LODA,R0 ROW13,R3        ; its first tile
        STRZ    R1
        LODZ    R3              ; and the cell before its first
        ADDI,R0 AROW
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        ADDI,R0 ACOL-1
        STRZ    R2
        LODI,R0 ARENA_W
        STRA,R0 T1
AR1:    LODA,R0 ST+OFF_MAP,R1
        COMA,R1 PEND
        BCFR,EQ AR2
        LODI,R0 T_BOMB
AR2:    STRZ    R3
        LODA,R0 TCELLS,R3
        COMA,R0 SCR,R2,+
        BCTR,EQ AR3
        STRA,R0 SCR,R2
AR3:    ADDI,R1 1
        LODA,R0 T1
        SUBI,R0 1
        STRA,R0 T1
        BCFR,EQ AR1
AR4:    LODA,R3 T0
        ADDI,R3 1
        COMI,R3 ARENA_H
        BCFA,EQ AR0
        EORZ    R0
        STRA,R0 MAPDL
        STRA,R0 MAPDH
AR5:    LODA,R0 MFLAGS          ; a blast, a dud
        STRZ    R1
        EORZ    R0
        STRA,R0 MFLAGS
        LODZ    R1
        TMI,R0  MF_BOOM
        BCFR,EQ AS5
        SND     SND_BOOM
        BCTR,UN AS6
AS5:    LODZ    R1
        TMI,R0  MF_FIZZ
        BCFR,EQ AS6
        SND     SND_FIZZLE
; the movers: a bomb (or a dud) on the tile under each one's centre
AS6:    LODI,R3 0
AM1:    STRA,R3 T0
        LODA,R0 MOVOFF,R3
        STRZ    R2
        LODA,R0 ST+OFF_MOVERS,R2
        ANDI,R0 3
        BCTA,EQ AM3
        LODA,R0 ST+OFF_MOVERS+1,R2      ; its column, (x + 8) >> 4
        ADDI,R0 UNIT/2
        RRR,R0
        RRR,R0
        RRR,R0
        RRR,R0
        ANDI,R0 $0F
        COMI,R0 ARENA_W
        BCTR,LT AM1A
        LODI,R0 ARENA_W-1
AM1A:   STRA,R0 T1
        LODA,R0 ST+OFF_MOVERS+2,R2      ; its row
        ADDI,R0 UNIT/2
        RRR,R0
        RRR,R0
        RRR,R0
        RRR,R0
        ANDI,R0 $0F
        COMI,R0 ARENA_H
        BCTR,LT AM1B
        LODI,R0 ARENA_H-1
AM1B:   STRZ    R1
        LODA,R0 ROW13,R1
        ADDA,R0 T1
        STRA,R0 MOVT,R3
        STRZ    R1
        LODA,R0 ST+OFF_MOVERS,R2
        ANDI,R0 M_DUD
        BCFR,EQ AM2A
        LODI,R0 TC_BOMB
        BCTR,UN AM2
AM2A:   LODI,R0 TC_DUD
AM2:    ZBSR    *ZTCELL
AM3:    LODA,R3 T0
        ADDI,R3 1
        COMI,R3 MOVERS
        BCTA,LT AM1
; a seat knocked out in play: its fall, a sound
        LODI,R3 SEATS-1
AD1:    LODA,R0 WASALV
        ANDA,R0 SEATBIT,R3
        BCTR,EQ AD2
        LODA,R0 ST+OFF_ALIVE
        ANDA,R0 SEATBIT,R3
        BCFR,EQ AD2
        LODA,R0 ST+OFF_PHASE
        COMI,R0 PH_PLAY
        BCFR,EQ AD2
        LODI,R0 1
        STRA,R0 DIET,R3
        SND     SND_DIE
AD2:    SUBI,R3 1
        BCFR,LT AD1
; the countdown's beeps, then GO
        LODA,R0 ST+OFF_PHASE
        COMI,R0 PH_COUNTDOWN
        BCFR,EQ AB1
        LODA,R0 ST+OFF_TIMER
        COMA,R0 HUDTMR
        BCTR,EQ AB2
        SND     SND_BEEP
        BCTR,UN AB2
AB1:    COMI,R0 PH_PLAY
        BCFR,EQ AB2
        LODA,R0 LASTPH
        COMI,R0 PH_COUNTDOWN
        BCFR,EQ AB2
        SND     SND_GO
        BSTA,UN WALL12          ; (its "ROUND n" off)
AB2:    LODA,R0 ST+OFF_PHASE
        STRA,R0 LASTPH
; the HUD, when it has changed
        LODA,R0 ST+OFF_TIMER
        COMA,R0 HUDTMR
        BCFR,EQ AH1
        LODA,R0 ST+OFF_ALIVE
        COMA,R0 HUDALV
        BCFR,EQ AH1
        LODI,R3 SEATS-1
AH0:    LODA,R0 SEATOFF,R3
        STRZ    R2
        LODA,R0 ST+OFF_PLAYERS+P_WINS,R2
        COMA,R0 HUDWIN,R3
        BCFR,EQ AH1
        SUBI,R3 1
        BCFR,LT AH0
        BCTR,UN AH2
AH1:    BSTA,UN HUD
AH2:    LODA,R0 ST+OFF_ALIVE
        STRA,R0 WASALV
        EORZ    R0              ; our powers, those that changed
        BCTA,UN STATDRAW        ; tail call

; MARKROW: tile R0's row to be redrawn (MAPDL/MAPDH). Clobbers R0-R2.
MARKROW: LODI,R2 0
MKR1:   COMI,R0 ARENA_W
        BCTR,LT MKR2
        SUBI,R0 ARENA_W
        ADDI,R2 1
        BCTR,UN MKR1
MKR2:   COMI,R2 8
        BCFR,LT MKR3
        LODA,R0 MAPDL
        IORA,R0 BITS,R2
        STRA,R0 MAPDL
        RETC,UN
MKR3:   LODA,R0 MAPDH
        IORA,R0 BITS-8,R2
        STRA,R0 MAPDH
        RETC,UN

; ---- hud ------------------------------------------------------------------------
; HUD: row 0: each seat in play, its initial (a bar if it is out) and its
; wins' last digit; the clock (m.ss; the countdown's second). Clobbers
; R0-R3, T0.
HUD:    LODI,R3 SEATS-1
HU1:    STRA,R3 T0
        LODA,R0 HUDCOL,R3
        STRZ    R2
        LODA,R0 ST+OFF_INPLAY
        ANDA,R0 SEATBIT,R3
        BCFR,EQ HU2
        EORZ    R0
        STRA,R0 SCR,R2
        STRA,R0 SCR+1,R2
        BCTR,UN HU5
HU2:    LODA,R0 ST+OFF_ALIVE
        ANDA,R0 SEATBIT,R3
        BCFR,EQ HU3
        LODA,R0 ST+OFF_PHASE
        COMI,R0 PH_PLAY
        BCFR,EQ HU3
        LODI,R0 GBOTBAR         ; out: a bar
        BCTR,UN HU4
HU3:    LODA,R0 NAMEOFF,R3      ; its initial
        STRZ    R1
        LODA,R0 ST+OFF_NAMES,R1
        BSTA,UN A2G
HU4:    IORA,R0 HUDINK,R3
        STRA,R0 SCR,R2
        LODA,R0 SEATOFF,R3      ; its wins
        STRZ    R1
        LODA,R0 ST+OFF_PLAYERS+P_WINS,R1
        STRA,R0 HUDWIN,R3
HU4A:   COMI,R0 10
        BCTR,LT HU4B
        SUBI,R0 10
        BCTR,UN HU4A
HU4B:   IORI,R0 GDIGIT
        IORA,R0 HUDINK,R3
        STRA,R0 SCR+1,R2
HU5:    LODA,R3 T0
        SUBI,R3 1
        BCFA,LT HU1
        LODA,R0 ST+OFF_ALIVE
        STRA,R0 HUDALV
        DCOL    TXTI            ; the clock
        DAT     0,6
        LODA,R0 ST+OFF_TIMER
        STRA,R0 HUDTMR
        LODA,R1 ST+OFF_PHASE
        COMI,R1 PH_COUNTDOWN
        BCFR,EQ HC1
        EORZ    R0              ; the countdown: its second, at the right
        ZBSR    *ZDPUTC
        LODA,R0 ST+OFF_TIMER
        LODI,R1 3
        ZBRR    *ZDNUM          ; tail call
HC1:    LODI,R1 0               ; m.ss: minutes in R1, seconds in R0
HC2:    COMI,R0 60
        BCTR,LT HC3
        SUBI,R0 60
        ADDI,R1 1
        BCTR,UN HC2
HC3:    STRA,R0 T1
        LODZ    R1
        IORI,R0 GDIGIT
        ZBSR    *ZDPUTC
        LODI,R0 GDOT
        ZBSR    *ZDPUTC
        LODA,R0 T1
        LODI,R1 0
HC4:    COMI,R0 10
        BCTR,LT HC5
        SUBI,R0 10
        ADDI,R1 1
        BCTR,UN HC4
HC5:    STRA,R0 T1
        LODZ    R1
        IORI,R0 GDIGIT
        ZBSR    *ZDPUTC
        LODA,R0 T1
        IORI,R0 GDIGIT
        ZBRR    *ZDPUTC         ; tail call

; A2G: R0 an ASCII letter or digit to its glyph (anything else a space).
; Leaf.
A2G:    COMI,R0 $61             ; (lower case up)
        BCTR,LT A2G0
        SUBI,R0 $20
A2G0:   COMI,R0 $41
        BCTR,LT A2G1
        COMI,R0 $5A
        BCTR,GT A2G9
        SUBI,R0 $41-GALPHA
        RETC,UN
A2G1:   COMI,R0 $30
        BCTR,LT A2G9
        COMI,R0 $39
        BCTR,GT A2G9
        SUBI,R0 $30-GDIGIT
        RETC,UN
A2G9:   EORZ    R0
        RETC,UN

; ---- stats_draw ------------------------------------------------------------------
; STATDRAW: our powers in column 15 -- range, bombs, speed (digits under
; F, B, S), kick, glove (K, G lit or not) -- those that changed (R0 = 1:
; all, and no sound; one that grew is a pickup's). Clobbers R0-R3, T0-T2.
STATDRAW:
        STRA,R0 T2
        LODI,R3 0
SD1:    STRA,R3 T0
        LODA,R0 MYSEAT          ; watching: all 0
        COMI,R0 NONE
        BCTR,EQ SD2
        STRZ    R1
        LODA,R0 SEATOFF,R1
        ADDA,R0 STFLD,R3
        STRZ    R1
        LODA,R0 ST+OFF_PLAYERS,R1
        ADDA,R0 STADD,R3
        ANDA,R0 STMASK,R3
        BCTR,UN SD3
SD2:    EORZ    R0
SD3:    STRA,R0 T1              ; the value
        LODA,R1 T2
        BCFR,EQ SD5             ; all: drawn
        COMA,R0 STATS,R3
        BCTA,EQ SD9
        BCTR,LT SD5             ; (less: quietly)
        SND     SND_PICKUP
        LODA,R3 T0
SD5:    LODA,R0 T1
        STRA,R0 STATS,R3
        LODA,R0 STROW,R3        ; its cell: the row's, column 15
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        ADDI,R0 SCOL
        STRZ    R2
        COMI,R3 3
        BCFR,LT SD6
        LODA,R0 T1              ; a number: its last digit
SD5A:   COMI,R0 10
        BCTR,LT SD5B
        SUBI,R0 10
        BCTR,UN SD5A
SD5B:   IORI,R0 GDIGIT|STATI1
        BCTR,UN SD8
SD6:    LODA,R0 STLET,R3        ; a power: its letter, lit or not
        LODA,R1 T1
        BCTR,EQ SD7
        IORI,R0 STATI1
        BCTR,UN SD8
SD7:    IORI,R0 STATI0
SD8:    STRA,R0 SCR,R2
SD9:    LODA,R3 T0
        ADDI,R3 1
        COMI,R3 5
        BCTA,LT SD1
        RETC,UN

STFLD:  DB      P_RANGE,P_BOMBS,P_SPEED,P_FLAGS,P_FLAGS
STADD:  DB      0,0,1,0,0
STMASK: DB      $FF,$FF,$FF,PF_KICK,PF_GLOVE
STROW:  DB      2,4,6,7,9
STLET:  DB      0,0,0,'K','G'

BITS:   DB      1,2,4,8,$10,$20,$40,$80
ROW13:  DB      0,13,26,39,52,65,78,91,104,117,130
MOVOFF: DB      0,MOVER_SIZE,2*MOVER_SIZE,3*MOVER_SIZE
HUDCOL: DB      0,3,11,14
HUDINK: DB      HUDI0,HUDI1,HUDI2,HUDI3
NAMEOFF: DB     0,NAME_LEN,2*NAME_LEN,3*NAME_LEN

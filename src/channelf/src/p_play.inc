; ---------------------------------------------------------------------------
; p_play.inc -- the arena's pass (src/play.c play_frame; src/move.c line for
; line: the server walks us with the same rules, internal/game/move.go).
; The Astrocade's (src/astrocade/src/p_play.inc), transcribed. In page
; ARENA, with p_arena.inc.
;
; Each pass runs the jiffies the clock says went by (DT, at most 8): our
; walking, our bomb's wait; then once: the others drawn toward where the
; server has them, a bump, FIRE, our move sent, and the bombers and movers
; handed to the slots (SLSET: the engine draws what changed).
; ---------------------------------------------------------------------------

MOVE_EVERY EQU 4                ; jiffies between moves sent (and 60 anyway)
PEND_WAIT  EQU 90               ; jiffies our bomb shows before the server has it
FR_WIN0    EQU 12
FR_DIE0    EQU 14
FR_UP0     EQU 3
FR_RIGHT0  EQU 6
FR_DOWN0   EQU 0
FR_LEFT0   EQU 9

; the play's variables (VARS+300H on)
MVX     EQU VARS+300H           ; our walker (move.c): position, direction,
MVY     EQU VARS+301H           ;  speed accumulator, the bomb it bumped
MVDIR   EQU VARS+302H
MVACC   EQU VARS+303H
MVBUMP  EQU VARS+304H
MSPEED  EQU VARS+305H           ; our speed level this jiffy
MSD     EQU VARS+306H           ; mv_step's direction
MSN     EQU VARS+307H           ;  its unit steps left
MSMOV   EQU VARS+308H           ;  it moved
UDD     EQU VARS+309H           ; unit_step's +1/-1
UOFF    EQU VARS+30AH           ;  and its off-lane
TDA     EQU VARS+30BH           ; try_dirs: the turn, the way on, and where
TDB     EQU VARS+30CH           ;  we were before trying the turn
TSX     EQU VARS+30DH
TSY     EQU VARS+30EH
TSA     EQU VARS+30FH
MOVED   EQU VARS+310H           ; we moved this pass
PME     EQU VARS+311H           ; we walk: seated, alive, in play
PEND    EQU VARS+312H           ; our bomb, shown before the server has it (FF none)
PENDL   EQU VARS+313H           ;  its jiffies left
BUMPT   EQU VARS+314H           ; 2: the jiffy of our last bump
SENTT   EQU VARS+316H           ; 2: and of our last move sent
SEATR   EQU VARS+318H           ; 4 x 3: each seat as drawn: x, y (units),
SR_X    EQU 0                   ;  and its fall's jiffies (0: none)
SR_Y    EQU 1
SR_DIE  EQU 2
WASALV  EQU VARS+324H           ; the alive mask at the last state
LASTPH  EQU VARS+325H           ; the phase at the last state
PJIF    EQU VARS+326H           ; this pass's jiffies
T3      EQU VARS+327H           ;  x 3: the others' step
PFT     EQU VARS+328H           ; FIRE's tile
PMD     EQU VARS+329H           ; the move's dir | moving
PCNT    EQU VARS+32AH           ; the jiffy loop's count
FLASH   EQU VARS+32BH           ; a blast's flash: jiffies left
FLROW   EQU VARS+32CH           ;  and its tile row
SEAMC   EQU VARS+32DH           ; the seams' palette now (the last 10 s pulse)
PSEAT   EQU VARS+32EH           ; a seat's loop
PBIT    EQU VARS+32FH           ;  and its bit
PDBG    EQU VARS+330H           ; 6: the rigs' record of our last jiffy (count,
                                ;  stick, x, y, dir, acc)


; ---------------------------------------------------------------------------
; PFRAME -- the arena's pass (ATICK calls it before the drawing).
; ---------------------------------------------------------------------------
PFRAME: ENTER
        LDB DT
        STB PCNT
        STB PJIF
        LR 0,A                  ; the others: 3 units a jiffy
        SL 1
        AS 0
        STB T3
        CLR
        STB MOVED
        STB PME
        LDB MYSEAT              ; we walk: a seat, alive, in play
        CI 3
        BNC PFJ
        LR 0,A
        DCI SEATBIT
        ADC
        LM
        LR 1,A
        LDB ST+OFF_ALIVE
        NS 1
        BZ PFJ
        LDB ST+OFF_PHASE
        CI PH_PLAY
        BNZ PFJ
        LIS 1
        STB PME
PFJ:    LDB PME                 ; a jiffy
        NI 0FFH
        BZ PFJ2
        LDB MYSEAT              ; our speed
        LR 0,A
        PI PLREC
        LIS P_SPEED
        ADC
        LM
        STB MSPEED
        PI TRYDIRS
        LR A,0
        NI 0FFH
        BZ PFJ1
        STB MOVED
PFJ1:   PI PBUMP
        DCI PDBG                ; (the rigs' record: count, stick, x, y, dir, acc)
        LM
        INC
        DCI PDBG
        ST
        LDB INPDIR
        STB PDBG+1
        DCI MVX
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        LR 2,A
        LM
        LR 3,A
        DCI PDBG+2
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
PFJ2:   LDB PEND                ; our bomb, never come: off again
        CI 0FFH
        BZ PFJ3
        LR 0,A
        LDB PENDL
        NI 0FFH
        BZ PFJ3
        AI -1
        STB PENDL
        BNZ PFJ3
        LI 0FFH
        STB PEND
        PI DMARK
PFJ3:   DECB PCNT
        BZ PFJ4
        JMP PFJ
PFJ4:   LDB PME                 ; once a pass
        NI 0FFH
        BZ PF9
        PI PFIRE
        PI PMOVE
PF9:    PI PSEATS
        PI PMOVERS
        LEAVE

; PLREC -- DC0 = seat A's record in ST.  (A: PI destroys it: in r0.)
;   Leaf.  Clobbers A, r0 (r0 the seat in), DC0.
PLREC:  LR A,0
        SL 1
        LR 0,A
        SL 1
        SL 1
        AS 0                    ; 10 s
        DCI ST+OFF_PLAYERS
        ADC
        POP

; ---- move.c ---------------------------------------------------------------
; TRYDIRS -- the stick to a direction; on a diagonal, the turn first (the
;   way we aren't going), and if that's walled, on as we were (play.c
;   try_dirs).  r0 = 1 if we moved.
TRYDIRS: ENTER
        LI 0FFH
        LR 4,A                  ; the vertical
        LR 5,A                  ; the horizontal
        LDB INPDIR
        LR 3,A
        NI IN_UP
        BZ TD1
        LIS DIR_UP
        LR 4,A
        BR TD2
TD1:    LR A,3
        NI IN_DOWN
        BZ TD2
        LIS DIR_DOWN
        LR 4,A
TD2:    LR A,3
        NI IN_LEFT
        BZ TD3
        LIS DIR_LEFT
        LR 5,A
        BR TD4
TD3:    LR A,3
        NI IN_RIGHT
        BZ TD4
        LIS DIR_RIGHT
        LR 5,A
TD4:    LR A,4
        CI 0FFH
        BNZ TD5
        LR A,5                  ; across, or still
        LR 6,A
        PI MVSTEP
        LEAVE
TD5:    LR A,5
        CI 0FFH
        BNZ TD6
        LR A,4                  ; up or down
        LR 6,A
        PI MVSTEP
        LEAVE
TD6:    LDB MVDIR               ; both: a = the turn, b = on
        CI DIR_LEFT
        BZ TD7
        CI DIR_RIGHT
        BZ TD7
        LR A,5                  ; going up or down: across first
        STB TDA
        LR A,4
        STB TDB
        BR TD8
TD7:    LR A,4                  ; going across: up or down first
        STB TDA
        LR A,5
        STB TDB
TD8:    DCI MVX
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        LM
        LR 2,A
        DCI TSX
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        LDB TDA
        LR 6,A
        PI MVSTEP
        LR A,0
        NI 0FFH
        BZ TD9
        LEAVE                   ; (r0 = 1)
TD9:    DCI TSX
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        LR 2,A
        DCI MVX
        LR A,0
        ST
        LR A,1
        ST
        LM                      ; (MVDIR: MVSTEP sets it)
        LR A,2
        ST
        LDB TDB
        LR 6,A
        PI MVSTEP
        LEAVE

; MVSTEP -- one jiffy toward direction r6 (FF: still) at speed MSPEED.
;   r0 = 1 if it moved (mv_step).
MVSTEP: ENTER
        LR A,6
        STB MSD
        LI 0FFH
        STB MVBUMP
        LR A,6
        CI 0FFH
        BNZ MS1
        CLR
        STB MVACC
        LR 0,A
        LEAVE
MS1:    STB MVDIR
        LDB MSPEED              ; acc += 16 + 4 lv: acc >> 4 steps
        SL 1
        SL 1
        AI SPEED_BASE
        LR 0,A
        LDB MVACC
        AS 0
        LR 0,A
        NI 15
        STB MVACC
        LR A,0
        SR 4
        STB MSN
        CLR
        STB MSMOV
MS2:    LDB MSN
        NI 0FFH
        BZ MS9
        AI -1
        STB MSN
        PI USTEP
        LR A,0
        NI 0FFH
        BNZ MS3
        CLR                     ; walled: no more
        STB MVACC
        BR MS9
MS3:    LIS 1
        STB MSMOV
        BR MS2
MS9:    LDB MSMOV
        LR 0,A
        LEAVE

; TILE45 -- r1 = tile (r4, r5) = 13 r5 + r4.  Leaf.  Clobbers A.
TILE45  MACRO
        LR A,5
        SL 1
        AS 5
        SL 1
        SL 1
        AS 5
        AS 4
        LR 1,A
        ENDM

; WALKM -- (inline) r0 = 1 if tile (r4, r5) can be walked on: in the arena,
;   not our pending bomb's, not a pillar, block, burning block, bomb or dud.
;   Clobbers A, r0, r1, DC0.
WALKM   MACRO
        CLR
        LR 0,A
        LR A,4                  ; (-1 is 255: out too)
        CI ARENA_W-1
        BNC m_out
        LR A,5
        CI ARENA_H-1
        BNC m_out
        TILE45
        DCI PEND
        CM
        BZ m_out
        LR A,1
        DCI ST+OFF_MAP
        ADCU
        LM
        NI 0FFH
        BZ m_yes                ; floor
        CI T_DUD
        BC m_out                ; pillar .. dud
m_yes:  LIS 1
        LR 0,A
m_out:
        ENDM

; BUMPM -- (inline) tile (r4, r5) a bomb or a dud: MVBUMP = it.
;   Clobbers A, r1, DC0.
BUMPM   MACRO
        LR A,4
        CI ARENA_W-1
        BNC m_out
        LR A,5
        CI ARENA_H-1
        BNC m_out
        TILE45
        DCI ST+OFF_MAP
        ADCU
        LM
        CI T_BOMB
        BZ m_hit
        CI T_DUD
        BNZ m_out
m_hit:  LR A,1
        STB MVBUMP
m_out:
        ENDM

; USTEP -- a unit toward MSD (unit_step): r0 = 1 if it went. Off a lane by
;   up to LANE, it slides onto the lane toward an open tile.
USTEP:                          ; (a leaf: WALKM, BUMPM inline)
        LDB MSD
        CI DIR_LEFT
        BZ USTH
        CI DIR_RIGHT
        BNZ USTV
USTH:   JMP USH                 ; (USH loads MSD again: JMP spends A)
USTV:   CI DIR_DOWN             ; up or down: dy
        LIS 1
        BZ USV1
        LI 0FFH
USV1:   STB UDD
        LR 3,A
        LDB MVX                 ; c, r + dy
        SR 4
        LR 4,A
        LDB MVY
        SR 4
        AS 3
        LR 5,A
        LDB MVX                 ; off the column's lane?
        NI 15
        JZ USV4
        STB UOFF
        CI LANE
        BNC USV2
        WALKM
        LR A,0
        NI 0FFH
        BZ USV2
        DECB MVX
        JMP USVYES
USV2:   LDB UOFF
        CI UNIT-LANE-1
        JC USVNO
        LR A,4
        INC
        LR 4,A
        WALKM
        LR A,0
        NI 0FFH
        JZ USVNO
        INCB MVX
        JMP USVYES
USV4:   LDB MVY                 ; on the lane: a tile's edge, walled?
        NI 15
        BNZ USV5
        WALKM
        LR A,0
        NI 0FFH
        BNZ USV5
        BUMPM
        JMP USVNO
USV5:   LDB UDD
        LR 3,A
        LDB MVY
        AS 3
        STB MVY
        JMP USVYES
USVYES: LIS 1                   ; (the vertical half's own exits: in reach)
        LR 0,A
        POP
USVNO:  CLR
        LR 0,A
        POP
USH:    LDB MSD                 ; left or right: dx
        CI DIR_RIGHT
        LIS 1
        BZ USH1
        LI 0FFH
USH1:   STB UDD
        LR 3,A
        LDB MVX                 ; c + dx, r
        SR 4
        AS 3
        LR 4,A
        LDB MVY
        SR 4
        LR 5,A
        LDB MVY                 ; off the row's lane?
        NI 15
        JZ USH4
        STB UOFF
        CI LANE
        BNC USH2
        WALKM
        LR A,0
        NI 0FFH
        BZ USH2
        DECB MVY
        JMP USYES
USH2:   LDB UOFF
        CI UNIT-LANE-1
        JC USNO
        LR A,5
        INC
        LR 5,A
        WALKM
        LR A,0
        NI 0FFH
        JZ USNO
        INCB MVY
        JMP USYES
USH4:   LDB MVX
        NI 15
        BNZ USH5
        WALKM
        LR A,0
        NI 0FFH
        BNZ USH5
        BUMPM
        BR USNO
USH5:   LDB UDD
        LR 3,A
        LDB MVX
        AS 3
        STB MVX
USYES:  LIS 1
        LR 0,A
        POP
USNO:   CLR
        LR 0,A
        POP

; CTILE -- r1 = the tile under our centre (centre_tile).  Leaf.
;   Clobbers A, r4, r5, DC0.
CTILE:  LDB MVX
        AI UNIT/2
        SR 4
        CI ARENA_W-1
        BC CT1
        LIS ARENA_W-1
CT1:    LR 4,A
        LDB MVY
        AI UNIT/2
        SR 4
        CI ARENA_H-1
        BC CT2
        LIS ARENA_H-1
CT2:    LR 5,A
        TILE45
        POP

; MAPAT -- A = the map's code at tile r1.  Leaf.  Clobbers DC0.
MAPAT:  LR A,1
        DCI ST+OFF_MAP
        ADCU
        LM
        POP

; ---- play_frame's actions -----------------------------------------------------
; SINCE -- r2 = jiffies since the jiffy at DC0 (2 bytes; 255 if more).
;   JIFFY + ~then + 1, in two carries (COM; INC of a 0 would lose one).
;   Leaf.  Clobbers A, r1, r2, DC0.
SINCE:  LM
        COM
        LR 1,A                  ; ~then
        LM
        COM
        LR 2,A
        DCI JIFFY
        LM
        AS 1                    ; lo + ~then lo: C0
        LR 1,A
        LM                      ; (LM keeps the flags)
        LNK
        AS 2
        LR 2,A                  ; hi + ~then hi + C0
        LR A,1
        INC                     ; + 1: C1 (never with C0)
        LR 1,A
        LR A,2
        LNK
        NI 0FFH
        LR A,1
        BZ SNC1
        LI 255
SNC1:   LR 2,A
        POP

; PBUMP -- bumped into a bomb, 30 jiffies since the last: with a kick (or a
;   dud), BUMP it.
PBUMP:  LDB MVBUMP              ; (no bump: out before ENTER)
        CI 0FFH
        BNZ PBG
        POP
PBG:    ENTER
        DCI BUMPT
        PI SINCE
        LR A,2
        CI 29
        BC PBX
        LDB MYSEAT
        LR 0,A
        PI PLREC
        LIS P_FLAGS
        ADC
        LM
        NI PF_KICK
        STB PFT                 ; (kept: the sound)
        BNZ PB1
        LDB MVBUMP
        LR 1,A
        PI MAPAT
        CI T_DUD
        BNZ PBX
PB1:    DCI JIFFY
        LM
        LR 0,A
        LM
        LR 1,A
        DCI BUMPT
        LR A,0
        ST
        LR A,1
        ST
        LIS ACT_BUMP
        LR 1,A
        LDB MVBUMP
        LR 2,A
        LDB MVDIR
        LR 3,A
        PI ACTQ
        LDB PFT
        NI 0FFH
        BZ PBX
        SND SND_KICK
PBX:    LEAVE

; PFIRE -- FIRE: with the glove, on a bomb (ours too), THROW it; else on the
;   floor with a bomb to spare, DROP one -- shown at once (PEND) until the
;   server's state has it.
PFIRE:  LDB EVBTN               ; (no FIRE: out before ENTER)
        NI 0FFH
        BNZ PFI0
        POP
PFI0:   ENTER
        PI CTILE
        LR A,1
        STB PFT
        LDB MYSEAT
        LR 0,A
        PI PLREC
        LIS P_FLAGS
        ADC
        LM
        NI PF_GLOVE
        BZ PDROP
        LDB PFT
        LR 1,A
        PI MAPAT
        CI T_BOMB
        BZ PTHROW
        LDB PFT
        LR 1,A
        LDB PEND
        XS 1
        BNZ PDROP
PTHROW: LIS ACT_THROW
        LR 1,A
        LDB MVDIR
        LR 2,A
        CLR
        LR 3,A
        PI ACTQ
        SND SND_THROW
        LDB PEND
        CI 0FFH
        BZ PFX
        LR 0,A
        LI 0FFH
        STB PEND
        PI DMARK
PFX:    LEAVE
PDROP:  LDB PFT
        LR 1,A
        PI MAPAT
        NI 0FFH
        BNZ PFX                 ; (not the floor)
        LDB PEND
        XS 1
        BZ PFX                  ; (it is ours already)
        LDB PEND                ; out + pending < bombs
        CI 0FFH
        LIS 0
        BZ PD1
        LIS 1
PD1:    LR 4,A
        LDB MYSEAT
        LR 0,A
        PI PLREC
        LIS P_BOMBS
        ADC
        LM
        LR 5,A                  ; bombs
        LM
        LM                      ; out (P_OUT = P_BOMBS + 2)
        AS 4
        COM
        AS 5                    ; carry when bombs > out + pending
        BNC PFX
        LIS ACT_DROP
        LR 1,A
        LDB PFT
        LR 2,A
        CLR
        LR 3,A
        PI ACTQ
        LDB PFT
        STB PEND
        LR 0,A
        LI PEND_WAIT
        STB PENDL
        PI DMARK
        SND SND_DROP
        LEAVE

; PMOVE -- our move sent, when it changed (at most every MOVE_EVERY
;   jiffies), or every 60 anyway.
PMOVE:  ENTER
        LDB MOVED
        NI 0FFH
        LDB MVDIR
        BZ PM1
        OI P_DIR_MOVING
PM1:    STB PMD
        DCI SENTT
        PI SINCE
        LR A,2
        CI 59
        BNC PM3
        CI MOVE_EVERY-1
        BC PMX
        DCI MVX                 ; changed?
        LM
        LR 0,A
        LM
        LR 1,A
        DCI SENTX
        LR A,0
        CM
        BNZ PM3
        LR A,1
        CM
        BNZ PM3
        LDB PMD
        DCI SENTD
        CM
        BZ PMX
PM3:    DCI MVX
        LM
        LR 0,A
        LM
        LR 1,A
        DCI SENTX
        LR A,0
        ST
        LR A,1
        ST
        LDB PMD
        STB SENTD
        DCI JIFFY
        LM
        LR 0,A
        LM
        LR 1,A
        DCI SENTT
        LR A,0
        ST
        LR A,1
        ST
        LDB TXF
        OI TX_MOVE
        STB TXF
PMX:    LEAVE

; ---- the bombers ---------------------------------------------------------------
; PSEATS -- each seat's bomber into its slot, once a pass: out of play,
;   hidden; knocked out, its fall (4 frames of 12 jiffies, counted on by
;   the pass's jiffies); ours while we walk, where we are; another, drawn
;   toward where the server has it (T3 units; more than a tile off, there)
;   and facing as it says (play.c's toward and spr_arena calls).
;   H walks SEATR; r9 = T3 (leaves only below: H and r9 last the loop).
PSEATS: ENTER
        LDB ST+OFF_INPLAY
        STB GT+20
        LDB ST+OFF_ALIVE
        STB GT+21
        LI 0FFH                 ; the seat drawn where we are: ours, walking
        LR 1,A
        LDB PME
        NI 0FFH
        BZ PS0
        LDB MYSEAT
        LR 1,A
PS0:    LR A,1
        STB GT+24
        LDB T3
        LR 9,A
        CLR
        STB PSEAT
        LIS 1
        STB PBIT
        DCI SEATR
        LR H,DC
PS1:    LDB PBIT
        LR 5,A
        LDB GT+20
        NS 5
        BNZ PS1A
        JMP PSHID
PS1A:   LDB GT+21
        NS 5
        BNZ PS3
        LR DC,H                 ; out: its fall
        LIS SR_DIE
        ADC
        LM
        NI 0FFH
        BNZ PS1B
        JMP PSHID
PS1B:   CI 47
        BC PS1C
        JMP PSHID
PS1C:   LR 1,A
        LDB PJIF
        AS 1
        CI 48
        BC PS2
        LI 48
PS2:    LR 1,A
        LR DC,H
        LIS SR_DIE
        ADC
        LR A,1
        ST
        LI FR_DIE0-1            ; its frame: FR_DIE0 + d / 12
        LR 3,A
PS2A:   LR A,3
        INC
        LR 3,A
        LR A,1
        AI -12
        LR 1,A
        BC PS2A
        LR DC,H                 ; where it was drawn
        LM
        LR 1,A
        LM
        LR 2,A
        BR PS7
PS3:    LDB GT+24               ; ours, walking: where we are
        LR 1,A
        LDB PSEAT
        XS 1
        BNZ PS4
        LDB MVX
        LR 1,A
        LDB MVY
        LR 2,A
        LR DC,H
        LR A,1
        ST
        LR A,2
        ST
        LDB MVDIR
        LR 6,A
        LDB MOVED
        LR 7,A
        PI FRAMEOF
        LR DC,H
        LM
        LR 1,A
        LM
        LR 2,A
        BR PS7
PS4:    LDB PSEAT               ; another: toward the server's, facing as it says
        LR 0,A
        PI PLREC
        LM
        LR 4,A                  ; P_X
        LM
        LR 8,A                  ; P_Y
        LM
        LR 6,A                  ; P_DIR
        NI P_DIR_MOVING
        LR 7,A
        LR DC,H
        LM
        LR 2,A
        LR A,4
        LR 3,A
        PI TOWARD
        LR DC,H
        LR A,2
        ST
        LM
        LR 2,A
        LR A,8
        LR 3,A
        PI TOWARD
        LR DC,H
        LM
        LR 1,A
        LR A,2
        ST
        PI FRAMEOF
PS7:    LI XPX>>8               ; pixels (page-aligned tables)
        LR QU,A
        LR A,1
        LR QL,A
        LR DC,Q
        LM
        AI AX0
        LR 1,A
        LI YPX>>8
        LR QU,A
        LR A,2
        LR QL,A
        LR DC,Q
        LM
        AI AY0
        LR 2,A
        LIS 1
        LR 4,A
        BR PS8
PSHID:  LDB PSEAT               ; hidden, where it was
        SL 4
        DCI SLOTS
        ADC
        LM
        LR 1,A
        LM
        LR 2,A
        LM
        LR 3,A
        CLR
        LR 4,A
PS8:    LDB PSEAT
        LR 0,A
        PI SLSET
        LR DC,H                 ; the next seat's record
        LIS 3
        ADC
        LR H,DC
        LDB PBIT
        SL 1
        STB PBIT
        INCB PSEAT
        CI SEATS
        BZ PSX
        JMP PS1
PSX:    LEAVE

; TOWARD -- r2 (where it is drawn) moved toward r3 (where it is): r9 (T3)
;   units at most; more than a tile off, there (play.c toward).
;   Unsigned: the positions run to 192.  Leaf.  Clobbers A, r1, r2.
TOWARD: LR A,2
        XS 3
        BZ TWX
        LR A,3                  ; COM; AS: at - to - 1, C when at > to
        COM
        AS 2
        BC TW2
        LR A,2                  ; behind: d = to - at (> 0)
        COM
        INC
        AS 3
        CI UNIT
        BNC TWTO
        LR 1,A
        LR A,9
        COM
        AS 1                    ; C: d > T3
        LR A,1
        BNC TW1
        LR A,9
TW1:    AS 2
        LR 2,A
        POP
TW2:    LR A,3                  ; ahead: d = at - to (> 0)
        COM
        INC
        AS 2
        CI UNIT
        BNC TWTO
        LR 1,A
        LR A,9
        COM
        AS 1
        LR A,1
        BNC TW3
        LR A,9
TW3:    COM
        INC
        AS 2
        LR 2,A
        POP
TWTO:   LR A,3
        LR 2,A
TWX:    POP

; FRAMEOF -- r3 = the frame facing r6 (DIR_*), walking if r7 (frame_of: the
;   steps 0, 1, 0, 2, 8 jiffies each).  Leaf.  Clobbers A, r3, DC0.
FRAMEOF: LR A,6
        NI 3
        DCI FRBASE
        ADC
        LM
        LR 3,A
        LR A,7
        NI 0FFH
        BZ FRX
        LDB JIFFY
        SR 1
        SR 1
        SR 1
        NI 3
        DCI WALKF
        ADC
        LM
        AS 3
        LR 3,A
FRX:    POP
FRBASE: DB FR_UP0, FR_RIGHT0, FR_DOWN0, FR_LEFT0
WALKF:  DB 0, 1, 0, 2

; PMOVERS -- the kicked and thrown bombs into slots 4-7: a bomb (or a dud)
;   at the mover's place; NSLOT 8 while any is shown.
PMOVERS: ENTER
        LIS 4
        STB NSLOT
        CLR
        STB PSEAT
MV1:    LDB PSEAT               ; its record: OFF_MOVERS + 4 m
        SL 1
        SL 1
        DCI ST+OFF_MOVERS
        ADC
        LM
        LR 5,A                  ; its kind
        LM
        LR 1,A                  ; x, y (units)
        LM
        LR 2,A
        LR A,5
        NI 3
        BNZ MV2
        LDB PSEAT               ; none: hidden where it was (already: nothing)
        AI 4
        SL 4
        DCI SLOTS
        ADC
        LM
        LR 1,A
        LM
        LR 2,A
        LM
        LR 3,A
        LM
        NI 0FFH
        BZ MV4
        CLR
        LR 4,A
        BR MV3
MV2:    LIS 8
        STB NSLOT
        LI XPX>>8
        LR QU,A
        LR A,1
        LR QL,A
        LR DC,Q
        LM
        AI AX0
        LR 1,A
        LI YPX>>8
        LR QU,A
        LR A,2
        LR QL,A
        LR DC,Q
        LM
        AI AY0
        LR 2,A
        LR A,5                  ; a bomb's first frame, or a dud's
        NI M_DUD
        LIS T_BOMB*2
        BZ MV2A
        LIS T_DUD*2
MV2A:   DCI TKIND
        ADC
        LM
        LR 3,A
        LIS 1
        LR 4,A
MV3:    LDB PSEAT
        AI 4
        LR 0,A
        PI SLSET
MV4:    INCB PSEAT
        CI MOVERS
        BNZ MV1
        LEAVE

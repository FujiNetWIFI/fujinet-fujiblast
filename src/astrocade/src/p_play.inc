; p_play.inc -- the arena's frame (src/play.c play_frame; src/move.c, line
; for line: the server walks us with the same rules, internal/game/move.go).
; The Arcadia's (src/arcadia/src/p_play.inc), transcribed.
;
; Each pass runs the jiffies the clock says went by (DT, at most 8): our
; walking, the others drawn toward where the server has them, the
; knocked-out falls, our bomb's wait; then once: a bump, FIRE, our move
; sent, a blast's flash, and the bombers handed to the compositor (BX, BY,
; BF, BVIS: p_draw.inc draws them, and the tiles marked, this frame).

        RET                     ; E_ENTER (p_arena's)
        NOP
        NOP
        RET                     ; E_STATE (p_arena's)
        NOP
        NOP
        JP      PFRAME          ; E_TICK

PFRAME: LD      A,(DT)          ; the jiffies owed (FRAME's DT: 8 at most; a
                                ;  slow pass runs them all)
PF0:    LD      (RCOUNT),A
        LD      (PJIF),A        ; (the others' smoothing: once, for them all)
        LD      B,A
        ADD     A,A
        ADD     A,B
        LD      (T3),A          ;  3 units a jiffy
        XOR     A
        LD      (MOVED),A
        LD      (PME),A         ; we walk: a seat, alive, in play
        LD      A,(MYSEAT)
        CP      NONE
        JR      Z,PFJ
        CALL    SBITA
        LD      A,(ST+OFF_ALIVE)
        AND     B
        JR      Z,PFJ
        LD      A,(ST+OFF_PHASE)
        CP      PH_PLAY
        JR      NZ,PFJ
        LD      A,1
        LD      (PME),A
PFJ:    LD      A,(PME)         ; a jiffy
        OR      A
        JR      Z,PFJ2
        LD      A,(INDIR)       ; (the rigs' record of it, written after:
        LD      (PDIN),A        ;  the disc, where it took us, its number last)
        LD      A,(MYSEAT)
        LD      C,A
        CALL    PLRECP
        LD      DE,P_SPEED
        ADD     HL,DE
        LD      A,(HL)
        LD      (MSPEED),A
        CALL    TRYDIRS
        OR      A
        JR      Z,PFJ1
        LD      (MOVED),A
PFJ1:   CALL    PBUMP
        LD      HL,PDIN
        LD      DE,PDBG+1
        LDI
        LD      HL,MVX
        LD      BC,4
        LDIR                    ; x, y, dir, acc
        LD      HL,PDBG
        INC     (HL)
PFJ2:   LD      A,(PEND)        ; our bomb, never come: off again
        CP      NONE
        JR      Z,PFJ3
        LD      HL,PENDL
        LD      B,A
        LD      A,(HL)
        OR      A
        JR      Z,PFJ3
        DEC     (HL)
        JR      NZ,PFJ3
        LD      A,NONE
        LD      (PEND),A
        LD      A,B
        CALL    MARKT
PFJ3:   LD      HL,RCOUNT
        DEC     (HL)
        JP      NZ,PFJ
; once a pass
        CALL    SEATSTEP
        LD      A,(PME)
        OR      A
        JR      Z,PF9
        CALL    PFIRE
        CALL    PMOVE
PF9:    CALL    FLASHS
        CALL    SPRITES
        FAR     PG_DRAW,DRFRAME
        RET

; FLASHS: a blast's flash: the frame (colour 0, both sides) in the flash
; colour until FLASH runs out.
FLASHS: LD      HL,FLASH
        LD      A,(HL)
        OR      A
        RET     Z
        DEC     (HL)
        LD      A,C_DARK
        JR      Z,FL1
        LD      A,FLASHD
        BIT     2,(HL)          ; (6, 5, 4 left: the first)
        JR      Z,FL1
        LD      A,FLASHC
FL1:    OUT     (COL0L),A
        OUT     (COL0R),A
        RET

; SBITA: B = seat A's bit. Clobbers A.
SBITA:  LD      B,1
        OR      A
        RET     Z
SBA1:   SLA     B
        DEC     A
        JR      NZ,SBA1
        RET

; PLRECP: HL = seat C's record in ST. Clobbers A, DE.
PLRECP: LD      A,C
        ADD     A,A
        LD      E,A
        ADD     A,A
        ADD     A,A
        ADD     A,E
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_PLAYERS
        ADD     HL,DE
        RET

; ---- move.c ------------------------------------------------------------------------
; TRYDIRS: the disc to a direction; on a diagonal, the turn first (the way
; we aren't going), and if that's walled, on as we were (play.c try_dirs).
; A = 1 if we moved.
TRYDIRS: LD     BC,NONE*256+NONE        ; B the vertical, C the horizontal
        LD      A,(INDIR)
        BIT     0,A             ; D_UP
        JR      Z,TD1
        LD      B,DIR_UP
        JR      TD2
TD1:    BIT     1,A             ; D_DOWN
        JR      Z,TD2
        LD      B,DIR_DOWN
TD2:    BIT     2,A             ; D_LEFT
        JR      Z,TD3
        LD      C,DIR_LEFT
        JR      TD4
TD3:    BIT     3,A             ; D_RIGHT
        JR      Z,TD4
        LD      C,DIR_RIGHT
TD4:    LD      A,B
        CP      NONE
        JR      NZ,TD5
        LD      A,C             ; across, or still
        JP      MVSTEP
TD5:    LD      A,C
        CP      NONE
        LD      A,B             ; up or down
        JP      Z,MVSTEP
        LD      A,(MVDIR)       ; both: a = the turn, b = on
        CP      DIR_LEFT
        JR      Z,TD7
        CP      DIR_RIGHT
        JR      Z,TD7
        LD      A,C             ; going up or down: across first
        LD      (TDA),A
        LD      A,B
        LD      (TDB),A
        JR      TD8
TD7:    LD      A,B             ; going across: up or down first
        LD      (TDA),A
        LD      A,C
        LD      (TDB),A
TD8:    LD      A,(MVX)
        LD      (TSX),A
        LD      A,(MVY)
        LD      (TSY),A
        LD      A,(MVACC)
        LD      (TSA),A
        LD      A,(TDA)
        CALL    MVSTEP
        OR      A
        RET     NZ              ; (A = 1)
        LD      A,(TSX)
        LD      (MVX),A
        LD      A,(TSY)
        LD      (MVY),A
        LD      A,(TSA)
        LD      (MVACC),A
        LD      A,(TDB)
; MVSTEP: one jiffy toward direction A (NONE: still) at speed MSPEED.
; A = 1 if it moved (mv_step).
MVSTEP: LD      (MSD),A
        LD      B,A
        LD      A,NONE
        LD      (MVBUMP),A
        LD      A,B
        CP      NONE
        JR      NZ,MS1
        XOR     A
        LD      (MVACC),A
        RET
MS1:    LD      (MVDIR),A
        LD      A,(MSPEED)      ; acc += 16 + 4 lv: acc >> 4 steps
        ADD     A,A
        ADD     A,A
        ADD     A,SPEED_BASE
        LD      HL,MVACC
        ADD     A,(HL)
        LD      B,A
        AND     15
        LD      (HL),A
        LD      A,B
        RRCA
        RRCA
        RRCA
        RRCA
        AND     0FH
        LD      (MSN),A
        XOR     A
        LD      (MSMOV),A
MS2:    LD      A,(MSN)
        OR      A
        JR      Z,MS9
        DEC     A
        LD      (MSN),A
        CALL    USTEP
        OR      A
        JR      NZ,MS3
        LD      (MVACC),A       ; walled: no more
        JR      MS9
MS3:    LD      A,1
        LD      (MSMOV),A
        JR      MS2
MS9:    LD      A,(MSMOV)
        RET

; USTEP: a unit toward MSD (unit_step): A = 1 if it went. Off a lane by up
; to LANE, it slides onto the lane toward an open tile.
USTEP:  LD      A,(MSD)
        CP      DIR_LEFT
        JP      Z,USH
        CP      DIR_RIGHT
        JP      Z,USH
        LD      C,1             ; up or down: dy
        CP      DIR_DOWN
        JR      Z,USV1
        LD      C,0FFH
USV1:   LD      A,C
        LD      (UDD),A
        LD      A,(MVX)         ; c, r + dy
        CALL    HI4P
        LD      D,A
        LD      A,(MVY)
        CALL    HI4P
        ADD     A,C
        LD      E,A
        LD      A,(MVX)         ; off the column's lane?
        AND     15
        JR      Z,USV4
        LD      (UOFF),A
        CP      LANE+1
        JR      NC,USV2
        CALL    WALKABLE
        OR      A
        JR      Z,USV2
        LD      HL,MVX
        DEC     (HL)
        LD      A,1
        RET
USV2:   LD      A,(UOFF)
        CP      UNIT-LANE
        JR      C,USV3
        INC     D
        CALL    WALKABLE
        OR      A
        JR      Z,USV3
        LD      HL,MVX
        INC     (HL)
        LD      A,1
        RET
USV3:   XOR     A
        RET
USV4:   LD      A,(MVY)         ; on the lane: a tile's edge, walled?
        AND     15
        JR      NZ,USV5
        CALL    WALKABLE
        OR      A
        JR      NZ,USV5
        CALL    BUMPED
        XOR     A
        RET
USV5:   LD      A,(UDD)
        LD      HL,MVY
        ADD     A,(HL)
        LD      (HL),A
        LD      A,1
        RET
USH:    LD      C,1             ; left or right: dx
        CP      DIR_RIGHT
        JR      Z,USH1
        LD      C,0FFH
USH1:   LD      A,C
        LD      (UDD),A
        LD      A,(MVX)         ; c + dx, r
        CALL    HI4P
        ADD     A,C
        LD      D,A
        LD      A,(MVY)
        CALL    HI4P
        LD      E,A
        LD      A,(MVY)         ; off the row's lane?
        AND     15
        JR      Z,USH4
        LD      (UOFF),A
        CP      LANE+1
        JR      NC,USH2
        CALL    WALKABLE
        OR      A
        JR      Z,USH2
        LD      HL,MVY
        DEC     (HL)
        LD      A,1
        RET
USH2:   LD      A,(UOFF)
        CP      UNIT-LANE
        JR      C,USV3
        INC     E
        CALL    WALKABLE
        OR      A
        JR      Z,USV3
        LD      HL,MVY
        INC     (HL)
        LD      A,1
        RET
USH4:   LD      A,(MVX)
        AND     15
        JR      NZ,USH5
        CALL    WALKABLE
        OR      A
        JR      NZ,USH5
        CALL    BUMPED
        XOR     A
        RET
USH5:   LD      A,(UDD)
        LD      HL,MVX
        ADD     A,(HL)
        LD      (HL),A
        LD      A,1
        RET

; HI4P: A >> 4.
HI4P:   RRCA
        RRCA
        RRCA
        RRCA
        AND     0FH
        RET

; TILEDE: A = tile (D, E) = 13 E + D. Clobbers nothing else.
TILEDE: LD      A,E
        PUSH    BC
        LD      B,A
        ADD     A,A
        ADD     A,B
        ADD     A,A
        ADD     A,A
        ADD     A,B
        ADD     A,D
        POP     BC
        RET

; WALKABLE: A = 1 if tile (D, E) can be walked on: in the arena, not our
; pending bomb's, not a pillar, block, burning block, bomb or dud.
; Preserves BC, DE.
WALKABLE: LD    A,D
        CP      ARENA_W         ; (-1 is 255: out too)
        JR      NC,WK0
        LD      A,E
        CP      ARENA_H
        JR      NC,WK0
        CALL    TILEDE
        LD      HL,PEND
        CP      (HL)
        JR      Z,WK0
        LD      L,A
        LD      H,0
        PUSH    DE
        LD      DE,ST+OFF_MAP
        ADD     HL,DE
        POP     DE
        LD      A,(HL)
        CP      T_PILLAR
        JR      C,WK1
        CP      T_DUD+1
        JR      C,WK0
WK1:    LD      A,1
        RET
WK0:    XOR     A
        RET

; BUMPED: tile (D, E) a bomb or a dud: MVBUMP (bumped). Preserves BC, DE.
BUMPED: LD      A,D
        CP      ARENA_W
        RET     NC
        LD      A,E
        CP      ARENA_H
        RET     NC
        CALL    TILEDE
        LD      L,A
        LD      H,0
        PUSH    DE
        LD      DE,ST+OFF_MAP
        ADD     HL,DE
        POP     DE
        LD      A,(HL)
        CP      T_BOMB
        JR      Z,BU1
        CP      T_DUD
        RET     NZ
BU1:    CALL    TILEDE
        LD      (MVBUMP),A
        RET

; CTILE: A = the tile under our centre (centre_tile). Clobbers DE.
CTILE:  LD      A,(MVX)
        ADD     A,UNIT/2
        CALL    HI4P
        CP      ARENA_W
        JR      C,CT1
        LD      A,ARENA_W-1
CT1:    LD      D,A
        LD      A,(MVY)
        ADD     A,UNIT/2
        CALL    HI4P
        CP      ARENA_H
        JR      C,CT2
        LD      A,ARENA_H-1
CT2:    LD      E,A
        JR      TILEDE

; MAPAT: A = the map's code at tile A. Clobbers HL.
MAPAT:  LD      L,A
        LD      H,0
        PUSH    DE
        LD      DE,ST+OFF_MAP
        ADD     HL,DE
        POP     DE
        LD      A,(HL)
        RET

; ---- play_frame's actions -----------------------------------------------------------
; PBUMP: bumped into a bomb, 30 frames since the last: with a kick (or a
; dud), BUMP it.
PBUMP:  LD      A,(MVBUMP)
        CP      NONE
        RET     Z
        LD      HL,(BUMPT)
        CALL    SINCE
        CP      30
        RET     C
        LD      A,(MYSEAT)
        LD      C,A
        CALL    PLRECP
        LD      DE,P_FLAGS
        ADD     HL,DE
        LD      A,(HL)
        AND     PF_KICK
        LD      (PKK),A
        JR      NZ,PB1
        LD      A,(MVBUMP)
        CALL    MAPAT
        CP      T_DUD
        RET     NZ
PB1:    LD      HL,(FRML)
        LD      (BUMPT),HL
        LD      B,ACT_BUMP
        LD      A,(MVBUMP)
        LD      D,A
        LD      A,(MVDIR)
        LD      E,A
        CALL    ACTQ
        LD      A,(PKK)
        OR      A
        RET     Z
        SND     SND_KICK
        RET

; PFIRE: FIRE: with the glove, on a bomb (ours too), THROW it; else on
; the floor with a bomb to spare, DROP one -- shown at once (PEND) until
; the server's state has it.
PFIRE:  LD      A,(EVBTN)
        OR      A
        RET     Z
        CALL    CTILE
        LD      (PFT),A
        LD      A,(MYSEAT)
        LD      C,A
        CALL    PLRECP          ; (HL: our record, all along)
        PUSH    HL
        LD      DE,P_FLAGS
        ADD     HL,DE
        LD      A,(HL)
        POP     HL
        AND     PF_GLOVE
        JR      Z,PDROP
        PUSH    HL
        LD      A,(PFT)
        CALL    MAPAT
        POP     HL
        CP      T_BOMB
        JR      Z,PTHROW
        LD      A,(PFT)
        LD      B,A
        LD      A,(PEND)
        CP      B
        JR      NZ,PDROP
PTHROW: LD      B,ACT_THROW
        LD      A,(MVDIR)
        LD      D,A
        LD      E,0
        CALL    ACTQ
        SND     SND_THROW
        LD      A,(PEND)
        CP      NONE
        RET     Z
        LD      B,A
        LD      A,NONE
        LD      (PEND),A
        LD      A,B
        JP      MARKT
PDROP:  PUSH    HL
        LD      A,(PFT)
        CALL    MAPAT
        POP     HL
        OR      A
        RET     NZ              ; (not the floor)
        LD      A,(PFT)
        LD      B,A
        LD      A,(PEND)
        CP      B
        RET     Z
        CP      NONE            ; out + pending < bombs
        LD      A,0
        JR      Z,PD1
        INC     A
PD1:    LD      DE,P_OUT
        ADD     HL,DE
        ADD     A,(HL)
        INC     HL
        INC     HL
        INC     HL
        DEC     HL              ; (P_BOMBS = P_OUT - 2)
        DEC     HL
        DEC     HL
        DEC     HL
        DEC     HL
        CP      (HL)
        RET     NC
        LD      B,ACT_DROP
        LD      A,(PFT)
        LD      D,A
        LD      E,0
        CALL    ACTQ
        LD      A,(PFT)
        LD      (PEND),A
        LD      A,90
        LD      (PENDL),A
        LD      A,(PFT)
        CALL    MARKT
        SND     SND_DROP
        RET

; PMOVE: our move sent, when it changed (at most every MOVE_EVERY jiffies),
; or every 60 anyway.
PMOVE:  LD      A,(MOVED)
        OR      A
        LD      A,(MVDIR)
        JR      Z,PM1
        OR      P_DIR_MOVING
PM1:    LD      (PMD),A
        LD      HL,(SENTT)
        CALL    SINCE
        CP      60
        JR      NC,PM3
        CP      MOVE_EVERY
        RET     C
        LD      A,(MVX)
        LD      HL,SENTX
        CP      (HL)
        JR      NZ,PM3
        LD      A,(MVY)
        LD      HL,SENTY
        CP      (HL)
        JR      NZ,PM3
        LD      A,(PMD)
        LD      HL,SENTD
        CP      (HL)
        RET     Z
PM3:    LD      A,(MVX)
        LD      (SENTX),A
        LD      A,(MVY)
        LD      (SENTY),A
        LD      A,(PMD)
        LD      (SENTD),A
        LD      HL,(FRML)
        LD      (SENTT),HL
        LD      A,(TXF)
        OR      TX_MOVE
        LD      (TXF),A
        RET

; SINCE: A = frames since the frame HL (255 if more). Clobbers DE, HL.
SINCE:  EX      DE,HL
        LD      HL,(FRML)
        OR      A
        SBC     HL,DE
        LD      A,H
        OR      A
        LD      A,L
        RET     Z
        LD      A,255
        RET

; ---- the bombers ---------------------------------------------------------------------
; SEATSTEP: the pass's jiffies (PJIF) of the others: each drawn toward
; where the server has it (3 units a jiffy: T3; more than a tile off,
; there); a knocked-out one's fall counted.
SEATSTEP: LD    C,0
SS1:    LD      A,C
        CALL    SBITA
        LD      A,(ST+OFF_INPLAY)
        AND     B
        JR      Z,SS9
        LD      A,(ST+OFF_ALIVE)
        AND     B
        JR      NZ,SS3
        LD      B,0             ; out: its fall
        LD      HL,DIET
        ADD     HL,BC
        LD      A,(HL)
        OR      A
        JR      Z,SS9
        CP      48
        JR      NC,SS9
        LD      B,A
        LD      A,(PJIF)
        ADD     A,B
        CP      48
        JR      C,SS2
        LD      A,48
SS2:    LD      (HL),A
        JR      SS9
SS3:    LD      A,(MYSEAT)      ; ours, walking: drawn where we are
        CP      C
        JR      NZ,SS4
        LD      A,(PME)
        OR      A
        JR      NZ,SS9
SS4:    PUSH    BC
        CALL    PLRECP
        POP     BC
        LD      B,0
        LD      E,(HL)          ; P_X
        INC     HL
        LD      D,(HL)          ; P_Y
        PUSH    DE
        LD      HL,DISPX
        ADD     HL,BC
        LD      A,(HL)
        CALL    TOWARD
        LD      (HL),A
        POP     DE
        LD      E,D
        LD      HL,DISPY
        ADD     HL,BC
        LD      A,(HL)
        CALL    TOWARD
        LD      (HL),A
SS9:    INC     C
        LD      A,C
        CP      SEATS
        JR      NZ,SS1
        RET

; TOWARD: A (where it is drawn) moved toward E (where it is): T3 units at
; most; more than a tile off, there (play.c toward). Preserves BC, HL.
TOWARD: CP      E
        RET     Z
        JR      NC,TW2
        LD      D,A             ; behind: d = to - at
        LD      A,E
        SUB     D
        CP      UNIT+1
        JR      NC,TWTO
        PUSH    HL
        LD      HL,T3
        CP      (HL)
        JR      C,TW1
        LD      A,(HL)
TW1:    POP     HL
        ADD     A,D
        RET
TW2:    LD      D,A             ; ahead: d = at - to
        SUB     E
        CP      UNIT+1
        JR      NC,TWTO
        PUSH    HL
        LD      HL,T3
        CP      (HL)
        JR      C,TW3
        LD      A,(HL)
TW3:    POP     HL
        NEG
        ADD     A,D
        RET
TWTO:   LD      A,E
        RET

; SPRITES: each seat's bomber for the compositor: where it is drawn (BX a
; pixel = a unit's half, BY a line: 7 a tile) and its frame (play.c's
; spr_arena calls); hidden when out of play.
SPRITES: LD     C,0
        LD      A,(BVIS)
        LD      (T2),A
        XOR     A
        LD      (BVIS),A
SP1:    LD      A,C
        CALL    SBITA
        LD      A,(ST+OFF_INPLAY)
        AND     B
        JP      Z,SP9
        LD      A,(ST+OFF_ALIVE)
        AND     B
        JR      NZ,SP3
        LD      B,0             ; out: its fall, 4 frames of 12 jiffies
        LD      HL,DIET
        ADD     HL,BC
        LD      A,(HL)
        OR      A
        JP      Z,SP9
        CP      48
        JR      NC,SP9
        LD      B,FR_DIE0-1
SP2:    INC     B
        SUB     12
        JR      NC,SP2
        LD      A,B
        JR      SP6
SP3:    LD      A,(MYSEAT)      ; ours, walking: where we are
        CP      C
        JR      NZ,SP4
        LD      A,(PME)
        OR      A
        JR      Z,SP4
        LD      B,0
        LD      HL,DISPX
        ADD     HL,BC
        LD      A,(MVX)
        LD      (HL),A
        LD      HL,DISPY
        ADD     HL,BC
        LD      A,(MVY)
        LD      (HL),A
        LD      A,(MVDIR)
        LD      D,A
        LD      A,(MOVED)
        LD      E,A
        JR      SP5
SP4:    PUSH    BC              ; another: the way the server has it facing
        CALL    PLRECP
        POP     BC
        INC     HL
        INC     HL
        LD      A,(HL)          ; P_DIR
        LD      D,A
        AND     P_DIR_MOVING
        LD      E,A
SP5:    CALL    FRAMEOF
SP6:    LD      B,0             ; frame A at DISPX, DISPY
        LD      HL,BF
        ADD     HL,BC
        LD      (HL),A
        LD      HL,DISPX
        ADD     HL,BC
        LD      A,(HL)
        SRL     A
        LD      HL,BX
        ADD     HL,BC
        LD      (HL),A
        LD      HL,DISPY
        ADD     HL,BC
        LD      A,(HL)          ; 7 (y >> 4) + YFR[y & 15]
        LD      E,A
        AND     15
        LD      HL,YFR
        ADD     A,L
        LD      L,A
        JR      NC,SP7
        INC     H
SP7:    LD      D,(HL)
        LD      A,E
        CALL    HI4P
        LD      E,A
        ADD     A,A
        ADD     A,A
        ADD     A,A
        SUB     E
        ADD     A,D
        LD      HL,BY
        ADD     HL,BC
        LD      (HL),A
        LD      A,C
        CALL    SBITA
        LD      A,(BVIS)
        OR      B
        LD      (BVIS),A
SP9:    INC     C
        LD      A,C
        CP      SEATS
        JP      NZ,SP1
        RET
YFR:    DB      0,0,0,1,1,2,2,3,3,3,4,4,5,5,6,6         ; a unit's line in its tile

; FRAMEOF: A = the frame facing D (DIR_*), walking if E (frame_of: the
; steps 0, 1, 0, 2, 8 jiffies each).
FRAMEOF: LD     A,D
        AND     3
        LD      L,A
        LD      H,0
        PUSH    DE
        LD      DE,FRBASE
        ADD     HL,DE
        POP     DE
        LD      A,E
        OR      A
        LD      A,(HL)
        RET     Z
        LD      D,A
        LD      A,(FRML)
        RRCA
        RRCA
        RRCA
        AND     3
        LD      L,A
        LD      H,0
        PUSH    DE
        LD      DE,WALKF
        ADD     HL,DE
        POP     DE
        LD      A,(HL)
        ADD     A,D
        RET

FRBASE: DB      FR_UP0,FR_RIGHT0,FR_DOWN0,FR_LEFT0
WALKF:  DB      0,1,0,2

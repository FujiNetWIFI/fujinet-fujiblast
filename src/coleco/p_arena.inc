; p_arena.inc -- the arena, as a new state draws it (src/play.c play_enter,
; play_state, hud, stats_draw). The Astrocade's (src/astrocade/src/
; p_arena.inc), transcribed; its frame (walking, bombs, the bombers) is
; p_play.inc's, the drawing arena.inc's (fixed).
;
; The screen (src/adam/gfx.c): the HUD on row 0, the arena (13 x 11 tiles of
; 2 x 2 cells) in its walls, the stats and the round on the left. The tiles
; the patches touched are marked (MAPD) and drawn a few a frame with the
; bombs in the air (ARFRAME); a new screen draws all of them (ARALL).

        JP      AENTER          ; E_ENTER
        JP      ASTATE          ; E_STATE
        RET                     ; E_TICK (p_play's)
        NOP
        NOP

HALF_W  EQU     ARENA_W*2
HALF_H  EQU     ARENA_H*2

; ---- play_enter --------------------------------------------------------------
AENTER: CALL    GFXBEGIN
        CALL    GFXARENA
        LD      A,NONE          ; nothing ours or moving shown
        LD      (PEND),A
        LD      (HUDTMR),A
        XOR     A
        LD      (MOVON),A
        LD      (MOVD),A
        LD      HL,DIET
        LD      B,4
AEN2:   LD      (HL),A
        INC     HL
        DJNZ    AEN2
        LD      C,0             ; each drawn where the server has it
AEN3:   CALL    PLREC           ; HL = its record
        LD      A,(HL)          ; P_X
        INC     HL
        LD      E,(HL)          ; P_Y
        LD      B,0
        LD      HL,DISPX
        ADD     HL,BC
        LD      (HL),A
        LD      HL,DISPY
        ADD     HL,BC
        LD      (HL),E
        INC     C
        LD      A,C
        CP      SEATS
        JR      NZ,AEN3
        LD      A,(ST+OFF_ALIVE)
        LD      (WASALV),A
        LD      A,(ST+OFF_PHASE)
        LD      (LASTPH),A
        LD      A,(MYSEAT)      ; we walk from where it has us
        CP      NONE
        JR      Z,AEN4
        LD      C,A
        CALL    PLREC
        LD      A,(HL)
        LD      (MVX),A
        INC     HL
        LD      A,(HL)
        LD      (MVY),A
        LD      DE,P_WARP-P_Y
        ADD     HL,DE
        LD      A,(HL)
        LD      (MYWARP),A
        LD      A,DIR_DOWN
        LD      (MVDIR),A
        XOR     A
        LD      (MVACC),A
AEN4:   LD      A,1             ; our powers, all drawn (no sound)
        CALL    STATDRAW
        XOR     A
        LD      (MFLAGS),A
        CALL    ASTATE          ; the movers; the HUD (HUDTMR: none yet)
        CALL    ARALL           ; every tile, and the bombs in the air
        LD      A,(ST+OFF_ROUND)
        CALL    GFXROUND
        CALL    GFXSHOW
        LD      A,(ST+OFF_PHASE)
        CP      PH_COUNTDOWN
        RET     NZ
        SND     MUS_ROUND
        RET

; PLREC: HL = seat C's record in ST (OFF_PLAYERS + 10 C). Clobbers A, DE.
PLREC:  LD      A,C
        ADD     A,A
        LD      E,A
        ADD     A,A
        ADD     A,A
        ADD     A,E             ; 10 C
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_PLAYERS
        ADD     HL,DE
        RET

; ---- play_state ------------------------------------------------------------------
ASTATE: LD      A,(MYSEAT)      ; our own position, when the server says so
        CP      NONE
        JR      Z,AS1
        LD      C,A
        CALL    PLREC
        PUSH    HL
        LD      DE,P_WARP
        ADD     HL,DE
        LD      A,(HL)
        POP     HL
        LD      DE,MYWARP
        EX      DE,HL
        CP      (HL)
        EX      DE,HL
        JR      Z,AS1
        LD      (MYWARP),A
        LD      A,(HL)
        LD      (MVX),A
        INC     HL
        LD      A,(HL)
        LD      (MVY),A
        XOR     A
        LD      (MVACC),A
AS1:    LD      A,(PEND)        ; our bomb: the server has it (or the tile is taken)
        CP      NONE
        JR      Z,AS2
        LD      C,A
        LD      B,0
        LD      HL,ST+OFF_MAP
        ADD     HL,BC
        LD      A,(HL)
        OR      A
        JR      Z,AS2
        LD      A,NONE
        LD      (PEND),A
        LD      A,C
        CALL    MARKT
AS2:    LD      A,(MFLAGS)      ; a blast, a dud
        LD      C,A
        XOR     A
        LD      (MFLAGS),A
        BIT     1,C             ; MF_BOOM
        JR      Z,AS3
        SND     SND_BOOM
        JR      AS4
AS3:    BIT     2,C             ; MF_FIZZ
        JR      Z,AS4
        SND     SND_FIZZLE
; the movers (a bomb in the air): each at its half-tile cell; the tiles it
; covered redrawn
AS4:    XOR     A
        LD      (T0),A
AM1:    LD      A,(T0)
        LD      C,A
        LD      A,(MOVON)       ; it was on: its tiles redrawn
        LD      B,C
        INC     B
AM1A:   RRCA
        DJNZ    AM1A
        JR      NC,AM2
        LD      E,C
        LD      D,0
        LD      HL,MOVHX
        ADD     HL,DE
        LD      B,(HL)
        LD      HL,MOVHY
        ADD     HL,DE
        LD      C,(HL)
        CALL    MARKHB
AM2:    LD      A,(T0)          ; its record: OFF_MOVERS + 4 m
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_MOVERS
        ADD     HL,DE
        LD      A,(HL)
        LD      (T1),A          ; its kind
        AND     3
        JR      Z,AM5           ; none
        INC     HL              ; hx = ((x + 4) >> 3) % HALF_W
        LD      A,(HL)
        ADD     A,4
        RRCA
        RRCA
        RRCA
        AND     1FH
        CP      HALF_W
        JR      C,AM3
        SUB     HALF_W
AM3:    CP      HALF_W-1        ; (wrapping: shown at the edge)
        JR      NZ,AM3A
        LD      A,HALF_W-2
AM3A:   LD      B,A
        INC     HL
        LD      A,(HL)
        ADD     A,4
        RRCA
        RRCA
        RRCA
        AND     1FH
        CP      HALF_H
        JR      C,AM4
        SUB     HALF_H
AM4:    CP      HALF_H-1
        JR      NZ,AM4A
        LD      A,HALF_H-2
AM4A:   LD      C,A
        LD      A,(T0)
        LD      E,A
        LD      D,0
        LD      HL,MOVHX
        ADD     HL,DE
        LD      (HL),B
        LD      HL,MOVHY
        ADD     HL,DE
        LD      (HL),C
        LD      A,(T0)
        CALL    BITOFA          ; D = its bit
        LD      A,(MOVON)
        OR      D
        LD      (MOVON),A
        LD      A,(T1)
        AND     M_DUD
        JR      NZ,AMDUD
        LD      A,D
        CPL
        LD      E,A
        LD      A,(MOVD)
        AND     E
        JR      AMDS
AMDUD:  LD      A,(MOVD)
        OR      D
AMDS:   LD      (MOVD),A
        JR      AM6
AM5:    LD      A,(T0)          ; off
        CALL    BITOFA
        LD      A,D
        CPL
        LD      E,A
        LD      A,(MOVON)
        AND     E
        LD      (MOVON),A
AM6:    LD      HL,T0
        INC     (HL)
        LD      A,(HL)
        CP      MOVERS
        JP      NZ,AM1
; a seat knocked out in play: its fall, a sound
        LD      C,0
AD1:    LD      A,C
        CALL    BITOFA
        LD      A,(WASALV)
        AND     D
        JR      Z,AD2
        LD      A,(ST+OFF_ALIVE)
        AND     D
        JR      NZ,AD2
        LD      A,(ST+OFF_PHASE)
        CP      PH_PLAY
        JR      NZ,AD2
        LD      B,0
        LD      HL,DIET
        ADD     HL,BC
        LD      (HL),1
        SND     SND_DIE
AD2:    INC     C
        LD      A,C
        CP      SEATS
        JR      NZ,AD1
; the countdown's beeps, then GO
        LD      A,(ST+OFF_PHASE)
        CP      PH_COUNTDOWN
        JR      NZ,AB1
        LD      A,(ST+OFF_TIMER)
        LD      HL,HUDTMR
        CP      (HL)
        JR      Z,AB2
        SND     SND_BEEP
        JR      AB2
AB1:    CP      PH_PLAY
        JR      NZ,AB2
        LD      A,(LASTPH)
        CP      PH_COUNTDOWN
        JR      NZ,AB2
        SND     SND_GO
AB2:    LD      A,(ST+OFF_PHASE)
        LD      (LASTPH),A
; the HUD, when it has changed
        LD      A,(ST+OFF_TIMER)
        LD      HL,HUDTMR
        CP      (HL)
        JR      NZ,AH1
        LD      A,(ST+OFF_ALIVE)
        LD      HL,HUDALV
        CP      (HL)
        JR      NZ,AH1
        LD      C,0
AH0:    CALL    PLREC
        LD      DE,P_WINS
        ADD     HL,DE
        LD      A,(HL)
        LD      B,0
        LD      HL,HUDWIN
        ADD     HL,BC
        CP      (HL)
        JR      NZ,AH1
        INC     C
        LD      A,C
        CP      SEATS
        JR      NZ,AH0
        JR      AH2
AH1:    CALL    HUD
AH2:    LD      A,(ST+OFF_ALIVE)
        LD      (WASALV),A
        XOR     A               ; our powers, those that changed
        JP      STATDRAW

; MARKHB: the tiles a bomb at half-tile cell B, C covers marked: its own, the
; next across if it is half over, the next down, and that one's. Clobbers A, DE.
MARKHB: LD      A,C
        SRL     A
        CALL    ROW13
        LD      D,A
        LD      A,B
        SRL     A
        LD      E,A             ; E = c
        ADD     A,D
        LD      D,A             ; D = its tile
        CALL    MARKT
        BIT     0,B
        JR      Z,MH2
        LD      A,E
        INC     A
        CP      ARENA_W
        JR      NC,MH2
        LD      A,D
        INC     A
        CALL    MARKT
MH2:    BIT     0,C
        RET     Z
        LD      A,C
        SRL     A
        INC     A
        CP      ARENA_H
        RET     NC
        LD      A,D
        ADD     A,ARENA_W
        CALL    MARKT
        BIT     0,B
        RET     Z
        LD      A,E
        INC     A
        CP      ARENA_W
        RET     NC
        LD      A,D
        ADD     A,ARENA_W+1
        JP      MARKT

; BITOFA: D = bit A (0-7). Clobbers A.
BITOFA: LD      D,1
        OR      A
        RET     Z
BOA1:   SLA     D
        DEC     A
        JR      NZ,BOA1
        RET

; ROW13: A = 13 A. Clobbers nothing else.
ROW13:  PUSH    BC
        LD      B,A
        ADD     A,A
        ADD     A,B
        ADD     A,A
        ADD     A,A
        ADD     A,B
        POP     BC
        RET

; ---- hud (play.c hud) -----------------------------------------------------------
; HUD: 20 characters (WORK) and their seats' colours (WORK+20) for BIGLINE:
; each seat in play: its name's 3 letters (dashes if it is out) and its
; wins; the clock in the middle group (m:ss; the countdown's second).
HUD:    LD      HL,WORK
        LD      B,20
        LD      A,' '
HU0:    LD      (HL),A
        INC     HL
        DJNZ    HU0
        XOR     A
        LD      (T0),A          ; T0: the seat
HU1:    LD      A,(T0)          ; T1: its group's first character
        LD      E,A
        LD      D,0
        LD      HL,HCOL
        ADD     HL,DE
        LD      A,(HL)
        LD      (T1),A
        LD      E,A             ; its four characters' colour: the seat
        LD      HL,WORK+20
        ADD     HL,DE
        LD      A,(T0)
        LD      B,4
HU2:    LD      (HL),A
        INC     HL
        DJNZ    HU2
        LD      A,(T0)
        CALL    BITOFA
        LD      A,(ST+OFF_INPLAY)
        AND     D
        JP      Z,HU9
        LD      A,(T0)          ; T2: alive
        CALL    BITOFA
        LD      A,(ST+OFF_ALIVE)
        AND     D
        LD      (T2),A
        LD      A,(T1)          ; DE = its place in the line
        LD      E,A
        LD      D,0
        LD      HL,WORK
        ADD     HL,DE
        EX      DE,HL
        LD      A,(T2)          ; out in play: dashes
        OR      A
        JR      NZ,HU3
        LD      A,(ST+OFF_PHASE)
        CP      PH_PLAY
        JR      NZ,HU3
        LD      B,3
        LD      A,'-'
HU2A:   LD      (DE),A
        INC     DE
        DJNZ    HU2A
        JR      HU6
HU3:    LD      A,(T0)          ; its name: ST + OFF_NAMES + 8 s, 3 letters
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      C,A
        LD      B,0
        LD      HL,ST+OFF_NAMES
        ADD     HL,BC
        LD      B,3
HU4:    LD      A,(HL)
        OR      A
        JR      NZ,HU5
        LD      A,' '
HU5:    LD      (DE),A
        INC     HL
        INC     DE
        DJNZ    HU4
HU6:    LD      A,(T0)          ; its wins' last digit
        LD      C,A
        CALL    PLREC
        LD      DE,P_WINS
        ADD     HL,DE
        LD      A,(HL)
HU7:    CP      10
        JR      C,HU8
        SUB     10
        JR      HU7
HU8:    ADD     A,'0'
        LD      C,A
        LD      HL,WORK+3
        LD      A,(T1)
        LD      E,A
        LD      D,0
        ADD     HL,DE
        LD      (HL),C
HU9:    LD      HL,T0
        INC     (HL)
        LD      A,(HL)
        CP      SEATS
        JP      NZ,HU1
        LD      A,(ST+OFF_TIMER)        ; the clock
        LD      E,A
        LD      A,(ST+OFF_PHASE)
        CP      PH_COUNTDOWN
        LD      A,E
        JR      NZ,HC1
HC0:    CP      10              ; the countdown: its second, in the middle
        JR      C,HC0A
        SUB     10
        JR      HC0
HC0A:   ADD     A,'0'
        LD      (WORK+9),A
        JR      HC9
HC1:    LD      B,'0'           ; m:ss: the minutes
HC2:    CP      60
        JR      C,HC3
        SUB     60
        INC     B
        JR      HC2
HC3:    LD      C,A
        LD      A,B
        LD      (WORK+8),A
        LD      A,':'
        LD      (WORK+9),A
        LD      A,C
        LD      B,'0'
HC4:    CP      10
        JR      C,HC5
        SUB     10
        INC     B
        JR      HC4
HC5:    LD      C,A
        LD      A,B
        LD      (WORK+10),A
        LD      A,C
        ADD     A,'0'
        LD      (WORK+11),A
HC9:    CALL    BIGLINE
        LD      A,(ST+OFF_TIMER)
        LD      (HUDTMR),A
        LD      A,(ST+OFF_ALIVE)
        LD      (HUDALV),A
        LD      C,0
HC10:   PUSH    BC
        CALL    PLREC
        LD      DE,P_WINS
        ADD     HL,DE
        LD      A,(HL)
        POP     BC
        LD      B,0
        LD      HL,HUDWIN
        ADD     HL,BC
        LD      (HL),A
        INC     C
        LD      A,C
        CP      SEATS
        JR      NZ,HC10
        RET
HCOL:   DB      0,4,12,16

; ---- stats_draw ------------------------------------------------------------------
; STATDRAW: our powers on the left -- range, bombs, speed (figures beside
; their icons), kick, glove (their icons, when had) -- those that changed
; (A = 1: all, and no sound; one that grew is a pickup's).
STATDRAW: LD    (T2),A
        LD      C,0
SD1:    LD      A,(MYSEAT)      ; watching: all 0
        CP      NONE
        LD      A,0
        JR      Z,SD3
        PUSH    BC
        LD      A,(MYSEAT)
        LD      C,A
        CALL    PLREC
        POP     BC
        LD      B,0
        PUSH    HL
        LD      HL,STFLD
        ADD     HL,BC
        LD      E,(HL)
        LD      D,0
        POP     HL
        ADD     HL,DE
        LD      A,(HL)
        LD      HL,STADD
        ADD     HL,BC
        ADD     A,(HL)
        LD      HL,STMASK
        ADD     HL,BC
        AND     (HL)
SD3:    LD      (T1),A          ; the value
        LD      B,0
        LD      HL,STATS
        ADD     HL,BC
        LD      E,A
        LD      A,(T2)
        OR      A
        JR      NZ,SD5          ; all: drawn
        LD      A,E
        CP      (HL)
        JR      Z,SD9
        JR      C,SD5           ; (less: quietly)
        PUSH    BC
        PUSH    HL
        SND     SND_PICKUP
        POP     HL
        POP     BC
SD5:    LD      A,(T1)
        LD      (HL),A
        PUSH    BC
        LD      A,(T1)
        LD      B,A
        LD      A,C
        CALL    GFXSTAT
        POP     BC
SD9:    INC     C
        LD      A,C
        CP      5
        JP      NZ,SD1
        RET

STFLD:  DB      P_RANGE,P_BOMBS,P_SPEED,P_FLAGS,P_FLAGS
STADD:  DB      0,0,1,0,0
STMASK: DB      0FFH,0FFH,0FFH,PF_KICK,PF_GLOVE

; p_arena.inc -- the arena, as a new state draws it (src/play.c play_enter,
; play_state, hud, stats_draw). The Arcadia's (src/arcadia/src/
; p_arena.inc), transcribed; its frame (walking, bombs, the bombers) is
; p_play.inc's, the drawing p_draw.inc's.
;
; The screen: lines 0-6 the HUD on its strip (each seat's bomber, name and
; wins); the arena from line 7 (tile (c, r) at bytes 1+2c, lines 7+7r)
; between the dark walls; bytes 28-39 the panel (the clock, the round, our
; powers: each row its own colours, bands.inc B_ARENA). The tiles the
; patches touched are marked (MAPD) and the draw page redraws them, a few a
; frame, with whatever stands on them.

        JP      AENTER          ; E_ENTER
        JP      ASTATE          ; E_STATE
        RET                     ; E_TICK (p_play's)
        NOP
        NOP

PNICON  EQU     PANB+1          ; the panel's icons: bytes 29-30
PNNUM   EQU     PANB+4          ; its figures
HUDSEAT EQU     10              ; a seat's HUD: 10 bytes, its icon at +1, name +4
                                ;  (4 letters), wins +9
LNCLK   EQU     ATOP            ; the panel's lines: the clock
LNRND   EQU     ATOP+7          ;  the round
LNMSG   EQU     ATOP+63         ;  "ROUND n" in the countdown

; ---- play_enter --------------------------------------------------------------
AENTER: LD      HL,P_ARENA
        CALL    SETPAL
        LD      HL,B_ARENA
        CALL    SETBANDS
        LD      D,ATOP          ; all of it dark (the frame, the walls) ...
        LD      C,LINES-ATOP
        CALL    CLRLN
        LD      DE,HUDLN*256+0  ; ... but the HUD's strip, colour 1
        LD      BC,BYTEPL*256+TXH
        LD      A,55H
        CALL    FILLB
        LD      A,NONE          ; nothing ours or moving shown
        LD      (PEND),A
        LD      (HUDTMR),A
        LD      HL,MOVT
        LD      B,4
AEN1:   LD      (HL),A
        INC     HL
        DJNZ    AEN1
        XOR     A
        LD      (MOVD),A
        LD      (BVIS),A
        LD      (DRVIS),A
        LD      (ROWPH),A
        LD      (ROWPH+1),A
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
AEN4:   CALL    APANEL          ; the panel's labels
        LD      A,1             ; our powers, all drawn (no sound)
        CALL    STATDRAW
        CALL    HUD
        XOR     A
        LD      (MFLAGS),A
        GOPAGE  PG_DRAW,E_ENTER ; every tile
        CALL    ASTATE
        LD      A,(ST+OFF_PHASE)
        CP      PH_COUNTDOWN
        RET     NZ
        LD      HL,SROUND       ; "ROUND n" in the panel
        LD      DE,LNMSG*256+PANB+1
        LD      C,3*4+0
        CALL    TXTAT
        LD      A,(ST+OFF_ROUND)
        LD      B,1
        CALL    NUMN
        SND     MUS_ROUND
        RET
SROUND: DB      "ROUND ",0

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

; APANEL: the panel's fixed parts: the round, the powers' icons.
APANEL: LD      HL,SRND
        LD      DE,LNRND*256+PANB+1
        LD      C,1*4+0
        CALL    TXTAT
        LD      A,(ST+OFF_ROUND)
        LD      B,1
        CALL    NUMN
        LD      A,'/'
        CALL    GLYPHC
        LD      A,(ST+OFF_ROUNDS)
        LD      B,1
        CALL    NUMN
        LD      HL,PNICONS      ; F, B, S: their icons (K, G: when had)
        LD      B,3
AP1:    PUSH    BC
        LD      D,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        PUSH    HL
        LD      E,PNICON
        FAR     PG_ICON,DRICON
        POP     HL
        POP     BC
        DJNZ    AP1
        RET
SRND:   DB      "RND ",0
PNICONS: DB     ATOP+21,T_FIRE
        DB      ATOP+28,T_BOMBUP
        DB      ATOP+35,T_SPEED
        DB      ATOP+42,T_KICK
        DB      ATOP+49,T_GLOVE

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
        LD      A,FLASHN        ; (and the frame flashes)
        LD      (FLASH),A
        JR      AS4
AS3:    BIT     2,C             ; MF_FIZZ
        JR      Z,AS4
        SND     SND_FIZZLE
; the movers: a bomb (or a dud) on the tile under each one's centre, the
; tiles they leave and reach redrawn
AS4:    LD      C,0
AM1:    LD      B,0
        LD      HL,MOVT
        ADD     HL,BC
        LD      A,(HL)          ; the tile it was on
        LD      (T0),A
        LD      A,C             ; its record: OFF_MOVERS + 4 C
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      D,0
        PUSH    HL
        LD      HL,ST+OFF_MOVERS
        ADD     HL,DE
        LD      A,(HL)
        LD      (T1),A          ; its kind
        AND     3
        LD      A,NONE
        JR      Z,AM2           ; none
        INC     HL              ; its tile: ((y + 8) >> 4) * 13 + (x + 8) >> 4
        LD      A,(HL)
        ADD     A,UNIT/2
        CALL    HI4
        CP      ARENA_W
        JR      C,AM1A
        LD      A,ARENA_W-1
AM1A:   LD      E,A
        INC     HL
        LD      A,(HL)
        ADD     A,UNIT/2
        CALL    HI4
        CP      ARENA_H
        JR      C,AM1B
        LD      A,ARENA_H-1
AM1B:   CALL    ROW13           ; A = 13 A
        ADD     A,E
AM2:    POP     HL
        LD      (HL),A          ; MOVT: now
        LD      E,A
        LD      A,C             ; its dud bit
        CALL    BITOFA          ; D = the bit
        LD      A,(MOVD)
        OR      D
        LD      B,A
        LD      A,(T1)
        AND     M_DUD
        LD      A,B
        JR      NZ,AM2A
        LD      A,D
        CPL
        LD      B,A
        LD      A,(MOVD)
        AND     B
AM2A:   LD      (MOVD),A
        LD      A,(T0)          ; moved: both tiles redrawn
        CP      E
        JR      Z,AM3
        CP      NONE
        CALL    NZ,MARKT
        LD      A,E
        CP      NONE
        CALL    NZ,MARKT
AM3:    INC     C
        LD      A,C
        CP      MOVERS
        JR      NZ,AM1
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
        LD      HL,SBLANK7      ; (its "ROUND n" off)
        LD      DE,LNMSG*256+PANB+1
        LD      C,0
        CALL    TXTAT
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
SBLANK7: DB     "       ",0

; BITOFA: D = bit A (0-7). Clobbers A.
BITOFA: LD      D,1
        OR      A
        RET     Z
BOA1:   SLA     D
        DEC     A
        JR      NZ,BOA1
        RET

; HI4: A >> 4.
HI4:    RRCA
        RRCA
        RRCA
        RRCA
        AND     0FH
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

; ---- hud ------------------------------------------------------------------------
; HUD: lines 0-6: each seat in play: its bomber, its name (dark if it is
; out), its wins; the clock in the panel (m:ss; the countdown's second).
HUD:    LD      C,0
HU1:    PUSH    BC
        LD      A,C             ; its bytes: 10 C
        ADD     A,A
        LD      E,A
        ADD     A,A
        ADD     A,A
        ADD     A,E
        LD      (T0),A
        LD      A,C
        CALL    BITOFA
        LD      A,(ST+OFF_INPLAY)
        AND     D
        JR      NZ,HU2
        LD      A,(T0)          ; not playing: the strip bare
        LD      E,A
        LD      D,HUDLN
        LD      BC,HUDSEAT*256+TXH
        LD      A,55H
        CALL    FILLB
        JR      HU5
HU2:    LD      A,(T0)
        INC     A
        LD      E,A
        LD      D,HUDLN
        POP     BC
        PUSH    BC
        LD      A,C
        LD      B,55H
        FAR     PG_ICON,DRBICON
        POP     BC
        PUSH    BC
        LD      A,C             ; out in play: its name dark
        CALL    BITOFA
        LD      A,(ST+OFF_ALIVE)
        AND     D
        LD      C,3*4+1
        JR      NZ,HU3
        LD      A,(ST+OFF_PHASE)
        CP      PH_PLAY
        JR      NZ,HU3
        LD      C,0*4+1
HU3:    POP     DE              ; (E: the seat)
        PUSH    DE
        LD      A,E             ; its name: ST + OFF_NAMES + 8 s, 3 letters
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_NAMES
        ADD     HL,DE
        LD      A,(T0)
        ADD     A,4
        LD      E,A
        LD      D,HUDLN
        LD      B,4
        CALL    TXTN
        POP     BC              ; its wins
        PUSH    BC
        CALL    PLREC
        LD      DE,P_WINS
        ADD     HL,DE
        LD      A,(HL)
        LD      B,0
        LD      HL,HUDWIN
        ADD     HL,BC
        LD      (HL),A
        LD      E,A             ; (the last digit)
HU4:    SUB     10
        JR      NC,HU4
        ADD     A,10
        PUSH    AF
        LD      A,(T0)
        ADD     A,9
        LD      E,A
        LD      D,HUDLN
        POP     AF
        LD      B,1
        LD      C,2*4+1
        CALL    NUMAT
HU5:    POP     BC
        INC     C
        LD      A,C
        CP      SEATS
        JP      NZ,HU1
        LD      A,(ST+OFF_ALIVE)
        LD      (HUDALV),A
        LD      A,(ST+OFF_TIMER)        ; the clock
        LD      (HUDTMR),A
        LD      E,A
        LD      A,(ST+OFF_PHASE)
        CP      PH_COUNTDOWN
        LD      A,E
        JR      NZ,HC1
        LD      DE,LNCLK*256+PANB+3     ; the countdown: its second
        LD      C,3*4+0
        PUSH    AF
        LD      A,' '
        CALL    CHRAT
        POP     AF
        LD      B,2
        CALL    NUMN
        LD      A,' '
        JP      GLYPHC
HC1:    LD      B,0             ; m:ss
HC2:    SUB     60
        JR      C,HC3
        INC     B
        JR      HC2
HC3:    ADD     A,60
        PUSH    AF
        LD      A,B
        LD      B,1
        LD      DE,LNCLK*256+PANB+3
        LD      C,3*4+0
        CALL    NUMAT
        LD      A,':'
        CALL    GLYPHC
        POP     AF
        LD      C,0             ; two places, a leading zero kept
HC4:    SUB     10
        JR      C,HC5
        INC     C
        JR      HC4
HC5:    ADD     A,10
        PUSH    AF
        LD      A,C
        ADD     A,'0'
        CALL    GLYPHC
        POP     AF
        ADD     A,'0'
        JP      GLYPHC

; ---- stats_draw ------------------------------------------------------------------
; STATDRAW: our powers in the panel -- range, bombs, speed (figures beside
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
        LD      HL,STLINE
        ADD     HL,BC
        LD      D,(HL)          ; its line
        LD      A,C
        CP      3
        JR      NC,SD6
        LD      A,(T1)          ; a number
        LD      E,PNNUM
        LD      B,2
        LD      C,1*4+0
        CALL    NUMAT
        JR      SD8
SD6:    LD      A,(T1)          ; a power: its icon, or nothing
        OR      A
        JR      Z,SD7
        POP     BC
        PUSH    BC
        LD      A,C
        ADD     A,T_FIRE
        LD      E,PNICON
        FAR     PG_ICON,DRICON
        JR      SD8
SD7:    LD      E,PNICON
        LD      BC,2*256+TILEH
        XOR     A
        CALL    FILLB
SD8:    POP     BC
SD9:    INC     C
        LD      A,C
        CP      5
        JP      NZ,SD1
        RET

STFLD:  DB      P_RANGE,P_BOMBS,P_SPEED,P_FLAGS,P_FLAGS
STADD:  DB      0,0,1,0,0
STMASK: DB      0FFH,0FFH,0FFH,PF_KICK,PF_GLOVE
STLINE: DB      ATOP+21,ATOP+28,ATOP+35,ATOP+42,ATOP+49

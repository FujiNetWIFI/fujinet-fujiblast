; p_room.inc -- the screens at a table besides the arena (src/screens.c):
; the waiting room, the trophies after a round and after the game. Which
; one is SHOWN's (main.inc). The Arcadia's (src/arcadia/src/p_room.inc),
; transcribed, laid out on 12 rows of 7 lines: each seat's row its own
; colour (bands.inc B_ROOM).

        JP      RENTER          ; E_ENTER: drawn afresh
        JP      RSTATE          ; E_STATE: a new state
        JP      RTICK           ; E_TICK: a frame

RENTER: LD      A,(SHOWN)
        CP      S_WAIT
        JP      Z,WENTER
        JP      RESENTER
RSTATE: LD      A,(SHOWN)
        CP      S_WAIT
        JP      Z,WSTATE
        RET
RTICK:  LD      A,(SHOWN)
        CP      S_WAIT
        JP      Z,WFRAME
        JP      RESFRAME

; RSCR: the screen: its colours, cleared.
RSCR:   LD      HL,B_ROOM
        CALL    SETBANDS
        LD      HL,P_TEXT
        CALL    SETPAL
        LD      D,0
        LD      C,LINES
        JP      CLRLN

; SEATROW: seat C's strip on text row B (colour 1, the whole width), its
; bomber in frame A at byte 1.
SEATROW: PUSH   AF
        LD      A,B
        CALL    ROWLN           ; D = its line
        PUSH    DE
        PUSH    BC
        LD      E,0
        LD      BC,BYTEPL*256+TXH
        LD      A,55H
        CALL    FILLB
        POP     BC
        POP     DE
        POP     AF
        LD      B,A             ; frame
        LD      A,C             ; seat
        LD      E,4             ; pixel 4: byte 1
        LD      C,55H
        FAR     PG_ICON,DRSTRIP
        RET

; ROWLN: D = text row A's line (7 A). Clobbers A.
ROWLN:  LD      D,A
        ADD     A,A
        ADD     A,A
        ADD     A,A
        SUB     D
        LD      D,A
        RET

; NAMEHL: HL = seat C's name in ST. Clobbers A, DE.
NAMEHL: LD      A,C
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_NAMES
        ADD     HL,DE
        RET

; SEATBIT: B = seat C's bit. Clobbers A.
SEATBIT: LD     A,C
        LD      B,1
        OR      A
        RET     Z
SBT1:   SLA     B
        DEC     A
        JR      NZ,SBT1
        RET

; ---- the waiting room --------------------------------------------------------
; Row 0 the room; rows 2, 4, 6, 8 the seats (a bomber on each seated one's
; strip, HOST by the host's); row 10 the rounds and what happens next;
; row 11 the key that leaves.
WENTER: CALL    RSCR
        LD      HL,SLEAVE
        LD      D,11*TXH
        LD      C,1*4+0
        CALL    CENTRE
WSTATE: LD      DE,0*256+2      ; ROOM <table>
        LD      HL,SROOM
        LD      C,3*4+0
        CALL    TXTAT
        LD      HL,SRVTBL
        LD      B,TABLE_LEN
        CALL    TXT1            ; (at TXPOS, padded)
        XOR     A
        LD      (T2),A          ; T2: players seated
        LD      C,0
WS3:    LD      A,C             ; the seat's row: 2 + 2 s
        ADD     A,A
        ADD     A,2
        LD      (T0),A
        LD      (T1),A
        CALL    SEATBIT
        LD      A,(ST+OFF_SEATED)
        AND     B
        JR      Z,WS5
        LD      HL,T2
        INC     (HL)
        LD      A,(T0)          ; seated: its strip, its bomber, its name
        LD      B,A
        LD      A,FR_DOWN0
        PUSH    BC
        CALL    SEATROW
        POP     BC
        PUSH    BC
        CALL    NAMEHL
        LD      A,(T0)
        CALL    ROWLN
        LD      E,5
        LD      B,NAME_LEN
        LD      C,3*4+1
        CALL    TXTN
        POP     BC
        LD      A,(ST+OFF_HOST) ; the host's: HOST
        CP      C
        JR      NZ,WS6
        PUSH    BC
        LD      A,(T0)
        CALL    ROWLN
        LD      E,16
        LD      HL,SHOST
        LD      C,2*4+1
        CALL    TXTAT
        POP     BC
        JR      WS6
WS5:    PUSH    BC              ; empty: dashes, no strip
        LD      A,(T0)
        CALL    ROWLN
        PUSH    DE
        LD      E,0
        LD      BC,BYTEPL*256+TXH
        XOR     A
        CALL    FILLB
        POP     DE
        LD      E,5
        LD      HL,SDASH
        LD      C,1*4+0
        CALL    TXTAT
        POP     BC
WS6:    INC     C
        LD      A,C
        CP      SEATS
        JR      NZ,WS3
        LD      HL,SROUNDS      ; ROUNDS: n  (the host's L/R)
        LD      DE,10*TXH*256+1
        LD      C,3*4+0
        CALL    TXTAT
        LD      A,(ST+OFF_ROUNDS)
        LD      B,1
        CALL    NUMN
        LD      HL,SLR
        LD      A,(MYSEAT)
        LD      B,A
        LD      A,(ST+OFF_HOST)
        CP      B
        JR      Z,WS7
        LD      HL,SNOLR
WS7:    LD      DE,10*TXH*256+11
        LD      C,2*4+0
        CALL    TXTAT
        LD      HL,SWATCH       ; what next
        LD      A,(MYSEAT)
        CP      NONE
        JR      Z,WS8
        LD      HL,SWAITH
        LD      B,A
        LD      A,(ST+OFF_HOST)
        CP      B
        JR      NZ,WS8
        LD      HL,SNEED2
        LD      A,(T2)
        CP      2
        JR      C,WS8
        LD      HL,SSTART
WS8:    LD      DE,10*TXH*256+24
        LD      B,15
        LD      C,1*4+0
        JP      TXTN

; WFRAME: the host's: left and right (pushed) the rounds, FIRE starts.
WFRAME: LD      A,(ST+OFF_HOST)
        LD      B,A
        LD      A,(MYSEAT)
        CP      B
        RET     NZ
        LD      A,(PRVDIR)      ; pushed this frame
        CPL
        LD      B,A
        LD      A,(INDIR)
        AND     B
        LD      (T0),A
        BIT     2,A             ; D_LEFT
        JR      Z,WF1
        LD      A,(ST+OFF_ROUNDS)
        CP      2
        JR      C,WF1
        DEC     A
        LD      D,A
        LD      B,ACT_ROUNDS
        LD      E,0
        CALL    ACTQ
        SND     SND_SELECT
WF1:    LD      A,(T0)
        BIT     3,A             ; D_RIGHT
        JR      Z,WF2
        LD      A,(ST+OFF_ROUNDS)
        CP      MAX_ROUNDS
        JR      NC,WF2
        INC     A
        LD      D,A
        LD      B,ACT_ROUNDS
        LD      E,0
        CALL    ACTQ
        SND     SND_SELECT
WF2:    LD      A,(EVBTN)
        OR      A
        RET     Z
        LD      B,ACT_START
        LD      DE,0
        CALL    ACTQ
        SND     SND_SELECT
        RET

; ---- the trophies ---------------------------------------------------------------
; Row 0 the winner (or a draw), big; row 1 the round (or sudden death);
; each seated seat on its strip (its bomber, name and wins), a gold trophy
; a win under it; row 11 the time to the next. The winner's bomber cheers.
RESENTER: LD    A,(SHOWN)
        SUB     S_GAME          ; (0: the game's end)
        LD      (RESFIN),A
        LD      A,(ST+OFF_RESULT)
        LD      (RESSEAT),A
        LD      A,NONE
        LD      (RESTMR),A
        LD      (T3),A
        CALL    RSCR
        LD      A,(RESSEAT)     ; the winner: <name> WINS
        CP      SEATS
        JR      NC,RE2
        LD      C,A
        CALL    NAMEHL
        LD      DE,WORK         ; (its name, then " WINS", in WORK)
        LD      B,NAME_LEN
RE1:    LD      A,(HL)
        OR      A
        JR      Z,RE1A
        LD      (DE),A
        INC     HL
        INC     DE
        DJNZ    RE1
RE1A:   LD      HL,SWINS
        LD      BC,6
        LDIR
        LD      HL,WORK
        JR      RE3
RE2:    LD      HL,SDRAW        ; none
        LD      A,(RESFIN)
        OR      A
        JR      NZ,RE3
        LD      HL,SNOWIN
RE3:    PUSH    HL              ; big, centred: 2 cells a letter
        CALL    STRLEN
        ADD     A,A
        LD      E,A
        LD      A,BYTEPL
        SUB     E
        SRL     A
        LD      E,A
        LD      D,0
        POP     HL
        LD      C,3*4+0
        CALL    BIGAT
        LD      A,(RESFIN)      ; a round's: ROUND n, or SUDDEN DEATH
        OR      A
        JR      Z,RE5
        LD      A,(ST+OFF_SUDDEN)
        OR      A
        JR      Z,RE4
        LD      HL,SSUDDEN
        LD      D,TXH
        LD      C,1*4+0
        CALL    CENTRE
        JR      RE5
RE4:    LD      HL,SRROUND
        LD      DE,TXH*256+16
        LD      C,1*4+0
        CALL    TXTAT
        LD      A,(ST+OFF_ROUND)
        LD      B,1
        CALL    NUMN
RE5:    LD      C,0             ; the seats, each on its row (2 + 2 s)
RE6:    LD      B,0
        LD      HL,RESROW
        ADD     HL,BC
        LD      (HL),0
        CALL    SEATBIT
        LD      A,(ST+OFF_SEATED)
        AND     B
        JP      Z,RE9
        LD      A,C
        ADD     A,A
        ADD     A,2
        LD      (HL),A
        LD      (T0),A
        LD      B,A
        LD      A,FR_DOWN0
        PUSH    BC
        CALL    SEATROW
        POP     BC
        PUSH    BC
        CALL    NAMEHL          ; its name, its wins
        LD      A,(T0)
        CALL    ROWLN
        LD      E,5
        LD      B,NAME_LEN
        LD      C,3*4+1
        CALL    TXTN
        POP     BC
        PUSH    BC
        LD      A,C             ; its wins: ST + OFF_PLAYERS + 10 s + P_WINS
        ADD     A,A
        LD      E,A
        ADD     A,A
        ADD     A,A
        ADD     A,E
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_PLAYERS+P_WINS
        ADD     HL,DE
        LD      A,(HL)
        LD      (T2),A
        PUSH    AF
        LD      A,(T0)
        CALL    ROWLN
        LD      E,16
        POP     AF
        LD      B,2
        LD      C,2*4+1
        CALL    NUMAT
        LD      A,(T2)          ; a trophy a win (18 at most), on the row under
        OR      A
        JR      Z,RE8
        CP      19
        JR      C,RE7A
        LD      A,18
RE7A:   LD      B,A
        LD      A,(T0)
        INC     A
        CALL    ROWLN
        LD      E,2
RE7B:   PUSH    BC
        PUSH    DE
        LD      A,T_TROPHY
        FAR     PG_ICON,DRICON
        POP     DE
        INC     E
        INC     E
        POP     BC
        DJNZ    RE7B
RE8:    POP     BC
RE9:    INC     C
        LD      A,C
        CP      SEATS
        JP      NZ,RE6
        LD      A,(RESFIN)      ; the jingle
        OR      A
        JR      NZ,RE10
        SND     MUS_GAME
        RET
RE10:   LD      A,(RESSEAT)
        CP      SEATS
        RET     NC
        SND     MUS_WIN
        RET

; RESFRAME: the winner cheers; the timer, when it changes.
RESFRAME: LD    A,(RESSEAT)
        CP      SEATS
        JR      NC,RF1
        LD      C,A
        LD      B,0
        LD      HL,RESROW
        ADD     HL,BC
        LD      A,(HL)
        OR      A
        JR      Z,RF1
        LD      B,A
        LD      A,(FRML)        ; its two frames, 16 jiffies each
        RRCA
        RRCA
        RRCA
        RRCA
        AND     1
        ADD     A,FR_WIN0
        LD      HL,T3           ; (drawn when it turns: T3 the one shown)
        CP      (HL)
        JR      Z,RF1
        LD      (HL),A
        CALL    SEATROWB
RF1:    LD      A,(ST+OFF_TIMER)
        LD      HL,RESTMR
        CP      (HL)
        RET     Z
        LD      (HL),A
        LD      HL,SNEXT
        LD      A,(RESFIN)
        OR      A
        JR      NZ,RF2
        LD      HL,SBACK
RF2:    LD      DE,11*TXH*256+10
        LD      C,1*4+0
        CALL    TXTAT
        LD      A,(RESTMR)
        LD      B,2
        JP      NUMN

; SEATROWB: seat C's bomber in frame A at byte 1 of text row B (its strip
; there already).
SEATROWB: PUSH  AF
        LD      A,B
        CALL    ROWLN
        POP     AF
        LD      B,A
        LD      A,C
        LD      E,4
        LD      C,55H
        FAR     PG_ICON,DRSTRIP
        RET

SROOM:  DB      "ROOM ",0
SHOST:  DB      "HOST",0
SDASH:  DB      "--------",0
SROUNDS: DB     "ROUNDS: ",0
SLR:    DB      "L/R",0
SNOLR:  DB      "   ",0
SWATCH: DB      "WATCHING",0
SWAITH: DB      "WAIT FOR HOST",0
SNEED2: DB      "NEED 2 PLAYERS",0
SSTART: DB      "FIRE: START",0
SLEAVE: DB      "C: LEAVE",0
SWINS:  DB      " WINS",0
SDRAW:  DB      "DRAW",0
SNOWIN: DB      "NO WINNER",0
SSUDDEN: DB     "SUDDEN DEATH",0
SRROUND: DB     "ROUND ",0
SNEXT:  DB      "NEXT ROUND IN ",0
SBACK:  DB      "BACK TO ROOM ",0

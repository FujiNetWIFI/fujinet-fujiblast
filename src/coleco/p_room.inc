; p_room.inc -- the screens at a table besides the arena (src/screens.c):
; the waiting room, the trophies after a round and after the game. Which
; one is SHOWN's (main.inc). The Astrocade's (src/astrocade/src/p_room.inc),
; laid out for the Adam's screen (src/adam/gfx.c): the bombers sprites.
;
; The draw routines clobber every register, so a loop keeps its state in
; RAM: T0 the row, T1 the seat, T2 the count, T3 the frame shown.

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

; SEATTC: A = seat C's text colour byte (its TMS colour on blue). Keeps all else.
SEATTC: PUSH    HL
        PUSH    BC
        LD      B,0
        LD      HL,STCOL
        ADD     HL,BC
        LD      A,(HL)
        POP     BC
        POP     HL
        RET
STCOL:  DB      0F4H,84H,74H,0B4H

; ROWBLANK: row D cleared. Clobbers all.
ROWBLANK: LD    HL,SNOB
        LD      E,0
        LD      B,32
        LD      C,TC_WHITE
        JP      TXTN

; ---- the waiting room --------------------------------------------------------
; Row 0 the room; rows 3, 7, 11, 15 the seats (a bomber on each seated one's
; row, HOST by the host's); row 19 the rounds; row 21 what happens next;
; row 23 the key that leaves.
WENTER: CALL    GFXBEGIN
        LD      HL,SLEAVE
        LD      D,23
        LD      C,TC_WHITE
        CALL    CENTRE
        CALL    WSTATE
        JP      GFXSHOW

WSTATE: LD      D,0
        CALL    ROWBLANK
        LD      HL,SROOM        ; ROOM <table>
        LD      DE,WORK
        LD      BC,5
        LDIR
        LD      HL,SRVTBL
        LD      BC,TABLE_LEN+1
        LDIR
        LD      HL,WORK
        LD      D,0
        LD      C,TC_CYAN
        CALL    CENTRE
        XOR     A
        LD      (T2),A          ; T2: players seated
        LD      (T1),A          ; T1: the seat
WS3:    LD      A,(T1)          ; the seat's row: 3 + 4 s
        ADD     A,A
        ADD     A,A
        ADD     A,3
        LD      (T0),A
        LD      A,(T1)
        LD      C,A
        CALL    SEATBIT
        LD      A,(ST+OFF_SEATED)
        AND     B
        JR      Z,WS5
        LD      HL,T2
        INC     (HL)
        LD      A,(T1)          ; its name
        LD      C,A
        CALL    NAMEHL
        LD      A,(T1)
        LD      C,A
        CALL    SEATTC
        LD      C,A
        LD      A,(T0)
        LD      D,A
        LD      E,4
        LD      B,NAME_LEN
        CALL    TXTN
        LD      A,(T1)          ; the host's: HOST
        LD      B,A
        LD      A,(ST+OFF_HOST)
        CP      B
        LD      HL,SHOST
        JR      Z,WS4
        LD      HL,SNOB
WS4:    LD      A,(T0)
        LD      D,A
        LD      E,14
        LD      B,4
        LD      C,TC_WHITE
        CALL    TXTN
        LD      A,(T0)          ; its bomber: x 6, y the row's (SPRAT: y - 8)
        ADD     A,A
        ADD     A,A
        ADD     A,A
        ADD     A,8
        LD      C,A
        LD      A,(T1)
        LD      E,A
        LD      D,FR_DOWN0
        LD      B,6
        CALL    SPRAT
        JR      WS6
WS5:    LD      A,(T0)          ; empty: dashes, no bomber
        LD      D,A
        LD      E,4
        LD      HL,SDASH
        LD      B,NAME_LEN
        LD      C,TC_WHITE
        CALL    TXTN
        LD      A,(T0)
        LD      D,A
        LD      E,14
        LD      HL,SNOB
        LD      B,4
        LD      C,TC_WHITE
        CALL    TXTN
        LD      A,(T1)
        CALL    SPROFF
WS6:    LD      HL,T1
        INC     (HL)
        LD      A,(HL)
        CP      SEATS
        JP      NZ,WS3
        LD      HL,SROUNDS      ; ROUNDS: n  (the host's L/R)
        LD      DE,19*256+10
        LD      C,TC_CYAN
        CALL    TXTAT
        LD      A,(ST+OFF_ROUNDS)
        LD      B,1
        LD      DE,19*256+18
        LD      C,TC_WHITE
        CALL    NUMAT
        LD      A,(MYSEAT)
        LD      B,A
        LD      A,(ST+OFF_HOST)
        CP      B
        LD      HL,SLR
        JR      Z,WS7
        LD      HL,SNOLR
WS7:    LD      DE,19*256+21
        LD      B,5
        LD      C,TC_WHITE
        CALL    TXTN
        LD      D,21
        CALL    ROWBLANK
        LD      HL,SWATCH       ; what next
        LD      C,TC_WHITE
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
        LD      C,TC_YELLOW
WS8:    LD      D,21
        JP      CENTRE

; WFRAME: the host's: left and right (pushed) the rounds, FIRE starts.
WFRAME: CALL    SPRFLUSH
        LD      A,(ST+OFF_HOST)
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
; Row 0 the winner (or a draw); row 1 the round (or sudden death); each
; seated seat on its rows (3 + 5 k): its name, its wins, a trophy a win
; under it, its bomber; row 23 the time to the next. The winner's bomber
; cheers (the game's end: a doubled one at the right).
RESENTER: LD    A,(SHOWN)
        SUB     S_GAME          ; (0: the game's end)
        LD      (RESFIN),A
        LD      A,(ST+OFF_RESULT)
        LD      (RESSEAT),A
        LD      A,NONE
        LD      (RESTMR),A
        LD      (T3),A
        CALL    GFXBEGIN
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
        LD      A,(RESFIN)      ; the game's: WINS!
        OR      A
        JR      NZ,RE1B
        DEC     DE
        LD      A,'!'
        LD      (DE),A
        INC     DE
        XOR     A
        LD      (DE),A
RE1B:   LD      A,(RESSEAT)
        LD      C,A
        CALL    SEATTC
        LD      C,A
        LD      HL,WORK
        JR      RE3
RE2:    LD      HL,SDRAW        ; none: a round is a draw
        LD      A,(RESFIN)
        OR      A
        JR      NZ,RE2A
        LD      HL,SNOWIN
RE2A:   LD      C,TC_WHITE
RE3:    LD      D,0
        CALL    CENTRE
        LD      A,(RESFIN)      ; a round's: ROUND n, or SUDDEN DEATH
        OR      A
        JR      Z,RE5
        LD      A,(ST+OFF_SUDDEN)
        OR      A
        JR      Z,RE4
        LD      HL,SSUDDEN
        LD      D,1
        LD      C,TC_CYAN
        CALL    CENTRE
        JR      RE5
RE4:    LD      HL,SRROUND
        LD      DE,1*256+12
        LD      C,TC_CYAN
        CALL    TXTAT
        LD      A,(ST+OFF_ROUND)
        LD      B,1
        LD      DE,1*256+18
        LD      C,TC_CYAN
        CALL    NUMAT
RE5:    XOR     A
        LD      (T1),A          ; T1: the seat
        LD      (T2),A          ; T2: seats shown so far
RE6:    LD      A,(T1)
        LD      E,A
        LD      D,0
        LD      HL,RESROW
        ADD     HL,DE
        LD      (HL),0
        LD      C,E
        CALL    SEATBIT
        LD      A,(ST+OFF_SEATED)
        AND     B
        JP      Z,RE9
        LD      A,(T2)          ; its row: 3 + 5 k (RESROW: the row + 1)
        LD      B,A
        ADD     A,A
        ADD     A,A
        ADD     A,B
        ADD     A,3
        LD      (T0),A
        INC     A
        LD      (HL),A
        LD      A,(T1)          ; its name
        LD      C,A
        CALL    NAMEHL
        LD      A,(T1)
        LD      C,A
        CALL    SEATTC
        LD      C,A
        LD      A,(T0)
        LD      D,A
        LD      E,4
        LD      B,NAME_LEN
        CALL    TXTN
        LD      A,(T1)          ; its wins: ST + OFF_PLAYERS + 10 s + P_WINS
        LD      B,A
        ADD     A,A
        LD      E,A
        LD      A,B
        ADD     A,A
        ADD     A,A
        ADD     A,A
        ADD     A,E
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_PLAYERS+P_WINS
        ADD     HL,DE
        LD      A,(HL)
        LD      (T3),A          ; (T3 the wins; RESFRAME's own use comes after)
        LD      B,2
        LD      A,(T0)
        LD      D,A
        LD      E,14
        LD      C,TC_WHITE
        LD      A,(T3)
        CALL    NUMAT
        LD      A,(T3)          ; a trophy a win (15 at most), on the row under
        CP      16
        JR      C,RE7
        LD      A,15
RE7:    LD      B,A
        LD      A,(T0)
        INC     A
        LD      D,A
        LD      E,10
        CALL    GFXTROPH
        LD      A,(T0)          ; its bomber: x 6, y the name row's
        INC     A
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      C,A
        LD      A,(T1)
        LD      E,A
        LD      D,FR_DOWN0
        LD      B,6
        CALL    SPRAT
        LD      HL,T2
        INC     (HL)
        JR      RE9A
RE9:    LD      A,(T1)
        CALL    SPROFF
RE9A:   LD      HL,T1
        INC     (HL)
        LD      A,(HL)
        CP      SEATS
        JP      NZ,RE6
        LD      A,NONE          ; (RESFRAME: no frame shown yet)
        LD      (T3),A
        LD      A,(RESFIN)      ; the game's end: its winner, doubled, at the right
        OR      A
        JR      NZ,RE10
        LD      A,(RESSEAT)
        CP      SEATS
        JR      NC,RE10
        LD      E,A
        OR      4
        LD      E,A
        LD      A,(RESSEAT)
        LD      B,136
        LD      C,96
        LD      D,FR_WIN0
        CALL    SPRAT
RE10:   CALL    SPRFLUSH
        CALL    GFXSHOW
        LD      A,(RESFIN)      ; the jingle
        OR      A
        JR      NZ,RE11
        SND     MUS_GAME
        RET
RE11:   LD      A,(RESSEAT)
        CP      SEATS
        RET     NC
        SND     MUS_WIN
        RET

; RESFRAME: the winner cheers; the timer, when it changes.
RESFRAME: LD    A,(FRML)        ; its two frames, 16 jiffies each
        RRCA
        RRCA
        RRCA
        RRCA
        AND     1
        ADD     A,FR_WIN0
        LD      D,A
        LD      A,(RESSEAT)
        CP      SEATS
        JR      NC,RF1
        LD      C,A
        LD      B,0
        LD      A,(RESFIN)
        OR      A
        JR      NZ,RF0
        LD      A,C             ; the game's: the doubled one
        LD      E,A
        OR      4
        LD      E,A
        LD      A,C
        LD      B,136
        LD      C,96
        CALL    SPRAT
        JR      RF1
RF0:    LD      HL,RESROW
        ADD     HL,BC
        LD      A,(HL)
        OR      A
        JR      Z,RF1
        ADD     A,A             ; its row: the name row + 1, in pixels
        ADD     A,A
        ADD     A,A
        LD      B,6
        LD      E,C
        LD      C,A
        LD      A,E
        CALL    SPRAT
RF1:    CALL    SPRFLUSH
        LD      A,(ST+OFF_TIMER)
        LD      HL,RESTMR
        CP      (HL)
        RET     Z
        LD      (HL),A
        LD      D,23
        CALL    ROWBLANK
        LD      HL,SNEXT
        LD      E,8
        LD      A,(RESFIN)
        OR      A
        JR      NZ,RF2
        LD      HL,SBACK
        LD      E,4
RF2:    PUSH    DE
        LD      D,23
        LD      C,TC_WHITE
        CALL    TXTAT
        POP     DE
        LD      A,E             ; the number after the text (14 or 20 cells)
        ADD     A,14
        LD      E,A
        LD      A,(RESFIN)
        OR      A
        JR      NZ,RF3
        LD      A,E
        ADD     A,6
        LD      E,A
RF3:    LD      D,23
        LD      A,(RESTMR)
        LD      B,2
        LD      C,TC_WHITE
        JP      NUMAT

SNOB:   DB      0               ; (a string is read from this page: page 0's SNONE is not here)
SROOM:  DB      "ROOM ",0
SHOST:  DB      "HOST",0
SDASH:  DB      "--------",0
SROUNDS: DB     "ROUNDS:",0
SLR:    DB      "(L/R)",0
SNOLR:  DB      "     ",0
SWATCH: DB      "WATCHING: THE ROOM IS FULL",0
SWAITH: DB      "WAITING FOR THE HOST TO START",0
SNEED2: DB      "WAITING FOR PLAYERS",0
SSTART: DB      "PRESS FIRE TO START",0
SLEAVE: DB      "*: LEAVE   9: SOUND",0
SWINS:  DB      " WINS",0
SDRAW:  DB      "DRAW",0
SNOWIN: DB      "NO WINNER",0
SSUDDEN: DB     "SUDDEN DEATH",0
SRROUND: DB     "ROUND ",0
SNEXT:  DB      "NEXT ROUND IN ",0
SBACK:  DB      "BACK TO THE ROOM IN ",0

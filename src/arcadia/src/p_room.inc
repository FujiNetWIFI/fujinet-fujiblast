; p_room.inc -- the screens at a table besides the arena (src/screens.c):
; the waiting room, the trophies after a round and after the game. Which
; one is SHOWN's (main.inc).

        BCTA,UN RENTER          ; E_ENTER: drawn afresh
        BCTA,UN RSTATE          ; E_STATE: a new state
        BCTA,UN RTICK           ; E_TICK: a frame

RENTER: LODA,R0 SHOWN
        COMI,R0 S_WAIT
        BCTA,EQ WENTER
        BCTA,UN RESENTER
RSTATE: LODA,R0 SHOWN
        COMI,R0 S_WAIT
        BCTA,EQ WSTATE
        RETC,UN
RTICK:  LODA,R0 SHOWN
        COMI,R0 S_WAIT
        BCTA,EQ WFRAME
        BCTA,UN RESFRAME

; TEXTSCR: a text screen: cleared, the sprites off, the text's user glyphs
; ('>' '-' ':' and the trophy), the bombers' colours. Calls two down.
; Clobbers R0-R3.
TEXTSCR: ZBSR   *ZSCRCLR
        LODI,R0 A_CURSOR>>8
        LODI,R1 A_CURSOR&$FF
        LODI,R2 GCURSOR-GUDC
        ZBSR    *ZUDCSET
        LODI,R0 A_DASH>>8
        LODI,R1 A_DASH&$FF
        LODI,R2 GDASH-GUDC
        ZBSR    *ZUDCSET
        LODI,R0 A_COLON>>8
        LODI,R1 A_COLON&$FF
        LODI,R2 GCOLON-GUDC
        ZBSR    *ZUDCSET
        LODI,R0 A_TROPHY>>8
        LODI,R1 A_TROPHY&$FF
        LODI,R2 GTROPHY-GUDC
        ZBSR    *ZUDCSET
        LODI,R1 0
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
        DCOL    TXTI
        RETC,UN

; SEATSPR: seat R3's bomber at column 1 of row R0, facing us. Leaf.
; Clobbers R0-R2.
SEATSPR: RRL,R0                 ; Y: 256 - 16 row
        RRL,R0
        RRL,R0
        RRL,R0
        STRZ    R1
        EORZ    R0
        SUBZ    R1
        STRZ    R1
        LODZ    R3
        ADDZ    R3
        STRZ    R2
        LODZ    R1
        STRA,R0 SPRSH,R2
        LODI,R0 SPRX0
        STRA,R0 SPRSH+1,R2
        LODI,R0 FR_DOWN0
        STRA,R0 FRWANT,R3
        RETC,UN
; SPRHIDE: seat R3's bomber off. Leaf. Clobbers R0, R2.
SPRHIDE: LODZ   R3
        ADDZ    R3
        STRZ    R2
        EORZ    R0
        STRA,R0 SPRSH,R2
        RETC,UN

; NAMEAT: seat R3's name (ASCII, NUL-padded) at the next cells, R1 of
; them at most, in DCOLOR. Calls two down. Clobbers R0, R1; R3 kept.
NAMEAT: STRA,R3 T3
        LODA,R0 NAMEOFF,R3
        SUBI,R0 1
        STRZ    R3
NA1:    LODA,R0 ST+OFF_NAMES,R3,+
        BCTR,EQ NA2
        BSTA,UN A2G
        ZBSR    *ZDPUTC
        BDRR,R1 NA1
NA2:    LODA,R3 T3
        RETC,UN

; ---- the waiting room --------------------------------------------------------
; Row 0 the room; rows 2, 4, 6, 8 the seats (a bomber beside each seated
; one, "HOST" by the host's); row 10 the rounds; row 11 what happens next;
; row 12 the key that leaves.
WENTER: BSTA,UN TEXTSCR
        LODI,R0 SLEAVE>>8
        LODI,R1 SLEAVE&$FF
        LODI,R3 12
        ZBSR    *ZCENTR
WSTATE: LODI,R1 0
        LODI,R3 1
        ZBSR    *ZDCLR
        DAT     0,2             ; ROOM <table>
        DCOL    TXTI
        TXTAT   SROOM
        LODI,R3 $FF
WS1:    LODA,R0 SRVTBL,R3,+
        BCTR,EQ WS2
        BSTA,UN A2G
        ZBSR    *ZDPUTC
        COMI,R3 TABLE_LEN-1
        BCFR,EQ WS1
WS2:    EORZ    R0
        STRA,R0 T2              ; T2: players seated
        LODI,R3 0
WS3:    STRA,R3 T1              ; the seats
        LODZ    R3
        ADDZ    R3
        ADDI,R0 2
        STRA,R0 T0              ; its row
        STRZ    R1
        LODI,R3 1
        ZBSR    *ZDCLR
        LODA,R3 T1
        LODA,R0 ST+OFF_SEATED
        ANDA,R0 SEATBIT,R3
        BCTR,EQ WS5
        LODA,R0 T2
        ADDI,R0 1
        STRA,R0 T2
        LODA,R0 HUDINK,R3       ; seated: its name, in its colour
        STRA,R0 DCOLOR
        LODA,R1 T0
        LODI,R2 2
        ZBSR    *ZDPOS
        LODA,R3 T1
        LODI,R1 NAME_LEN
        BSTA,UN NAMEAT
        LODA,R0 ST+OFF_HOST     ; the host's: HOST
        COMA,R0 T1
        BCFR,EQ WS4
        LODA,R1 T0
        LODI,R2 11
        ZBSR    *ZDPOS
        TXTAT   SHOST
WS4:    LODA,R3 T1
        LODA,R0 T0
        BSTA,UN SEATSPR
        BCTR,UN WS6
WS5:    DCOL    TXTI            ; empty: dashes
        LODA,R1 T0
        LODI,R2 2
        ZBSR    *ZDPOS
        LODI,R0 GDASH|TXTI
        LODI,R3 NAME_LEN
        ZBSR    *ZDFILL
        LODA,R3 T1
        BSTA,UN SPRHIDE
WS6:    LODA,R3 T1
        ADDI,R3 1
        COMI,R3 SEATS
        BCFA,EQ WS3
        LODI,R1 10              ; ROUNDS: n  (the host's L/R)
        LODI,R3 2
        ZBSR    *ZDCLR
        DCOL    TXTI
        DAT     10,1
        TXTAT   SROUNDS
        LODA,R0 ST+OFF_ROUNDS
        LODI,R1 1
        ZBSR    *ZDNUM
        LODA,R0 MYSEAT
        COMA,R0 ST+OFF_HOST
        BCFR,EQ WS7
        DAT     10,12
        TXTAT   SLR
WS7:    LODI,R0 SWATCH>>8       ; what next
        LODI,R1 SWATCH&$FF
        LODA,R2 MYSEAT
        COMI,R2 NONE
        BCTR,EQ WS8
        LODI,R0 SWAITH>>8
        LODI,R1 SWAITH&$FF
        COMA,R2 ST+OFF_HOST
        BCFR,EQ WS8
        LODI,R0 SNEED2>>8
        LODI,R1 SNEED2&$FF
        LODA,R2 T2
        COMI,R2 2
        BCTR,LT WS8
        LODI,R0 SSTART>>8
        LODI,R1 SSTART&$FF
WS8:    LODI,R3 11
        ZBRR    *ZCENTR         ; tail call

; WFRAME: the host's: left and right (pushed) the rounds, FIRE starts.
; Calls one down.
WFRAME: LODA,R0 MYSEAT
        COMA,R0 ST+OFF_HOST
        RETC,LT
        RETC,GT
        LODA,R0 INDIR           ; pushed this frame
        STRA,R0 T0
        LODA,R0 PRVDIR
        EORI,R0 $FF
        ANDA,R0 T0
        STRA,R0 T0
        TMI,R0  D_LEFT
        BCFR,EQ WF1
        LODA,R2 ST+OFF_ROUNDS
        COMI,R2 2
        BCTR,LT WF1
        SUBI,R2 1
        LODI,R1 ACT_ROUNDS
        LODI,R3 0
        ZBSR    *ZACTQ
        SND     SND_SELECT
WF1:    LODA,R0 T0
        TMI,R0  D_RIGHT
        BCFR,EQ WF2
        LODA,R2 ST+OFF_ROUNDS
        COMI,R2 MAX_ROUNDS
        BCFR,LT WF2
        ADDI,R2 1
        LODI,R1 ACT_ROUNDS
        LODI,R3 0
        ZBSR    *ZACTQ
        SND     SND_SELECT
WF2:    LODA,R0 EVBTN
        RETC,EQ
        LODI,R1 ACT_START
        LODI,R2 0
        LODI,R3 0
        ZBSR    *ZACTQ
        SND     SND_SELECT
        RETC,UN

; ---- the trophies ---------------------------------------------------------------
; Row 0 the winner (or a draw); row 1 the round (or sudden death); two
; rows a seated seat: its bomber, name and wins, then a trophy a win;
; row 12 the time to the next. The winner's bomber cheers.
RESENTER:
        LODA,R0 SHOWN
        SUBI,R0 S_GAME          ; (0: the game's end)
        STRA,R0 RESFIN
        LODA,R0 ST+OFF_RESULT
        STRA,R0 RESSEAT
        LODI,R0 NONE
        STRA,R0 RESTMR
        BSTA,UN TEXTSCR
        LODA,R3 RESSEAT         ; the winner: <name> WINS
        COMI,R3 SEATS
        BCFR,LT RE2
        LODA,R0 HUDINK,R3
        STRA,R0 DCOLOR
        LODI,R2 $FF             ; (its name, then " WINS", into SCRATCH)
        LODA,R0 NAMEOFF,R3
        SUBI,R0 1
        STRZ    R3
        LODI,R1 NAME_LEN
RE1:    LODA,R0 ST+OFF_NAMES,R3,+
        BCTR,EQ RE1A
        BSTA,UN A2G
        STRA,R0 SCRATCH,R2,+
        BDRR,R1 RE1
RE1A:   LODI,R3 $FF
RE1B:   LODA,R0 SWINS,R3,+
        STRA,R0 SCRATCH,R2,+
        COMI,R0 $FF
        BCFR,EQ RE1B
        LODI,R0 SCRATCH>>8
        LODI,R1 SCRATCH&$FF
        BCTR,UN RE3
RE2:    LODI,R0 SDRAW>>8        ; none
        LODI,R1 SDRAW&$FF
        LODA,R2 RESFIN
        BCFR,EQ RE3
        LODI,R0 SNOWIN>>8
        LODI,R1 SNOWIN&$FF
RE3:    LODI,R3 0
        ZBSR    *ZCENTR
        DCOL    TXTI
        LODA,R0 RESFIN          ; a round's: ROUND n, or SUDDEN DEATH
        BCTR,EQ RE5
        LODA,R0 ST+OFF_SUDDEN
        BCTR,EQ RE4
        LODI,R0 SSUDDEN>>8
        LODI,R1 SSUDDEN&$FF
        LODI,R3 1
        ZBSR    *ZCENTR
        BCTR,UN RE5
RE4:    DAT     1,5
        TXTAT   SROUND
        LODA,R0 ST+OFF_ROUND
        LODI,R1 1
        ZBSR    *ZDNUM
RE5:    LODI,R0 3               ; the seats: T0 the row
        STRA,R0 T0
        LODI,R3 0
RE6:    STRA,R3 T1
        EORZ    R0
        STRA,R0 RESROW,R3
        LODA,R0 ST+OFF_SEATED
        ANDA,R0 SEATBIT,R3
        BCFR,EQ RE7
        BSTA,UN SPRHIDE
        BCTA,UN RE9
RE7:    LODA,R0 T0
        STRA,R0 RESROW,R3
        BSTA,UN SEATSPR
        LODA,R3 T1
        LODA,R0 HUDINK,R3       ; its name, its wins
        STRA,R0 DCOLOR
        LODA,R1 T0
        LODI,R2 2
        ZBSR    *ZDPOS
        LODA,R3 T1
        LODI,R1 NAME_LEN
        BSTA,UN NAMEAT
        LODA,R1 T0
        LODI,R2 12
        ZBSR    *ZDPOS
        LODA,R3 T1
        LODA,R0 SEATOFF,R3
        STRZ    R3
        LODA,R0 ST+OFF_PLAYERS+P_WINS,R3
        STRA,R0 T2
        LODI,R1 2
        ZBSR    *ZDNUM
        LODA,R1 T0              ; a trophy a win (13 at most)
        ADDI,R1 1
        LODI,R2 2
        ZBSR    *ZDPOS
        LODA,R3 T2
        BCTR,EQ RE8
        COMI,R3 14
        BCTR,LT RE7A
        LODI,R3 13
RE7A:   LODI,R0 GTROPHY
        IORA,R0 DCOLOR
        ZBSR    *ZDFILL
RE8:    LODA,R0 T0
        ADDI,R0 2
        STRA,R0 T0
RE9:    LODA,R3 T1
        ADDI,R3 1
        COMI,R3 SEATS
        BCFA,EQ RE6
        LODA,R0 RESFIN          ; the jingle
        BCFR,EQ RE10
        SND     MUS_GAME
        RETC,UN
RE10:   LODA,R0 RESSEAT
        COMI,R0 SEATS
        RETC,GT
        RETC,EQ
        SND     MUS_WIN
        RETC,UN

; RESFRAME: the winner cheers; the timer, when it changes.
RESFRAME:
        LODA,R3 RESSEAT
        COMI,R3 SEATS
        BCFR,LT RF1
        LODA,R0 RESROW,R3
        BCTR,EQ RF1
        LODA,R0 FRML            ; its two frames, 16 jiffies each
        RRR,R0
        RRR,R0
        RRR,R0
        RRR,R0
        ANDI,R0 1
        ADDI,R0 FR_WIN0
        STRA,R0 FRWANT,R3
RF1:    LODA,R0 ST+OFF_TIMER
        COMA,R0 RESTMR
        RETC,EQ
        STRA,R0 RESTMR
        LODI,R1 12
        LODI,R3 1
        ZBSR    *ZDCLR
        DCOL    TXTI
        DAT     12,0
        LODA,R0 RESFIN
        BCTR,EQ RF2
        TXTAT   SNEXT
        BCTR,UN RF3
RF2:    TXTAT   SBACK
RF3:    LODA,R0 RESTMR
        LODI,R1 2
        ZBRR    *ZDNUM          ; tail call

; A2G: R0 an ASCII letter or digit to its glyph (anything else a space).
; Leaf.
A2G:    COMI,R0 $61
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

HUDINK: DB      HUDI0,HUDI1,HUDI2,HUDI3
NAMEOFF: DB     0,NAME_LEN,2*NAME_LEN,3*NAME_LEN
SROOM:  DB      "ROOM ",$FF
SHOST:  DB      "HOST",$FF
SROUNDS: DB     "ROUNDS: ",$FF
SLR:    DB      "L/R",$FF
SWATCH: DB      "WATCHING",$FF
SWAITH: DB      "WAIT FOR HOST",$FF
SNEED2: DB      "NEED 2 PLAYERS",$FF
SSTART: DB      "FIRE: START",$FF
SLEAVE: DB      "CLEAR: LEAVE",$FF
SWINS:  DB      " WINS",$FF
SDRAW:  DB      "DRAW",$FF
SNOWIN: DB      "NO WINNER",$FF
SSUDDEN: DB     "SUDDEN DEATH",$FF
SROUND: DB      "ROUND ",$FF
SNEXT:  DB      "NEXT ROUND IN ",$FF
SBACK:  DB      "BACK TO ROOM ",$FF

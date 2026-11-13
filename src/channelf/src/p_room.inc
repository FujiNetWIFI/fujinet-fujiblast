; ---------------------------------------------------------------------------
; p_room.inc -- page ROOM: the waiting room, a round's end and the game's
; (src/screens.c). The Astrocade's (src/astrocade/src/p_room.inc),
; transcribed for the Channel F: each seat a band of rows in its own
; background (the row palettes), its bomber a slot.
;
; Entries: E_ENTER drawn afresh; E_STATE a new state; E_TICK a pass.
; ---------------------------------------------------------------------------

RESFIN  EQU VARS+3E0H           ; 0: the game's end, else a round's
RESSEAT EQU VARS+3E1H           ; the winner (4+: none)
RESTMR  EQU VARS+3E2H           ; the countdown shown
RFTIME  EQU VARS+3E3H           ; the freeze's jiffies left (0: the results up)
RT0     EQU VARS+3E4H           ; scratch
RT1     EQU VARS+3E5H
RT2     EQU VARS+3E6H
RPREV   EQU VARS+3E7H           ; the stick last pass (edges)
RBUF    EQU VARS+3E8H           ; 16: "<NAME> WINS"

WSEATY  EQU 12                  ; the waiting room: seat s's band at 12 + 9 s
RSEATY  EQU 22                  ; the results: seat s's band at 22 + 8 s
FREEZE  EQU 60                  ; jiffies the arena stands frozen first

; the seats' bands' palettes (light blue, grey, green, grey)
SEATPAL: DB 1, 2, 3, 2

RENTER: ENTER
        LDB SHOWS
        CI S_WAIT
        BNZ REN1
        PI WENTER
        LEAVE
REN1:   PI RESENTER
        LEAVE
RSTATE: ENTER
        LDB SHOWS
        CI S_WAIT
        BNZ RST1
        PI WSTATE
RST1:   LEAVE
RTICK:  ENTER
        LDB SHOWS
        CI S_WAIT
        BNZ RTK1
        PI WFRAME
        LEAVE
RTK1:   PI RESFRAME
        LEAVE

; RSCR -- a room screen: cleared, black; the slots afresh.
RSCR:   ENTER
        CLR
        LR 0,A
        PI ECLS
        PI SLSEATS
        LEAVE

; NAMEDC -- DC0 = seat r0's name in ST.  Leaf.  Clobbers A.
NAMEDC: LR A,0
        SL 1
        SL 1
        SL 1
        DCI ST+OFF_NAMES
        ADC
        POP

; SEATBAND -- seat r0's band from row r1, r2 rows, its palette.
SEATBAND: ENTER
        LR A,0
        DCI SEATPAL
        ADC
        LM
        LR 2,A
        LR A,1
        LR 0,A
        AI 7
        LR 1,A
        PI EPAL
        LEAVE

; ---- the waiting room -----------------------------------------------------------
WENTER: ENTER
        PI RSCR
        LDB INPDIR
        STB RPREV
        PI WSTATE
        LEAVE

WSTATE: ENTER
        DCI S_ROOM              ; ROOM <table>, white on black
        LIS 4
        LR 1,A
        LI CV1
        LR 2,A
        LR 3,A
        PI ECENTX
        CLR
        STB RT2                 ; players seated
        STB RT0                 ; seat
WS3:    LDB RT0                 ; its band: 12 + 9 s
        LR 0,A
        SL 1
        SL 1
        SL 1
        AS 0
        AI WSEATY
        STB RT1
        LR 1,A
        PI SEATBAND
        LDB RT0
        DCI SEATBIT
        ADC
        LM
        LR 1,A
        LDB ST+OFF_SEATED
        NS 1
        BZ WS5
        INCB RT2
        LDB RT0                 ; seated: its bomber, its name
        LR 0,A
        LI 8
        LR 1,A
        LDB RT1
        AI 1
        LR 2,A
        LIS FR_DOWN0
        LR 3,A
        LIS 1
        LR 4,A
        PI SLSET
        LDB RT0
        SL 1
        DCI SEATINK
        ADC
        LM
        LR 6,A
        LM
        LR 7,A                  ; (STB's DCI would move DC0 between the two)
        LR A,6
        STB TXINK
        LR A,7
        STB TXINK2
        LI 18
        STB TXX
        LDB RT1
        STB TXY
        LDB RT0
        LR 0,A
        PI RNAME
        LDB ST+OFF_HOST         ; the host's: HOST
        LR 1,A
        LDB RT0
        XS 1
        BNZ WS6
        LI 70
        STB TXX
        DCI S_HOST
        PI ETXT
        BR WS6
WS5:    LDB RT0                 ; empty: dashes, no bomber
        LR 0,A
        PI SLOFF
        LI CV1
        STB TXINK
        STB TXINK2
        LI 18
        STB TXX
        LDB RT1
        STB TXY
        DCI S_DASH
        PI ETXT
WS6:    INCB RT0
        CI SEATS
        BZ WS6A
        JMP WS3
WS6A:   LI CV1                  ; ROUNDS: n  (the host's L/R)
        STB TXINK
        STB TXINK2
        LIS 6
        STB TXX
        LI 48
        STB TXY
        DCI S_ROUNDS
        PI ETXT
        LDB ST+OFF_ROUNDS
        LR 0,A
        PI RDIGIT
        LDB TXX                 ; (past the rounds)
        AI 4
        STB TXX
        LDB MYSEAT
        LR 1,A
        LDB ST+OFF_HOST
        XS 1
        DCI S_LR
        BZ WS7
        DCI S_NOLR
WS7:    PI ETXT
        CLR                     ; what next: watching, wait for the host, need
        LR 5,A                  ;  2 players, FIRE: START
        LDB MYSEAT
        CI 0FFH
        BZ WS8
        LIS 1
        LR 5,A
        LDB MYSEAT
        LR 1,A
        LDB ST+OFF_HOST
        XS 1
        BNZ WS8
        LIS 2
        LR 5,A
        LDB RT2
        CI 1
        BC WS8
        LIS 3
        LR 5,A
WS8:    LR A,5
        SL 1
        DCI WSMSG
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LI 55
        LR 1,A
        LI CV1
        LR 2,A
        LR 3,A
        PI ECENTX
        PI ESLOTS
        LEAVE

; WFRAME -- the host: left/right the rounds, FIRE starts.
WFRAME: ENTER
        PI ESLOTS
        LDB MYSEAT
        LR 1,A
        LDB ST+OFF_HOST
        XS 1
        BNZ WFX
        LDB RPREV               ; pushed this pass
        COM
        LR 4,A
        LDB INPDIR
        STB RPREV
        NS 4
        STB RT0
        NI IN_LEFT
        BZ WF1
        LDB ST+OFF_ROUNDS
        CI 1
        BC WF1
        AI -1
        LR 2,A
        LIS ACT_ROUNDS
        LR 1,A
        CLR
        LR 3,A
        PI ACTQ
        SND SND_SELECT
WF1:    LDB RT0
        NI IN_RIGHT
        BZ WF2
        LDB ST+OFF_ROUNDS
        CI MAX_ROUNDS-1
        BNC WF2
        INC
        LR 2,A
        LIS ACT_ROUNDS
        LR 1,A
        CLR
        LR 3,A
        PI ACTQ
        SND SND_SELECT
WF2:    LDB EVBTN
        NI 0FFH
        BZ WFX
        LIS ACT_START
        LR 1,A
        CLR
        LR 2,A
        LR 3,A
        PI ACTQ
        SND SND_SELECT
WFX:    LEAVE

; ---- a round's end, the game's ------------------------------------------------------
; First the arena stands frozen in black and white (its rows palette 0) for
; FREEZE jiffies; then the results: <NAME> WINS (or DRAW, NO WINNER) big,
; the round (or SUDDEN DEATH), each seat's band with its bomber, name, wins
; and a trophy a win; the winner cheering; the countdown.
RESENTER: ENTER
        LDB SHOWS
        AI -S_GAME
        STB RESFIN
        LDB ST+OFF_RESULT
        STB RESSEAT
        LI 0FFH
        STB RESTMR
        LI FREEZE
        STB RFTIME
        LI AY0                  ; the arena's rows: black and white
        LR 0,A
        LI AY1-1
        LR 1,A
        CLR
        LR 2,A
        PI EPAL
        LEAVE

RESFRAME: ENTER
        LDB RFTIME
        NI 0FFH
        BZ RF0
        LR 0,A                  ; frozen: count down
        LDB DT
        COM
        INC
        AS 0
        BZ RFD
        BNC RFD
        STB RFTIME
        LEAVE
RFD:    CLR
        STB RFTIME
        PI RESDRAW
        LEAVE
RF0:    PI ESLOTS               ; the winner cheering: win0/win1, 16 jiffies each
        LDB RESSEAT
        CI SEATS-1
        BNC RF1
        LR 0,A
        DCI SEATBIT
        ADC
        LM
        LR 1,A
        LDB ST+OFF_SEATED
        NS 1
        BZ RF1
        LDB RESSEAT             ; its band's place
        LR 0,A
        SL 1
        SL 1
        SL 1
        AI RSEATY+1
        LR 2,A
        LIS 6
        LR 1,A
        LDB JIFFY
        SR 4
        NI 1
        AI FR_WIN0
        LR 3,A
        LIS 1
        LR 4,A
        LDB RESSEAT
        LR 0,A
        PI SLSET
RF1:    LDB ST+OFF_TIMER        ; the countdown, when it turns
        LR 1,A
        LDB RESTMR
        XS 1
        BZ RFX
        LR A,1
        STB RESTMR
        LI CV1
        STB TXINK
        STB TXINK2
        LI 10
        STB TXX
        LI 56
        STB TXY
        LDB RESFIN
        NI 0FFH
        DCI S_NEXT
        BNZ RF2
        DCI S_BACK
RF2:    PI ETXT
        LDB RESTMR
        LR 2,A
        PI RDIG2
RFX:    LEAVE

; RESDRAW -- the results screen.
RESDRAW: ENTER
        PI RSCR
        LDB RESSEAT             ; the winner: <NAME> WINS
        CI SEATS-1
        BNC RE2
        LR 0,A
        PI NAMEDC
        XDC
        DCI RBUF
        LI NAME_LEN
        LR 2,A
RE1:    XDC
        LM
        XDC
        NI 0FFH
        BZ RE1A
        ST
        DS 2
        BNZ RE1
RE1A:   XDC                     ; " WINS" after it
        DCI S_WINS
        XDC
RE1B:   XDC
        LM
        XDC
        ST
        NI 0FFH
        BNZ RE1B
        DCI RBUF
        BR RE3
RE2:    LDB RESFIN              ; none: a round's DRAW, the game's NO WINNER
        NI 0FFH
        DCI S_DRAW
        BNZ RE3
        DCI S_NOWIN
RE3:    LR Q,DC                 ; big, centred: 8 a letter, white on black
        CLR
        LR 1,A
RE3A:   LM
        NI 0FFH
        BZ RE3B
        LR A,1
        INC
        LR 1,A
        BR RE3A
RE3B:   LR A,1                  ; x = 4 + (102 - 8 len) / 2 = 55 - 4 len
        SL 1
        SL 1
        COM
        INC
        AI 55
        STB TXX
        LIS 4
        STB TXY
        LI CV1
        STB TXINK
        STB TXINK2
        LR DC,Q
        PI EBTXT
        LDB RESFIN              ; a round's: ROUND n, or SUDDEN DEATH
        NI 0FFH
        BZ RE5
        LDB ST+OFF_SUDDEN
        NI 0FFH
        BZ RE4
        DCI S_SUDDEN
        LI 16
        LR 1,A
        LI CV2
        LR 2,A
        LR 3,A
        PI ECENTX
        BR RE5
RE4:    LI CV1
        STB TXINK
        STB TXINK2
        LI 39
        STB TXX
        LI 16
        STB TXY
        DCI S_RROUND
        PI ETXT
        LDB ST+OFF_ROUND
        LR 0,A
        PI RDIGIT
RE5:    CLR                     ; the seats, each a band (22 + 8 s)
        STB RT0
RE6:    LDB RT0
        DCI SEATBIT
        ADC
        LM
        LR 1,A
        LDB ST+OFF_SEATED
        NS 1
        BNZ RE6A
        JMP RE9
RE6A:   LDB RT0
        SL 1
        SL 1
        SL 1
        AI RSEATY
        STB RT1
        LR 1,A
        LDB RT0
        LR 0,A
        PI SEATBAND
        LDB RT0                 ; its bomber
        LR 0,A
        LIS 6
        LR 1,A
        LDB RT1
        INC
        LR 2,A
        LIS FR_DOWN0
        LR 3,A
        LIS 1
        LR 4,A
        PI SLSET
        LDB RT0                 ; its name, in its ink
        SL 1
        DCI SEATINK
        ADC
        LM
        LR 6,A
        LM
        LR 7,A                  ; (STB's DCI would move DC0 between the two)
        LR A,6
        STB TXINK
        LR A,7
        STB TXINK2
        LI 15
        STB TXX
        LDB RT1
        INC
        STB TXY
        LDB RT0
        LR 0,A
        PI RNAME
        LDB RT0                 ; its wins: a trophy each (6 at most)
        LR 0,A
        PI PLRECR
        LI P_WINS
        ADC
        LM
        CI 6
        BC RE7
        LIS 6
RE7:    STB RT2
        NI 0FFH
        BZ RE9
        LI 56
        STB WX
RE7A:   LDB RT1
        INC
        STB WY
        LIS TW
        STB WW
        LIS TH
        STB WH
        CLR
        STB WSTEP
        LDB TKIND+2*N_TILES     ; the trophy's frame (the kind after the tiles)
        SL 1
        DCI TFOFF
        ADC
        LM
        LR 1,A
        LM
        LR 2,A
        LR A,1
        STB WSRC
        LR A,2
        STB WSRC+1
        PI KTILE
        LDB WX
        AI 7
        STB WX
        DECB RT2
        BNZ RE7A
RE9:    INCB RT0
        CI SEATS
        BZ RE10
        JMP RE6
RE10:   PI ESLOTS
        LDB RESFIN              ; the jingle
        NI 0FFH
        BNZ RE11
        SND MUS_GAME
        LEAVE
RE11:   LDB RESSEAT
        CI SEATS-1
        BNC RE12
        SND MUS_WIN
RE12:   LEAVE

; PLRECR -- DC0 = seat r0's record in ST.  Leaf.  Clobbers A, r0.
PLRECR: LR A,0
        SL 1
        LR 0,A
        SL 1
        SL 1
        AS 0
        DCI ST+OFF_PLAYERS
        ADC
        POP

; RNAME -- seat r0's name (8 at most) at TXX, TXY.
RNAME:  ENTER
        PI NAMEDC
        XDC
        DCI RBUF
        LI NAME_LEN
        LR 2,A
RNM1:   XDC
        LM
        XDC
        ST
        DS 2
        BNZ RNM1
        CLR
        ST
        DCI RBUF
        PI ETXT
        LEAVE

; SLOFF -- slot r0 hidden, where it was.
SLOFF:  ENTER
        LR A,0
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
        PI SLSET
        LEAVE

; RDIGIT -- r0 (0-9) as a digit at TXX.
RDIGIT: ENTER
        LR A,0
        CI 9
        BC RDG1
        LIS 9
RDG1:   AI '0'
        LR 0,A
        PI ETCH
        LEAVE

; RDIG2 -- r2 (0-99) as two digits at TXX.
RDIG2:  ENTER
        CLR
        LR 1,A
RD21:   LR A,2
        CI 9
        BC RD22
        AI -10
        LR 2,A
        LR A,1
        INC
        LR 1,A
        BR RD21
RD22:   LR A,2
        STB RT2
        LR A,1
        AI '0'
        LR 0,A
        PI ETCH
        LDB RT2
        AI '0'
        LR 0,A
        PI ETCH
        LEAVE

; ECENTX -- ECENT for a page (the string in this page: copied first, the
;   fixed ECENT reads it through DC0 all the same).
ECENTX: ENTER
        PI ECENT
        LEAVE

WSMSG:  DW S_WATCH, S_WAITH, S_NEED2, S_START
S_ROOM: DB "WAITING ROOM",0
S_HOST: DB "HOST",0
S_DASH: DB "--------",0
S_ROUNDS: DB "ROUNDS: ",0
S_LR:   DB "L/R",0
S_NOLR: DB "   ",0
S_WATCH: DB "WATCHING",0
S_WAITH: DB "WAIT FOR THE HOST",0
S_NEED2: DB "NEED 2 PLAYERS",0
S_START: DB "FIRE: START",0
S_WINS: DB " WINS",0
S_DRAW: DB "DRAW",0
S_NOWIN: DB "NO WINNER",0
S_SUDDEN: DB "SUDDEN DEATH",0
S_RROUND: DB "ROUND ",0
S_NEXT: DB "NEXT ROUND IN ",0
S_BACK: DB "BACK TO ROOM ",0

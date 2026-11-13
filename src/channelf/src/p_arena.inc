; ---------------------------------------------------------------------------
; p_arena.inc -- page ARENA: the arena drawn (src/play.c play_enter,
; play_state, play_frame's drawing). Its entries:
;   E_ENTER  the arena afresh: the row palettes (the look), the walls, every
;            tile, the panel, the bombers' slots
;   E_STATE  a new state: (the link's step marks DIRTY tiles; the screen
;            picks up the rest in E_TICK)
;   E_TICK   a pass: the bombers first (theirs is the latency you see), then
;            the tiles that changed, then a row of the animation, the panel
; ---------------------------------------------------------------------------

; the looks (build.sh LOOK=): the floor's palette and the seam's (each tile
; row's last line)
        IF LOOK=0
FLOORP  EQU 2                   ; A: grey floor, green seams
SEAMP   EQU 3
        ELSEIF LOOK=1
FLOORP  EQU 3                   ; B: green floor, grey seams
SEAMP   EQU 2
        ELSE
FLOORP  EQU 1                   ; C: blue floor, grey seams
SEAMP   EQU 2
        ENDIF

; a direction's first bomber frame (DIR_UP 0, RIGHT 1, DOWN 2, LEFT 3)
DIRFR:  DB 3, 6, 0, 9
FR_DIE3 EQU 17

; ---------------------------------------------------------------------------
AENTER: ENTER
        CLR                     ; the panel's caches: nothing shown
        STB ANROW
        STB DQH                 ; the dirty queue empty
        STB DQT
        STB DQOVF
        LI 0FFH
        DCI HUDC
        LIS HUDLEN
        LR 0,A
AEN0:   LI 0FFH
        ST
        DS 0
        BNZ AEN0
        PI APALS                ; the rows' palettes
        LI AX0-1                ; the walls
        STB WX
        LI AY0
        STB WY
        LIS 1
        STB WW
        LI AY1-AY0
        STB WH
        LI CV1
        LR 0,A
        PI EFILL
        LI AX1
        STB WX
        LI CV1
        LR 0,A
        PI EFILL
        LI 0FFH                 ; nothing ours shown, no flash, no fall
        STB PEND
        CLR
        STB FLASH
        STB MFLAGS
        DCI SEATR               ; each drawn where the server has it, no fall
        XDC
        CLR
        STB PSEAT
AENQ:   LDB PSEAT
        LR 0,A
        PI PLREC
        LM
        XDC
        ST
        XDC
        LM
        XDC
        ST
        CLR
        ST
        XDC
        INCB PSEAT
        CI SEATS
        BNZ AENQ
        LIS SEAMP
        STB SEAMC
        LDB ST+OFF_ALIVE
        STB WASALV
        LDB ST+OFF_PHASE
        STB LASTPH
        LDB MYSEAT              ; we walk from where it has us
        CI 3
        BNC AENR
        LR 0,A
        PI PLREC
        LM
        LR 1,A
        LM
        LR 2,A
        LI P_WARP-2
        ADC
        LM
        STB MYWARP
        LR A,1
        STB MVX
        LR A,2
        STB MVY
        LIS DIR_DOWN
        STB MVDIR
        CLR
        STB MVACC
AENR:   PI SLSEATS              ; the bombers: their inks, not on screen yet
        DCI SHOWN               ; every tile, drawn
        LI 143
        LR 0,A
AEN3:   LI 0FFH
        ST
        DS 0
        BNZ AEN3
        CLR
        STB GT
AEN4:   LDB GT
        LR 0,A
        PI TFRAME
        LDB GT
        LR 0,A
        PI ETILE
        LDB GT
        DCI DIRTY
        ADCU
        CLR
        ST
        INCB GT
        CI 143
        BNZ AEN4
        PI ASTATE
        LEAVE

; APALS -- the arena's palettes: the bands above and below black (palette
;   0), the floor's rows, each tile row's seam.
APALS:  ENTER
        CLR
        STB GT+1                ; tile row
APL1:   LDB GT+1                ; y = AY0 + 5r
        LR 0,A
        SL 1
        SL 1
        AS 0
        AI AY0
        LR 0,A
        AI TH-2
        LR 1,A
        LIS FLOORP
        LR 2,A
        PI EPAL
        LDB GT+1
        LR 0,A
        SL 1
        SL 1
        AS 0
        AI AY0+TH-1
        LR 0,A
        LR 1,A
        LIS SEAMP
        LR 2,A
        PI EPAL
        INCB GT+1
        CI 11
        BNZ APL1
        LEAVE

; TFRAME -- r1 = the art frame tile r0 shows now: its kind's first frame,
;   animated: bombs, duds, burning blocks and powerups by the clock, each
;   tile its own phase; a blast's piece thick, then thin from its 15th
;   jiffy.  Leaf.  Clobbers A, r0-r3, DC0.
TFRAME: LDB PEND                ; our bomb, before the server has it
        XS 0
        BNZ TFR0
        LR A,0
        DCI ST+OFF_MAP
        ADCU
        LM
        NI 0FFH
        BNZ TFR0
        LDB TKIND+2*T_BOMB
        LR 1,A
        POP
TFR0:   LR A,0
        DCI ST+OFF_MAP
        ADCU
        LM                      ; the kind
        SL 1
        DCI TKIND
        ADC
        LM
        LR 1,A                  ; its first frame
        LM
        LR 2,A                  ; frames
        CI 1
        BZ TFRX
        LR A,0                  ; (the kind again: flames go by age)
        DCI ST+OFF_MAP
        ADCU
        LM
        CI T_FLAME_C-1
        BC TFR1
        CI T_FLAME_D
        BNC TFR1
        LR A,0                  ; a flame: thin from 15 jiffies on
        DCI BORN
        ADCU
        LM
        COM
        INC
        LR 3,A
        LDB JIFFY
        AS 3
        CI 14
        BC TFRX
        LR A,1
        INC
        LR 1,A
        POP
TFR1:   LR A,0                  ; the clock: ((JIFFY >> 3) + 3t) mod frames
        LR 3,A
        SL 1
        AS 3
        LR 3,A
        LDB JIFFY+1
        SL 4
        SL 1
        LR 0,A
        LDB JIFFY
        SR 1
        SR 1
        SR 1
        AS 0
        AS 3
        LR 3,A
        LR A,2
        AI -1
        NS 3                    ; (2 or 4 frames)
        AS 1
        LR 1,A
TFRX:   POP

; ---------------------------------------------------------------------------
; ATICK -- a pass: the bombers' slots from the state, drawn; the dirty
;   tiles; a row of the animation; the panel.
; ---------------------------------------------------------------------------
ATICK:  ENTER
        PI PFRAME               ; the play: walking, the others, the slots
        PI ESLOTS               ; drawn: the bombers first
        PI ADIRTY               ;  then the tiles that changed
        PI AANIM                ;  a row of the animation
        PI AFX                  ;  the palettes' effects
        LEAVE

; ASTATE -- a new state: our position if the server moved us (a warp), our
;   bomb if the server has it, a blast's sound and flash, a dud's, a fall
;   begun, the countdown's beeps and GO; the panel.
ASTATE: ENTER
        LDB MYSEAT              ; our own position, when the server says so
        CI 3
        BNC AS1
        LR 0,A
        PI PLREC
        LM
        LR 1,A
        LM
        LR 2,A
        LI P_WARP-2
        ADC
        LM
        LR 3,A
        DCI MYWARP
        CM
        BZ AS1
        LR A,3
        STB MYWARP
        LR A,1
        STB MVX
        LR A,2
        STB MVY
        CLR
        STB MVACC
AS1:    LDB PEND                ; our bomb: the server has it (or the tile is taken)
        CI 0FFH
        BZ AS2
        LR 0,A
        DCI ST+OFF_MAP
        ADCU
        LM
        NI 0FFH
        BZ AS2
        LI 0FFH
        STB PEND
        PI DMARK
AS2:    LDB MFLAGS              ; a blast, a dud
        LR 4,A
        CLR
        STB MFLAGS
        LR A,4
        NI MF_BOOM
        BZ AS3
        SND SND_BOOM
        LDB FLASH               ; a flash still on: its row back first
        NI 0FFH
        BZ ASR0
        LDB FLROW
        LR 1,A
        LIS FLOORP
        LR 2,A
        LDB SEAMC
        LR 3,A
        PI APALROW
ASR0:   LDB BOOMT               ; its row flashes: palette 0 (all white on black)
        LR 1,A
        CLR
        LR 2,A
ASR1:   LR A,1
        CI 12
        BC ASR2
        AI -13
        LR 1,A
        LR A,2
        INC
        LR 2,A
        BR ASR1
ASR2:   LR A,2
        STB FLROW
        LR 1,A                  ; (APALROW: r1 the tile row)
        CLR
        LR 2,A
        PI APALROW
        LIS 6
        STB FLASH
        BR AS4
AS3:    LR A,4
        NI MF_FIZZ
        BZ AS4
        SND SND_FIZZLE
AS4:    CLR                     ; a seat knocked out in play: its fall, a sound
        STB PSEAT
        LIS 1
        STB PBIT
AD1:    LDB PBIT
        LR 5,A
        LDB WASALV
        NS 5
        BZ AD2
        LDB ST+OFF_ALIVE
        NS 5
        BNZ AD2
        LDB ST+OFF_PHASE
        CI PH_PLAY
        BNZ AD2
        LDB PSEAT               ; SEATR + 3 s + SR_DIE
        LR 0,A
        SL 1
        AS 0
        AI SR_DIE
        DCI SEATR
        ADC
        LIS 1
        ST
        SND SND_DIE
AD2:    LDB PBIT
        SL 1
        STB PBIT
        INCB PSEAT
        CI SEATS
        BNZ AD1
        LDB ST+OFF_PHASE        ; the countdown's beeps, then GO
        CI PH_COUNTDOWN
        BNZ AB1
        LDB ST+OFF_TIMER
        DCI HUDC+4
        CM
        BZ AB2
        SND SND_BEEP
        BR AB2
AB1:    CI PH_PLAY
        BNZ AB2
        LDB LASTPH
        CI PH_COUNTDOWN
        BNZ AB2
        SND SND_GO
AB2:    LDB ST+OFF_PHASE
        STB LASTPH
        LDB ST+OFF_ALIVE
        STB WASALV
        PI AHUD
        LEAVE

; AFX -- the palettes' effects, a pass: a blast's flash run out (the row's
;   own palettes again); in the last 10 s of a round the seams pulse, light
;   blue and the look's seam colour by turns, a second each.
AFX:    LDB FLASH               ; (no flash, no pulse due: out before ENTER)
        NI 0FFH
        BNZ AFXE
        LDB SEAMC
        CI SEAMP
        BNZ AFXE
        LDB ST+OFF_TIMER
        CI 10
        BC AFXE
        POP
AFXE:   ENTER
        LDB FLASH
        NI 0FFH
        BZ AFX1
        LR 0,A
        LDB DT
        COM
        INC
        AS 0
        BZ AFX0
        BC AFXF                 ; (no borrow: still on)
AFX0:   CLR
        STB FLASH
        LDB FLROW
        LR 1,A
        LDB SEAMC
        LR 3,A
        LIS FLOORP
        LR 2,A
        PI APALROW
        BR AFX1
AFXF:   STB FLASH
AFX1:   LIS SEAMP               ; the seams' palette wanted
        LR 4,A
        LDB ST+OFF_PHASE
        CI PH_PLAY
        BNZ AFX2
        LDB ST+OFF_TIMER
        CI 10
        BNC AFX2
        NI 0FFH
        BZ AFX2
        NI 1
        BZ AFX2
        LIS 1
        LR 4,A
AFX2:   LDB SEAMC
        XS 4
        BZ AFX9
        LR A,4
        STB SEAMC
        CLR
        STB GT+1
AFX3:   LDB GT+1                ; each tile row's seam
        LR 0,A
        SL 1
        SL 1
        AS 0
        AI AY0+TH-1
        LR 0,A
        LR 1,A
        LDB SEAMC
        LR 2,A
        PI EPAL
        INCB GT+1
        CI 11
        BNZ AFX3
AFX9:   LEAVE

; APALROW -- tile row r1's lines in palette r2 (its seam line r3 too; r2 =
;   0, the flash: all of it).
APALROW: ENTER
        LR A,1
        SL 1
        SL 1
        AS 1
        AI AY0
        STB GT+2
        LR A,3
        STB GT+3
        LR A,2
        STB GT+4
        LDB GT+2
        LR 0,A
        AI TH-2
        LR 1,A
        LDB GT+4
        LR 2,A
        PI EPAL
        LDB GT+2
        AI TH-1
        LR 0,A
        LR 1,A
        LDB GT+4
        NI 0FFH
        BZ APR1
        LDB GT+3
APR1:   LR 2,A
        PI EPAL
        LEAVE

MOD3:   DB 0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1,2,0,1

; ADIRTY -- the tiles DMARK queued, redrawn (up to ADMAX a pass: a big
;   blast draws over a few passes while the bombers keep their pace). An
;   overflowed queue is filled again from DIRTY.
ADMAX   EQU 12
ADIRTY: LDB DQOVF               ; (nothing queued: out before ENTER)
        NI 0FFH
        BNZ ADG
        LDB DQT
        LR 1,A
        LDB DQH
        XS 1
        BNZ ADG
        POP
ADG:    ENTER
        LI ADMAX
        STB GT+3
        LDB DQOVF
        NI 0FFH
        BZ ADQ1
        CLR                     ; overflowed: queue DIRTY's tiles afresh
        STB DQOVF
        STB DQH
        STB DQT
        STB GT+4
ADO1:   LDB GT+4
        DCI DIRTY
        ADCU
        LM
        NI 0FFH
        BZ ADO2
        LDB GT+4
        DCI DIRTY
        ADCU
        CLR
        ST
        LDB GT+4
        LR 0,A
        PI DMARK
ADO2:   INCB GT+4
        CI 143
        BNZ ADO1
ADQ1:   LDB DQT                 ; empty when head = tail
        LR 1,A
        LDB DQH
        XS 1
        BZ ADIX
        LDB DQH
        LR 1,A
        DCI DQ
        ADC
        LM
        STB GT+4
        LR A,1
        INC
        NI 63
        STB DQH
        LDB GT+4
        DCI DIRTY
        ADCU
        CLR
        ST
        LDB GT+4
        LR 0,A
        PI TFRAME
        LDB GT+4
        LR 0,A
        PI ETILE
        DECB GT+3
        BNZ ADQ1
ADIX:   LEAVE

; AANIM -- one tile row a pass: a tile of a kind that animates (burning
;   blocks and on: floor, pillars and blocks have one frame), whose frame is
;   not the one shown, redrawn (the animation's steps and the flames'
;   thinning, row by row).  A still tile costs ~70 clocks.
AANIM:  ENTER
        LDB ANROW               ; the row's first tile: 13 r
        LR 0,A
        SL 1
        AS 0
        SL 1
        SL 1
        AS 0
        LR 5,A
        DCI ST+OFF_MAP
        ADCU
        LIS 13
        LR 6,A
AAN1:   LM                      ; the kind
        CI T_BLOCK
        BC AAN2                 ; (one frame)
        LR Q,DC                 ; an animated one: the calls spend registers
        LR A,QU
        STB GT+8
        LR A,QL
        STB GT+9
        LR A,5
        STB GT+5
        LR A,6
        STB GT+6
        LR A,5
        LR 0,A
        PI TFRAME
        LR A,1
        LR 2,A
        LDB GT+5
        DCI SHOWN
        ADCU
        LM
        XS 2
        BZ AAN3
        LDB GT+5
        LR 0,A
        LR A,2
        LR 1,A
        PI ETILE
AAN3:   LDB GT+5
        LR 5,A
        LDB GT+6
        LR 6,A
        DCI GT+8
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
AAN2:   LR A,5
        INC
        LR 5,A
        DS 6
        BNZ AAN1
        INCB ANROW
        CI 11
        BNZ AANX
        CLR
        STB ANROW
AANX:   LEAVE

; ---------------------------------------------------------------------------
; AHUD -- the panel: each field drawn again only when what it shows
;   changed (a cell costs ~3K clocks even when the screen does not change).
;   Rows of 6: the four seats (name, wins), the clock, the round, then
;   ours: F range B bombs, S speed K kick G glove.
; ---------------------------------------------------------------------------
HUDLEN  EQU 10
AHUD:   ENTER
        CLR
        STB GT+7                ; seat
AHD1:   LDB GT+7                ; what it shows: seated, alive, wins
        LR 0,A
        DCI SEATBIT
        ADC
        LM
        LR 1,A
        LDB ST+OFF_SEATED
        NS 1
        BZ AHD2
        LDB ST+OFF_ALIVE
        NS 1
        LIS 0
        BZ AHD3
        LI 080H
AHD3:   LR 2,A
        LR A,0
        LR 3,A
        SL 1
        SL 1
        AS 3
        SL 1
        DCI ST+OFF_PLAYERS+P_WINS
        ADC
        LM
        AS 2
        INC                     ; (0 = not seated)
AHD2:   LR 2,A
        LDB GT+7
        DCI HUDC
        ADC
        LM
        XS 2
        BZ AHD9
        LDB GT+7
        DCI HUDC
        ADC
        LR A,2
        ST
        PI AHSEAT
AHD9:   INCB GT+7
        CI 4
        BNZ AHD1
        LDB ST+OFF_TIMER        ; the clock
        LR 2,A
        DCI HUDC+4
        LM
        XS 2
        BZ AHDR
        LR A,2
        DCI HUDC+4
        ST
        PI AHCLK
AHDR:   LDB ST+OFF_ROUND
        LR 2,A
        DCI HUDC+5
        LM
        XS 2
        BZ AHDS
        LR A,2
        DCI HUDC+5
        ST
        PI AHRND
AHDS:   PI AHSTAT
        LEAVE

; the field's text cursor: panel row r0 (0-8)
AHAT:   LR A,0
        SL 1
        AS 0
        SL 1                    ; 6 r
        AI AY0
        STB TXY
        LI PANX
        STB TXX
        POP

; AHSEAT -- seat GT+7's row: its name's 3 letters (or "-  " knocked out)
;   and its wins, in its ink; a blank row for an empty seat.
AHSEAT: ENTER
        LDB GT+7
        LR 0,A
        PI AHAT
        LDB GT+7
        SL 1
        DCI SEATINK
        ADC
        LM
        LR 1,A
        LM
        LR 2,A
        LR A,1
        STB TXINK
        LR A,2
        STB TXINK2
        LDB GT+7
        DCI HUDC
        ADC
        LM
        NI 0FFH
        BNZ AHS1
        DCI S_BLANK4            ; empty
        PI ETXT
        LEAVE
AHS1:   NI 080H
        BNZ AHS2
        DCI S_OUT               ; knocked out
        PI ETXT
        BR AHS3
AHS2:   LDB GT+7                ; the name's first 3 (a NUL: a blank)
        SL 1
        SL 1
        SL 1
        DCI ST+OFF_NAMES
        ADC
        XDC
        DCI GT+11
        LIS 3
        LR 1,A
AHS4:   XDC
        LM
        XDC
        NI 0FFH
        BNZ AHS5
        LI ' '
AHS5:   ST
        DS 1
        BNZ AHS4
        CLR
        ST
        DCI GT+11
        PI ETXT
AHS3:   LDB GT+7                ; wins: a digit
        LR 1,A
        SL 1
        SL 1
        AS 1
        SL 1
        DCI ST+OFF_PLAYERS+P_WINS
        ADC
        LM
        CI 9
        BC AHS6
        LIS 9
AHS6:   AI '0'
        LR 0,A
        PI ETCH
        LEAVE
S_BLANK4: DB "    ",0
S_OUT:  DB "-  ",0

; AHCLK -- the clock, m:ss, from the seconds left (r2)
AHCLK:  ENTER
        LIS 4
        LR 0,A
        PI AHAT
        LI CV1
        STB TXINK
        STB TXINK2
        LDB ST+OFF_TIMER
        LR 2,A
        CLR
        LR 1,A                  ; minutes
AHC1:   LR A,2
        CI 59
        BC AHC2
        AI -60
        LR 2,A
        LR A,1
        INC
        LR 1,A
        BR AHC1
AHC2:   LR A,1
        AI '0'
        LR 0,A
        LR A,2
        STB GT+9
        PI ETCH
        LI ':'
        LR 0,A
        PI ETCH
        LDB GT+9
        LR 2,A
        PI DIG2
        LEAVE

; DIG2 -- r2 (0-99) as two digits.  (PI destroys A: arguments in registers.)
DIG2:   ENTER
        CLR
        LR 1,A
DG1:    LR A,2
        CI 9
        BC DG2
        AI -10
        LR 2,A
        LR A,1
        INC
        LR 1,A
        BR DG1
DG2:    LR A,2
        STB GT+10
        LR A,1
        AI '0'
        LR 0,A
        PI ETCH
        LDB GT+10
        AI '0'
        LR 0,A
        PI ETCH
        LEAVE

; AHRND -- "RD n"
AHRND:  ENTER
        LIS 5
        LR 0,A
        PI AHAT
        LI CV2
        STB TXINK
        STB TXINK2
        DCI S_RD
        PI ETXT
        LDB ST+OFF_ROUND
        CI 9
        BC AHR1
        LIS 9
AHR1:   AI '0'
        LR 0,A
        PI ETCH
        LEAVE
S_RD:   DB "RD ",0

; AHSTAT -- our powers, when they change: "F2B1" and "S1KG" (K, G only
;   when we have them).  Watching: nothing.
AHSTAT: ENTER
        LDB MYSEAT
        CI 3
        BNC AHT9
        LR 0,A                  ; our record
        SL 1
        SL 1
        AS 0
        SL 1
        DCI ST+OFF_PLAYERS+P_FLAGS
        ADC
        LM
        LR 4,A                  ; flags
        LM
        LR 5,A                  ; range
        LM
        LR 6,A                  ; bombs
        LM
        INC
        LR 7,A                  ; speed + 1
        DCI HUDC+6              ; changed?
        LR A,4
        CM
        BNZ AHT1
        LR A,5
        CM
        BNZ AHT1
        LR A,6
        CM
        BNZ AHT1
        LR A,7
        CM
        BNZ AHT1
AHT9:   LEAVE
AHT1:   DCI HUDC+6
        LR A,4
        ST
        LR A,5
        ST
        LR A,6
        ST
        LR A,7
        ST
        LIS 7
        LR 0,A
        PI AHAT
        LI CV3
        STB TXINK
        STB TXINK2
        LI 'F'
        LR 0,A
        PI ETCH
        LDB HUDC+7
        LR 0,A
        PI DIGIT
        LI 'B'
        LR 0,A
        PI ETCH
        LDB HUDC+8
        LR 0,A
        PI DIGIT
        LIS 8
        LR 0,A
        PI AHAT
        LI 'S'
        LR 0,A
        PI ETCH
        LDB HUDC+9
        LR 0,A
        PI DIGIT
        LDB HUDC+6
        NI PF_KICK
        LI ' '
        BZ AHT2
        LI 'K'
AHT2:   LR 0,A
        PI ETCH
        LDB HUDC+6
        NI PF_GLOVE
        LI ' '
        BZ AHT3
        LI 'G'
AHT3:   LR 0,A
        PI ETCH
        LEAVE

; DIGIT -- r0 (0-9, more shows 9) as a digit.
DIGIT:  ENTER
        LR A,0
        CI 9
        BC DGT1
        LIS 9
DGT1:   AI '0'
        LR 0,A
        PI ETCH
        LEAVE

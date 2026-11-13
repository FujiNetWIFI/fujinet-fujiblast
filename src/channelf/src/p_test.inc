; ---------------------------------------------------------------------------
; p_test.inc -- page TEST: the rigs' offline scenes (src/test.c). The scene
; and its argument come from the test hook (p_boot): TESTHK+2, +3.
;   1  the arena: the canned state (tools/chf/mkcanned.py), a blast every 4 s
;   7  the bench: the four bombers round the outer lanes (TESTHK+3: units a
;      jiffy, 0 = 1), a blast every second; the speed rigs measure it
;   8  the settle sweep, for a real console: a band of pixels drawn at each
;      wait after the strobe (56-176 clocks), labelled: a band whose wait is
;      too short comes out wrong (MAME draws them all)
; Entries: E_ENTER the scene's state (MYSEAT 0); E_TICK its jiffies (DT).
; ---------------------------------------------------------------------------

TSCENE  EQU TV+0                ; the scene
TSPEED  EQU TV+1                ; bench: units a jiffy
TBLT    EQU TV+2                ; jiffies to the next blast
TCLR    EQU TV+3                ; jiffies to the blast's end (0: none on)
TBN     EQU TV+4                ; the blast's tiles: count, then the list
TBL     EQU TV+5                ; (13)
TBC     EQU TV+18               ; which centre next
TLEG    EQU TV+20               ; 4: each bomber's leg of the ring
TK      EQU TV+24               ; scratch
TSTATE  EQU TV+40               ; the first state given

TENTER: ENTER
        LDB TESTHK+2            ; 8: the settle sweep (never comes back)
        CI 8
        BNZ TEN0
        JMP TSWEEP
TEN0:
        DCI CANNED              ; the canned state
        XDC
        DCI ST
        LI STATE_SIZE
        LR 0,A
TEN1:   XDC
        LM
        XDC
        ST
        DS 0
        BNZ TEN1
        CLR
        STB MYSEAT
        LDB TESTHK+2
        STB TSCENE
        LDB TESTHK+3
        NI 0FFH
        BNZ TEN2
        LIS 1
TEN2:   STB TSPEED
        LI 60
        STB TBLT
        CLR
        STB TCLR
        STB TBC
        STB TSTATE
        LDB TSCENE              ; 2 the waiting room, 3 a round's end, 4 the game's
        CI 2
        BNZ TEN2A
        LIS PH_WAIT
        STB ST+OFF_PHASE
        JMP TEN9
TEN2A:  CI 3
        BZ TEN2B
        CI 4
        BNZ TEN2C
TEN2B:  CI 3                    ; (A: the scene)
        LIS PH_ROUNDEND
        BZ TEN2D
        LIS PH_GAMEOVER
TEN2D:  STB ST+OFF_PHASE
        LIS 5
        STB ST+OFF_TIMER
        BR TEN9
TEN2C:  CI 7
        BZ TEN2E
        JMP TEN9
TEN2E:
        ; the bench: no blocks on the outer lanes, everyone alive and walking
        ; clockwise from a corner
        CLR
        LR 0,A                  ; t
TEN3:   LR A,0                  ; a tile of row 0 or 10, or column 0 or 12?
        CI 12
        BNC TEN4
        BR TEN5                 ; (row 0)
TEN4:   CI 129                  ; row 10: 130-142
        BC TEN6
TEN5:   LR A,0
        DCI ST+OFF_MAP
        ADCU
        LM
        CI T_BLOCK
        BNZ TEN7
        LR A,0
        DCI ST+OFF_MAP
        ADCU
        CLR
        ST
        BR TEN7
TEN6:   LR A,0                  ; columns 0 and 12: t mod 13 = 0 or 12
        LR 1,A
TEN6A:  LR A,1
        CI 12
        BC TEN6B
        AI -13
        LR 1,A
        BR TEN6A
TEN6B:  LR A,1
        NI 0FFH
        BZ TEN5
        CI 12
        BZ TEN5
TEN7:   LR A,0
        INC
        LR 0,A
        CI 143
        BNZ TEN3
        LI 0FH
        STB ST+OFF_ALIVE
        LI 0FFH                 ; (the bench watches: every bomber the server's)
        STB MYSEAT
        DCI TLEG                ; legs: 0 right, 1 down, 2 left, 3 up
        CLR
        ST                      ; seat 0 at (0, 0): right
        LIS 2
        ST                      ; seat 1 at (192, 160): left
        LIS 1
        ST                      ; seat 2 at (192, 0): down
        LIS 3
        ST                      ; seat 3 at (0, 160): up
TEN9:   LEAVE

; ---------------------------------------------------------------------------
TTICK:  ENTER
        LDB TSTATE              ; the first pass: a state (the screen picked)
        NI 0FFH
        BNZ TTK0
        LIS 1
        STB TSTATE
        LDB NEVT
        OI EV_STATE
        STB NEVT
TTK0:   LDB TSCENE              ; 6: the sound board
        CI 6
        BNZ TTK0A
        PI TSOUND
TTK0A:  LDB DT
        STB TK
TTK1:   LDB TK
        NI 0FFH
        BZ TTKX
        LDB TSCENE
        CI 7
        BNZ TTK2
        PI TWALK
TTK2:   LDB TSCENE
        CI 6
        BZ TTK3
        PI TBLAST
TTK3:
        DECB TK
        BR TTK1
TTKX:   LEAVE

; TSOUND -- the sound board: each sound in turn, 100 jiffies apart.
TSOUND: ENTER
        LDB TBLT                ; TBLT - DT (C: no borrow): jiffies to the next
        LR 1,A
        LDB DT
        COM
        INC
        AS 1
        BZ TSO1
        BNC TSO1
        STB TBLT
        LEAVE
TSO1:   LI 100
        STB TBLT
        LDB TBC
        CI MUS_GAME+1
        BC TSO2
        CLR
TSO2:   LR 0,A
        INC
        STB TBC
        PI SNDPLAY
        LEAVE

; TWALK -- a jiffy of the bench's walking: each bomber TSPEED units along
;   its leg of the ring, turning at the corners.
TWALK:  ENTER
        CLR
        STB TK+1
TWK1:   LDB TK+1
        LR 0,A
        SL 1
        SL 1
        AS 0
        SL 1
        DCI ST+OFF_PLAYERS
        ADC
        LR H,DC
        LM
        LR 1,A                  ; x
        LM
        LR 2,A                  ; y
        LDB TK+1
        DCI TLEG
        ADC
        LM
        LR 3,A                  ; leg
        LDB TSPEED
        LR 4,A
        LR A,3
        CI 0
        BNZ TWK2
        LR A,1                  ; right, to x 192
        AS 4
        LR 1,A
        CI MAX_X
        BC TWKD
        LI MAX_X
        LR 1,A
        BR TWKT
TWK2:   CI 1
        BNZ TWK3
        LR A,2                  ; down, to y 160
        AS 4
        LR 2,A
        CI MAX_Y
        BC TWKD
        LI MAX_Y
        LR 2,A
        BR TWKT
TWK3:   CI 2
        BNZ TWK4
        LR A,4                  ; left, to x 0
        COM
        INC
        AS 1
        LR 1,A
        BC TWKD                 ; (no borrow: still on; BP would be signed)
        CLR
        LR 1,A
        BR TWKT
TWK4:   LR A,4                  ; up, to y 0
        COM
        INC
        AS 2
        LR 2,A
        BC TWKD
        CLR
        LR 2,A
TWKT:   LR A,3                  ; the corner: the next leg
        INC
        NI 3
        LR 3,A
        LDB TK+1
        DCI TLEG
        ADC
        LR A,3
        ST
TWKD:   LR DC,H                 ; x, y, dir | moving
        LR A,1
        ST
        LR A,2
        ST
        LR A,3                  ; leg -> DIR_*: right 1, down 2, left 3, up 0
        INC
        NI 3
        OI P_DIR_MOVING
        ST
        INCB TK+1
        CI 4
        JNZ TWK1
        LEAVE

; TBLAST -- a jiffy of the blasts: one goes off every TBLTN jiffies at the
;   next centre, its pieces DIRTY, BORN now; 30 jiffies on, gone.
TBLTN   EQU 60
TBLAST: ENTER
        LDB TCLR
        NI 0FFH
        BZ TBS1
        AI -1
        STB TCLR
        BNZ TBS1
        PI TBGONE
TBS1:   LDB TBLT
        AI -1
        STB TBLT
        BNZ TBSX
        LDB TSCENE
        CI 7
        LI TBLTN
        BZ TBS2
        LI 240
TBS2:   STB TBLT
        PI TBGO
        LI 30
        STB TCLR
TBSX:   LEAVE

; TBGO -- a range-3 blast at the next centre (TBCEN).
TBGO:   ENTER
        LDB TBC
        LR 0,A
        INC
        CI TBCN
        BNZ TBG0
        CLR
TBG0:   STB TBC
        LR A,0
        DCI TBCEN
        ADC
        LM
        LR 5,A                  ; the centre's tile
        CLR
        STB TBN
        LR A,5
        LR 0,A
        LI T_FLAME_C
        LR 1,A
        PI TBSET
        ; the four arms: (step, middle, end, edge test)
        LIS 4
        STB TK+2
        DCI TBARM
TBG1:   LM
        LR 1,A                  ; the step
        LM
        LR 2,A                  ; the middle piece
        LM
        LR 3,A                  ; the end
        LM
        LR 4,A                  ; the edge test: 0 left, 1 right, 2 up, 3 down
        LR Q,DC                 ; (the next arm, kept in RAM across the calls)
        LR A,QU
        STB TK+8
        LR A,QL
        STB TK+9
        LR A,1
        STB TK+3
        LR A,2
        STB TK+4
        LR A,3
        STB TK+5
        LR A,4
        STB TK+6
        LR A,5
        STB TK+7                ; where the arm is
        LIS 3
        STB TK+10               ; its length left
TBG2:   PI TBNEXT               ; r0 = the next tile, 255: off the arena
        LR A,0
        CI 255
        BZ TBG9
        DCI ST+OFF_MAP
        ADCU
        LM
        CI T_PILLAR
        BZ TBG9
        CI T_BLOCK
        BNZ TBG3
        LIS T_BURN              ; a block: it burns, the arm stops
        LR 1,A
        PI TBSET
        BR TBG9
TBG3:   LDB TK+10               ; the last: the end piece
        CI 1
        LDB TK+4
        BNZ TBG4
        LDB TK+5
TBG4:   LR 1,A
        LDB TK+7
        LR 0,A
        PI TBSET
        DECB TK+10
        BNZ TBG2
TBG9:   DCI TK+8
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        DECB TK+2
        LR DC,Q
        JNZ TBG1
        LEAVE

; TBNEXT -- r0 = the tile one step on from TK+7 along the arm (TK+3, the
;   edge test TK+6), TK+7 moved there; 255 off the arena.
TBNEXT: ENTER
        LDB TK+7
        LR 0,A
        LR 1,A                  ; its column: t mod 13
TBN1:   LR A,1
        CI 12
        BC TBN2
        AI -13
        LR 1,A
        BR TBN1
TBN2:   LDB TK+6
        CI 0
        BNZ TBN3
        LR A,1                  ; left: not from column 0
        NI 0FFH
        BZ TBNO
        BR TBNS
TBN3:   CI 1
        BNZ TBN4
        LR A,1                  ; right: not from column 12
        CI 12
        BZ TBNO
        BR TBNS
TBN4:   CI 2
        BNZ TBN5
        LR A,0                  ; up: not from row 0
        CI 12
        BC TBNO
        BR TBNS
TBN5:   LR A,0                  ; down: not from row 10
        CI 129
        BNC TBNO
TBNS:   LDB TK+3
        AS 0
        STB TK+7
        LR 0,A
        LEAVE
TBNO:   LI 255
        LR 0,A
        LEAVE

; TBSET -- tile r0 becomes kind r1: DIRTY, BORN now, kept in the list.
TBSET:  ENTER
        LR A,0
        DCI ST+OFF_MAP
        ADCU
        LR A,1
        ST
        PI DMARK
        LDB JIFFY
        LR 2,A
        LR A,0
        DCI BORN
        ADCU
        LR A,2
        ST
        LDB TBN
        LR 2,A
        INC
        STB TBN
        LR A,2
        DCI TBL
        ADC
        LR A,0
        ST
        LEAVE

; TBGONE -- the blast's pieces gone: floor again.
TBGONE: ENTER
        LDB TBN
        NI 0FFH
        BZ TBGX
        LR 3,A
        DCI TBL
TBGN1:  LM
        LR 0,A
        LR Q,DC
        DCI ST+OFF_MAP
        ADCU
        CLR
        ST
        LR A,QU
        STB TK+12
        LR A,QL
        STB TK+13
        PI DMARK
        DCI TK+12
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        DS 3
        BNZ TBGN1
        CLR
        STB TBN
TBGX:   LEAVE

; the centres (even rows and columns: no pillar in the arms' way)
TBCEN:  DB 2*13+2, 4*13+6, 6*13+10, 8*13+4, 2*13+8, 6*13+2, 4*13+4, 8*13+8
TBCN    EQU 8
; the arms: step, middle piece, end piece, edge test
TBARM:  DB -1, T_FLAME_H, T_FLAME_L, 0
        DB 1, T_FLAME_H, T_FLAME_R, 1
        DB -13, T_FLAME_V, T_FLAME_U, 2
        DB 13, T_FLAME_V, T_FLAME_D, 3

; ---------------------------------------------------------------------------
; TSWEEP -- scene 8: 7 bands, band n drawn with n+1 turns of a 20-clock loop
;   after each strobe (about 36 + 20 (n+1) clocks before the next port
;   write: 56 to 176), each a stripe of values 1, 2, 3 by turns across 80
;   pixels, 4 rows, labelled with its wait.  The pixels are written by hand
;   (not the engine), every one, every pass.
; ---------------------------------------------------------------------------
TSWEEP: CLR
        LR 0,A
        PI ECLS
        CLR
        STB TK
TSW1:   LDB TK                  ; its label: the wait, 3 digits, at row 6 + 8 n
        LR 0,A
        SL 1
        SL 1
        SL 1
        AI 6
        STB TXY
        LIS 4
        STB TXX
        LI CV1
        STB TXINK
        STB TXINK2
        LDB TK
        SL 1
        SL 1
        DCI TSWLBL
        ADC
        PI ETXT
        INCB TK
        CI 7
        BNZ TSW1
TSW2:   CLR                     ; the bands, again and again
        STB TK
TSW3:   LDB TK
        INC
        LR 5,A                  ; the wait's turns
        LDB TK                  ; the band's first row: 6 + 8 n
        SL 1
        SL 1
        SL 1
        AI 6
        LR 4,A
        LIS 4
        LR 9,A                  ; rows
        DCI SNDB
        LM
        LR 8,A                  ; the tone bits (kept on port 5)
TSW4:   LR A,4                  ; the row out
        COM
        NI 03FH
        AS 8
        OUTS 5
        LI 80
        LR 3,A                  ; columns
        LI 20
        LR 2,A                  ; the column
        CLR
        LR 7,A                  ; the value's turn
TSW5:   LR A,7                  ; the value: 1, 2, 3 by turns (port-1 code)
        INC
        CI 3
        BC TSW6
        CLR
TSW6:   LR 7,A
        INC
        SL 4                    ; value << 6, complemented: the port-1 code
        SL 1
        SL 1
        COM
        NI 0C0H
        OUTS 1
        LR A,2
        COM
        OUTS 4
        LI 060H
        OUTS 0
        LI 050H
        OUTS 0
        LR A,5                  ; the wait
        LR 6,A
TSW7:   DS 6
        BNZ TSW7
        LR A,2
        INC
        LR 2,A
        DS 3
        BNZ TSW5
        LR A,4
        INC
        LR 4,A
        DS 9
        BNZ TSW4
        INCB TK
        CI 7
        BNZ TSW3
        JMP TSW2
TSWLBL: DB "056",0, "076",0, "096",0, "116",0, "136",0, "156",0, "176",0

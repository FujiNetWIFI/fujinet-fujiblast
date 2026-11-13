; ---------------------------------------------------------------------------
; p_menu.inc -- page MENU: the screens before a table (src/menus.c): the
; title, the name, the rooms. The Astrocade's (src/astrocade/src/
; p_menu.inc), transcribed for the Channel F's screen: 102 x 58 shown, the
; family's 3x5 font in 4x6 cells (EBIG: 8x12), and the row palettes for
; colour: a band of rows, a background each.
;
; Entries: E_ENTER the title, and the name if none (r0 = 1: a new one);
; E_STATE the room list asked for; E_TICK its pass (r0: 0 on, 1 picked,
; 2 no list).
; ---------------------------------------------------------------------------

MSEL    EQU VARS+3C0H           ; the room under the cursor
MNSEL   EQU VARS+3C1H           ; rooms in the list (0: not drawn yet)
MPREV   EQU VARS+3C2H           ; the stick last pass (edges)
MWAIT   EQU VARS+3C3H           ; 2: jiffies waiting for the list
MT0     EQU VARS+3C5H           ; scratch: letters so far, the letter, the
MT1     EQU VARS+3C6H           ;  repeat, the blink shown
MT3     EQU VARS+3C7H
MBLINK  EQU VARS+3C8H
MWX     EQU VARS+3C9H           ; 4: the title's walkers' x
MNAME   EQU VARS+3D0H           ; 3: a cursor glyph and a NUL

TLOGOY  EQU 8                   ; the title's rows
TFORY   EQU 23
TLANEY  EQU 32
TFIREY  EQU 43
TTAGY   EQU 51

; PALBAND -- rows r0..r1 in palette r2 (EPAL, kept for the caller's next).
PALB    MACRO r0v, r1v, p
        LI r0v
        LR 0,A
        LI r1v
        LR 1,A
        LIS p
        LR 2,A
        PI EPAL
        ENDM

; ---- title ---------------------------------------------------------------------------
; The logo, red over a band of light blue, grey and green; FOR FUJINET
; white on black; the four bombers walking a strip of the arena's floor;
; PRESS FIRE, blinking; the tagline on green.
MENTER: ENTER
        CLR
        LR 0,A
        PI ECLS
        PALB TLOGOY-1, TLOGOY+2, 1
        PALB TLOGOY+3, TLOGOY+6, 2
        PALB TLOGOY+7, TLOGOY+10, 3
        PALB TLANEY, TLANEY+4, FLOORP
        PALB TLANEY+5, TLANEY+5, SEAMP
        PALB TTAGY-1, TTAGY+5, 3
        LI CV2                  ; the logo: 9 letters of 8
        STB TXINK
        STB TXINK2
        LI 19
        STB TXX
        LI TLOGOY
        STB TXY
        DCI S_LOGO
        PI EBTXT
        DCI S_FORFN
        LI TFORY
        LR 1,A
        LI CV1
        LR 2,A
        LR 3,A
        PI ECENT
        DCI S_TAG
        LI TTAGY
        LR 1,A
        LI CV1
        LR 2,A
        LR 3,A
        PI ECENT
        PI SLSEATS              ; the walkers: slots 0-3
        DCI MWX
        LIS 4
        ST
        LI 30
        ST
        LI 56
        ST
        LI 82
        ST
        LI 0FFH
        STB MBLINK
        SND MUS_TITLE
MT6:    PI FRAME
        LDB JIFFY               ; PRESS FIRE: a second on, half off
        NI 03FH
        CI 39
        LIS 1
        BC MT6A
        CLR
MT6A:   LR 4,A
        LDB MBLINK
        XS 4
        BZ MT7
        LR A,4
        STB MBLINK
        NI 0FFH
        DCI S_FIRE
        BNZ MT6B
        DCI S_BLANK10
MT6B:   LI TFIREY
        LR 1,A
        LI CV1
        LR 2,A
        LR 3,A
        PI ECENT
MT7:    CLR                     ; the walkers: a pixel every other jiffy
        STB MT0
MT7A:   LDB MT0
        DCI MWX
        ADC
        LM
        LR 1,A
        LDB JIFFY
        NI 1
        BZ MT8
        LR A,1
        INC
        CI 99
        BC MT7B
        LIS 4
MT7B:   LR 1,A
        LDB MT0
        DCI MWX
        ADC
        LR A,1
        ST
MT8:    LDB JIFFY               ; right0, or right1/2 by seat, by turns
        NI 8
        LIS FR_RIGHT0
        BZ MT9
        LDB MT0
        NI 1
        AI FR_RIGHT0+1
MT9:    LR 3,A
        LI TLANEY
        LR 2,A
        LIS 1
        LR 4,A
        LDB MT0
        LR 0,A
        PI SLSET
        INCB MT0
        CI 4
        BNZ MT7A
        PI ESLOTS
        LDB EVBTN               ; FIRE or START
        NI 0FFH
        BNZ MT10
        LDB EVKEY
        CI KEY_END
        BZ MT10
        JMP MT6
MT10:   PI SNDINIT              ; the jingle off
        LDB MYNAME              ; a name: done (r0 = 0)
        NI 0FFH
        BNZ MT11
        PI NAMEIN
        LEAVE
MT11:   CLR
        LR 0,A
        LEAVE

S_LOGO: DB "FUJIBLAST",0
S_FORFN: DB "FOR FUJINET",0
S_FIRE: DB "PRESS FIRE",0
S_BLANK10: DB "          ",0
S_TAG:  DB "BOMBS FOR 2-4 PLAYERS",0

; ---- the name (menus.c name_entry: the stick's wheel) ---------------------------------
; Up and down turn the letter at the cursor through A-Z and 0-9 (held, it
; runs on); FIRE (push) or right takes it, left or pull rubs out, START ends
; (two letters at least).  r0 = 1 (the caller saves it: key 0).
NAMEIN: ENTER
        CLR
        LR 0,A
        PI ECLS
        PALB 18, 31, 2
        LI CV1
        STB TXINK
        STB TXINK2
        LI 19
        STB TXX
        LIS 4
        STB TXY
        DCI S_YNAME
        PI EBTXT
        DCI S_NH1
        LI 36
        LR 1,A
        LI CV1
        LR 2,A
        LR 3,A
        PI ECENT
        DCI S_NH2
        LI 42
        LR 1,A
        LI CV1
        LR 2,A
        LR 3,A
        PI ECENT
        DCI S_NH3
        LI 48
        LR 1,A
        LI CV1
        LR 2,A
        LR 3,A
        PI ECENT
        DCI S_NH4
        LI 54
        LR 1,A
        LI CV1
        LR 2,A
        LR 3,A
        PI ECENT
        CLR
        STB MT0                 ; letters so far
        STB MT1                 ; the letter at the cursor (0-35)
        STB MT3                 ; the repeat count
        DCI MYNAME
        LI NAME_LEN+1
        LR 0,A
NM0:    CLR
        ST
        DS 0
        BNZ NM0
NM1:    PI FRAME
        LDB MT3
        NI 0FFH
        BZ NM2
        AI -1
        STB MT3
NM2:    LDB EVKEY               ; ends
        CI KEY_END
        BNZ NM2A
        JMP NMEND
NM2A:   CI KEY_BACK             ; rubs out
        BZ NMRUB
        LDB MT3
        NI 0FFH
        BNZ NM3
        LDB INPDIR
        NI IN_LEFT
        BNZ NMRUB
NM3:    LDB EVBTN               ; takes it
        NI 0FFH
        BNZ NMTAKE
        LDB MT3
        NI 0FFH
        BNZ NMSHOW0
        LDB INPDIR
        LR 4,A
        NI IN_RIGHT
        BNZ NMTAKE
        LR A,4
        NI IN_UP
        BNZ NMUP
        LR A,4
        NI IN_DOWN
        BNZ NMDN
NMSHOW0: JMP NMSHOW
NMRUB:  LDB MT0
        NI 0FFH
        BZ NMSHOW0
        AI -1
        STB MT0
        DCI MYNAME
        ADC
        CLR
        ST
        LI 12
        STB MT3
        SND SND_SELECT
        JMP NMSHOW
NMTAKE: LDB MT0
        CI NAME_LEN
        BZ NMSHOW0
        LDB MT1
        DCI PICKSET
        ADC
        LM
        LR 1,A
        LDB MT0
        DCI MYNAME
        ADC
        LR A,1
        ST
        INCB MT0
        LI 12
        STB MT3
        SND SND_SELECT
        JMP NMSHOW
NMUP:   LDB MT1
        AI -1
        BC NMUD
        LI 35
        BR NMUD
NMDN:   LDB MT1
        INC
        CI 36
        BNZ NMUD
        CLR
NMUD:   STB MT1
        LIS 8
        STB MT3
NMSHOW: LI CV1                  ; the name, big, blue on grey, then the letter
        STB TXINK               ;  blinking
        STB TXINK2
        LI 22
        STB TXX
        LI 19
        STB TXY
        DCI MYNAME
        PI EBTXT
        LDB MT0
        CI NAME_LEN
        BZ NMS5
        LDB JIFFY               ; the cursor: the letter, or a dash
        NI 10H
        LI '-'
        BZ NMS4
        LDB MT1
        DCI PICKSET
        ADC
        LM
NMS4:   STB MNAME
        LI ' '                  ; and a space after it
        STB MNAME+1
        CLR
        STB MNAME+2
        LI CV3
        STB TXINK
        STB TXINK2
        LDB MT0
        SL 1
        SL 1
        SL 1
        AI 22
        STB TXX
        DCI MNAME
        PI EBTXT
NMS5:   JMP NM1
NMEND:  LDB MT0                 ; two letters at least
        CI 1
        BNC NME1
        JMP NM1
NME1:   SND SND_SELECT
        LIS 1
        LR 0,A
        LEAVE

PICKSET: DB "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
S_YNAME: DB "YOUR NAME",0
S_NH1:  DB "UP/DOWN: A LETTER",0
S_NH2:  DB "PUSH: TAKE IT",0
S_NH3:  DB "PULL: RUB OUT",0
S_NH4:  DB "START: DONE",0

; ---- the rooms (menus.c table_list, net.c net_tables) -------------------------------
; TLSTART: T asked for (again every 60 jiffies until the count comes); the
; list drawn once every record is in (600 jiffies at most); then up and
; down choose, FIRE or START joins: SRVTBL = its id.
TLSTART: ENTER
        LI 0FFH
        STB TBLN
        CLR
        STB TBLHAVE
        STB MSEL
        STB MNSEL
        STB MWAIT
        STB MWAIT+1
        LDB INPDIR
        STB MPREV
        LI 60
        STB MBLINK
        LDB TXF
        OI TX_TABLES
        STB TXF
        LEAVE

TLTICK: ENTER
        LDB MNSEL
        NI 0FFH
        BZ TL0
        JMP TLUI
TL0:    LDB DT                  ; waiting: 600 jiffies, T every 60
        LR 0,A
        DCI MWAIT
        LM
        AS 0
        LR 1,A
        LM
        LNK
        LR 2,A
        DCI MWAIT
        LR A,1
        ST
        LR A,2
        ST
        LR A,1                  ; >= 600: no answer
        AI 256-(600&0FFH)
        LR A,2
        LNK
        AI 255-(600>>8)
        BNC TL1
TLFAIL: LIS 2
        LR 0,A
        LEAVE
TL1:    LDB TBLN
        CI 0FFH
        BNZ TL3
        LDB DT                  ; T again every 60 jiffies until the count comes
        COM
        INC
        LR 0,A
        LDB MBLINK
        AS 0
        BZ TL2
        BC TL2X                 ; (no borrow: still counting)
TL2:    LDB TXF
        OI TX_TABLES
        STB TXF
        LI 60
TL2X:   STB MBLINK
        BR TLON
TL3:    NI 0FFH                 ; the count: none is no answer
        BZ TLFAIL
        LR 3,A                  ; every record in?
        DCI TMASK-1
        ADC
        LM
        LR 4,A
        LDB TBLHAVE
        NS 4
        XS 4
        BNZ TLON
        LR A,3
        STB MNSEL               ; the list
        PI TLDRAW
TLON:   CLR
        LR 0,A
        LEAVE

; TLDRAW -- the list: PICK A ROOM white on black, each room a row on grey
;   (its name, its players: green waiting, red playing); FIRE: JOIN.
TLDRAW: ENTER
        CLR
        LR 0,A
        PI ECLS
        PALB 11, 59, 2
        DCI S_PICK
        LIS 4
        LR 1,A
        LI CV1
        LR 2,A
        LR 3,A
        PI ECENT
        CLR
        STB MT0
TL4:    LDB MT0                 ; its record: TBLS + 24 i
        SL 1
        SL 1
        SL 1
        LR 1,A
        SL 1
        AS 1
        DCI TBLS
        ADCU
        LR Q,DC
        LR A,QU
        STB MT1+10
        LR A,QL
        STB MT1+11
        LR DC,Q                 ; its ink: waiting green, playing red
        LI 22
        ADC
        LM
        NI 0FFH
        LI CV3
        BZ TL5
        LI CV2
TL5:    STB TXINK
        STB TXINK2
        LDB MT0                 ; its row: 12 + 6 i
        LR 1,A
        SL 1
        AS 1
        SL 1
        AI 12
        STB TXY
        LI 14
        STB TXX
        DCI MT1+10              ; its name: 10 of its 12
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LIS 8
        ADC
        XDC
        DCI MNAME+4             ; (a copy: 10 and a NUL)
        LI 10
        LR 2,A
TL5A:   XDC
        LM
        XDC
        NI 0FFH
        BNZ TL5B
        LI ' '
TL5B:   ST
        DS 2
        BNZ TL5A
        CLR
        ST
        DCI MNAME+4
        PI ETXT
        LDB TXX                 ; seated / of
        AI 4
        STB TXX
        DCI MT1+10
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LI 20
        ADC
        LM
        AI '0'
        STB MNAME+16            ; (ETCH spends the registers)
        LR DC,Q
        LI 21
        ADC
        LM
        AI '0'
        STB MNAME+18
        LI '/'
        STB MNAME+17
        CLR
        STB MNAME+19
        DCI MNAME+16
        PI ETXT
        INCB MT0
        LR 1,A
        LDB MNSEL
        XS 1
        BZ TL6
        JMP TL4
TL6:    LI 0FFH                 ; the cursor drawn at the next pass
        STB MT3
        LEAVE

; TLUI: up, down (pushed, not held); the cursor; FIRE or START: that one.
TLUI:   LDB MPREV
        COM
        LR 4,A
        LDB INPDIR
        STB MPREV
        NS 4
        LR 4,A
        NI IN_UP
        BZ TLU1
        LDB MSEL
        NI 0FFH
        BZ TLU1
        AI -1
        STB MSEL
        SND SND_SELECT
TLU1:   LR A,4
        NI IN_DOWN
        BZ TLU2
        LDB MSEL
        INC
        LR 1,A
        LDB MNSEL
        XS 1
        BZ TLU2
        LR A,1
        STB MSEL
        SND SND_SELECT
TLU2:   LDB MSEL                ; the cursor, when it moved: off the old, on the new
        LR 1,A
        LDB MT3
        XS 1
        BZ TLU5
        LDB MT3
        CI 0FFH
        BZ TLU3
        LR 0,A
        LI ' '
        LR 2,A
        PI TLCUR
TLU3:   LDB MSEL
        STB MT3
        LR 0,A
        LI '>'
        LR 2,A
        PI TLCUR
TLU5:   LDB EVBTN               ; FIRE or START: that one
        NI 0FFH
        BNZ TLU6
        LDB EVKEY
        CI KEY_END
        BZ TLU6
        JMP TLON
TLU6:   LDB MSEL                ; SRVTBL = its id
        SL 1
        SL 1
        SL 1
        LR 1,A
        SL 1
        AS 1
        DCI TBLS
        ADCU
        XDC
        DCI SRVTBL
        LI TABLE_LEN
        LR 2,A
TLU7:   XDC
        LM
        XDC
        ST
        DS 2
        BNZ TLU7
        CLR
        ST
        SND SND_SELECT
        LIS 1
        LR 0,A
        LEAVE

; TLCUR -- the character r2 at room row r0's cursor place, white.
TLCUR:  ENTER
        LR A,0
        LR 1,A
        SL 1
        AS 1
        SL 1
        AI 12
        STB TXY
        LIS 6
        STB TXX
        LI CV1
        STB TXINK
        STB TXINK2
        LR A,2
        LR 0,A
        PI ETCH
        LEAVE

TMASK:  DB 1, 3, 7, 0FH, 1FH, 3FH, 7FH, 0FFH
S_PICK: DB "PICK A ROOM",0

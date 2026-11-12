; p_menu.inc -- the screens before a table (src/menus.c): the title, the
; name, the rooms.

        BCTA,UN MENTER          ; E_ENTER: the title; the name if none (R0 = 1: a new one)
        BCTA,UN TLSTART         ; E_STATE: the room list asked for
        BCTA,UN TLTICK          ; E_TICK: its frame: R0 0 on, 1 picked, 2 no list

; TEXTSCR: a text screen: cleared, the sprites off, the text's user glyphs
; ('>' '-' ':' and the trophy). Calls two down. Clobbers R0-R3.
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
        DCOL    TXTI
        RETC,UN

; ---- title -------------------------------------------------------------------------
; The logo in block graphics (rows 2-4: a $C0 cell starts each row's
; blocks), "FOR FUJINET", the four bombers walking across, PRESS FIRE.
MENTER: BSTR,UN TEXTSCR
        LODI,R2 16*2-1          ; the logo
        LODI,R3 0               ; (LOGO's cell before the next)
MT1:    LODI,R0 GMOSAIC         ; (the row's first cell: its switch)
        STRA,R0 SCR,R2,+
MT2:    LODA,R0 LOGO,R3,+
        BCTR,EQ MT3
        STRA,R0 T0
        LODZ    R3              ; its row's ink
        RRR,R0
        RRR,R0
        RRR,R0
        RRR,R0
        ANDI,R0 3
        STRZ    R1
        LODA,R0 LOGOINK,R1
        IORA,R0 T0
MT3:    STRA,R0 SCR,R2,+
        LODZ    R3
        ANDI,R0 15
        COMI,R0 15
        BCFR,EQ MT2
        ADDI,R3 1               ; (the next row's first: under its switch)
        COMI,R3 16*LOGON
        BCFR,EQ MT1
MT4:    LODI,R0 SFORFN>>8
        LODI,R1 SFORFN&$FF
        LODI,R3 6
        ZBSR    *ZCENTR
        LODI,R0 SFIRE>>8
        LODI,R1 SFIRE&$FF
        LODI,R3 10
        ZBSR    *ZCENTR
        LODI,R0 STAG>>8
        LODI,R1 STAG&$FF
        LODI,R3 12
        ZBSR    *ZCENTR
        BSTA,UN SEATCOL         ; the bombers: colours, places
        LODI,R3 4
MT5:    LODA,R0 TWX0-1,R3
        STRA,R0 SCRATCH-1,R3
        BDRR,R3 MT5
        SND     MUS_TITLE
MT6:    ZBSR    *ZFRAME         ; (a frame)
        LODI,R3 3
MT7:    LODA,R0 FRML            ; every other jiffy a pixel on
        ANDI,R0 1
        BCTR,EQ MT8
        LODA,R0 SCRATCH,R3
        ADDI,R0 1
        COMI,R0 136
        BCTR,LT MT7A
        EORZ    R0
MT7A:   STRA,R0 SCRATCH,R3
MT8:    LODZ    R3              ; its registers: Y on row 8, X off the left edge
        ADDZ    R3
        STRZ    R2
        LODI,R0 256-16*8
        STRA,R0 SPRSH,R2
        LODA,R0 SCRATCH,R3
        ADDI,R0 SPRX0-ACOL*8-8
        STRA,R0 SPRSH+1,R2
        LODA,R0 FRML            ; its step: right0, or right1/2 by seat
        ANDI,R0 8
        BCTR,EQ MT9
        LODZ    R3
        ANDI,R0 1
        ADDI,R0 FR_RIGHT1
MT9:    STRA,R0 FRWANT,R3       ; (0: FR_DOWN0... FR_RIGHT0 below)
        BCFR,EQ MT9A
        LODI,R0 FR_RIGHT0
        STRA,R0 FRWANT,R3
MT9A:   SUBI,R3 1
        BCFR,LT MT7
        LODA,R0 EVBTN           ; FIRE, Enter or Start
        BCFR,EQ MT10
        LODA,R0 EVKEY
        COMI,R0 K_ENT
        BCTR,EQ MT10
        COMI,R0 K_START
        BCFA,EQ MT6
MT10:   LODI,R0 $FF             ; the jingle off
        STRA,R0 SNDP
        EORZ    R0
        STRA,R0 VOLUME
        LODA,R0 MYNAME          ; a name: done (R0 = 0)
        BCTA,EQ NAMEIN          ; else ask
        EORZ    R0
        RETC,UN

; SEATCOL: the four sprites in the seats' colours. Calls one down.
SEATCOL: LODI,R1 0
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
        ZBRR    *ZSPRCOL        ; tail call

LOGOINK: DB     LOGOI0,LOGOI1,LOGOI2
TWX0:   DB      0,34,68,102     ; the walkers' first places
SFORFN: DB      "FOR FUJINET",$FF
SFIRE:  DB      "PRESS FIRE",$FF
STAG:   DB      "2-4 PLAYERS",$FF

; ---- the name (menus.c name_entry; no keyboard) ------------------------------
; Up and down turn the letter at the cursor through A-Z and 0-9 (held, it
; runs on); FIRE or right takes it, left or Clear rubs out, Enter or Start
; ends (two at least). R0 = 1 (the caller saves it: key 0).
NAMEIN: BSTA,UN TEXTSCR
        LODI,R0 SYNAME>>8
        LODI,R1 SYNAME&$FF
        LODI,R3 1
        ZBSR    *ZCENTR
        LODI,R0 SNH1>>8
        LODI,R1 SNH1&$FF
        LODI,R3 7
        ZBSR    *ZCENTR
        LODI,R0 SNH2>>8
        LODI,R1 SNH2&$FF
        LODI,R3 8
        ZBSR    *ZCENTR
        LODI,R0 SNH3>>8
        LODI,R1 SNH3&$FF
        LODI,R3 9
        ZBSR    *ZCENTR
        LODI,R0 SNH4>>8
        LODI,R1 SNH4&$FF
        LODI,R3 10
        ZBSR    *ZCENTR
        EORZ    R0
        STRA,R0 T0              ; T0: letters so far
        STRA,R0 T1              ; T1: the letter at the cursor (0-35)
        STRA,R0 T3              ; T3: the repeat count
        LODI,R3 NAME_LEN+1
NM0:    STRA,R0 MYNAME-1,R3
        BDRR,R3 NM0
NM1:    ZBSR    *ZFRAME
        LODA,R0 T3
        BCTR,EQ NM2
        SUBI,R0 1
        STRA,R0 T3
NM2:    LODA,R0 EVKEY           ; ends
        COMI,R0 K_ENT
        BCTA,EQ NMEND
        COMI,R0 K_START
        BCTA,EQ NMEND
        COMI,R0 K_CLR           ; rubs out
        BCTR,EQ NMRUB
        LODA,R0 T3
        BCFR,EQ NM3
        LODA,R0 INDIR
        TMI,R0  D_LEFT
        BCTR,EQ NMRUB
NM3:    LODA,R0 EVBTN           ; takes it
        BCFR,EQ NMTAKE
        LODA,R0 T3
        BCFA,EQ NMSHOW
        LODA,R0 INDIR
        TMI,R0  D_RIGHT
        BCTR,EQ NMTAKE
        TMI,R0  D_UP
        BCTA,EQ NMUP
        TMI,R0  D_DOWN
        BCTA,EQ NMDN
        BCTA,UN NMSHOW
NMRUB:  LODA,R0 T0
        BCTA,EQ NMSHOW
        SUBI,R0 1
        STRA,R0 T0
        STRZ    R3
        EORZ    R0
        STRA,R0 MYNAME,R3
        LODI,R0 12
        STRA,R0 T3
        SND     SND_SELECT
        BCTA,UN NMSHOW
NMTAKE: LODA,R3 T0
        COMI,R3 NAME_LEN
        BCTA,EQ NMSHOW
        LODA,R2 T1
        LODA,R0 PICKSET,R2
        STRA,R0 MYNAME,R3
        ADDI,R3 1
        STRA,R3 T0
        LODI,R0 12
        STRA,R0 T3
        SND     SND_SELECT
        BCTR,UN NMSHOW
NMUP:   LODA,R0 T1
        SUBI,R0 1
        BCFR,LT NMUD
        LODI,R0 35
        BCTR,UN NMUD
NMDN:   LODA,R0 T1
        ADDI,R0 1
        COMI,R0 36
        BCFR,EQ NMUD
        EORZ    R0
NMUD:   STRA,R0 T1
        LODI,R0 8
        STRA,R0 T3
NMSHOW: DAT     4,4             ; the name, then the letter blinking
        DCOL    TXTI
        LODI,R3 0
NMS1:   COMA,R3 T0              ; (R3 letters shown)
        BCTR,EQ NMS2
        LODA,R0 MYNAME,R3
        BSTA,UN A2G
        ZBSR    *ZDPUTC
        ADDI,R3 1
        BCTR,UN NMS1
NMS2:   LODA,R0 FRML            ; the cursor: the letter, or a dash
        ANDI,R0 $10
        BCTR,EQ NMS3
        LODA,R2 T1
        LODA,R0 PICKSET,R2
        BSTA,UN A2G
        BCTR,UN NMS4
NMS3:   LODI,R0 GDASH
NMS4:   STRA,R0 T2
        DAT     4,4
        LODA,R0 T0              ; (R2: the cell before the cursor)
        ADDZ    R2
        STRZ    R2
        LODA,R0 T0
        COMI,R0 NAME_LEN
        BCTR,EQ NMS5
        LODA,R0 T2
        ZBSR    *ZDPUTC
NMS5:   EORZ    R0              ; and nothing after it
        ZBSR    *ZDPUTC
        BCTA,UN NM1
NMEND:  LODA,R0 T0              ; two letters at least
        COMI,R0 2
        BCTA,LT NM1
        SND     SND_SELECT
        LODI,R0 1
        RETC,UN
PICKSET: ASCII
        DB      "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
        GLYPHS
SYNAME: DB      "YOUR NAME",$FF
SNH1:   DB      "UP/DOWN: LETTER",$FF
SNH2:   DB      "FIRE: TAKE IT",$FF
SNH3:   DB      "LEFT: RUB OUT",$FF
SNH4:   DB      "ENTER: DONE",$FF

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

; ---- the rooms (menus.c table_list, net.c net_tables) -----------------------------
; TLSTART: T asked for (again every 60 frames until the count comes);
; the list drawn once every record is in (600 frames at most); then up
; and down choose, FIRE or Enter joins: SRVTBL = its id.
TLSTART: LODI,R0 $FF
        STRA,R0 TBLN
        EORZ    R0
        STRA,R0 TBLHAVE
        STRA,R0 SEL
        STRA,R0 NSEL            ; (0: the list not drawn yet)
        STRA,R0 JOINTH          ; (the frames waited)
        STRA,R0 JOINTL
        LODA,R0 TXF
        IORI,R0 TX_TABLES
        STRA,R0 TXF
        RETC,UN

TLTICK: LODA,R0 NSEL
        BCFA,EQ TLUI
        LODA,R0 JOINTL          ; waiting: 600 frames, T every 60
        ADDI,R0 1
        STRA,R0 JOINTL
        PPSL    PSLWC
        LODA,R0 JOINTH
        ADDI,R0 0
        STRA,R0 JOINTH
        CPSL    PSLWC
        COMI,R0 600>>8
        BCTR,LT TL1
        LODA,R0 JOINTL
        COMI,R0 600&$FF
        BCTR,LT TL1
TLFAIL: LODI,R0 2
        RETC,UN
TL1:    LODA,R0 TBLN
        COMI,R0 $FF
        BCFR,EQ TL3
        LODA,R0 JOINTL          ; (every 60: its low byte, mod 60)
        STRA,R0 T0
TL2:    LODA,R0 T0
        COMI,R0 60
        BCTR,LT TL2A
        SUBI,R0 60
        STRA,R0 T0
        BCTR,UN TL2
TL2A:   COMI,R0 0
        BCFR,EQ TLON
        LODA,R0 TXF
        IORI,R0 TX_TABLES
        STRA,R0 TXF
TLON:   EORZ    R0
        RETC,UN
TL3:    COMI,R0 0               ; the count: none is no answer
        BCTR,EQ TLFAIL
        STRZ    R1              ; every record in?
        LODA,R0 TMASK-1,R1
        ANDA,R0 TBLHAVE
        COMA,R0 TMASK-1,R1
        BCFR,EQ TLON
        STRA,R1 NSEL            ; the list
        BSTA,UN TEXTSCR
        LODI,R0 SPICK>>8
        LODI,R1 SPICK&$FF
        LODI,R3 0
        ZBSR    *ZCENTR
        LODI,R0 SJOIN>>8
        LODI,R1 SJOIN&$FF
        LODI,R3 12
        ZBSR    *ZCENTR
        LODI,R3 0
TL4:    STRA,R3 T1              ; a room a row from row 2
        LODA,R0 TROWOFF,R3      ; its record
        STRA,R0 T0
        STRZ    R1
        LODI,R0 TXTI            ; its ink: waiting, or playing
        STRA,R0 DCOLOR
        LODA,R0 TBLS+22,R1
        BCTR,EQ TL5
        LODI,R0 LISTI1
        STRA,R0 DCOLOR
TL5:    LODA,R1 T1
        ADDI,R1 2
        LODI,R2 0
        ZBSR    *ZDPOS
        LODA,R3 T0              ; its name, 10 of its 12
        ADDI,R3 7
        LODI,R0 10
        STRA,R0 T2
TL6:    LODA,R0 TBLS,R3,+
        BSTA,UN A2G
        ZBSR    *ZDPUTC
        LODA,R0 T2
        SUBI,R0 1
        STRA,R0 T2
        BCFR,EQ TL6
        LODA,R3 T0              ; seated / of
        LODI,R2 11
        LODA,R0 TBLS+20,R3
        IORI,R0 GDIGIT
        ZBSR    *ZDPUTC
        LODI,R0 GSLASH
        ZBSR    *ZDPUTC
        LODA,R3 T0
        LODA,R0 TBLS+21,R3
        IORI,R0 GDIGIT
        ZBSR    *ZDPUTC
        LODA,R3 T1
        ADDI,R3 1
        COMA,R3 NSEL
        BCFA,EQ TL4
        BCTA,UN TLON

TLUI:   LODA,R0 INDIR           ; up, down (pushed, not held)
        STRA,R0 T0
        LODA,R0 PRVDIR
        EORI,R0 $FF
        ANDA,R0 T0
        STRA,R0 T0
        TMI,R0  D_UP
        BCFR,EQ TLU1
        LODA,R0 SEL
        BCTR,EQ TLU1
        SUBI,R0 1
        STRA,R0 SEL
        SND     SND_SELECT
TLU1:   LODA,R0 T0
        TMI,R0  D_DOWN
        BCFR,EQ TLU2
        LODA,R0 SEL
        ADDI,R0 1
        COMA,R0 NSEL
        BCTR,EQ TLU2
        STRA,R0 SEL
        SND     SND_SELECT
TLU2:   LODI,R3 0               ; the cursor
TLU3:   LODZ    R3
        ADDI,R0 2
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        STRZ    R2
        EORZ    R0
        COMA,R3 SEL
        BCFR,EQ TLU4
        LODI,R0 GCURSOR|TXTI
TLU4:   STRA,R0 SCR,R2
        ADDI,R3 1
        COMA,R3 NSEL
        BCFR,EQ TLU3
        LODA,R0 EVBTN           ; FIRE or Enter: that one
        BCFR,EQ TLU5
        LODA,R0 EVKEY
        COMI,R0 K_ENT
        BCFA,EQ TLON
TLU5:   LODA,R1 SEL             ; SRVTBL = its id
        LODA,R0 TROWOFF,R1
        SUBI,R0 1
        STRZ    R1
        LODI,R3 $FF
TLU6:   LODA,R0 TBLS,R1,+
        STRA,R0 SRVTBL,R3,+
        COMI,R3 TABLE_LEN-1
        BCFR,EQ TLU6
        EORZ    R0
        STRA,R0 SRVTBL+TABLE_LEN
        SND     SND_SELECT
        LODI,R0 1
        RETC,UN

TMASK:  DB      1,3,7,$F,$1F,$3F,$7F,$FF
TROWOFF: DB     0,TABLE_REC,2*TABLE_REC,3*TABLE_REC,4*TABLE_REC,5*TABLE_REC,6*TABLE_REC,7*TABLE_REC
SPICK:  DB      "PICK A ROOM",$FF
SJOIN:  DB      "FIRE: JOIN",$FF

        INCLUDE "logo.inc"

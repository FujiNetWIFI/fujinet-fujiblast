; p_menu.inc -- the screens before a table (src/menus.c): the title, the
; name, the rooms. The Arcadia's (src/arcadia/src/p_menu.inc), transcribed.

        JP      MENTER          ; E_ENTER: the title; the name if none (A = 1: a new one)
        JP      TLSTART         ; E_STATE: the room list asked for
        JP      TLTICK          ; E_TICK: its frame: A 0 on, 1 picked, 2 no list

; TEXTSCR: a text screen: its colours, band table HL, cleared.
TEXTSCR: CALL   SETBANDS
        LD      HL,P_TEXT
        CALL    SETPAL
        LD      D,0
        LD      C,LINES
        JP      CLRLN

; ---- title -------------------------------------------------------------------------
; The logo, its colours running down it a band at a time; FOR FUJINET; the
; four bombers walking across a strip of floor; PRESS FIRE, blinking.
MENTER: LD      HL,B_TITLE
        CALL    TEXTSCR
        LD      HL,LOGO         ; the logo: LOGOH lines of LOGOW bytes
        LD      DE,SCR+TLOGO*BYTEPL+(BYTEPL-LOGOW)/2
        LD      B,LOGOH
MT1:    PUSH    BC
        PUSH    DE
        LD      BC,LOGOW
        LDIR
        POP     DE
        EX      DE,HL
        LD      BC,BYTEPL
        ADD     HL,BC
        EX      DE,HL
        POP     BC
        DJNZ    MT1
        LD      HL,SFORFN
        LD      DE,26*256+(BYTEPL-22)/2
        LD      C,3*4+0
        CALL    BIGAT
        LD      HL,STAG
        LD      D,38
        LD      C,2*4+0
        CALL    CENTRE
        LD      DE,TLANEY*256+0 ; the lane: floor
        LD      BC,BYTEPL*256+TXH
        LD      A,55H
        CALL    FILLB
        LD      HL,TWX0         ; the walkers' places (WORK+40: DRSTRIP keeps out)
        LD      DE,WORK+40
        LD      BC,4
        LDIR
        SND     MUS_TITLE
MT6:    CALL    FRAME
        LD      A,(FRML)        ; PRESS FIRE: a second on, half off
        AND     3FH
        CP      40
        LD      HL,SPFIRE
        JR      C,MT6A
        LD      HL,SBLANK
MT6A:   LD      D,TLANEY+14
        LD      C,3*4+0
        CALL    CENTRE
        LD      C,0
MT7:    LD      B,0
        LD      HL,WORK+40
        ADD     HL,BC
        LD      A,(FRML)        ; every other jiffy a pixel on
        AND     1
        JR      Z,MT8
        INC     (HL)
        LD      A,(HL)
        CP      168
        JR      C,MT8
        LD      (HL),0
MT8:    LD      E,(HL)          ; its step: right0, or right1/2 by seat
        LD      A,(FRML)
        AND     8
        LD      B,FR_RIGHT0
        JR      Z,MT9
        LD      A,C
        AND     1
        ADD     A,FR_RIGHT1
        LD      B,A
MT9:    PUSH    BC
        LD      A,C
        LD      D,TLANEY
        LD      C,55H
        FAR     PG_ICON,DRSTRIP
        POP     BC
        INC     C
        LD      A,C
        CP      4
        JR      NZ,MT7
        LD      A,(EVBTN)       ; FIRE or =
        OR      A
        JR      NZ,MT10
        LD      A,(EVKEY)
        CP      K_ENT
        JR      NZ,MT6
MT10:   CALL    SNDOFF          ; the jingle off
        LD      A,(MYNAME)      ; a name: done (A = 0)
        OR      A
        JP      Z,NAMEIN        ; else ask
        XOR     A
        RET

TWX0:   DB      0,42,84,126     ; the walkers' first places
SFORFN: DB      "FOR FUJINET",0
SPFIRE: DB      "PRESS FIRE",0
SBLANK: DB      "          ",0
STAG:   DB      "2-4 PLAYERS",0

; ---- the name (menus.c name_entry; no keyboard) ------------------------------
; Up and down turn the letter at the cursor through A-Z and 0-9 (held, it
; runs on); FIRE or right takes it, left or C rubs out, = ends (two at
; least). A = 1 (the caller saves it: key 0).
NAMEIN: LD      HL,B_MENU
        CALL    TEXTSCR
        LD      HL,SYNAME
        LD      DE,TXH*256+11
        LD      C,3*4+0
        CALL    BIGAT
        LD      HL,SNH1
        LD      D,7*TXH
        LD      C,3*4+0
        CALL    CENTRE
        LD      HL,SNH2
        LD      D,8*TXH
        LD      C,3*4+0
        CALL    CENTRE
        LD      HL,SNH3
        LD      D,9*TXH
        LD      C,3*4+0
        CALL    CENTRE
        LD      HL,SNH4
        LD      D,10*TXH
        LD      C,3*4+0
        CALL    CENTRE
        XOR     A
        LD      (T0),A          ; T0: letters so far
        LD      (T1),A          ; T1: the letter at the cursor (0-35)
        LD      (T3),A          ; T3: the repeat count
        LD      HL,MYNAME
        LD      B,NAME_LEN+1
NM0:    LD      (HL),A
        INC     HL
        DJNZ    NM0
NM1:    CALL    FRAME
        LD      HL,T3
        LD      A,(HL)
        OR      A
        JR      Z,NM2
        DEC     (HL)
NM2:    LD      A,(EVKEY)       ; ends
        CP      K_ENT
        JP      Z,NMEND
        CP      K_CLR           ; rubs out
        JR      Z,NMRUB
        CP      K_BACK
        JR      Z,NMRUB
        LD      A,(T3)
        OR      A
        JR      NZ,NM3
        LD      A,(INDIR)
        BIT     2,A             ; D_LEFT
        JR      NZ,NMRUB
NM3:    LD      A,(EVBTN)       ; takes it
        OR      A
        JR      NZ,NMTAKE
        LD      A,(T3)
        OR      A
        JR      NZ,NMSHOW
        LD      A,(INDIR)
        BIT     3,A             ; D_RIGHT
        JR      NZ,NMTAKE
        BIT     0,A             ; D_UP
        JR      NZ,NMUP
        BIT     1,A             ; D_DOWN
        JR      NZ,NMDN
        JR      NMSHOW
NMRUB:  LD      A,(T0)
        OR      A
        JR      Z,NMSHOW
        DEC     A
        LD      (T0),A
        LD      E,A
        LD      D,0
        LD      HL,MYNAME
        ADD     HL,DE
        LD      (HL),0
        LD      A,12
        LD      (T3),A
        SND     SND_SELECT
        JR      NMSHOW
NMTAKE: LD      A,(T0)
        CP      NAME_LEN
        JR      Z,NMSHOW
        LD      E,A
        LD      D,0
        LD      HL,MYNAME
        ADD     HL,DE
        PUSH    HL
        LD      A,(T1)
        LD      E,A
        LD      HL,PICKSET
        ADD     HL,DE
        LD      A,(HL)
        POP     HL
        LD      (HL),A
        LD      HL,T0
        INC     (HL)
        LD      A,12
        LD      (T3),A
        SND     SND_SELECT
        JR      NMSHOW
NMUP:   LD      A,(T1)
        SUB     1
        JR      NC,NMUD
        LD      A,35
        JR      NMUD
NMDN:   LD      A,(T1)
        INC     A
        CP      36
        JR      NZ,NMUD
        XOR     A
NMUD:   LD      (T1),A
        LD      A,8
        LD      (T3),A
NMSHOW: LD      HL,MYNAME       ; the name, big, then the letter blinking
        LD      DE,4*TXH*256+12
        LD      C,3*4+0
        CALL    BIGAT
        LD      A,(T0)
        CP      NAME_LEN
        JR      Z,NMS5
        LD      A,(FRML)        ; the cursor: the letter, or a dash
        AND     10H
        LD      A,'-'
        JR      Z,NMS4
        LD      A,(T1)
        LD      E,A
        LD      D,0
        LD      HL,PICKSET
        ADD     HL,DE
        LD      A,(HL)
NMS4:   LD      (WORK),A
        LD      A,' '           ; and a space after it
        LD      (WORK+1),A
        XOR     A
        LD      (WORK+2),A
        LD      A,(T0)
        ADD     A,A
        ADD     A,12
        LD      E,A
        LD      D,4*TXH
        LD      HL,WORK
        LD      C,1*4+0
        CALL    BIGAT
NMS5:   JP      NM1
NMEND:  LD      A,(T0)          ; two letters at least
        CP      2
        JP      C,NM1
        SND     SND_SELECT
        LD      A,1
        RET
PICKSET: DB     "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
SYNAME: DB      "YOUR NAME",0
SNH1:   DB      "UP/DOWN: LETTER",0
SNH2:   DB      "FIRE: TAKE IT",0
SNH3:   DB      "LEFT: RUB OUT",0
SNH4:   DB      "=: DONE",0

; ---- the rooms (menus.c table_list, net.c net_tables) -----------------------------
; TLSTART: T asked for (again every 60 frames until the count comes);
; the list drawn once every record is in (600 frames at most); then up
; and down choose, FIRE or = joins: SRVTBL = its id.
TLSTART: LD     A,0FFH
        LD      (TBLN),A
        XOR     A
        LD      (TBLHAVE),A
        LD      (SEL),A
        LD      (NSEL),A        ; (0: the list not drawn yet)
        LD      HL,0            ; (the frames waited)
        LD      (JOINT),HL
        LD      A,(TXF)
        OR      TX_TABLES
        LD      (TXF),A
        RET

TLTICK: LD      A,(NSEL)
        OR      A
        JP      NZ,TLUI
        LD      HL,(JOINT)      ; waiting: 600 frames, T every 60
        INC     HL
        LD      (JOINT),HL
        LD      DE,600
        OR      A
        SBC     HL,DE
        JR      C,TL1
TLFAIL: LD      A,2
        RET
TL1:    LD      A,(TBLN)
        CP      0FFH
        JR      NZ,TL3
        LD      HL,(JOINT)      ; (every 60)
        LD      DE,60
TL2:    OR      A
        SBC     HL,DE
        JR      Z,TL2A
        JR      NC,TL2
        JR      TLON
TL2A:   LD      A,(TXF)
        OR      TX_TABLES
        LD      (TXF),A
TLON:   XOR     A
        RET
TL3:    OR      A               ; the count: none is no answer
        JR      Z,TLFAIL
        LD      C,A             ; every record in?
        LD      B,0
        LD      HL,TMASK-1
        ADD     HL,BC
        LD      A,(TBLHAVE)
        AND     (HL)
        CP      (HL)
        JR      NZ,TLON
        LD      A,C
        LD      (NSEL),A        ; the list
        LD      HL,B_MENU
        CALL    TEXTSCR
        LD      HL,SPICK
        LD      D,0
        LD      C,3*4+0
        CALL    CENTRE
        LD      HL,SFJOIN
        LD      D,11*TXH
        LD      C,3*4+0
        CALL    CENTRE
        LD      C,0
TL4:    PUSH    BC              ; a room a row from row 2
        LD      A,C             ; its record: TBLS + 24 C
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      E,A
        ADD     A,A
        ADD     A,E
        LD      E,A
        LD      D,0
        LD      HL,TBLS
        ADD     HL,DE
        PUSH    HL
        POP     IX
        LD      A,C             ; its line
        ADD     A,2
        CALL    ROWLNM
        LD      E,2
        LD      C,3*4+0         ; its ink: waiting, or playing
        LD      A,(IX+22)
        OR      A
        JR      Z,TL5
        LD      C,2*4+0
TL5:    PUSH    IX              ; its name, 10 of its 12
        POP     HL
        LD      A,L
        ADD     A,8
        LD      L,A
        JR      NC,TL5A
        INC     H
TL5A:   LD      B,10
        CALL    TXTN
        LD      A,(IX+20)       ; seated / of
        ADD     A,'0'
        CALL    GLYPHC
        LD      A,'/'
        CALL    GLYPHC
        LD      A,(IX+21)
        ADD     A,'0'
        CALL    GLYPHC
        POP     BC
        INC     C
        LD      A,(NSEL)
        CP      C
        JR      NZ,TL4
        JP      TLON

; ROWLNM: D = text row A's line (7 A). Clobbers A.
ROWLNM: LD      D,A
        ADD     A,A
        ADD     A,A
        ADD     A,A
        SUB     D
        LD      D,A
        RET

TLUI:   LD      A,(PRVDIR)      ; up, down (pushed, not held)
        CPL
        LD      B,A
        LD      A,(INDIR)
        AND     B
        LD      (T0),A
        BIT     0,A             ; D_UP
        JR      Z,TLU1
        LD      A,(SEL)
        OR      A
        JR      Z,TLU1
        DEC     A
        LD      (SEL),A
        SND     SND_SELECT
TLU1:   LD      A,(T0)
        BIT     1,A             ; D_DOWN
        JR      Z,TLU2
        LD      A,(SEL)
        INC     A
        LD      HL,NSEL
        CP      (HL)
        JR      Z,TLU2
        LD      (SEL),A
        SND     SND_SELECT
TLU2:   LD      C,0             ; the cursor
TLU3:   LD      A,C
        ADD     A,2
        CALL    ROWLNM
        LD      E,0
        LD      A,(SEL)
        CP      C
        LD      A,' '
        JR      NZ,TLU4
        LD      A,'>'
TLU4:   PUSH    BC
        LD      C,1*4+0
        CALL    CHRAT
        POP     BC
        INC     C
        LD      A,(NSEL)
        CP      C
        JR      NZ,TLU3
        LD      A,(EVBTN)       ; FIRE or =: that one
        OR      A
        JR      NZ,TLU5
        LD      A,(EVKEY)
        CP      K_ENT
        JP      NZ,TLON
TLU5:   LD      A,(SEL)         ; SRVTBL = its id
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      E,A
        ADD     A,A
        ADD     A,E
        LD      E,A
        LD      D,0
        LD      HL,TBLS
        ADD     HL,DE
        LD      DE,SRVTBL
        LD      BC,TABLE_LEN
        LDIR
        XOR     A
        LD      (SRVTBL+TABLE_LEN),A
        SND     SND_SELECT
        LD      A,1
        RET

TMASK:  DB      1,3,7,0FH,1FH,3FH,7FH,0FFH
SPICK:  DB      "PICK A ROOM",0
SFJOIN: DB      "FIRE: JOIN",0

        INCLUDE "logo.inc"

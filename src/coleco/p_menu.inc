; p_menu.inc -- the screens before a table (src/menus.c): the title, the
; name, the rooms. The Astrocade's (src/astrocade/src/p_menu.inc), laid out
; for the Adam's screen (src/adam/gfx.c: 32 x 24 cells, the bombers sprites).

        JP      MENTER          ; E_ENTER: the title; the name if none (A = 1: a new one)
        JP      TLSTART         ; E_STATE: the room list asked for
        JP      TLTICK          ; E_TICK: its frame: A 0 on, 1 picked, 2 no list

; ---- title -------------------------------------------------------------------------
; The logo; FOR FUJINET; the four bombers walking across; PRESS FIRE,
; blinking (menus.c title). WORK: the clock (2), the walkers' base, the
; seat, its x.
MENTER: CALL    GFXBEGIN
        LD      D,2
        CALL    GFXLOGO
        LD      HL,SFORFN
        LD      D,7
        LD      C,TC_WHITE
        CALL    CENTRE
        LD      HL,STAG
        LD      D,20
        LD      C,TC_WHITE
        CALL    CENTRE
        SND     MUS_TITLE
        CALL    GFXSHOW
        LD      HL,0
        LD      (WORK),HL
MT6:    CALL    FRAME
        LD      HL,(WORK)
        INC     HL
        LD      (WORK),HL
        LD      A,(WORK)        ; PRESS FIRE: a second on, half off
        AND     3FH
        CP      40
        LD      HL,SPFIRE
        JR      C,MT6A
        LD      HL,SBLANK
MT6A:   LD      D,17
        LD      C,TC_CYAN
        CALL    CENTRE
        LD      HL,(WORK)       ; the walkers' base: t >> 1
        SRL     H
        RR      L
        LD      A,L
        LD      (WORK+2),A
        XOR     A
        LD      (WORK+3),A
MT7:    LD      A,(WORK+3)      ; seat n: x = (base + 40 n) % 168
        LD      B,A
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      C,A
        ADD     A,A
        ADD     A,A
        ADD     A,C
        LD      C,A
        LD      A,(WORK+2)
        ADD     A,C
MT7A:   CP      168
        JR      C,MT7B
        SUB     168
        JR      MT7A
MT7B:   LD      (WORK+4),A
        LD      A,(WORK)        ; its frame: right 0, or 1 or 2 by seat
        AND     8
        LD      D,FR_RIGHT0
        JR      Z,MT7C
        LD      A,B
        AND     1
        ADD     A,FR_RIGHT1
        LD      D,A
MT7C:   LD      A,B             ; its y: 80 + 8 n (the VDP shows 4 a line)
        ADD     A,A
        ADD     A,A
        ADD     A,A
        ADD     A,80
        LD      C,A
        LD      E,B             ; the seat
        LD      A,(WORK+4)
        LD      B,A             ; x
        LD      A,E             ; n
        CALL    SPRAT
        LD      HL,WORK+3
        INC     (HL)
        LD      A,(HL)
        CP      4
        JR      NZ,MT7
        CALL    SPRFLUSH
        LD      A,(EVBTN)       ; FIRE or #
        OR      A
        JR      NZ,MT10
        LD      A,(EVKEY)
        CP      K_ENT
        JP      NZ,MT6
MT10:   CALL    SNDOFF          ; the jingle off
        CALL    SPRHIDE
        LD      A,(MYNAME)      ; a name: done (A = 0)
        OR      A
        JP      Z,NAMEIN        ; else ask
        XOR     A
        RET

SFORFN: DB      "FOR FUJINET",0
SPFIRE: DB      "PRESS FIRE",0
SBLANK: DB      "          ",0
STAG:   DB      "BOMBS AND BLASTS FOR 2-4",0

; ---- the name (menus.c name_entry; no keyboard) ------------------------------
; Up and down turn the letter at the cursor through A-Z and 0-9 (held, it
; runs on); FIRE or right takes it, left or * rubs out, # ends (two at
; least). A = 1 (the caller saves it: key 0).
NAMEIN: CALL    GFXBEGIN
        LD      HL,SYNAME
        LD      D,3
        LD      C,TC_CYAN
        CALL    CENTRE
        LD      HL,SNH1
        LD      D,6
        LD      C,TC_WHITE
        CALL    CENTRE
        LD      HL,SNH2
        LD      D,8
        LD      C,TC_WHITE
        CALL    CENTRE
        LD      HL,SNH3
        LD      D,10
        LD      C,TC_WHITE
        CALL    CENTRE
        LD      HL,SNH4
        LD      D,12
        LD      C,TC_WHITE
        CALL    CENTRE
        CALL    GFXSHOW
        LD      A,1
        LD      (INNAME),A      ; (the keypad's * rubs out here)
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
NMSHOW: LD      HL,MYNAME       ; the name, then the letter blinking after it
        LD      DE,15*256+12
        LD      C,TC_YELLOW
        LD      B,NAME_LEN
        CALL    TXTN
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
NMS4:   LD      B,A
        LD      A,(T0)
        ADD     A,12
        LD      E,A
        LD      D,15
        LD      C,TC_YELLOW
        LD      A,B
        CALL    CHRAT
NMS5:   JP      NM1
NMEND:  LD      A,(T0)          ; two letters at least
        CP      2
        JP      C,NM1
        SND     SND_SELECT
        XOR     A
        LD      (INNAME),A
        LD      A,1
        RET
PICKSET: DB     "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
SYNAME: DB      "WHAT IS YOUR NAME?",0
SNH1:   DB      "UP/DOWN: THE LETTER",0
SNH2:   DB      "FIRE OR RIGHT: TAKE IT",0
SNH3:   DB      "LEFT OR *: RUB OUT",0
SNH4:   DB      "#: DONE",0

; ---- the rooms (menus.c table_list, net.c net_tables) -----------------------------
; TLSTART: T asked for (again every 60 frames until the count comes);
; the list drawn once every record is in (600 frames at most); then up
; and down choose, FIRE or # joins: SRVTBL = its id.
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
        CALL    GFXBEGIN
        LD      HL,SPICK
        LD      D,1
        LD      C,TC_CYAN
        CALL    CENTRE
        LD      HL,SFJOIN
        LD      D,21
        LD      C,TC_WHITE
        CALL    CENTRE
        XOR     A
        LD      (WORK+40),A
TL4:    LD      A,(WORK+40)     ; a room a pair of rows from row 4
        LD      C,A             ; its record: TBLS + 24 C
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
        PUSH    IX              ; its name, 12 characters (a NUL a space)
        POP     HL
        LD      DE,8
        ADD     HL,DE
        LD      DE,WORK
        LD      B,12
TL5A:   LD      A,(HL)
        OR      A
        JR      NZ,TL5B
        LD      A,' '
TL5B:   LD      (DE),A
        INC     HL
        INC     DE
        DJNZ    TL5A
        LD      A,' '           ; seated / of
        LD      (DE),A
        INC     DE
        LD      A,(IX+20)
        ADD     A,'0'
        LD      (DE),A
        INC     DE
        LD      A,'/'
        LD      (DE),A
        INC     DE
        LD      A,(IX+21)
        ADD     A,'0'
        LD      (DE),A
        INC     DE
        LD      A,' '
        LD      (DE),A
        INC     DE
        LD      HL,SWAITING     ; its phase, and its ink: waiting, or playing
        LD      A,(IX+22)
        OR      A
        JR      Z,TL5C
        LD      HL,SPLAYING
TL5C:   LD      BC,8
        LDIR
        LD      A,(WORK+40)
        ADD     A,A
        ADD     A,4
        LD      D,A
        LD      E,4
        LD      HL,WORK
        LD      C,TC_WHITE
        LD      A,(IX+22)
        OR      A
        JR      Z,TL5D
        LD      C,TC_GREY
TL5D:   CALL    TXTAT
        LD      HL,WORK+40
        INC     (HL)
        LD      A,(NSEL)
        CP      (HL)
        JR      NZ,TL4
        CALL    GFXSHOW
        JP      TLON

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
TLU2:   XOR     A               ; the cursor
        LD      (WORK+40),A
TLU3:   LD      A,(WORK+40)
        ADD     A,A
        ADD     A,4
        LD      D,A
        LD      E,2
        LD      A,(WORK+40)
        LD      HL,SEL
        CP      (HL)
        LD      A,' '
        JR      NZ,TLU4
        LD      A,'>'
TLU4:   LD      C,TC_YELLOW
        CALL    CHRAT
        LD      HL,WORK+40
        INC     (HL)
        LD      A,(NSEL)
        CP      (HL)
        JR      NZ,TLU3
        LD      A,(EVBTN)       ; FIRE or #: that one
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
SWAITING: DB    "WAITING",0
SPLAYING: DB    "PLAYING",0

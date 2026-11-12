; p_icon.inc -- the bombers and tiles drawn outside the arena: the HUD's
; and the panel's icons, the title's walkers, the waiting room's and the
; trophies' (the arena's own are the compositor's, p_draw.inc). Plain
; drawing on a plain ground, a bomber as two 1-bit masks a row (BOMBART)
; shifted into place and widened (IEXP16), in its seat's colours; the tiles
; a copy of the arena's (icon.inc). Its scratch is the compositor's WORK
; (LB, RNC; WORK+50 up), never both at once.

        RET                     ; E_ENTER
        NOP
        NOP
        RET                     ; E_STATE
        NOP
        NOP
        RET                     ; E_TICK
        NOP
        NOP

; ---- pictures outside the arena (the HUD, the panel, the trophies) ------------------------------
; DRICON: tile code A's picture (frame 0) at line D, byte E, as it is.
DRICON: ADD     A,A
        LD      C,A
        LD      B,0
        LD      HL,ITILEPTR
        ADD     HL,BC
        ADD     HL,BC
        LD      C,(HL)
        INC     HL
        LD      B,(HL)
        PUSH    BC
        CALL    LINEAD
        LD      DE,SCR
        ADD     HL,DE
        EX      DE,HL
        POP     HL
        LD      B,TILEH
DIC1:   LD      A,(HL)
        LD      (DE),A
        INC     HL
        INC     DE
        LD      A,(HL)
        LD      (DE),A
        INC     HL
        LD      A,E
        ADD     A,BYTEPL-1
        LD      E,A
        JR      NC,DIC2
        INC     D
DIC2:   DJNZ    DIC1
        RET

; DRBICON: seat A's bomber standing (frame down0, unshifted) at line D,
; byte E, on ground byte B.
DRBICON: LD     C,B
        LD      (WORK+54),BC
        PUSH    AF
        CALL    LINEAD
        LD      DE,SCR
        ADD     HL,DE
        LD      (WORK+56),HL
        POP     AF
        LD      C,A             ; the seat's patterns
        LD      B,0
        LD      HL,ISEATVP
        ADD     HL,BC
        LD      A,(HL)
        LD      (WORK+59),A
        LD      A,C
        ADD     A,A
        LD      (WORK+55),A     ; SEATBP's index, even rows
        LD      IY,BOMBART+FR_DOWN0*14
        XOR     A
        LD      (WORK+62),A     ; the row
DBI1:   LD      A,(WORK+62)
        AND     1
        LD      HL,WORK+55
        ADD     A,(HL)
        LD      E,A
        LD      D,0
        LD      HL,ISEATBP
        ADD     HL,DE
        LD      A,(HL)
        LD      (WORK+63),A
        LD      A,(WORK+54)     ; LB: the ground
        LD      (LB),A
        LD      (LB+1),A
        LD      A,2
        LD      (RNC),A
        LD      A,(IY+0)
        LD      B,A
        LD      A,(IY+1)
        LD      C,A
        XOR     A
        CALL    ONE
        LD      A,(IY+0)
        RLCA
        RLCA
        RLCA
        RLCA
        LD      B,A
        LD      A,(IY+1)
        RLCA
        RLCA
        RLCA
        RLCA
        LD      C,A
        LD      A,1
        CALL    ONE
        LD      HL,(WORK+56)
        LD      A,(LB)
        LD      (HL),A
        INC     HL
        LD      A,(LB+1)
        LD      (HL),A
        LD      DE,BYTEPL-1
        ADD     HL,DE
        LD      (WORK+56),HL
        INC     IY
        INC     IY
        LD      HL,WORK+62
        INC     (HL)
        LD      A,(HL)
        CP      TILEH
        JR      NZ,DBI1
        RET

; DRSTRIP: seat A's bomber in frame B at line D, pixel E (0-159), on a
; strip of ground byte C: four bytes a row from the byte before it (so a
; walker moving right leaves nothing behind), cut at the screen's edges.
DRSTRIP: LD     (WORK+50),A     ; the seat
        LD      A,C
        LD      (WORK+54),A     ; the ground
        LD      A,E
        AND     3
        LD      (WORK+52),A     ; the shift
        LD      A,E
        RRCA
        RRCA
        AND     3FH
        DEC     A
        LD      (WORK+53),A     ; the first byte (0FFH: off the left)
        LD      E,0
        CALL    LINEAD
        LD      DE,SCR
        ADD     HL,DE
        LD      (WORK+56),HL    ; the line's start
        LD      L,B             ; the frame: BOMBART + 14 B
        LD      H,0
        ADD     HL,HL
        LD      D,H
        LD      E,L
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        OR      A
        SBC     HL,DE
        LD      DE,BOMBART
        ADD     HL,DE
        PUSH    HL
        POP     IY
        LD      A,4
        LD      (RNC),A
        XOR     A
        LD      (WORK+62),A
STR1:   LD      A,(WORK+50)     ; the patterns
        LD      C,A
        LD      B,0
        LD      HL,ISEATVP
        ADD     HL,BC
        LD      A,(HL)
        LD      (WORK+59),A
        LD      A,(WORK+62)
        AND     1
        LD      E,A
        LD      A,C
        ADD     A,A
        ADD     A,E
        LD      E,A
        LD      D,0
        LD      HL,ISEATBP
        ADD     HL,DE
        LD      A,(HL)
        LD      (WORK+63),A
        LD      A,(WORK+54)
        LD      HL,LB
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        LD      D,(IY+0)        ; the row, shifted
        LD      B,(IY+1)
        LD      E,0
        LD      C,E
        LD      A,(WORK+52)
        OR      A
        JR      Z,STR3
STR2:   SRL     D
        RR      E
        SRL     B
        RR      C
        DEC     A
        JR      NZ,STR2
STR3:   PUSH    DE
        PUSH    BC
        LD      C,B
        LD      B,D
        LD      A,1
        CALL    ONE
        POP     BC
        POP     DE
        PUSH    DE
        PUSH    BC
        LD      A,D
        RLCA
        RLCA
        RLCA
        RLCA
        LD      D,A
        LD      A,B
        RLCA
        RLCA
        RLCA
        RLCA
        LD      C,A
        LD      B,D
        LD      A,2
        CALL    ONE
        POP     BC
        POP     DE
        LD      B,E
        LD      A,3
        CALL    ONE
        LD      HL,(WORK+56)    ; written, inside the screen
        LD      A,(WORK+53)
        LD      C,A
        LD      B,0
        INC     A
        JR      NZ,STR3A
        DEC     B               ; (0FFH: -1)
STR3A:  ADD     HL,BC
        LD      DE,LB
        LD      B,4
STR4:   LD      A,C
        CP      BYTEPL
        JR      NC,STR5         ; (off the right, or the left's 0FFH)
        LD      A,(DE)
        LD      (HL),A
STR5:   INC     HL
        INC     DE
        INC     C
        DJNZ    STR4
        LD      HL,(WORK+56)
        LD      DE,BYTEPL
        ADD     HL,DE
        LD      (WORK+56),HL
        INC     IY
        INC     IY
        LD      HL,WORK+62
        INC     (HL)
        LD      A,(HL)
        CP      TILEH
        JP      NZ,STR1
        RET

; ONE: LB byte A (if 0 <= A < RNC) gets the body nibble B>>4 and the
; visor nibble C>>4, in the seat's patterns (WORK+63, WORK+59).
ONE:    LD      HL,RNC
        CP      (HL)
        RET     NC              ; (a "negative" one is large: out too)
        LD      E,A
        LD      D,0
        LD      HL,LB
        ADD     HL,DE
        PUSH    HL
        LD      A,B             ; the body: EXP16[B >> 4]
        RRCA
        RRCA
        RRCA
        RRCA
        AND     0FH
        LD      E,A
        LD      HL,IEXP16
        ADD     HL,DE
        LD      B,(HL)
        LD      A,C             ; the visor
        RRCA
        RRCA
        RRCA
        RRCA
        AND     0FH
        LD      E,A
        LD      HL,IEXP16
        ADD     HL,DE
        LD      C,(HL)
        LD      A,B
        OR      C
        POP     HL
        RET     Z               ; nothing of it here
        CPL
        AND     (HL)            ; the ground under it kept
        LD      E,A
        LD      A,(WORK+63)
        AND     B
        OR      E
        LD      E,A
        LD      A,(WORK+59)
        AND     C
        OR      E
        LD      (HL),A
        RET


IEXP16: DB      00H,03H,0CH,0FH,30H,33H,3CH,3FH
        DB      0C0H,0C3H,0CCH,0CFH,0F0H,0F3H,0FCH,0FFH
ISEATBP: DB     SB0E,SB0O,SB1E,SB1O,SB2E,SB2O,SB3E,SB3O ; body: even, odd rows
ISEATVP: DB     SV0,SV1,SV2,SV3                         ; visor

; text.inc -- the 4x6 font through the magic expander, numbers, fills.
; Resident. (M.U.L.E.'s clients/astrocade/text.inc, at any line.)
;
; At 2 bits a pixel four pixels are a byte and a glyph is four pixels wide,
; so a glyph row is one byte and nothing shifts. The expander paints it:
; OUT the colour pair to XPAND (fg*4 + bg, colour indexes 0-3), reset its
; nibble flip-flop by writing MAGIC, and the next write through magic
; memory (0000H + the screen offset) paints the HIGH nibble as four
; pixels; a second write without a reset paints the LOW nibble (big text).
; A glyph is 7 lines: the font's 6, then a line of ground, so text
; overwrites a whole cell and never needs the row cleared first.
;
; Positions: D = the line (0-83), E = the byte column (0-39).

MAGEXP  EQU     08H             ; MAGIC: expand, no shift/rotate/or/xor

; TXTAT: the string at HL (NUL-terminated, ASCII; lower case shows as upper)
; at line D, column E, in colour pair C. TXTN: exactly B cells (spaces
; pad). Clobbers all but IX, IY.
TXTAT:  LD      B,0
        JR      TXT0
TXTN:   LD      A,B
        OR      A
        RET     Z
TXT0:   CALL    TXSET
TXT1:   LD      A,(HL)
        OR      A
        JR      Z,TXT3
        INC     HL
        CALL    GLYPHC
        LD      A,B
        OR      A
        JR      Z,TXT1
        DJNZ    TXT1
        RET
TXT3:   LD      A,B
        OR      A
        RET     Z
TXT5:   LD      A,' '
        CALL    GLYPHC
        DJNZ    TXT5
        RET

; CHRAT: character A at line D, column E, colour pair C. Clobbers all.
CHRAT:  PUSH    AF
        CALL    TXSET
        POP     AF
; GLYPHC: character A at TXPOS, which moves a cell right. Preserves BC,
; DE, HL.
GLYPHC: CP      60H
        JR      C,GLC1
        SUB     20H             ; lower case -> upper case
GLC1:   SUB     20H
        CP      40H
        JR      C,GLYPH
        LD      A,'?'-20H
; GLYPH: glyph A (0-3FH) at TXPOS.
GLYPH:  PUSH    BC
        PUSH    DE
        PUSH    HL
        LD      L,A             ; HL = FONT46 + 6A
        LD      H,0
        ADD     HL,HL
        LD      B,H
        LD      C,L
        ADD     HL,HL
        ADD     HL,BC
        LD      BC,FONT46
        ADD     HL,BC
        LD      DE,(TXPOS)
        LD      B,6
GLY1:   LD      A,MAGEXP
        OUT     (MAGIC),A
        LD      A,(HL)
        LD      (DE),A
        INC     HL
        LD      A,E
        ADD     A,BYTEPL
        LD      E,A
        JR      NC,GLY2
        INC     D
GLY2:   DJNZ    GLY1
        LD      A,MAGEXP        ; the line under it: ground
        OUT     (MAGIC),A
        XOR     A
        LD      (DE),A
        LD      HL,(TXPOS)
        INC     HL
        LD      (TXPOS),HL
        POP     HL
        POP     DE
        POP     BC
        RET

; TXSET: TXPOS = the magic address of line D, column E; XPAND = C (kept in
; TXCOL). Preserves BC, DE, HL.
TXSET:  PUSH    HL
        CALL    LINEAD
        LD      (TXPOS),HL
        LD      A,C
        LD      (TXCOL),A
        OUT     (XPAND),A
        POP     HL
        RET

; LINEAD: HL = D * 40 + E, the offset of line D, column E (add NORMEM for the
; screen, nothing for a magic write). Preserves BC, DE.
LINEAD: PUSH    DE
        LD      L,D
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL           ; 8D
        LD      D,H
        LD      A,L
        ADD     HL,HL
        ADD     HL,HL           ; 32D
        ADD     A,L
        LD      L,A
        LD      A,D
        ADC     A,H
        LD      H,A             ; 40D
        POP     DE
        LD      A,L
        ADD     A,E
        LD      L,A
        RET     NC
        INC     H
        RET

; NUMAT: A (0-255) in decimal, right-aligned in B places (1-3, a leading
; zero a space) at line D, column E, colour pair C. NUMN: at TXPOS (the
; colour as set). Clobbers all.
NUMAT:  PUSH    AF
        CALL    TXSET
        POP     AF
NUMN:   LD      (NUMV),A
        LD      C,0             ; digits shown so far
        LD      A,B
        CP      3
        JR      C,NU2
        LD      B,100
        CALL    NUMPL
        LD      A,2
NU2:    CP      2
        JR      C,NU3
        LD      B,10
        CALL    NUMPL
NU3:    LD      A,(NUMV)
        ADD     A,'0'-20H
        JP      GLYPH
NUMPL:  LD      A,(NUMV)
        LD      D,-1
NP1:    INC     D
        SUB     B
        JR      NC,NP1
        ADD     A,B
        LD      (NUMV),A
        LD      A,D
        OR      C
        LD      A,0             ; a leading zero: a space
        JP      Z,GLYPH
        LD      C,1
        LD      A,D
        ADD     A,'0'-20H
        JP      GLYPH

; ---- big text: double width ---------------------------------------------------------
; BIGAT: the string at HL, each character two cells wide, at line D,
; column E, colour pair C. Clobbers all.
BIGAT:  CALL    TXSET
BIG1:   LD      A,(HL)
        OR      A
        RET     Z
        INC     HL
        CP      60H
        JR      C,BGL1
        SUB     20H
BGL1:   SUB     20H
        CP      40H
        JR      C,BGL2
        LD      A,'?'-20H
BGL2:   PUSH    HL
        LD      L,A
        LD      H,0
        ADD     HL,HL
        LD      B,H
        LD      C,L
        ADD     HL,HL
        ADD     HL,BC
        LD      BC,FONT46
        ADD     HL,BC
        LD      DE,(TXPOS)
        LD      B,6
BGL3:   PUSH    HL
        LD      A,(HL)          ; the high nibble, each bit doubled
        RRCA
        RRCA
        RRCA
        RRCA
        AND     0FH
        LD      HL,DBLTAB
        ADD     A,L
        LD      L,A
        JR      NC,BGL4
        INC     H
BGL4:   LD      A,MAGEXP
        OUT     (MAGIC),A
        LD      A,(HL)
        LD      (DE),A          ; the high nibble: the left 4 pixels
        INC     E
        LD      (DE),A          ; the low nibble: the right 4
        DEC     E
        POP     HL
        INC     HL
        LD      A,E
        ADD     A,BYTEPL
        LD      E,A
        JR      NC,BGL5
        INC     D
BGL5:   DJNZ    BGL3
        LD      A,MAGEXP        ; the line under it
        OUT     (MAGIC),A
        XOR     A
        LD      (DE),A
        INC     E
        LD      (DE),A
        LD      HL,(TXPOS)
        INC     HL
        INC     HL
        LD      (TXPOS),HL
        POP     HL
        JR      BIG1

DBLTAB: DB      00H,03H,0CH,0FH,30H,33H,3CH,3FH
        DB      0C0H,0C3H,0CCH,0CFH,0F0H,0F3H,0FCH,0FFH

; ---- fills --------------------------------------------------------------------------------
; FILLB: B bytes from line D, column E, for C lines, all = A (plain writes,
; four pixels a byte). Clobbers all.
FILLB:  PUSH    AF
        CALL    LINEAD
        LD      DE,NORMEM
        ADD     HL,DE
        POP     AF
FIL1:   PUSH    BC
        PUSH    HL
FIL2:   LD      (HL),A
        INC     HL
        DJNZ    FIL2
        POP     HL
        LD      DE,BYTEPL
        ADD     HL,DE
        POP     BC
        DEC     C
        JR      NZ,FIL1
        RET

; CLRLN: C lines from line D, all colour 0. Clobbers all.
CLRLN:  LD      E,0
        LD      B,BYTEPL
        XOR     A
        JR      FILLB

; STRLEN: A = the length of the string at HL. Preserves HL.
STRLEN: PUSH    HL
        LD      B,0
SL1:    LD      A,(HL)
        OR      A
        JR      Z,SL2
        INC     HL
        INC     B
        JR      SL1
SL2:    LD      A,B
        POP     HL
        RET

; CENTRE: the string at HL centred on line D (40 cells), colour pair C.
; Clobbers all.
CENTRE: CALL    STRLEN
        LD      E,A
        LD      A,BYTEPL
        SUB     E
        SRL     A
        LD      E,A
        JP      TXTAT

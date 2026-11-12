; gfx.inc -- the screens (src/adam/gfx.c, transcribed): the TMS9918A in
; Graphics II as a character map, 32 x 24 cells. Fixed.
;
; A cell is a character code; each code has its own 8 pattern bytes and 8
; colour bytes (two colours a line of 8 pixels), loaded alike into all three
; thirds of the screen. Text is the Namco font, one character a cell; a
; style (a colour byte, fg*16 + bg) is a set of SSZ glyphs from FIRSTG (59 from
; the space: no lower case, no punctuation past Z), made in all three thirds
; the first time it is used and stacked down from code 256: slot s starts at
; 256 - (s + 1) * SSZ, so four styles take codes 20-255 and leave 1-19 for
; what a screen builds from code 1 up (NEXTC). Code 0 is the screen's blank
; (CH_PANEL). A screen keeps to four colour bytes.
;
; Registers: text and cell routines take D = row (0-23), E = column (0-31).

NSTY    EQU     4               ; styles a screen can have
GLYPHS  EQU     59              ; a style's glyphs: the space to 'Z' (ASCII 20H-5AH)
R1_OFF  EQU     0A2H            ; 16K, the interrupt, 16 x 16 sprites; display off
R1_ON   EQU     0E2H
SPR_END EQU     0D0H

; NTADR: HL = the name table's address of row D, column E. Preserves BC,DE.
NTADR:  LD      L,D
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL           ; 32 D
        LD      A,E
        ADD     A,L
        LD      L,A
        LD      A,H
        ADC     A,VR_NAME/256
        LD      H,A
        RET

; ---- start-up and screens ---------------------------------------------------------
; GFXINIT: Graphics II, 16K, 16 x 16 sprites, the display off, VRAM cleared,
; the sprite patterns made.
GFXINIT: LD     A,1
        LD      (BLANKED),A
        LD      BC,0002H        ; R0: Graphics II
        CALL    VREG
        LD      BC,0100H+R1_OFF
        CALL    VREG
        LD      BC,0200H+VR_NAME/1024
        CALL    VREG
        LD      BC,03FFH        ; colours at 2000H, each third its own
        CALL    VREG
        LD      BC,0403H        ; patterns at 0000H, each third its own
        CALL    VREG
        LD      BC,0500H+VR_SAT/128
        CALL    VREG
        LD      BC,0600H+VR_SPAT/2048
        CALL    VREG
        LD      HL,0            ; all of it zero
        LD      DE,4000H
        XOR     A
        CALL    VFILL
        LD      HL,A_SPR_B      ; the sprites' patterns: per frame B (black,
        LD      DE,VR_SPAT      ;  in front) then A (the seat's colour)
        LD      B,N_FRAMES
GI1:    PUSH    BC
        PUSH    HL
        PUSH    DE
        LD      BC,32
        CALL    VWRITE
        POP     DE
        POP     HL
        PUSH    HL
        PUSH    DE
        LD      BC,A_SPR_A-A_SPR_B
        ADD     HL,BC
        EX      DE,HL
        LD      BC,32
        ADD     HL,BC
        EX      DE,HL
        LD      BC,32
        CALL    VWRITE
        POP     DE
        POP     HL
        LD      BC,32
        ADD     HL,BC
        EX      DE,HL
        LD      BC,64
        ADD     HL,BC
        EX      DE,HL
        POP     BC
        DJNZ    GI1
        LD      HL,A_SPR_BIGB   ; the cheering frames, doubled: per frame 4 B
        LD      DE,VR_SPAT+N_FRAMES*64          ;  blocks, then 4 A blocks
        LD      B,2
GI2:    PUSH    BC
        PUSH    HL
        PUSH    DE
        LD      BC,128
        CALL    VWRITE
        POP     DE
        POP     HL
        PUSH    HL
        PUSH    DE
        LD      BC,A_SPR_BIGA-A_SPR_BIGB
        ADD     HL,BC
        EX      DE,HL
        LD      BC,128
        ADD     HL,BC
        EX      DE,HL
        LD      BC,128
        CALL    VWRITE
        POP     DE
        POP     HL
        LD      BC,128
        ADD     HL,BC
        EX      DE,HL
        LD      BC,256
        ADD     HL,BC
        EX      DE,HL
        POP     BC
        DJNZ    GI2
        LD      HL,VR_SAT       ; no sprites: the first one ends the list
        LD      DE,1
        LD      A,SPR_END
        CALL    VFILL
        JP      GFXBEGIN

; GFXBEGIN: a new screen: the display off, sprites off, characters afresh on
; blue (full-width styles), the names blank.
GFXBEGIN: CALL  FRWAIT1
        LD      BC,0100H+R1_OFF
        CALL    VREG
        LD      A,1
        LD      (BLANKED),A
        CALL    SPRHIDE
        CALL    SPRFLUSH
        LD      A,SC_BLUE
        CALL    CHARSET
        LD      HL,VR_NAME
        LD      DE,768
        XOR     A
        JP      VFILL

; FRWAIT1: wait for the next frame. Clobbers A,DE,HL.
FRWAIT1: LD     HL,(FRAMES)
FW1:    LD      DE,(FRAMES)
        LD      A,E
        CP      L
        RET     NZ
        HALT
        JR      FW1

; GFXSHOW: the display on, from the next frame.
GFXSHOW: CALL   FRWAIT1
        LD      BC,0100H+R1_ON
        CALL    VREG
        XOR     A
        LD      (BLANKED),A
        RET

; CHARSET: characters start again: A = the screen's blank colour (code 0 a
; blank of it, the border it); full styles (64 glyphs from the space).
; Clobbers A,BC,DE,HL.
CHARSET: LD     (SCRBG),A
        LD      L,A
        RLCA
        RLCA
        RLCA
        RLCA
        OR      L
        LD      (CPC),A         ; the colour byte: bg on bg
        LD      HL,0            ; code 0's patterns (zero) and colours, three times
        LD      B,3
CS1:    PUSH    BC
        PUSH    HL
        LD      DE,8
        XOR     A
        CALL    VFILL
        POP     HL
        PUSH    HL
        LD      DE,VR_COL
        ADD     HL,DE
        LD      DE,8
        LD      A,(CPC)
        CALL    VFILL
        POP     HL
        LD      DE,800H
        ADD     HL,DE
        POP     BC
        DJNZ    CS1
        LD      A,1
        LD      (NEXTC),A
        XOR     A
        LD      (STYN),A
        LD      (FIRSTG),A
        LD      (TROC),A
        LD      A,GLYPHS
        LD      (SSZ),A
        LD      A,(SCRBG)
        LD      C,A
        LD      B,7
        JP      VREG            ; the border

; CELLPUT: character A = the 16 bytes at HL (8 patterns, 8 colours), in all
; three thirds. Clobbers A,BC,DE,HL.
CELLPUT: LD     (CPSRC),HL
        LD      L,A
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL           ; * 8
        LD      (CPADR),HL
        LD      B,3
CP1:    PUSH    BC
        LD      DE,(CPADR)
        LD      HL,(CPSRC)
        LD      BC,8
        CALL    VWRITE
        LD      HL,(CPADR)
        LD      DE,VR_COL
        ADD     HL,DE
        EX      DE,HL
        LD      HL,(CPSRC)
        LD      BC,8
        ADD     HL,BC
        CALL    VWRITE
        LD      HL,(CPADR)
        LD      BC,800H
        ADD     HL,BC
        LD      (CPADR),HL
        POP     BC
        DJNZ    CP1
        RET

; ---- styles -------------------------------------------------------------------------
; STYLE: C = a colour byte (fg*16 + bg): its glyph set found, or made; sets
; TXB (the code of glyph FIRSTG) and TXSP (a space's). Clobbers A,BC,DE,HL.
STYLE:  LD      A,(STYN)
        LD      B,A
        LD      D,0
        LD      HL,STYC
        OR      A
        JR      Z,STYNEW
STYF:   LD      A,(HL)
        CP      C
        JR      Z,STYHAV
        INC     HL
        INC     D
        DJNZ    STYF
STYNEW: LD      A,D             ; = STYN
        CP      NSTY
        JR      C,STYN1
        LD      D,0             ; no room: the first style's glyphs
        JR      STYHAV
STYN1:  LD      (HL),C
        INC     A
        LD      (STYN),A
        DEC     A               ; the slot
        LD      D,A
        PUSH    DE
        CALL    STYUP           ; A = slot, C = the colour byte
        POP     DE
STYHAV: LD      A,D
        LD      E,A
        LD      D,0
        LD      HL,STYC
        ADD     HL,DE
        LD      A,(HL)
        AND     0FH
        LD      C,A             ; its background
        LD      A,E
        INC     A
        LD      B,A
        LD      A,(SSZ)
        LD      D,A
        XOR     A
STYB:   ADD     A,D
        DJNZ    STYB
        NEG                     ; 256 - (slot + 1) * SSZ
        LD      (TXB),A
        LD      E,A
        LD      A,(FIRSTG)
        OR      A
        JR      NZ,STYP
        LD      A,(SCRBG)
        CP      C
        JR      Z,STYP
        LD      A,E             ; a space in a style of its own
        JR      STYS
STYP:   XOR     A               ; the screen's blank
STYS:   LD      (TXSP),A
        RET

; STYUP: slot A, colour byte C: the style's glyphs made in the three thirds.
; Clobbers A,BC,DE,HL.
STYUP:  LD      B,C
        LD      C,A
        LD      A,B
        LD      (CPC),A
        LD      A,C
        INC     A
        LD      B,A
        LD      A,(SSZ)
        LD      C,A
        XOR     A
STU1:   ADD     A,C
        DJNZ    STU1
        NEG
        LD      L,A
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      (CPADR),HL      ; its first pattern's address
        LD      A,(SSZ)
        LD      L,A
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      (CPCNT),HL      ; the bytes in a third
        LD      B,3
STU2:   PUSH    BC
        LD      A,(FIRSTG)
        LD      L,A
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      DE,A_FONT_GFX
        ADD     HL,DE
        LD      DE,(CPADR)
        LD      BC,(CPCNT)
        CALL    VWRITE
        LD      HL,(CPADR)
        LD      DE,VR_COL
        ADD     HL,DE
        LD      DE,(CPCNT)
        LD      A,(CPC)
        CALL    VFILL
        LD      HL,(CPADR)
        LD      DE,800H
        ADD     HL,DE
        LD      (CPADR),HL
        POP     BC
        DJNZ    STU2
        RET

; ---- text ---------------------------------------------------------------------------
; TXTAT: the string at HL (NUL-terminated, ASCII; lower case shows as upper)
; at row D, column E, in colour byte C. TXTN: exactly B cells (spaces pad).
; Clobbers all but IX, IY. A row's end is the caller's to keep to.
TXTAT:  LD      B,0
        JR      TXT0
TXTN:   LD      A,B
        OR      A
        RET     Z
TXT0:   PUSH    HL
        PUSH    BC
        PUSH    DE
        CALL    STYLE           ; (before the VDP is locked: it may make glyphs)
        POP     DE
        POP     BC
        POP     HL
        PUSH    BC
        PUSH    HL
        CALL    NTADR
        CALL    VLOCK
        CALL    VWA
        POP     HL
        POP     BC
TXT1:   LD      A,(HL)
        OR      A
        JR      Z,TXT3
        INC     HL
        CALL    GLYPHC
        LD      A,B
        OR      A
        JR      Z,TXT1
        DJNZ    TXT1
        JP      VFREE
TXT3:   LD      A,B
        OR      A
        JP      Z,VFREE
TXT5:   LD      A,' '
        CALL    GLYPHC
        DJNZ    TXT5
        JP      VFREE

; CHRAT: character A at row D, column E, colour byte C. Clobbers all.
CHRAT:  LD      (CHRBUF),A
        XOR     A
        LD      (CHRBUF+1),A
        LD      HL,CHRBUF
        JP      TXTAT

; GFXLOGO: the logo (3 rows from row D, centred): its cells made from the
; next code up. Clobbers all.
GFXLOGO: LD     A,D
        LD      (CPC),A         ; (the row, kept: CELLPUT doesn't use CPC)
        LD      A,(NEXTC)
        LD      (LOGOC),A
        LD      HL,A_LOGO_CELLS
        LD      B,LOGO_CELLS
GL1:    PUSH    BC
        PUSH    HL
        LD      A,(NEXTC)
        CALL    CELLPUT
        LD      HL,NEXTC
        INC     (HL)
        POP     HL
        LD      DE,16
        ADD     HL,DE
        POP     BC
        DJNZ    GL1
        LD      HL,A_LOGO_MAP
        LD      B,3
GL2:    PUSH    BC
        PUSH    HL
        LD      A,(CPC)
        LD      D,A
        LD      E,(CELLS_W-LOGO_W)/2
        CALL    NTADR
        CALL    VLOCK
        CALL    VWA
        POP     HL
        LD      B,LOGO_W
        LD      A,(LOGOC)
        LD      C,A
GL3:    LD      A,(HL)
        ADD     A,C
        OUT     (VDPD),A
        INC     HL
        DJNZ    GL3
        CALL    VFREE
        LD      A,(CPC)
        INC     A
        LD      (CPC),A
        POP     BC
        DJNZ    GL2
        RET

; GFXTROPH: B (0-15) trophies, two cells wide and two rows high, from row D,
; column E (the cells made the first time). Clobbers all.
GFXTROPH: LD    A,(TROC)
        OR      A
        JR      NZ,GT1
        PUSH    BC
        PUSH    DE
        LD      A,(NEXTC)
        LD      (TROC),A
        LD      HL,A_TROPHY_CELLS
        LD      B,4
GT0:    PUSH    BC
        PUSH    HL
        LD      A,(NEXTC)
        CALL    CELLPUT
        LD      HL,NEXTC
        INC     (HL)
        POP     HL
        LD      DE,16
        ADD     HL,DE
        POP     BC
        DJNZ    GT0
        POP     DE
        POP     BC
GT1:    LD      A,B
        OR      A
        RET     Z
        LD      A,32            ; at most what fits the row
        SUB     E
        SRL     A
        CP      B
        JR      NC,GT1A
        LD      B,A
GT1A:   LD      A,(TROC)
        LD      (CPC),A
        PUSH    BC              ; the rows' two halves: [TROC, TROC+1], [TROC+2, TROC+3]
        PUSH    DE
        CALL    GTROW
        POP     DE
        POP     BC
        INC     D
        LD      A,(TROC)
        ADD     A,2
        LD      (CPC),A
; GTROW: row D from column E: B times the pair at CPC.
GTROW:  CALL    NTADR
        CALL    VLOCK
        CALL    VWA
        LD      A,(CPC)
        LD      C,A
GTR1:   LD      A,C
        OUT     (VDPD),A
        PAD4
        INC     A
        OUT     (VDPD),A
        PAD4
        NOP
        DJNZ    GTR1
        JP      VFREE

; CENTRE: the string at HL (at most 32 cells) centred in row D, colour byte C.
; Clobbers all but IX, IY.
CENTRE: PUSH    HL
        LD      E,0
CEN1:   LD      A,(HL)
        OR      A
        JR      Z,CEN2
        INC     HL
        INC     E
        JR      CEN1
CEN2:   LD      A,32
        SUB     E
        JR      NC,CEN3
        XOR     A
CEN3:   SRL     A
        LD      E,A
        POP     HL
        JP      TXTAT

; GLYCODE: ASCII A -> its character code in the style STYLE set (lower case
; as upper). Preserves BC, DE, HL.
GLYCODE: CP     60H
        JR      C,GLC1
        SUB     20H             ; lower case -> upper case
GLC1:   SUB     20H
        CP      60H
        JR      C,GLC2
        LD      A,'?'-20H
GLC2:   PUSH    BC
        LD      C,A
        OR      A
        JR      Z,GLSP
        LD      A,(FIRSTG)
        LD      B,A
        LD      A,C
        SUB     B
        JR      C,GLPAN         ; below the set
        LD      B,A
        LD      A,(SSZ)
        CP      B
        JR      C,GLPAN
        JR      Z,GLPAN         ; past it
        LD      A,(TXB)
        ADD     A,B
        POP     BC
        RET
GLSP:   LD      A,(TXSP)
        POP     BC
        RET
GLPAN:  XOR     A
        POP     BC
        RET

; GLYPHC: character A (ASCII) out as the next cell, in the style STYLE set.
; Preserves BC, DE, HL. (Inside VLOCK.)
GLYPHC: CALL    GLYCODE
        OUT     (VDPD),A
        RET

; NUMAT: A (0-255) in decimal, right-aligned in B places (1-3, a leading
; zero a space) at row D, column E, colour byte C. Clobbers all.
NUMAT:  PUSH    DE
        PUSH    BC
        LD      HL,NUMBUF
        LD      B,100
        CALL    NMDIG
        LD      B,10
        CALL    NMDIG
        ADD     A,'0'
        LD      (HL),A
        INC     HL
        LD      (HL),0
        LD      HL,NUMBUF
        LD      A,(HL)
        CP      '0'
        JR      NZ,NU2
        LD      (HL),' '
        INC     HL
        LD      A,(HL)
        CP      '0'
        JR      NZ,NU2
        LD      (HL),' '
NU2:    POP     BC
        POP     DE
        LD      HL,NUMBUF
        LD      A,3
        SUB     B
        ADD     A,L
        LD      L,A
        JP      TXTN
; NMDIG: A / B into (HL)++ as a digit; A = the remainder. Clobbers D, E.
NMDIG:  LD      E,'0'
NMD1:   CP      B
        JR      C,NMD2
        SUB     B
        INC     E
        JR      NMD1
NMD2:   LD      (HL),E
        INC     HL
        RET

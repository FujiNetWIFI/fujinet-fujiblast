; p_draw.inc -- the compositor: the arena's tiles and the four bombers,
; flicker-free and in their own colours.
;
; There are no sprites: a bomber is drawn into the bitmap. M.U.L.E.'s
; Astrocade client XORs its movers on and off, but a bomber standing on
; its own bomb would mix the two's colours, so this one composes: a
; region (at most 4 bytes by 14 lines) is built a line at a time -- each
; byte the tile under it, then every bomber over it masked in (AND NOT
; its mask, OR its picture) -- and written once, top to bottom, so the
; beam never sees a half-drawn bomber (the 2026-10-02 flicker rule: one
; pass a line).
;
; A move redraws the union of the bomber's old and new places (or the two
; apart when it jumped); a tile that changed is redrawn with whatever
; stands on it; the animated tiles (flames, fuses, burning blocks, the
; powerups' edges) turn a row a frame (ROWPH), so each flickers at about
; 5 Hz and no frame pays for them all.
;
; Bombers come pre-shifted as 2-bit roles (mkgfx_astro.py ROLEART: body,
; visor, nothing), three bytes a row; MASKT and BODYT turn a role
; byte into its mask and its body's pixels (its visor's: the rest of the
; mask), which the seat's patterns colour (look.inc's: the fourth seat's body is striped, a
; pattern for even rows and one for odd).

        JP      DRALL           ; E_ENTER: the whole arena drawn, the bombers as wanted
        JP      DRDIRTY         ; E_STATE: the tiles the decoder marked
        JP      DRFRAME         ; E_TICK: a frame's drawing
        JP      DRTILE          ; E_EXTRA: tile A drawn now

; WORK, while drawing (ram.inc)
RC0     EQU     WORK            ; the region: its first byte column (1-26)
RNC     EQU     WORK+1          ;  columns (1-4)
RY0     EQU     WORK+2          ;  its first arena line (0-76)
RNY     EQU     WORK+3          ;  lines (1-14)
TR0     EQU     WORK+4          ; its first tile row
OVL     EQU     WORK+5          ; the seats over it, a bit each
YC      EQU     WORK+6          ; the line being built
DCNT    EQU     WORK+7          ; the dirty tiles still allowed this frame
LB      EQU     WORK+10         ; 4: the line's bytes (WORK+8, 9 and 14, 15: spare,
                                ;  so a bomber's bytes past the region land there)
SCRP    EQU     WORK+16         ; 2: the screen address of the line being built
                                ; WORK+19-23: PTSET's last tile, its picture, its row
                                ;  (13 r), its frame (FASTT's too)
PT      EQU     WORK+24         ; 24: 3 tile rows x 4 columns: each byte's tile, line 0
SR      EQU     WORK+48         ; 32: each seat's record, built a frame (PREP):
SRSZ    EQU     8               ;  +0 its first line, +1 its first byte column,
                                ;  +2/+3 the body's pattern ^ the visor's (even,
                                ;  odd rows), +4 the visor's, +5 its shift, +6/+7 its picture
                                ;  (ROLEART: its row 0's three bytes of roles)
DTMAX   EQU     6               ; dirty tiles drawn a frame, at most

; ---- a frame ------------------------------------------------------------------------
; DRFRAME: one row's animation, the dirty tiles (a few), the bombers.
DRFRAME: CALL   PREP
        CALL    ANSTEP
        CALL    DIRTY
        JP      DRBOMBS

; DRDIRTY: the dirty tiles, a few.
DRDIRTY: CALL   PREP
        JP      DIRTY

; DRALL: the whole arena: every tile, the bombers as wanted.
DRALL:  XOR     A
        LD      (DRVIS),A
        LD      HL,MAPD         ; nothing left dirty
        LD      B,18
DA0:    LD      (HL),A
        INC     HL
        DJNZ    DA0
        CALL    PREP
        XOR     A
DA1:    PUSH    AF
        CALL    DRTILE
        POP     AF
        INC     A
        CP      TILES
        JR      NZ,DA1
        JP      DRBOMBS

; PREP: each seat's record (SR), from BX, BY, BF and its patterns.
PREP:   LD      IX,SR
        LD      C,0
PR1:    LD      B,0
        LD      HL,BY
        ADD     HL,BC
        LD      A,(HL)
        LD      (IX+0),A
        LD      HL,BX
        ADD     HL,BC
        LD      A,(HL)
        LD      E,A             ; x
        AND     3
        LD      (IX+5),A
        LD      A,E
        RRCA
        RRCA
        AND     3FH
        INC     A
        LD      (IX+1),A
        LD      HL,SEATVP       ; the patterns: the visor's, the body's ^ it
        ADD     HL,BC
        LD      D,(HL)
        LD      (IX+4),D
        LD      HL,SEATBP
        ADD     HL,BC
        ADD     HL,BC
        LD      A,(HL)
        XOR     D
        LD      (IX+2),A
        INC     HL
        LD      A,(HL)
        XOR     D
        LD      (IX+3),A
        LD      HL,BF           ; its picture: ROLEART + (FRMAP[f] * 4 + shift) * 21
        ADD     HL,BC
        LD      L,(HL)
        LD      H,0
        LD      DE,FRMAP
        ADD     HL,DE
        LD      A,(HL)
        ADD     A,A
        ADD     A,A
        ADD     A,(IX+5)
        LD      L,A
        LD      H,0
        LD      D,H
        LD      E,L
        ADD     HL,HL
        ADD     HL,HL           ; 4n
        ADD     HL,DE           ; 5n
        ADD     HL,HL
        ADD     HL,HL           ; 20n
        ADD     HL,DE           ; 21n
        LD      DE,ROLEART
        ADD     HL,DE
        LD      (IX+6),L
        LD      (IX+7),H
        LD      DE,SRSZ
        ADD     IX,DE
        INC     C
        LD      A,C
        CP      4
        JR      NZ,PR1
        LD      A,DTMAX
        LD      (DCNT),A
        RET

; ---- the tiles ------------------------------------------------------------------------
; DRTILE: tile A (0-142) as shown (EFF), with whatever stands on it.
DRTILE: CALL    TROW            ; row C, column A
DT2:    ADD     A,A
        INC     A
        LD      (RC0),A
        LD      A,2
        LD      (RNC),A
        LD      A,C             ; 7 x row
        ADD     A,A
        ADD     A,A
        ADD     A,A
        SUB     C
        LD      (RY0),A
        LD      A,TILEH
        LD      (RNY),A
        CALL    OVLAP           ; a bomber on it: the compositor
        OR      A
        JP      NZ,DRAWREG2
; FASTT: no bomber over it: its picture copied straight (14 bytes).
FASTT:  LD      A,(RY0)         ; its row (RY0 / 7) and first tile
        LD      L,A
        LD      H,0
        LD      DE,YROWT
        ADD     HL,DE
        LD      A,(HL)
        LD      C,A
        ADD     A,A
        ADD     A,C
        ADD     A,A
        ADD     A,A
        ADD     A,C             ; 13 row
        LD      B,A
        LD      A,(RC0)
        DEC     A
        SRL     A               ; its column
        ADD     A,B
        PUSH    AF              ; (the tile)
        LD      A,C
        CALL    ROWBIT          ; its row's frame (ROWBIT clobbers B)
        AND     (HL)
        LD      (WORK+22),A
        POP     AF
        CALL    EFF
        ADD     A,A             ; TILEPTR + 2 (code*2 + frame)
        LD      E,A
        LD      A,(WORK+22)
        OR      A
        JR      Z,FT1
        INC     E
FT1:    LD      D,0
        LD      HL,TILEPTR
        ADD     HL,DE
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        PUSH    DE
        LD      A,(RY0)         ; the screen: line ATOP + RY0, byte RC0
        CALL    SCRAD
FT2:    POP     HL
        LD      B,TILEH         ; 7 lines of 2 bytes
FT3:    LD      A,(HL)
        LD      (DE),A
        INC     HL
        INC     DE
        LD      A,(HL)
        LD      (DE),A
        INC     HL
        LD      A,E
        ADD     A,BYTEPL-1
        LD      E,A
        JR      NC,FT4
        INC     D
FT4:    DJNZ    FT3
        RET

; EFF: A = the code tile A shows: the map's, but our bomb not yet in it
; (PEND) and the movers (MOVT) are bombs (or duds, MOVD). Clobbers BC, HL.
EFF:    LD      HL,PEND
        CP      (HL)
        JR      Z,EF2
        LD      HL,MOVT
        LD      B,4
EF1:    CP      (HL)
        JR      Z,EF3
        INC     HL
        DJNZ    EF1
        LD      HL,ST+OFF_MAP
        LD      C,A
        LD      B,0
        ADD     HL,BC
        LD      A,(HL)
        RET
EF2:    LD      A,T_BOMB
        RET
EF3:    LD      A,5             ; mover 4 - B: a dud? (its bit to CY: 5 - B turns)
        SUB     B
        LD      B,A
        LD      A,(MOVD)
EF4:    RRCA
        DJNZ    EF4
        LD      A,T_BOMB
        RET     NC
        LD      A,T_DUD
        RET

; ANSTEP: the next tile row's animation frame turns, and its animated
; tiles are drawn so.
ANSTEP: LD      A,(AROW)
        INC     A
        CP      ARENA_H
        JR      C,AN1
        XOR     A
AN1:    LD      (AROW),A
        CALL    ROWBIT          ; HL = ROWPH's byte, A = the bit
        XOR     (HL)
        LD      (HL),A
        LD      A,(AROW)        ; its first tile
        LD      C,A
        ADD     A,A
        ADD     A,C
        ADD     A,A
        ADD     A,A
        ADD     A,C             ; 13 x row
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_MAP
        ADD     HL,DE
        LD      B,ARENA_W
AN2:    LD      A,(HL)          ; the map's: floor, pillar, block are still
        CP      T_BURN
        JR      C,AN3
        PUSH    BC
        PUSH    DE
        PUSH    HL
        LD      A,E
        CALL    DRTILE
        POP     HL
        POP     DE
        POP     BC
AN3:    INC     HL
        INC     E
        DJNZ    AN2
        LD      A,(AROW)        ; and our bomb, the movers, if in this row
        LD      C,A
        LD      HL,PEND
        CALL    ANOV
        LD      HL,MOVT
        LD      B,4
AN4:    PUSH    BC
        PUSH    HL
        CALL    ANOV
        POP     HL
        POP     BC
        INC     HL
        DJNZ    AN4
        RET
; ANOV: the tile at (HL), if it isn't NONE and is in row C, drawn.
ANOV:   LD      A,(HL)
        CP      NONE
        RET     Z
        LD      E,A
        LD      B,C
        CALL    TROW
        LD      A,C
        CP      B
        LD      C,B
        RET     NZ
        LD      A,E
        PUSH    BC
        CALL    DRTILE
        POP     BC
        RET

; TROW: tile A's row (C) and column (A). Preserves B, DE, HL.
TROW:   LD      C,0
TRW1:   CP      ARENA_W
        RET     C
        SUB     ARENA_W
        INC     C
        JR      TRW1

; ROWBIT: HL = the ROWPH byte of row A, A = its bit. Clobbers B.
ROWBIT: LD      HL,ROWPH
        CP      8
        JR      C,RB1
        INC     HL
        SUB     8
RB1:    LD      B,A
        INC     B
        LD      A,80H
RB2:    RLCA
        DJNZ    RB2
        RET

; DIRTY: the tiles the decoder marked (MAPD), DCNT of them at most; the
; rest stay marked for the next frame.
DIRTY:  LD      HL,MAPD
        LD      C,0             ; the tile of bit 0 of (HL)
DI1:    LD      A,(HL)
        OR      A
        JR      NZ,DI3
DI2:    INC     HL
        LD      A,C
        ADD     A,8
        LD      C,A
        CP      TILES
        JR      C,DI1
        RET
DI3:    LD      B,C             ; the lowest bit set: its tile
        LD      E,1
DI4:    RRCA
        JR      C,DI5
        INC     B
        SLA     E
        JR      DI4
DI5:    LD      A,E             ; cleared
        CPL
        AND     (HL)
        LD      (HL),A
        PUSH    HL
        PUSH    BC
        LD      A,B
        CALL    DRTILE
        POP     BC
        POP     HL
        LD      A,(DCNT)
        DEC     A
        LD      (DCNT),A
        RET     Z
        JR      DI1

; ---- the bombers ------------------------------------------------------------------------
; DRBOMBS: each seat that moved, changed its picture, came or went: the
; union of its old and new places (both apart if it jumped).
DRBOMBS: LD     C,0
DB1:    PUSH    BC
        CALL    DRSEAT
        POP     BC
        INC     C
        LD      A,C
        CP      4
        JR      NZ,DB1
        RET

; DRSEAT: seat C.
DRSEAT: LD      B,0
        LD      A,C
        CALL    SBIT            ; E = its bit
        LD      A,(BVIS)
        AND     E
        LD      D,A             ; D: wanted
        LD      A,(DRVIS)
        AND     E
        PUSH    AF              ; drawn
        OR      D
        JR      NZ,DS1
        POP     AF
        RET                     ; neither
DS1:    POP     AF
        JP      Z,DSNEW         ; not drawn: only its new place
        LD      A,D
        OR      A
        JP      Z,DSOLD         ; gone: only its old place
        LD      HL,BX           ; both: unchanged?
        ADD     HL,BC
        LD      A,(HL)
        LD      HL,DRX
        ADD     HL,BC
        CP      (HL)
        JR      NZ,DS2
        LD      HL,BY
        ADD     HL,BC
        LD      A,(HL)
        LD      HL,DRY
        ADD     HL,BC
        CP      (HL)
        JR      NZ,DS2
        LD      HL,BF
        ADD     HL,BC
        LD      A,(HL)
        LD      HL,DRF
        ADD     HL,BC
        CP      (HL)
        RET     Z
DS2:    PUSH    BC              ; the union, if it fits a region
        LD      HL,BX
        ADD     HL,BC
        LD      D,(HL)
        LD      HL,DRX
        ADD     HL,BC
        LD      E,(HL)
        LD      HL,BY
        ADD     HL,BC
        LD      B,(HL)
        LD      HL,DRY-0        ; (C still the seat)
        LD      A,L
        ADD     A,C
        LD      L,A
        JR      NC,DS3
        INC     H
DS3:    LD      C,(HL)          ; D, E: new x, old x; B, C: new y, old y
        LD      A,D             ; columns: min x >> 2 .. max x >> 2, then 3
        CP      E
        JR      C,DS4
        LD      A,E
        LD      E,D
        LD      D,A             ; D = min, E = max
DS4:    LD      A,E
        SRL     A
        SRL     A
        LD      E,A
        LD      A,D
        SRL     A
        SRL     A
        LD      D,A
        LD      A,E
        SUB     D
        CP      2               ; 4 columns at most
        JR      NC,DSAPART
        ADD     A,3
        LD      (RNC),A
        LD      A,D
        INC     A
        LD      (RC0),A
        LD      A,B             ; lines: min y .. max y + 6
        CP      C
        JR      C,DS5
        LD      A,C
        LD      C,B
        LD      B,A             ; B = min, C = max
DS5:    LD      A,C
        SUB     B
        CP      8               ; 14 lines at most
        JR      NC,DSAPART
        ADD     A,7
        LD      (RNY),A
        LD      A,B
        LD      (RY0),A
        CALL    CLIPREG
        CALL    DRAWREG
        POP     BC
        JR      DSDONE
DSAPART: POP    BC
        PUSH    BC
        CALL    OLDREG
        CALL    DRAWREG
        POP     BC
DSNEW:  PUSH    BC
        CALL    NEWREG
        CALL    DRAWREG
        POP     BC
        JR      DSDONE
DSOLD:  PUSH    BC
        CALL    OLDREG
        CALL    DRAWREG
        POP     BC
; DSDONE: seat C as drawn = as wanted
DSDONE: LD      B,0
        LD      HL,BX
        ADD     HL,BC
        LD      A,(HL)
        LD      HL,DRX
        ADD     HL,BC
        LD      (HL),A
        LD      HL,BY
        ADD     HL,BC
        LD      A,(HL)
        LD      HL,DRY
        ADD     HL,BC
        LD      (HL),A
        LD      HL,BF
        ADD     HL,BC
        LD      A,(HL)
        LD      HL,DRF
        ADD     HL,BC
        LD      (HL),A
        LD      A,C
        CALL    SBIT
        LD      A,(DRVIS)
        OR      E
        LD      D,A
        LD      A,(BVIS)
        AND     E
        JR      NZ,DSD1
        LD      A,E             ; gone: its bit clear
        CPL
        AND     D
        LD      D,A
DSD1:   LD      A,D
        LD      (DRVIS),A
        RET

; SBIT: E = seat A's bit. Clobbers A.
SBIT:   LD      E,1
        OR      A
        RET     Z
SB1:    SLA     E
        DEC     A
        JR      NZ,SB1
        RET

; NEWREG / OLDREG: the region of seat C as wanted / as drawn (B = 0).
NEWREG: LD      HL,BX
        ADD     HL,BC
        LD      D,(HL)
        LD      HL,BY
        JR      RG1
OLDREG: LD      B,0
        LD      HL,DRX
        ADD     HL,BC
        LD      D,(HL)
        LD      HL,DRY
RG1:    ADD     HL,BC
        LD      A,(HL)
        LD      (RY0),A
        LD      A,TILEH
        LD      (RNY),A
        LD      A,D
        SRL     A
        SRL     A
        INC     A
        LD      (RC0),A
        LD      A,3
        LD      (RNC),A
; CLIPREG: the region kept inside the arena (columns 1-26, lines 0-76).
CLIPREG: LD     A,(RC0)
        LD      B,A
        LD      A,(RNC)
        ADD     A,B             ; one past its last column
        CP      2*ARENA_W+2
        RET     C
        LD      A,2*ARENA_W+1
        SUB     B
        LD      (RNC),A
        RET

; ---- a region ------------------------------------------------------------------------------
; DRAWREG: the region RC0/RNC, RY0/RNY built a line at a time and written.
; DRAWREG2: the same, OVL known.
DRAWREG: CALL   OVLAP
DRAWREG2: LD    A,(RY0)
        LD      L,A
        LD      H,0
        LD      DE,YROWT
        ADD     HL,DE
        LD      A,(HL)
        LD      (TR0),A
        CALL    PTSET
        LD      A,(RY0)
        LD      (YC),A
        CALL    SCRAD           ; its first line's address
        LD      (SCRP),DE
        LD      A,(RNY)
        LD      B,A
DR1:    PUSH    BC
        CALL    LINE
        LD      HL,YC
        INC     (HL)
        POP     BC
        DJNZ    DR1
        RET

; OVLAP: OVL = the seats wanted that overlap the region's lines (columns are
; checked per byte).
OVLAP:  LD      A,(BVIS)
        LD      E,A
        XOR     A
        LD      D,A             ; the result
        LD      C,0
OV1:    SRL     E
        JR      NC,OV3
        LD      B,0
        LD      HL,BY
        ADD     HL,BC
        LD      A,(RY0)         ; its lines y .. y+6 against RY0 .. RY0+RNY-1
        LD      B,A
        LD      A,(HL)
        ADD     A,TILEH-1
        CP      B
        JR      C,OV3           ; it ends above
        LD      A,(RNY)
        ADD     A,B
        DEC     A               ; the region's last line
        CP      (HL)
        JR      C,OV3           ; it starts below
        LD      HL,BX       ; its columns BC0 .. BC0+2 against RC0 .. RC0+RNC-1
        LD      B,0
        ADD     HL,BC
        LD      A,(HL)
        RRCA
        RRCA
        AND     3FH
        INC     A               ; BC0
        LD      B,A
        ADD     A,2             ; BC0 + 2 < RC0: to the left
        LD      HL,RC0
        CP      (HL)
        JR      C,OV3
        LD      A,(RNC)
        ADD     A,(HL)          ; one past the region
        CP      B
        JR      C,OV3           ; BC0 past it
        JR      Z,OV3
        LD      A,C
        PUSH    DE
        CALL    SBIT
        LD      A,E
        POP     DE
        OR      D
        LD      D,A
OV3:    INC     C
        LD      A,C
        CP      4
        JR      NZ,OV1
        LD      A,D
        LD      (OVL),A         ; (and in A)
        RET

; PTSET: PT = for the region's tile rows (up to 3) and columns, the address
; of the byte's tile picture at its line 0 (its frame, its half).
PTSET:  LD      IX,PT
        LD      A,0FFH          ; (no tile looked up yet)
        LD      (WORK+19),A
        LD      A,(TR0)
        LD      B,3
PS1:    PUSH    BC
        PUSH    AF
        CP      ARENA_H
        JR      NC,PS4          ; past the last row
        LD      C,A             ; the row's first tile
        ADD     A,A
        ADD     A,C
        ADD     A,A
        ADD     A,A
        ADD     A,C
        LD      (WORK+22),A
        LD      A,C
        CALL    ROWBIT          ; this row's frame
        AND     (HL)
        LD      (WORK+23),A     ; 0 or not
        LD      A,(RNC)
        LD      B,A
        LD      A,(RC0)
        DEC     A               ; the byte's column from 0
PS2:    PUSH    BC
        PUSH    AF
        LD      C,A
        SRL     A               ; its tile's column
        LD      HL,WORK+22
        ADD     A,(HL)
        LD      HL,WORK+19      ; the tile the last byte had: its picture again
        CP      (HL)
        JR      NZ,PS2A
        LD      DE,(WORK+20)
        JR      PS3A
PS2A:   LD      (HL),A
        CALL    EFF             ; its code
        ADD     A,A             ; (code*2 + frame) words
        LD      E,A
        LD      A,(WORK+23)
        OR      A
        JR      Z,PS3
        INC     E
PS3:    LD      D,0
        LD      HL,TILEPTR
        ADD     HL,DE
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        LD      (WORK+20),DE
PS3A:   POP     AF              ; the half
        PUSH    AF
        AND     1
        ADD     A,E
        LD      (IX+0),A
        LD      A,D
        ADC     A,0
        LD      (IX+1),A
        INC     IX
        INC     IX
        POP     AF
        INC     A
        POP     BC
        DJNZ    PS2
PS4:    POP     AF
        POP     BC
        LD      DE,PT           ; the next row's: PT + 8 x row
        LD      A,3
        SUB     B
        INC     A
        ADD     A,A
        ADD     A,A
        ADD     A,A
        ADD     A,E
        LD      E,A
        PUSH    DE
        POP     IX
        LD      A,(TR0)
        ADD     A,4
        SUB     B
        DJNZ    PS1
        RET

; LINE: line YC of the region: its tiles' bytes, the bombers over them,
; written.
LINE:   LD      A,(YC)
        LD      L,A
        LD      H,0
        LD      DE,YROWT
        ADD     HL,DE
        LD      A,(TR0)
        LD      B,A
        LD      A,(HL)          ; its tile row in the region: PT + 8 ri
        SUB     B
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,PT
        ADD     HL,DE
        PUSH    HL
        LD      A,(YC)          ; its line in the tile, x 2
        LD      L,A
        LD      H,0
        LD      DE,YTL2T
        ADD     HL,DE
        LD      C,(HL)
        LD      B,0
        POP     IX              ; IX: this row's PT
        LD      HL,LB
        LD      A,(RNC)
LN1:    LD      E,(IX+0)        ; the tile byte: (PT) + 2 tl
        LD      D,(IX+1)
        EX      DE,HL
        ADD     HL,BC
        LDI                     ; (BC counts down: put it back)
        INC     BC
        EX      DE,HL
        INC     IX
        INC     IX
        DEC     A
        JR      NZ,LN1
        LD      A,(OVL)         ; the bombers
        OR      A
        CALL    NZ,BOMBRS
        LD      DE,(SCRP)       ; written, and the next line's address
        LD      HL,BYTEPL
        ADD     HL,DE
        LD      (SCRP),HL
LN2:    LD      HL,LB
        LD      A,(RNC)
        LD      C,A
        LD      B,0
        LDIR
        RET

; BOMBRS: the seats in OVL (A) over this line, onto LB.
BOMBRS: LD      IX,SR
        LD      C,A
BR1:    SRL     C
        JR      NC,BR2
        PUSH    BC
        CALL    BOMB1
        POP     BC
BR2:    LD      DE,SRSZ
        ADD     IX,DE
        LD      A,C
        OR      A
        JR      NZ,BR1
        RET

; BOMB1: the seat whose record IX is: its row on this line (if it has one),
; three bytes of roles (pre-shifted), onto LB. A byte g under role byte r
; becomes g ^ ((g ^ X) & MASKT[r]), X = vp ^ (BODYT[r] & K): the ground
; kept outside the mask, the body in its pattern (K = bp ^ vp), the rest
; of the mask (the visor) in vp. LB has two spare bytes each side, so a
; byte past the region needs no test.
BOMB1:  LD      A,(YC)
        SUB     (IX+0)
        CP      TILEH
        RET     NC              ; not on this line
        LD      E,A             ; its row
        AND     1               ; K: even or odd row
        LD      A,(IX+2)
        JR      Z,BM0
        LD      A,(IX+3)
BM0:    LD      C,A             ; C: K
        LD      B,(IX+4)        ; B: vp
        LD      A,E             ; DE: its row's roles, the picture + 3 row
        ADD     A,A
        ADD     A,E
        ADD     A,(IX+6)
        LD      E,A
        LD      A,(IX+7)
        ADC     A,0
        LD      D,A
        LD      A,(RC0)         ; HL: its first byte's place in LB
        LD      L,A
        LD      A,(IX+1)
        SUB     L
        LD      L,A
        ADD     A,A             ; (sign-extended)
        SBC     A,A
        LD      H,A
        PUSH    DE
        LD      DE,LB
        ADD     HL,DE
        POP     DE
        CALL    ONEB
        CALL    ONEB
; ONEB: role byte (DE) onto LB byte (HL); both step on.
ONEB:   LD      A,(DE)
        INC     DE
        OR      A
        JR      Z,ONB9          ; nothing of it here
        PUSH    DE
        LD      E,A
        LD      D,MASKP+1       ; BODYT[r]
        LD      A,(DE)
        AND     C
        XOR     B               ; X
        XOR     (HL)            ; ^ g
        DEC     D               ; & MASKT[r]
        EX      DE,HL
        AND     (HL)
        EX      DE,HL
        XOR     (HL)            ; ^ g
        LD      (HL),A
        POP     DE
ONB9:   INC     HL
        RET

; SCRAD: DE = the screen address of arena line A, byte RC0. Clobbers A, HL.
SCRAD:  ADD     A,ATOP
        LD      D,A
        LD      A,(RC0)
        LD      E,A
        CALL    LINEAD
        LD      DE,SCR
        ADD     HL,DE
        EX      DE,HL
        RET

; ---- tables ---------------------------------------------------------------------------------
SEATBP: DB      SB0E,SB0O,SB1E,SB1O,SB2E,SB2O,SB3E,SB3O ; body: even, odd rows
SEATVP: DB      SV0,SV1,SV2,SV3                         ; visor
; an arena line's tile row, its line in the tile x 2, its screen address
YROWT:
        DB      0,0,0,0,0,0,0,1,1,1,1,1,1,1
        DB      2,2,2,2,2,2,2,3,3,3,3,3,3,3
        DB      4,4,4,4,4,4,4,5,5,5,5,5,5,5
        DB      6,6,6,6,6,6,6,7,7,7,7,7,7,7
        DB      8,8,8,8,8,8,8,9,9,9,9,9,9,9
        DB      10,10,10,10,10,10,10
YTL2T:
        DB      0,2,4,6,8,10,12,0,2,4,6,8,10,12
        DB      0,2,4,6,8,10,12,0,2,4,6,8,10,12
        DB      0,2,4,6,8,10,12,0,2,4,6,8,10,12
        DB      0,2,4,6,8,10,12,0,2,4,6,8,10,12
        DB      0,2,4,6,8,10,12,0,2,4,6,8,10,12
        DB      0,2,4,6,8,10,12

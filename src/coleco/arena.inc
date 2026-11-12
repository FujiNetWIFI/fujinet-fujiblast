; arena.inc -- the arena's screen (src/adam/gfx.c gfx_arena, gfx_tile,
; gfx_bomb_at, gfx_stat, gfx_round, gfx_big_line, anim_step, transcribed).
; Fixed.
;
; Row 0 the HUD (20 big characters in 5 groups of 4, a cell apart, from
; column 4); rows 1-22 the arena: 13 x 11 tiles of 2 x 2 cells from column
; 5, in walls (column 4 and 31, row 23); columns 0-3 the player's stats and
; the round. A tile's 4 cells are characters fixed for the screen
; (tile_chars); the tiles that animate have their characters redrawn in
; the blank, one a frame (ANIMSTEP).
;
; The glyph sets here are 46 glyphs from '-': the digits, the letters and
; a dash, in four colours on black (the seats' own), so the 69 cells and
; four sets fit in the 256 codes.

ASSZ    EQU     46
AFIRSTG EQU     13              ; '-' - ' '
SEATBG  EQU     SC_BLACK
; the four seats' text colour bytes on black
AC_WHT  EQU     15*16+SC_BLACK
AC_RED  EQU     8*16+SC_BLACK
AC_CYAN EQU     7*16+SC_BLACK
AC_YEL  EQU     11*16+SC_BLACK

; GFXARENA: the arena screen's characters and walls (the display is off:
; GFXBEGIN's). Clobbers all.
GFXARENA: LD    A,SC_BLACK
        CALL    CHARSET
        LD      A,ASSZ
        LD      (SSZ),A
        LD      A,AFIRSTG
        LD      (FIRSTG),A
        LD      HL,A_ARENA_CELLS
        XOR     A
        LD      (WORK),A        ; the code
GA1:    LD      A,(WORK)
        PUSH    HL
        CALL    CELLPUT
        POP     HL
        LD      DE,16
        ADD     HL,DE
        LD      A,(WORK)
        INC     A
        LD      (WORK),A
        CP      ARENA_CELLS
        JR      NZ,GA1
        LD      A,ARENA_CELLS
        LD      (NEXTC),A
        LD      HL,ANPH         ; no animated tile drawn yet in any phase
        LD      B,N_ANIM
GA2:    LD      (HL),0
        INC     HL
        DJNZ    GA2
        XOR     A
        LD      (ANNEXT),A
        LD      D,1             ; rows 1-22: the walls and the floor
GA3:    PUSH    DE
        LD      E,4
        CALL    NTADR
        LD      A,CH_WALL
        LD      DE,1
        CALL    VFILL
        POP     DE
        PUSH    DE
        LD      E,31
        CALL    NTADR
        LD      A,CH_WALL
        LD      DE,1
        CALL    VFILL
        POP     DE
        PUSH    DE
        LD      E,5
        CALL    NTADR
        LD      A,CH_FLOOR
        LD      DE,26
        CALL    VFILL
        POP     DE
        INC     D
        LD      A,D
        CP      23
        JR      NZ,GA3
        LD      DE,23*256+4     ; row 23: wall
        CALL    NTADR
        LD      A,CH_WALL
        LD      DE,28
        JP      VFILL

; PUT4: the 4 cells at HL as a 2 x 2 block at row D, column E. Clobbers all.
PUT4:   PUSH    HL
        CALL    NTADR
        EX      DE,HL
        POP     HL
        PUSH    HL
        PUSH    DE
        LD      BC,2
        CALL    VWRITE
        POP     HL
        LD      DE,32
        ADD     HL,DE
        EX      DE,HL
        POP     HL
        INC     HL
        INC     HL
        LD      BC,2
        JP      VWRITE

; TILECH: HL = tile A's 4 characters. Clobbers A, DE.
TILECH: LD      L,A
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        LD      DE,A_TILE_CHARS
        ADD     HL,DE
        RET

; GFXTILE: tile A at column B, row C of the arena. Clobbers all.
GFXTILE: PUSH   BC
        CALL    TILECH
        POP     BC
        LD      A,C             ; row 1 + 2 r, column 5 + 2 c
        ADD     A,A
        INC     A
        LD      D,A
        LD      A,B
        ADD     A,A
        ADD     A,ARENA_COL
        LD      E,A
        JP      PUT4

; GFXBOMB: a bomb (a dud if A) in the air at half-tile cell B, C. Clobbers all.
GFXBOMB: OR     A
        PUSH    AF              ; (Z: a bomb)
        LD      D,C
        INC     D
        LD      A,B
        ADD     A,ARENA_COL
        LD      E,A
        POP     AF
        LD      A,T_BOMB
        JR      Z,GB1
        LD      A,T_DUD
GB1:    PUSH    DE
        CALL    TILECH
        POP     DE
        JP      PUT4

; GLYSTY: character A (ASCII) in colour byte C: its code in A. Clobbers B.
GLYSTY: PUSH    HL
        PUSH    DE
        PUSH    AF
        CALL    STYLE
        POP     AF
        CALL    GLYCODE
        POP     DE
        POP     HL
        RET

; GFXSTAT: our power A (0 range, 1 bombs, 2 speed: a figure beside its
; icon; 3 kick, 4 glove: the icon, or nothing if B is 0) at value B on the
; left. Clobbers all.
GFXSTAT: LD     (T0),A
        LD      A,B
        LD      (T1),A
        LD      A,(T0)          ; row 3 + 4 i
        ADD     A,A
        ADD     A,A
        ADD     A,3
        LD      (T2),A
        LD      A,(T0)
        CP      3
        JR      C,GS2
        LD      A,(T1)
        OR      A
        JR      NZ,GS2
        LD      HL,ZERO4        ; had none: the 4 cells and the figure's two blank
        LD      A,(T2)
        LD      D,A
        LD      E,0
        CALL    PUT4
        LD      A,(T2)
        INC     A
        LD      D,A
        LD      E,2
        LD      HL,ZERO4
        LD      BC,2
        CALL    NTADR
        EX      DE,HL
        LD      HL,ZERO4
        LD      BC,2
        JP      VWRITE
GS2:    LD      A,(T0)          ; its icon
        ADD     A,T_FIRE
        CALL    TILECH
        LD      A,(T2)
        LD      D,A
        LD      E,0
        CALL    PUT4
        LD      A,(T0)
        CP      3
        RET     NC
        LD      A,(T1)          ; the figure: two cells under the icon's right
        LD      B,0
GS3:    CP      10
        JR      C,GS4
        SUB     10
        INC     B
        JR      GS3
GS4:    ADD     A,'0'
        LD      (WORK+1),A
        LD      A,B
        OR      A
        JR      Z,GS5
        ADD     A,'0'
GS5:    LD      (WORK),A        ; (0: a blank)
        LD      C,AC_WHT
        LD      A,(WORK)
        OR      A
        JR      Z,GS6
        CALL    GLYSTY
GS6:    LD      (WORK+2),A
        LD      C,AC_WHT
        LD      A,(WORK+1)
        CALL    GLYSTY
        LD      (WORK+3),A
        LD      A,(T2)
        INC     A
        LD      D,A
        LD      E,2
        CALL    NTADR
        EX      DE,HL
        LD      HL,WORK+2
        LD      BC,2
        JP      VWRITE
ZERO4:  DB      0,0,0,0

; GFXROUND: "RND" and the round A on the left, rows 21 and 22. Clobbers all.
GFXROUND: LD    (T0),A
        LD      C,AC_WHT
        LD      A,'R'
        CALL    GLYSTY
        LD      (WORK),A
        LD      C,AC_WHT
        LD      A,'N'
        CALL    GLYSTY
        LD      (WORK+1),A
        LD      C,AC_WHT
        LD      A,'D'
        CALL    GLYSTY
        LD      (WORK+2),A
        LD      DE,21*256+0
        CALL    NTADR
        EX      DE,HL
        LD      HL,WORK
        LD      BC,3
        CALL    VWRITE
        LD      A,(T0)
        LD      B,0
GR1:    CP      10
        JR      C,GR2
        SUB     10
        INC     B
        JR      GR1
GR2:    ADD     A,'0'
        LD      (WORK+4),A
        LD      A,B
        OR      A
        JR      Z,GR3
        ADD     A,'0'
GR3:    LD      (WORK+3),A
        LD      C,AC_CYAN
        LD      A,(WORK+3)
        OR      A
        JR      Z,GR4
        CALL    GLYSTY
GR4:    LD      (WORK+5),A
        LD      C,AC_CYAN
        LD      A,(WORK+4)
        CALL    GLYSTY
        LD      (WORK+6),A
        LD      DE,22*256+0
        CALL    NTADR
        EX      DE,HL
        LD      HL,WORK+5
        LD      BC,2
        JP      VWRITE

; BIGLINE: the HUD's 20 characters (WORK: ASCII; WORK+20: each one's seat,
; 0-3, for its colour) on row 0 from column 4, five groups of 4 a cell
; apart. Clobbers all.
BIGLINE: LD     HL,WORK+44      ; the line, blank
        LD      B,24
BL0:    LD      (HL),0
        INC     HL
        DJNZ    BL0
        XOR     A
        LD      (WORK+40),A     ; i
BL1:    LD      A,(WORK+40)
        LD      E,A
        LD      D,0
        LD      HL,WORK+20
        ADD     HL,DE
        LD      A,(HL)
        AND     3
        LD      E,A
        LD      HL,BLCOL
        ADD     HL,DE
        LD      C,(HL)
        LD      A,(WORK+40)
        LD      E,A
        LD      HL,WORK
        ADD     HL,DE
        LD      A,(HL)
        CALL    GLYSTY
        LD      D,A
        LD      A,(WORK+40)     ; line[i + i / 4]
        LD      E,A
        SRL     A
        SRL     A
        ADD     A,E
        LD      E,A
        LD      HL,WORK+44
        LD      A,D
        LD      D,0
        ADD     HL,DE
        LD      (HL),A
        LD      A,(WORK+40)
        INC     A
        LD      (WORK+40),A
        CP      20
        JR      NZ,BL1
        LD      DE,4
        CALL    NTADR
        EX      DE,HL
        LD      HL,WORK+44
        LD      BC,24
        JP      VWRITE
BLCOL:  DB      AC_WHT,AC_RED,AC_CYAN,AC_YEL

; ANIMSTEP: the animated tiles' characters, redrawn for the phase: the next
; kind that is behind, one a frame (the display is on). Clobbers all.
ANIMSTEP: LD    A,(FRAMES)
        RRCA
        RRCA
        RRCA
        AND     3
        LD      (T0),A          ; the phase, (frames >> 3) & 3
        LD      B,N_ANIM
AN1:    PUSH    BC
        LD      A,(ANNEXT)
        LD      C,A             ; k
        INC     A
        CP      N_ANIM
        JR      C,AN2
        XOR     A
AN2:    LD      (ANNEXT),A
        LD      B,0
        LD      HL,ANPH
        ADD     HL,BC
        LD      A,(T0)
        CP      (HL)
        JR      Z,AN9           ; drawn in this phase already
        LD      (HL),A
        LD      A,C             ; its 4 cells: anim_cells[k][ph][j] at k * 256 + ph * 64 + j * 16
        LD      H,A
        LD      L,0
        LD      A,(T0)
        RRCA
        RRCA
        LD      E,A             ; ph * 64
        LD      D,0
        LD      A,E
        AND     0C0H
        LD      E,A
        ADD     HL,DE
        LD      DE,A_ANIM_CELLS
        ADD     HL,DE
        LD      A,C             ; the first character code: ANIM_BASE + 4 k
        ADD     A,A
        ADD     A,A
        ADD     A,ANIM_BASE
        LD      B,4
AN3:    PUSH    BC
        PUSH    AF
        PUSH    HL
        CALL    CELLPUT
        POP     HL
        LD      DE,16
        ADD     HL,DE
        POP     AF
        INC     A
        POP     BC
        DJNZ    AN3
        POP     BC
        RET
AN9:    POP     BC
        DJNZ    AN1
        RET

; DRTILE: map tile A drawn: what the map has there (a bomb of ours, pending,
; where the server has none yet). Clobbers all.
DRTILE: LD      (T1),A
        LD      C,A
        LD      B,0
        LD      HL,ST+OFF_MAP
        ADD     HL,BC
        LD      A,(HL)
        LD      (T2),A          ; the tile
        LD      A,(PEND)
        LD      HL,T1
        CP      (HL)
        JR      NZ,DRT1
        LD      A,T_BOMB
        LD      (T2),A
DRT1:   LD      A,(T1)          ; (c, r): 13 r + c
        LD      C,0
DRT2:   CP      ARENA_W
        JR      C,DRT3
        SUB     ARENA_W
        INC     C
        JR      DRT2
DRT3:   LD      B,A
        LD      A,(T2)
        JP      GFXTILE

; DRDIRTY: the tiles marked (MAPD) drawn, A at the most (the lowest first).
; Clobbers all.
DRDIRTY: LD     (T3),A
        LD      HL,MAPD
        LD      D,0             ; D = the byte
DD1:    LD      A,(HL)
        OR      A
        JR      Z,DDNX
        LD      B,A             ; its lowest bit set, in C
        LD      C,0
DD2:    SRL     B
        JR      C,DD3
        INC     C
        JR      DD2
DD3:    LD      A,1             ; cleared
        LD      B,C
        INC     B
DD4:    DEC     B
        JR      Z,DD5
        ADD     A,A
        JR      DD4
DD5:    CPL
        AND     (HL)
        LD      (HL),A
        LD      A,D             ; tile = 8 D + C
        ADD     A,A
        ADD     A,A
        ADD     A,A
        ADD     A,C
        PUSH    HL
        PUSH    DE
        CALL    DRTILE
        POP     DE
        POP     HL
        LD      A,(T3)
        DEC     A
        LD      (T3),A
        RET     Z
        JR      DD1
DDNX:   INC     HL
        INC     D
        LD      A,D
        CP      18
        JR      NZ,DD1
        RET

; DRMOVERS: each bomb in the air drawn on the cell grid. Clobbers all.
DRMOVERS: XOR   A
        LD      (T0),A
DM1:    LD      A,(T0)
        LD      C,A
        LD      A,(MOVON)
        LD      B,C
        INC     B
DM2:    RRCA
        DJNZ    DM2
        JR      NC,DM9          ; (the carry: bit m)
        LD      E,C
        LD      D,0
        LD      HL,MOVHX
        ADD     HL,DE
        LD      B,(HL)
        LD      HL,MOVHY
        ADD     HL,DE
        LD      C,(HL)
        LD      A,(MOVD)        ; a dud: its bit
        LD      D,A
        LD      A,(T0)
        INC     A
        LD      E,A
DM3:    RRC     D
        DEC     E
        JR      NZ,DM3
        LD      A,0
        JR      NC,DM4
        LD      A,1
DM4:    CALL    GFXBOMB
DM9:    LD      A,(T0)
        INC     A
        LD      (T0),A
        CP      MOVERS
        JR      NZ,DM1
        RET

; ARFRAME: the arena's drawing each frame: a few marked tiles, the bombs in
; the air, one animated tile's characters. Clobbers all.
ARFRAME: LD     A,24
        CALL    DRDIRTY
        CALL    DRMOVERS
        JP      ANIMSTEP

; ARALL: every tile drawn (the display is off: no limit). Clobbers all.
ARALL:  LD      HL,MAPD
        LD      B,17
        LD      A,0FFH
AL1:    LD      (HL),A
        INC     HL
        DJNZ    AL1
        LD      (HL),7FH        ; (143 tiles: 17 bytes and 7 bits)
        LD      A,160
        CALL    DRDIRTY
        JP      DRMOVERS

; sprites.inc -- the bombers (src/adam/gfx.c spr_*, transcribed). Fixed.
;
; Each bomber is two 16 x 16 sprites (the VDP's sprites have a colour each):
; A in the seat's colour, B in black in front of it with the eyes cut out.
; Four sprites fit on a line: when 3 bombers share lines they take turns at
; the front (SROT). The cheering pair is doubled to 32 x 32: 4 B and 4 A
; sprites, the last 8 slots.
;
; A seat's record (SPR + 6 n, IX): +0 on, +1 the cheering pair, +2 its
; frame, +3 its colour's index, +4 x, +5 y. SPRFLUSH writes the sprite
; attribute table from them, direct to VRAM (OUTs 31 cycles apart at least).

SR_ON   EQU     0
SR_BIG  EQU     1
SR_FR   EQU     2
SR_SEAT EQU     3
SR_X    EQU     4
SR_Y    EQU     5

; SPRIX: IX = seat A's record. Preserves BC, DE, HL.
SPRIX:  PUSH    BC
        LD      C,A
        LD      B,0
        LD      IX,SPR
        ADD     IX,BC
        ADD     IX,BC
        ADD     IX,BC
        ADD     IX,BC
        ADD     IX,BC
        ADD     IX,BC
        POP     BC
        RET

; SPRAT: seat A's sprite at the screen's own place: B = x (0-159), C = y
; (0-199), D = frame (FR_*), E = seat (bit 2: the cheering pair).
; Clobbers A, E, HL, IX.
SPRAT:  CALL    SPRIX
        LD      (IX+SR_ON),1
        LD      A,E
        AND     4
        LD      (IX+SR_BIG),A
        LD      A,E
        AND     3
        LD      (IX+SR_SEAT),A
        LD      (IX+SR_FR),D
        PUSH    DE
        LD      E,B             ; x * 3 / 2
        LD      D,0
        LD      H,D
        LD      L,E
        ADD     HL,HL
        ADD     HL,DE
        SRL     H
        RR      L
        LD      (IX+SR_X),L
        POP     DE
        LD      A,C             ; y - 8
        SUB     8
        JR      NC,SPA1
        XOR     A
SPA1:   LD      (IX+SR_Y),A
        RET

; SPRARENA: seat A's sprite in arena units: B = x, C = y, D = frame, E =
; seat (a walking bomber). Clobbers A, E, IX.
SPRARENA: CALL  SPRIX
        LD      (IX+SR_ON),1
        LD      (IX+SR_BIG),0
        LD      (IX+SR_FR),D
        LD      A,E
        AND     3
        LD      (IX+SR_SEAT),A
        LD      A,B
        ADD     A,40
        LD      (IX+SR_X),A
        LD      A,C
        ADD     A,8
        LD      (IX+SR_Y),A
        RET

; SPROFF: seat A's sprite off. Clobbers A, IX.
SPROFF: CALL    SPRIX
        LD      (IX+SR_ON),0
        RET

; SPRHIDE: all four off (the next SPRFLUSH shows it). Clobbers A.
SPRHIDE: XOR    A
        LD      (SPR),A
        LD      (SPR+6),A
        LD      (SPR+12),A
        LD      (SPR+18),A
        RET

; SPRFLUSH: the sprite attribute table written as set. Clobbers all.
SPRFLUSH: LD    IX,SPR          ; crowd: any bomber with 3 within 16 lines
        LD      B,4
        LD      H,0             ; H = crowd
SFO:    LD      A,(IX+SR_ON)
        OR      A
        JR      Z,SFON
        LD      A,(IX+SR_BIG)
        OR      A
        JR      NZ,SFON
        LD      IY,SPR
        LD      C,4
        LD      L,0             ; L = near
SFI:    LD      A,(IY+SR_ON)
        OR      A
        JR      Z,SFIN
        LD      A,(IY+SR_BIG)
        OR      A
        JR      NZ,SFIN
        LD      A,(IY+SR_Y)
        SUB     (IX+SR_Y)
        CP      16
        JR      NC,SFIN
        INC     L
SFIN:   LD      DE,6
        ADD     IY,DE
        DEC     C
        JR      NZ,SFI
        LD      A,L
        CP      3
        JR      C,SFON
        LD      H,1
SFON:   LD      DE,6
        ADD     IX,DE
        DJNZ    SFO
        LD      A,H             ; SROT: on while crowded
        OR      A
        LD      A,0
        JR      Z,SF6
        LD      A,(SROT)
        INC     A
        AND     3
SF6:    LD      (SROT),A
        CALL    VLOCK
        LD      HL,VR_SAT
        CALL    VWA
        LD      C,0             ; C = the slot
SF7:    LD      A,C
        CP      8
        JP      NC,SFBG
        SRL     A               ; a normal slot: seat ((s >> 1) - rot) & 3
        LD      B,A
        LD      A,(SROT)
        LD      D,A
        LD      A,B
        SUB     D
        AND     3
        CALL    SPRIX
        LD      A,(IX+SR_ON)
        OR      A
        JR      Z,SFOFF
        LD      A,(IX+SR_BIG)
        OR      A
        JR      NZ,SFOFF
        LD      A,(IX+SR_FR)    ; the pattern's name: frame * 8, + 4 for A
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      B,A
        LD      A,C
        AND     1
        LD      E,SC_BLACK
        JR      Z,SF8
        LD      A,B
        ADD     A,4
        LD      B,A
        LD      E,(IX+SR_SEAT)
        LD      D,0
        LD      HL,SEATCOL
        ADD     HL,DE
        LD      E,(HL)          ; the seat's colour
SF8:    LD      A,(IX+SR_Y)
        DEC     A
        OUT     (VDPD),A
        LD      A,(IX+SR_X)
        OUT     (VDPD),A
        LD      A,B
        PAD4
        OUT     (VDPD),A
        LD      A,E
        PAD4
        OUT     (VDPD),A
        JR      SFNEXT
SFOFF:  LD      A,0C0H          ; off the bottom
        OUT     (VDPD),A
        XOR     A
        PAD4
        OUT     (VDPD),A
        PAD4
        OUT     (VDPD),A
        PAD4
        OUT     (VDPD),A
SFNEXT: INC     C
        LD      A,C
        CP      16
        JP      NZ,SF7
        JP      VFREE
; slots 8-15: the cheering pair of the first seat that has one: slot
; 8 + q the B block q, 12 + q the A block q (q: bit 0 right, bit 1 down)
SFBG:   LD      IX,SPR
        LD      B,4
SFB1:   LD      A,(IX+SR_ON)
        OR      A
        JR      Z,SFB2
        LD      A,(IX+SR_BIG)
        OR      A
        JR      NZ,SFB3
SFB2:   LD      DE,6
        ADD     IX,DE
        DJNZ    SFB1
        JR      SFOFF           ; none
SFB3:   LD      A,(IX+SR_FR)    ; f: the cheering frame, 0 or 1
        SUB     FR_WIN0
        CP      2
        JR      C,SFB4
        XOR     A
SFB4:   ADD     A,A
        ADD     A,A
        ADD     A,A             ; f * 8
        LD      B,A
        LD      A,C
        AND     3
        LD      D,A             ; D = q
        ADD     A,B
        ADD     A,N_FRAMES*2
        ADD     A,A
        ADD     A,A             ; the block's name
        LD      B,A
        LD      E,SC_BLACK
        LD      A,C
        AND     4
        JR      Z,SFB5
        LD      A,B
        ADD     A,16            ; the A layer
        LD      B,A
        LD      L,(IX+SR_SEAT)
        LD      H,0
        PUSH    DE
        LD      DE,SEATCOL
        ADD     HL,DE
        POP     DE
        LD      E,(HL)
SFB5:   LD      A,D             ; y: (q >> 1) * 16 + sy - 1
        SRL     A
        AND     1
        RLCA
        RLCA
        RLCA
        RLCA
        ADD     A,(IX+SR_Y)
        DEC     A
        OUT     (VDPD),A
        LD      A,D             ; x: (q & 1) * 16 + sx
        AND     1
        RLCA
        RLCA
        RLCA
        RLCA
        ADD     A,(IX+SR_X)
        NOP
        OUT     (VDPD),A
        LD      A,B
        PAD4
        OUT     (VDPD),A
        LD      A,E
        PAD4
        OUT     (VDPD),A
        JR      SFNEXT

SEATCOL: DB     15,8,7,11       ; each seat's TMS colour (src/adam/gfx.c)

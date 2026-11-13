; gfx.inc -- the cart's pictures: the upload at boot, the tile stamp, a
; seat's frame, and the tables the arena reads (where a tile goes, the line
; of a y).

; gfx_boot: every fixed picture (build/studio2/artdata.inc ART_BOOT) into the
; cart's image store.
        RFIT    44
gfx_boot:
        LDR     R8, ART_BOOT
gb1:    LDN     R8
        XRI     0FFH
        BZ      gb9
        PARS
        LDA     R8                      ; id
        PD
        LDA     R8                      ; width
        PD
        LDA     R8                      ; height: the rows that follow
        PLO     R9
        PD
gb2:    LDA     R8
        PD
        DEC     R9
        GLO     R9
        BNZ     gb2
        GOP     TOP_IMAGE
        BR      gb1
gb9:    FRET

; ---------------------------------------------------------------------------
; tstamp -- tile R9.0 stamped as ST's map has it (our own pending bomb on a
;   floor, PEND); its ANIMB bit set if it is a two-frame tile, the frame
;   ANIMPH's. Leaf (LCALL). Clobbers R8, R9, RD.1, D, DF, M(R2).
; ---------------------------------------------------------------------------
        RFIT    126
tstamp: GLO     R9                      ; t
        ADI     LO(ST+OFF_MAP)
        PLO     RB
        LDN     RB                      ; its kind
        BNZ     tsk
        GETV    PEND                    ; a floor: our bomb, not the server's yet?
        STR     R2
        GLO     R9
        XOR
        BNZ     tsk0
        LDI     T_BOMB
        BR      tsk
tsk0:   LDI     T_FLOOR
tsk:    ADI     LO(TILE_IMG)
        PLO     R8
        LDI     HI(TILE_IMG)
        PHI     R8
        LDN     R8                      ; its image
        PHI     RD
        GLO     R8
        ADI     N_TILES
        PLO     R8
        LDN     R8                      ; and its other frame's
        STR     R2
        GHI     RD
        XOR
        PHI     R9                      ; R9.1: 0 = one frame
        BZ      tsa
        GETV    ANIMPH                  ; two: ANIMPH picks
        BZ      tsa
        LDN     R2
        PHI     RD
tsa:    GLO     R9                      ; its ANIMB bit, in M(R2)
        ANI     7
        ADI     LO(BITS)
        PLO     R8
        LDI     HI(BITS)
        PHI     R8
        LDN     R8
        STR     R2
        GLO     R9                      ; R8 -> its ANIMB byte
        SHR
        SHR
        SHR
        ADI     LO(ANIMB)
        PLO     R8
        LDI     HI(ANIMB)
        PHI     R8
        GHI     R9
        BZ      tsb0
        LDN     R8                      ; two frames: on
        OR
        STR     R8
        BR      tsxy
tsb0:   LDN     R2                      ; one: off
        XRI     0FFH
        STR     R2
        LDN     R8
        AND
        STR     R8
tsxy:   GLO     R9                      ; x, y from TTX / TTY
        ADI     LO(TTX)
        PLO     R8
        LDI     HI(TTX)
        PHI     R8
        PARS
        GHI     RD                      ; the image
        PD
        LDN     R8                      ; x
        PD
        GLO     R9
        ADI     LO(TTY)
        PLO     R8
        LDI     HI(TTY)
        PHI     R8
        LDN     R8                      ; y
        PD
        P       TM_SET
        GOP     TOP_STAMP
        SEP     R3

; ---------------------------------------------------------------------------
; seat_up -- seat R9.0's sprite image (IM_SEAT + s) made frame R9.1 (FR_*):
;   SEAT_FR's 15 rows, unrolled. Leaf. Clobbers R8, D.
; ---------------------------------------------------------------------------
        RFIT    82
seat_up:
        GHI     R9
        SHL                             ; R8 -> SEAT_FR + 256 s + 16 f
        SHL
        SHL
        SHL
        PLO     R8
        GLO     R9
        ADI     HI(SEAT_FR)
        PHI     R8
        PARS
        GLO     R9
        ADI     IM_SEAT
        PD
        P       4
        P       BOMBER_H
        REPT    BOMBER_H
        LDA     R8
        PD
        ENDM
        GOP     TOP_IMAGE
        SEP     R3

; seat_upw -- the same, a cheering frame: R9.1 = 16 or 17 (WIN_FR + 32 s
;   + 16 (k - 16)). Leaf. Clobbers R8, D.
        RFIT    97
seat_upw:
        GLO     R9
        SHL
        SHL
        SHL
        SHL
        SHL
        PLO     R8
        GHI     R9
        ANI     1
        SHL
        SHL
        SHL
        SHL
        STR     R2
        GLO     R8
        ADD
        ADI     LO(WIN_FR)
        PLO     R8
        LDI     HI(WIN_FR)
        ADCI    0
        PHI     R8
        PARS
        GLO     R9
        ADI     IM_SEAT
        PD
        P       4
        P       BOMBER_H
        REPT    BOMBER_H
        LDA     R8
        PD
        ENDM
        GOP     TOP_IMAGE
        SEP     R3

; ---------------------------------------------------------------------------
; the tables: where tile t goes (TTX, TTY), the line of a y in units
; (YLINE: 16 units to 10 lines), a bit (BITS)
; ---------------------------------------------------------------------------
        PFIT    TILES
TTX:
tt      SET     0
        WHILE   tt < TILES
        DB      1 + TILE_W * (tt # ARENA_W)
tt      SET     tt + 1
        ENDM
        PFIT    TILES
TTY:
tt      SET     0
        WHILE   tt < TILES
        DB      ARENA_Y + TILE_H * (tt / ARENA_W)
tt      SET     tt + 1
        ENDM
        PFIT    MAX_Y+1
YLINE:
tt      SET     0
        WHILE   tt <= MAX_Y
        DB      (tt * TILE_H + UNIT / 2) / UNIT
tt      SET     tt + 1
        ENDM
        PFIT    8
BITS:   DB      01H, 02H, 04H, 08H, 10H, 20H, 40H, 80H

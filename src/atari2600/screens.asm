; screens.asm -- bank 5: the text screens (waiting room, round end, game over,
; connecting, link lost, title). A display bank like the arena's: it draws a
; frame, then hops to PLAY and NET, which hand back here.
;
; A screen is 21 rows of 12 characters the CARTRIDGE composes (TROW/TCHR/TEND);
; this bank only streams them to the player registers (the text kernel, from
; the Battleship client) with a colour a row. The strings are generated
; (tools/a26/mkscr.py); names, wins and timers are blitted over them as the
; decoder meets them.
        CPU     6502
        INCLUDE "vcs.inc"
        INCLUDE "fujinet.inc"
        INCLUDE "fbdefs.inc"
        INCLUDE "../../build/a26/tail.inc"


        ORG     $1000
SENTRY: lda     MENT
        cmp     #ENENTER
        bne     SFR
        jsr     SETUP           ; a new screen: compose its rows
        lda     #OSTIM
        sta     TIM64T
SFR:    jsr     SFRAME
        lda     #(BKSCR<<4)|ENFRAME
        sta     MRET
        lda     #ENWORK
        sta     MENT
        lda     #BKPLAY
        jmp     MGOTO

; ---------------------------------------------------------------- one frame
SFRAME: bit     TIMINT
        bpl     SFRAME
        sta     WSYNC
        lda     #2
        sta     VSYNC
        sta     WSYNC
        sta     WSYNC
        sta     WSYNC
        lda     #0
        sta     VSYNC
        lda     #2
        sta     VBLANK
        lda     #VBTIM
        sta     TIM64T
        inc     MFRAME
        bne     SF1
        inc     MFRAMEH
SF1:    lda     #0
        sta     CTRLPF
        jsr     TXTPOS
DVBLP:  bit     TIMINT
        bpl     DVBLP
        sta     WSYNC
        lda     #0
        sta     VBLANK
        lda     #VITIM
        sta     TIM64T
        ldy     #0              ; the plane byte of the row
        lda     #0
        sta     KT0             ; the row
SROW:   ldx     KT0
        sty     KT1
        ldy     KT0
        lda     (MTABL),y       ; its colour
        bpl     SR0
        and     #3              ; a seat row: the seat's colour (the winner's gold)
        tax
        lda     SEATCOL,x
        cpx     RESULT
        bne     SR0
        ldx     PHASE
        cpx     #PH_ROUNDEND
        bcc     SR0
        lda     #$1E
SR0:     ldx    MSCR            ; the room list: the picked row in gold
        cpx     #SC_ROOMS
        bne     SR1B
        ldx     KT0
        dex
        dex
        dex
        cpx     MSEL
        bne     SR1B
        lda     #$1E
SR1B:
SR1:    sta     COLUP0
        sta     COLUP1
        ldy     KT1
        jsr     TINK5
        inc     KT0
        lda     KT0
        cmp     #FNTROW
        bne     SROW
SPAD:   bit     TIMINT
        bpl     SPAD
        sta     WSYNC
        lda     #2
        sta     VBLANK
        lda     #OSTIM
        sta     TIM64T
        rts

SEATCOL: DB     $0E,$4C,$9C,$CC

; ---------------------------------------------------------------- setup
; SETUP -- MSCR is the new screen: its row-colour table, and every row composed.
LSTP    EQU     POOL+10         ; the list of rows with text
STRP    EQU     POOL+12         ; the string being copied
SETUP:  lda     #$FF
        sta     MSHOW           ; (the value last drawn: nothing yet)
        ldx     MSCR
        dex
        lda     SCCL,x
        sta     MTABL
        lda     SCCH,x
        sta     MTABH
        lda     SCRL,x
        sta     LSTP
        lda     SCRH,x
        sta     LSTP+1
        lda     #0
        sta     KT2             ; the row being composed
SU1:    lda     KT2
        jsr     FNROWA          ; begin it (blank unless a string follows)
        ldy     #0
        lda     (LSTP),y        ; the next row with text
        cmp     KT2
        bne     SU3
        iny
        lda     (LSTP),y
        sta     STRP
        iny
        lda     (LSTP),y
        sta     STRP+1
        lda     LSTP            ; step the list on
        clc
        adc     #3
        sta     LSTP
        bcc     SU2
        inc     LSTP+1
SU2:    ldy     #0
SU2A:   lda     (STRP),y
        beq     SU3
        jsr     FNCHR
        iny
        bne     SU2A
SU3:    lda     #0
        sta     FNRSEL+FH_TEND
        inc     KT2
        lda     KT2
        cmp     #FNTROW
        bne     SU1
        rts

        INCLUDE "textkern.inc"

DSTART:
        INCLUDE "../../build/a26/scr.inc"
        END

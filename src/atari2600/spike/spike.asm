; spike.asm -- FujiBlast Atari 2600 arena spike: a plain 4K image (no cart,
; no network) that draws a canned 13 x 11 arena, bomber 0 (player 0, steady)
; and three more bombers on player 1, moved between tile rows at the seams.
;
; Kernel = M.U.L.E.'s map loop: an ENTRY is two scanlines, line A updates
; player 0 and line B player 1, six playfield tables are read every line.
; Here a tile row is 7 entries; the seam before it clears the playfield and,
; on a row where player 1 changes bomber, takes two more lines to position it
; (coarse in SEAM1, HMOVE in SEAM2).  Tables stand in ROM for the cart's planes.
;
; Build: tools/a26/spike.sh [LOOK]

        CPU     6502UNDOC
        INCLUDE "../../../tools/a26/vcs.inc"

; ---- zero page ----
ROW     EQU     $80             ; tile row 0-10
NSEAM   EQU     $81             ; the row at which P1 changes bomber
JOBI    EQU     $82
CA      EQU     $83             ; this row's colours: even entries
CB      EQU     $84             ;   odd entries
P0CNT   EQU     $85
P1CNT   EQU     $86
P0PTR   EQU     $87             ; 2 bytes: shape - top entry
P1PTR   EQU     $89
TMP     EQU     $8B
FRM     EQU     $8C

P0H     EQU     7
P1H     EQU     7
VBTIM   EQU     43
OSTIM   EQU     35
VITIM   EQU     227

        ORG     $F000

START:  sei
        cld
        ldx     #$FF
        txs
        lda     #0
        tax
CLR:    sta     0,x
        dex
        bne     CLR
        lda     #OSTIM
        sta     TIM64T
MAIN:   jsr     FRAME
        jmp     MAIN

FRAME:  bit     TIMINT
        bpl     FRAME
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
        inc     FRM

; ---- vblank work: per-frame state ----
        lda     #0
        sta     CTRLPF
        sta     PF0
        sta     PF1
        sta     PF2
        sta     GRP0
        sta     GRP1
        sta     REFP0
        sta     REFP1
        sta     NUSIZ0
        sta     NUSIZ1
        sta     VDELP0
        sta     VDELP1
        sta     ROW
        lda     JROW            ; the first bomber's seam row
        sta     NSEAM
        lda     #0
        sta     JOBI
; player 0: window [P0TOPE, +P0H); pointer = shape - top
        lda     #P0TOPE+P0H
        sta     P0CNT
        lda     #SHAPE&$FF
        sec
        sbc     #P0TOPE
        sta     P0PTR
        lda     #SHAPE>>8
        sbc     #0
        sta     P0PTR+1
        lda     #$FF            ; player 1 idle until a job starts
        sta     P1CNT
        lda     #$0E
        sta     COLUP0
; position player 0
        lda     #P0XPIX
        jsr     POS0
        sta     WSYNC
        sta     HMOVE
        sta     WSYNC
        sta     HMCLR
DVBLP:  bit     TIMINT
        bpl     DVBLP
        sta     WSYNC
        lda     #0
        sta     VBLANK
        lda     #VITIM
        sta     TIM64T
        lda     #C_FLOOR
        sta     COLUBK
        lda     #P0H*2
        ldx     #8              ; the top margin (the HUD row, later)
MARG:   sta     WSYNC
        dex
        bne     MARG
        ldy     #0

; ---- the seam: one line, plus two on a row where player 1 moves ----
SEAM0:  sta     WSYNC
        lda     #0
        sta     PF0
        sta     PF1
        sta     PF2
        ldx     ROW
        cpx     #11
        bne     SEAMGO
        jmp     ARENADONE
SEAMGO:
        lda     RCA,x
        sta     CA
        lda     RCB,x
        sta     CB
        cpx     NSEAM
        beq     JOB
        inc     ROW
        jmp     P0A

JOB:    inc     ROW
        ldx     JOBI
        lda     JX,x
        jmp     SEAM1

; SEAM1: the coarse position of player 1 (the classic divide by 15).
SEAM1:  sta     WSYNC
        sec
SPOS:   sbc     #15
        bcs     SPOS
        IF (SPOS>>8) <> ((SPOS+4)>>8)
        ERROR   "SPOS loop crosses a page"
        ENDIF
        eor     #7
        asl     a
        asl     a
        asl     a
        asl     a
        sta     HMP1
        sta     RESP1
; SEAM2: HMOVE at the start of the line, then the job's cells.
        sta     WSYNC
        sta     HMOVE
        lda     JCOL,x
        sta     COLUP1
        lda     JCNT,x
        sta     P1CNT
        lda     JTOP,x
        sta     TMP
        lda     #SHAPE&$FF
        sec
        sbc     TMP
        sta     P1PTR
        lda     #SHAPE>>8
        sbc     #0
        sta     P1PTR+1
        inx
        stx     JOBI
        lda     JROW,x
        sta     NSEAM
        jmp     P0A

        INCLUDE "rows.inc"

ARENADONE:
        lda     #0
        sta     GRP0
        sta     GRP1
DFPAD:  bit     TIMINT
        bpl     DFPAD
        sta     WSYNC
        lda     #2
        sta     VBLANK
        lda     #OSTIM
        sta     TIM64T
        rts

; POS0 -- player 0 to colour clock A (its HMOVE is the caller's).
POS0:   sta     WSYNC
        sec
POS1:   sbc     #15
        bcs     POS1
        eor     #7
        asl     a
        asl     a
        asl     a
        asl     a
        sta     HMP0
        sta     RESP0
        rts

        INCLUDE "gen.inc"

        ORG     $FFFC
        DW      START
        DW      START

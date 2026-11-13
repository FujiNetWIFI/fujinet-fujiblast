; textkern.inc -- the text rows' kernel, verbatim from M.U.L.E.'s dispmap.inc
; (itself the Battleship client's, from the cartridge's testrom): do NOT retime.
RNOP    EQU     16
R3      EQU     1
HM0     EQU     15
HM1     EQU     0
TP0     EQU     FNTEXT+$000
TP1     EQU     FNTEXT+$080
TP2     EQU     FNTEXT+$100
TP3     EQU     FNTEXT+$180
TP4     EQU     FNTEXT+$200
TP5     EQU     FNTEXT+$280
TINK    EQU     FNTCLH-1
; ----------------

; ---------------------------------------------------------------------------
; TXTPOS -- two lines: the players where the 48-pixel text wants them
; (disptext.inc's DINIT), three close copies, VDEL; the playfield and the
; background off.
TXTPOS: sta     WSYNC
        lda     #0
        sta     REFP0
        sta     REFP1
        sta     COLUBK
        sta     PF0
        sta     PF1
        sta     PF2
        sta     GRP0
        sta     GRP1
        sta     GRP0
        sta     WSYNC
        REPT    RNOP
        nop
        ENDM
        IF      R3
        sta     PAD3
        ENDIF
        sta     RESP0
        sta     RESP1
        lda     #HM0*16
        sta     HMP0
        lda     #(HM1*16)&$FF
        sta     HMP1
        sta     WSYNC
        sta     HMOVE
        lda     #$03
        sta     NUSIZ0
        sta     NUSIZ1
        lda     #1
        sta     VDELP0
        sta     VDELP1
        rts

; POSOBJ -- one line: object X (0 = P0, 1 = P1) to colour clock A. The
; classic divide-by-15 loop; the HMOVE that finishes it is the caller's.
POSOBJ: sta     WSYNC           ; 0
        sec                     ; 2
POS1:   sbc     #15             ; 4
        bcs     POS1            ; 6/7 per 15 clocks
        eor     #7
        asl     a
        asl     a
        asl     a
        asl     a
        sta     HMP0,x
        sta     RESP0,x
        rts

; ---------------------------------------------------------------------------
; TINK5 -- a text row from plane byte Y: five ink lines and a seam line
; that blanks the players. Y is left at the next row. (dispgame.inc's.)
TINK5:  tsx
        stx     SAVSP
        tya
        clc
        adc     #TINK
        sta     MLINE
TI1:    sta     WSYNC
        lda     TP0,y           ; 4
        sta     GRP0            ; 7
        lda     TP1,y           ; 11
        sta     GRP1            ; 14
        lda     TP2,y           ; 18
        sta     GRP0            ; 21
        lda     TP4,y           ; 25
        tax                     ; 27
        txs                     ; 29
        lda     TP3,y           ; 33
        tax                     ; 35
        lda     TP5,y           ; 39
        stx     GRP1            ; 42
        tsx                     ; 44
        stx     GRP0            ; 47
        sta     GRP1            ; 50
        sty     GRP0            ; 53
        iny                     ; 55
        cpy     MLINE           ; 58
        bne     TI1             ; 61
        ldx     SAVSP
        txs
        sta     WSYNC
        lda     #0
        sta     GRP0
        sta     GRP1
        sta     GRP0
        iny
        rts

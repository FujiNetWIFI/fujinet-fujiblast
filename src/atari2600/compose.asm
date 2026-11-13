; compose.asm -- bank 2: turn the map's dirty rows into playfield table bytes
; in the cartridge's planes. No firmware change: the bytes are streamed into
; path buffer 1 (one store each, no poll) and one PATHPOKE blit lands them.
;
; A row is composed in UNITS, each small enough for the overscan: unit 0 (the
; prelude) classifies the row and writes its two colours; units 1-6 are the
; six playfield registers, seven entries each.  The arena hops here at the
; start of its overscan; this bank works while the timer allows, then hops
; back to MRET.
        CPU     6502
        INCLUDE "vcs.inc"
        INCLUDE "fujinet.inc"
        INCLUDE "fbdefs.inc"
        INCLUDE "../../build/a26/tail.inc"

        IF      PAL
UMIN    EQU     46              ; ticks a unit needs, plus what BUILDJOBS needs after it
        ELSE
UMIN    EQU     30              ; timer ticks (64 cycles) a unit may need
        ENDIF

        ORG     $1000
CENTRY: lda     MENT
        cmp     #ENARENA
        bne     WLOOP
        jmp     AINIT
WLOOP:  bit     TIMINT          ; past zero INTIM free-runs: the latch first
        bmi     WDONE
        lda     INTIM
        cmp     #100            ; underflowed between the BIT and the read
        bcs     WLATCH
        cmp     #UMIN
        bcc     WDONE
        jsr     UNIT
        bcc     WLOOP
        jmp     WDONE
WLATCH: lda     #1              ; the read cleared the latch: put it back
        sta     TIM64T
WDONE:  lda     MRET
        and     #$0F
        sta     MENT
        lda     MRET
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        jmp     MGOTO

; UNIT -- one unit of work. Carry set: nothing to do.
UNIT:   lda     CROW
        cmp     #$FF
        bne     UGO
; pick the lowest dirty row
        ldx     #0
PK1:    cpx     #8
        bcs     PKH
        lda     DIRTYL
        and     BITS,x
        bne     PKF
        beq     PKN
PKH:    lda     DIRTYH
        and     BITS-8,x
        bne     PKF
PKN:    inx
        cpx     #ROWS
        bne     PK1
        sec
        rts
PKF:    cpx     #8              ; clear its bit now: a change that lands while
        bcs     PKFH            ; the row composes marks it again
        lda     DIRTYL
        eor     BITS,x
        sta     DIRTYL
        jmp     PKG
PKFH:   lda     DIRTYH
        eor     BITS-8,x
        sta     DIRTYH
PKG:    stx     CROW
        lda     #$FF
        sta     CREG
UGO:    ldx     CROW
        lda     MROWL,x
        sta     CRPTR
        lda     MROWH,x
        sta     CRPTR+1
        lda     CREG
        cmp     #$FF
        beq     PRELUDE
        jmp     REGUNIT

; ---- the prelude: the row's class, its colours ----
PRELUDE:
        lda     #0
        sta     CMAXC
        ldy     #COLS-1
PL1:    lda     (CRPTR),y
        tax
        lda     KCLASS,x
        cmp     CMAXC
        bcc     PL2
        sta     CMAXC
PL2:    dey
        bpl     PL1
        lda     #0              ; a busy row (power-up, bomb, flame) uses the
        ldx     CMAXC           ; second art set
        cpx     #2
        bcc     PL3
        lda     #126
PL3:    sta     CSET
        ldx     CMAXC           ; the row's odd-entry colour -> plane 1 byte 109+row
        lda     CBTAB,x
        pha
        lda     CROW
        clc
        adc     #109
        sta     CBD
        lda     #0
        sta     CBD+1
        lda     CBD
        ora     #$80            ; +128: plane 1 (109+row < 128 so no carry)
        sta     CBD
        pla
        ldx     #0
        ldy     #FB_POKE
        jsr     FBLIT
        lda     #0
        sta     CREG
        clc
        rts

; ---- one register of the row ----
REGUNIT:
        lda     #PB_STREAM
        sta     FNRSEL+FH_PATHO
        lda     #FP_RST
        sta     FNRSEL+FH_PATHO
        ldx     #6
        lda     #0
RZ:     sta     RACC-1,x
        dex
        bne     RZ
        sta     RACC+6
        ldx     CREG
        lda     RHEAD,x
        sta     CJ
        lda     RCOUNT,x
        sta     CJN
        lda     #(ART>>8)
        sta     CARTP+1
        lda     #(CTAB>>8)
        sta     CTP+1
JL:     ldx     CJ
        ldy     LCOL,x          ; the tile's column
        lda     (CRPTR),y       ; its kind
        beq     JNEXT           ; floor: nothing to add
        tax
        lda     K7,x            ; art offset = set + kind*7
        clc
        adc     CSET
        sta     CARTP
        lda     CJ
        asl     a
        asl     a
        asl     a
        sta     CTP             ; 8 contribution bytes a tile
        INCLUDE "jloop.inc"
JNEXT:  inc     CJ
        dec     CJN
        bne     JL
        ldx     #0
EM:     lda     RACC,x
        sta     FNRSEL+FH_PATHC
        inx
        cpx     #ENTS
        bne     EM
; dst = CREG*128 + PFB + CROW*7
        lda     CROW
        asl     a
        asl     a
        asl     a
        sec
        sbc     CROW            ; row*7
        clc
        adc     #PFB
        sta     CBD
        lda     CREG
        lsr     a
        sta     CBD+1           ; reg>>1 pages of 256
        bcc     RB1
        lda     CBD             ; an odd register: +128
        clc
        adc     #128
        sta     CBD
        bcc     RB1
        inc     CBD+1
RB1:    lda     #ENTS
        sta     CBC
        lda     #0
        tax
        ldy     #FB_PPOKE
        jsr     FBLIT
        inc     CREG
        lda     CREG
        cmp     #6
        bne     RB2
        lda     #$FF            ; the row is done
        sta     CROW
RB2:    clc
        rts

        INCLUDE "blit.inc"

; AINIT -- the arena was just entered: the planes held text rows. Zero the map
; (a state sends no floor tiles: zero is the default), every row dirty, the
; HUD and status rows composed.
AINIT:  lda     #$FF
        sta     CROW
        sta     DIRTYL
        lda     #7
        sta     DIRTYH
        lda     #PB_STREAM
        sta     FNRSEL+FH_PATHO
        lda     #FP_RST
        sta     FNRSEL+FH_PATHO
        ldx     #COLS
        lda     #0
AI1:    sta     FNRSEL+FH_PATHC
        dex
        bne     AI1
        sta     MTMP2
AI2:    ldx     MTMP2
        lda     MROWL,x
        sta     CBD
        lda     MROWH,x
        sec
        sbc     #$18            ; the planes' byte offset, not the address
        sta     CBD+1
        lda     #COLS
        sta     CBC
        lda     #0
        tax
        ldy     #FB_PPOKE
        jsr     FBLIT
        inc     MTMP2
        lda     MTMP2
        cmp     #ROWS
        bne     AI2
        lda     #0
        jsr     FNROWA
        ldx     #0
AI3:    lda     HUDS,x
        beq     AI4
        jsr     FNCHR
        inx
        bne     AI3
AI4:    lda     #0
        sta     FNRSEL+FH_TEND
        lda     #20
        jsr     FNROWA
        ldx     #0
AI5:    lda     STAS,x
        beq     AI6
        jsr     FNCHR
        inx
        bne     AI5
AI6:    lda     #0
        sta     FNRSEL+FH_TEND
        lda     #$0E
        sta     HTXT
        jmp     WDONE

DSTART:
HUDS:   DB      "000  0 0 0 0",0
STAS:   DB      "FUJIBLAST",0
BITS:   DB      1,2,4,8,16,32,64,128,1,2,4

        INCLUDE "../../build/a26/art.inc"
        END

; arena.asm -- bank 1: the arena kernel and its frame.
;
; The spike's kernel (tools/a26/mkspike.py's pair code), reading the six
; playfield tables from the cartridge's planes. One frame:
;   overscan end -> VSYNC -> vblank (bomber jobs) -> HUD text row ->
;   position player 0 -> the 11 tile rows -> status text row -> pad
; then a hop to the compose bank for the overscan's work; it returns here.
        CPU     6502UNDOC
        INCLUDE "vcs.inc"
        INCLUDE "fujinet.inc"
        INCLUDE "fbdefs.inc"
        INCLUDE "../../build/a26/tail.inc"

P0H     EQU     7
P1H     EQU     7

MT0     EQU     PFT0
MT1     EQU     PFT1
MT2     EQU     PFT2
MT3     EQU     PFT3
MT4     EQU     PFT4
MT5     EQU     PFT5
RCA     EQU     FNTEXT+109      ; row colours: plane 0 / plane 1, byte 109+row
RCB     EQU     FNTEXT+$80+109

        ORG     $1000
AENTRY: lda     MENT
        cmp     #ENVB
        beq     AVB
        cmp     #ENENTER
        bne     AFR
        lda     #OSTIM
        sta     TIM64T
        lda     #(BKARENA<<4)|ENFRAME
        sta     MRET
        lda     #ENARENA        ; a new arena: the compose bank sets the planes up
        sta     MENT
        lda     #BKCOMP
        jmp     MGOTO
AFR:    jsr     FSTART
        lda     #(BKARENA<<4)|ENVB
        sta     MRET
        lda     #ENWORK         ; the vblank's hop: one compose unit
        sta     MENT
        lda     #BKCOMP
        jmp     MGOTO
AVB:    jsr     FBODY
        lda     #(BKARENA<<4)|ENFRAME
        sta     MRET
        lda     #ENWORK         ; the overscan's hop: the walker, then the link
        sta     MENT
        lda     #BKPLAY
        jmp     MGOTO

; ---------------------------------------------------------------- the frame
FSTART: bit     TIMINT
        bpl     FSTART
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
        bne     FS1
        inc     MFRAMEH
FS1:    rts

FBODY:

; ---- vblank work ----
        lda     #0
        sta     CTRLPF
        sta     NUSIZ0
        sta     NUSIZ1
        jsr     BUILDJOBS
        lda     #0
        sta     ROW
        jsr     TXTPOS          ; players for the text, two lines
        lda     HTXT
        sta     COLUP0
        sta     COLUP1
DVBLP:  bit     TIMINT
        bpl     DVBLP
        sta     WSYNC
        lda     #0
        sta     VBLANK
        lda     #VITIM
        sta     TIM64T
; ---- the HUD row (text row 0) ----
        ldy     #0
        jsr     TINK5
; ---- player 0: position it, HMOVE, then the colours ----
        lda     #0
        sta     NUSIZ0          ; text left it at 3: a different reset delay
        sta     NUSIZ1
        lda     P0X
        ldx     #0
        jsr     POSOBJ
        sta     WSYNC
        sta     HMOVE
        lda     #C_FLOOR
        sta     COLUBK
        lda     P0COL           ; (bit 0: face left, the shape mirrored)
        sta     COLUP0
        and     #1
        asl     a
        asl     a
        asl     a
        sta     REFP0
        lda     #0
        sta     VDELP0
        sta     VDELP1
        sta     NUSIZ0
        sta     NUSIZ1
        ldy     #0              ; entry index

; ---- the seam: one line, plus two on a row where player 1 moves ----
SEAM0:  sta     WSYNC
        lda     #0
        sta     PF0
        sta     PF1
        sta     PF2
        sta     HMP0            ; HMOVE strobes below must move player 1 only
        ldx     ROW
        cpx     #ROWS
        bne     SEAMGO
        jmp     ARENADONE
SEAMGO: lda     RCB,x
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
        lda     JTOP,x
        sta     MTMP
        tay
        lda     E2MOD,y         ; 7 + (top - 7*row)
        clc
        adc     #P1H
        sta     P1CNT
        lda     JCOL,x          ; bit 0: the walking frame
        and     #1
        asl     a
        asl     a
        asl     a
        clc
        adc     #SHAPES&$FF
        sec
        sbc     MTMP
        sta     P1PTR
        lda     #SHAPES>>8
        sbc     #0
        sta     P1PTR+1
        inx
        stx     JOBI
        lda     JROW,x
        sta     NSEAM
        jmp     P0A

        INCLUDE "../../build/a26/rows.inc"

; ---- after the last row: the status text row ----
ARENADONE:
        lda     #0
        sta     GRP0
        sta     GRP1
        jsr     TXTPOS
        lda     HTXT
        sta     COLUP0
        sta     COLUP1
        ldy     #20*FNTCLH
        jsr     TINK5
DFPAD:  bit     TIMINT
        bpl     DFPAD
        sta     WSYNC
        lda     #2
        sta     VBLANK
        ldx     JOBI            ; each job's seam is 2 lines the kernel may not
        lda     OSTAB,x         ;   have: take them out of the overscan
        sta     TIM64T
        rts

; ------------------------------------------------- the bomber jobs (vblank)
; Own bomber -> player 0 (steady). The others -> player 1's job list, sorted
; by top entry; a bomber whose window starts at or before the previous
; one's last row is dropped this frame.
BUILDJOBS:
        ldx     MYSEAT
        lda     BY,x
        tay
        lda     YENT,y
        sta     P0TOP
        clc
        adc     #P0H
        sta     P0CNT
        lda     BX,x
        jsr     XPIX
        sta     P0X
        lda     BDIR,x          ; the frame: the way I face, walking toggles it
        and     #3
        tay
        lda     DIRSH,y
        sta     MTMP
        lda     REFTAB,y
        sta     MTMP2
        lda     BDIR,x
        and     #4
        beq     BP1
        lda     MFRAME
        and     #8
        beq     BP1
        inc     MTMP
BP1:    lda     MTMP
        asl     a
        asl     a
        asl     a
        clc
        adc     #SHAPES&$FF
        sec
        sbc     P0TOP
        sta     P0PTR
        lda     #SHAPES>>8
        sbc     #0
        sta     P0PTR+1
        lda     SEATCOL,x
        ora     MTMP2
        sta     P0COL
; the others: candidate tops. Shown: the seated; in play, only the alive.
        lda     SEATED
        ldy     PHASE
        cpy     #PH_PLAY
        bne     BJ0
        and     ALIVE
BJ0:    sta     BVIS
        lda     #0
        sta     TUSED           ; bit s: s is a candidate
        ldx     #3
BJ1:    cpx     MYSEAT
        beq     BJ2
        lda     BVIS
        and     SBITS,x
        beq     BJ2
        lda     TUSED
        ora     SBITS,x
        sta     TUSED
        ldy     BY,x
        lda     YENT,y
        sta     TEOP,x
BJ2:    dex
        bpl     BJ1
; selection: k = 0.. the candidate with the smallest top
        lda     #0
        sta     JOBI
        lda     #0
        sta     TLAST           ; the previous job's last row + 1
        ldy     #0              ; jobs built
BJ3:    lda     TUSED
        beq     BJEND
        ldx     #$FF            ; best seat
        lda     #$FF
        sta     MTMP            ; best top
        stx     MTMP2
        ldx     #3
BJ4:    lda     TUSED
        and     SBITS,x
        beq     BJ5
        lda     TEOP,x
        cmp     MTMP
        bcs     BJ5
        sta     MTMP
        stx     MTMP2
BJ5:    dex
        bpl     BJ4
        ldx     MTMP2
        lda     TUSED           ; remove it from the candidates
        eor     SBITS,x
        sta     TUSED
        ldx     MTMP            ; its top entry
        lda     E2ROW,x
        sta     BJROW
        clc
        adc     #1              ; TLAST is (last row + 1): 0 = none
        cmp     TLAST
        beq     BJ3             ; not past the previous window: drop
        bcc     BJ3
        lda     BJROW
        sta     JROW,y
        lda     MTMP
        sta     JTOP,y
        clc
        adc     #P1H-1
        tax
        lda     E2ROW,x
        clc
        adc     #1
        sta     TLAST
        ldx     MTMP2
        lda     SEATCOL,x
        sta     JCOL,y
        lda     BDIR,x          ; walking: bit 0 of the colour picks the other frame
        and     #4
        beq     BJW
        lda     MFRAME
        and     #8
        beq     BJW
        lda     JCOL,y
        ora     #1
        sta     JCOL,y
BJW:
        lda     BX,x
        jsr     XPIX
        sta     JX,y
        iny
        cpy     #NJOBS
        bne     BJ3
BJEND:  lda     #$FF
        sta     P1CNT           ; player 1 idle until a job starts
        sta     JROW,y
        lda     JROW
        sta     NSEAM
        rts

; XPIX -- world x (A) -> colour clock: x*3/4 + 2.
XPIX:   sta     MTMP
        lsr     a
        lsr     a
        sta     MTMP2
        lda     MTMP
        sec
        sbc     MTMP2
        clc
        adc     #2
        rts

        INCLUDE "textkern.inc"

DSTART:
SBITS:  DB      1,2,4,8
SEATCOL: DB     $0E,$4C,$9C,$CC
OSTAB:  DB      OSTIM,OSTIM,OSTIM,OSTIM-3+PAL
DIRSH:  DB      2,4,0,4         ; up, right, down, left -> the shape pair
REFTAB: DB      0,0,0,1         ; left is the right shape mirrored
SHAPES: ; 6 shapes of 7 rows, 8 bytes apart: down, up, side; stand then walk
        DB      $3C,$7E,$5A,$3C,$7E,$24,$66,0
        DB      $3C,$7E,$5A,$3C,$7E,$28,$6C,0
        DB      $3C,$7E,$7E,$3C,$7E,$24,$66,0
        DB      $3C,$7E,$7E,$3C,$7E,$28,$6C,0
        DB      $3C,$7E,$7A,$3C,$7E,$24,$66,0
        DB      $3C,$7E,$7A,$3C,$7E,$18,$38,0

        INCLUDE "../../build/a26/ptab.inc"
        END

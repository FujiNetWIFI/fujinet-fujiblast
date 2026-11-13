; netdec.asm -- bank 6: the frame decoder. (net.asm, bank 3, is the link.)
;
; A display bank hops here once a frame, in the overscan, and is sent on. This
; bank never draws and never waits: one hop does ONE thing -- completes the
; transaction that was in flight (and posts the next one), or decodes a few
; more bytes of the last reply.
;
; THE RULES THIS LINK IS BUILT ON (each one was paid for elsewhere):
;   * Every transaction repaints the reply window, so a READ's bytes are all
;     decoded before the next transaction is posted.
;   * SEQ is the cartridge's own ACKSEQ + 1 (FNLNCH in the tail), never a RAM
;     counter: a console RESET leaves the cart running.
;   * One blit slot, no queue: FBLIT polls FNBGEN.
;   * A STATUS says how many bytes wait; a READ takes up to RDMAX of them.
;   * The state is applied AS IT DECODES (the map through a path buffer and one
;     PATHPOKE a run); a bad checksum asks for a whole new state (R).
        CPU     6502
        INCLUDE "vcs.inc"
        INCLUDE "fujinet.inc"
        INCLUDE "fbdefs.inc"
        INCLUDE "../../build/a26/tail.inc"

UMINB   EQU     12              ; ticks the decode leaves in the overscan

        ORG     $1000
DENTRY: lda     RLEFT           ; bytes of a reply still to decode?
        beq     DEN1
        jsr     DECODE
DEN1:
        lda     MRET            ; back to the display bank: low nibble = the
        and     #$0F            ;   entry code, high nibble = its bank
        sta     MENT
        lda     MRET
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        jmp     MGOTO

; ---------------------------------------------------------------------------
; DECODE -- decode reply bytes while there are some and the timer allows.
DECODE: lda     #PB_STREAM      ; the map runs are buffered here
        sta     FNRSEL+FH_PATHO
        lda     #FP_RST
        sta     FNRSEL+FH_PATHO
DLOOP:  bit     TIMINT          ; past zero INTIM free-runs: test the latch FIRST
        bmi     DSTOP
        lda     INTIM
        cmp     #100            ; (more than a timer can hold: it underflowed between
        bcs     DLATCH          ;   the BIT and this read, which cleared the latch)
        cmp     #UMINB
        bcc     DSTOP
        ldx     RPOS
        lda     FNRPLY,x
        inc     RPOS
        inc     LCRED
        jsr     DBYTE
        dec     RLEFT
        bne     DLOOP
DSTOP:  jsr     FLUSHSEG        ; the compose bank reuses the stream buffer
        rts
DLATCH: lda     #1              ; put the latch back: one tick, and it is set again
        sta     TIM64T
        jmp     DSTOP

; DBYTE -- A = one byte of the COBS stream.
DBYTE:  ldx     DCNT
        bne     DDATA
        cmp     #0
        beq     DFRAME          ; a zero ends the frame
        ldx     DCODE           ; a new block: the previous one implied a zero
        beq     DB1             ;   unless it was the first or a full 255
        cpx     #255
        beq     DB1
        pha
        lda     #0
        jsr     DOUT
        pla
DB1:    sta     DCODE
        sec
        sbc     #1
        sta     DCNT
        rts
DDATA:  dec     DCNT
        jmp     DOUT

; DFRAME -- the frame is whole: check its sums, act on it, start the next.
DFRAME: lda     DFILL
        cmp     #2
        bne     DFR9            ; too short to have sums
        lda     DH1
        cmp     DS1
        bne     DFRBAD
        lda     DH2
        cmp     DS2
        bne     DFRBAD
        lda     DTYPE
        cmp     #'C'
        bne     DFR1
        jsr     COMMIT
DFR1:   cmp     #'E'
        bne     DFR9
        lda     LFLAGS
        ora     #LF_LOST
        sta     LFLAGS
        jmp     DFR9
DFRBAD: lda     LFLAGS
        ora     #LF_RESYNC
        sta     LFLAGS
DFR9:   jsr     FLUSHSEG
        lda     #0
        sta     DCODE
        sta     DCNT
        sta     DFILL
        sta     DS1
        sta     DS2
        sta     DST             ; DS_TYPE
        lda     #$FF
        sta     DMROW
        rts

; DOUT -- A = one decoded byte: through the two-byte delay line.
DOUT:   ldx     DFILL
        cpx     #2
        bcs     DO2
        beq     DO1A            ; (never: X < 2 here)
DO1A:   cpx     #0
        bne     DO1B
        sta     DH1
        inc     DFILL
        rts
DO1B:   sta     DH2
        inc     DFILL
        rts
DO2:    ldx     DH1             ; the byte leaving the line
        ldy     DH2
        sty     DH1
        sta     DH2
        txa
; fall into PBYTE

; PBYTE -- A = a payload byte (the type first): Fletcher, then the parser.
PBYTE:  sta     MTMP
        clc
        adc     DS1
        bcs     PB1S
        cmp     #255
        bcc     PB1A
PB1S:   sbc     #255
PB1A:   sta     DS1
        clc
        adc     DS2
        bcs     PB2S
        cmp     #255
        bcc     PB2A
PB2S:   sbc     #255
PB2A:   sta     DS2
        lda     MTMP
        ldx     DST
        cpx     #DS_Q_DATA
        bne     PP1
        jmp     QDATA
PP1:    cpx     #DS_TYPE
        bne     PP2
        jmp     GOTTYPE
PP2:    cpx     #DS_Q_OFF
        bne     PP3
        sta     DOFF
        lda     #DS_Q_LEN
        sta     DST
        rts
PP3:    cpx     #DS_Q_LEN
        bne     PP4
        sta     DLEN
        lda     #$FF
        sta     DMROW           ; a new run: the map position is unknown
        lda     DLEN
        beq     PP3A            ; an empty run
        lda     #DS_Q_DATA
        sta     DST
        rts
PP3A:   lda     #DS_Q_OFF
        sta     DST
        rts
PP4:    cpx     #DS_T_IDX
        bne     PP5
        sta     DOFF            ; a table record: its index,
        lda     #DS_T_CNT
        sta     DST
        rts
PP5:    cpx     #DS_T_CNT
        bne     PP6
        ldx     MSCR            ; ...how many there are,
        cpx     #SC_ROOMS
        bne     PP5A
        sta     MSHOW
PP5A:   lda     #0
        sta     DLEN
        lda     #DS_T_REC
        sta     DST
        rts
PP6:    cpx     #DS_T_REC
        bne     PP7
        jmp     TREC            ; ...and the record itself
PP7:    rts                     ; C, E bodies: ignored here

; TREC -- a byte of a table record (id[8], name[12], seated, max, phase,
; rounds), A = the byte, DLEN its index, DOFF the table's index. The picked
; table's id is kept (TABLE); every name is drawn in its row.
TREC:   sta     MTMP2
        ldx     DLEN
        inc     DLEN
        cpx     #8
        bcs     TR1
        lda     DOFF            ; the id of the table the cursor is on
        cmp     MSEL
        bne     TR9
        lda     MSCR
        cmp     #SC_ROOMS
        bne     TR9
        lda     MTMP2
        sta     TABLE,x
TR9:    rts
TR1:    cpx     #20
        bcs     TR9
        lda     MSCR
        cmp     #SC_ROOMS
        bne     TR9
        txa                     ; col = x - 8; row 3 + index
        sec
        sbc     #8
        sta     KT0
        lda     DOFF
        clc
        adc     #3
        tax
        lda     TIMES12,x
        clc
        adc     KT0
        sta     CBD
        lda     #0
        sta     CBD+1
        sta     CBC
        lda     MTMP2
        bne     TR2
        lda     #' '
TR2:    ldx     #0
        ldy     #FB_TCELL
        jmp     FBLIT

GOTTYPE:
        sta     DTYPE
        ldx     #DS_SKIP
        cmp     #'Q'
        bne     GT1
        ldx     #DS_Q_OFF
GT1:    cmp     #'T'
        bne     GT2
        ldx     #DS_T_IDX
GT2:    stx     DST
        rts

; QDATA -- A = a patch byte for state offset DOFF.
QDATA:  sta     MTMP
        ldy     DOFF
        cpy     #88
        bcs     QD1
        jsr     APPLY
        jmp     QD9
QD1:    cpy     #231
        bcs     QD9             ; the movers: not kept
        lda     MSCR            ; only the arena has a map: on a text screen the
        cmp     #SC_ARENA       ;   planes' map bytes are text rows
        bne     QD9
        lda     MTMP
        jsr     APMAP
QD9:    inc     DOFF
        dec     DLEN
        bne     QDR
        jsr     FLUSHSEG        ; the run is over
        lda     #$FF
        sta     DMROW
        lda     #DS_Q_OFF
        sta     DST
QDR:    rts

; APPLAYER -- X = SP_X/SP_Y/SP_DIR + seat, MTMP2 = the byte. My own seat's x
; and y go to SRVX/SRVY (the walker owns my position; a warp snaps it there)
; and its direction is ignored.
APPLAYER:
        stx     KT0
        txa
        and     #3
        tax                     ; the seat
        lda     KT0
        and     #$0C
        beq     APL_X
        cmp     #$04
        beq     APL_Y
        lda     MTMP2           ; (dir)
        cpx     MYSEAT
        beq     APLRET
        sta     BDIR,x
APLRET: rts
APL_X:  lda     MTMP2
        cpx     MYSEAT
        bne     APLX1
        sta     SRVX
        rts
APLX1:  sta     BX,x
        rts
APL_Y:  lda     MTMP2
        cpx     MYSEAT
        bne     APLY1
        sta     SRVY
        rts
APLY1:  sta     BY,x
        rts

; APWARP -- X = SP_WARP + seat: for MY seat a changed warp snaps the walker to
; the server's position.
APWARP: txa
        sec
        sbc     #SP_WARP
        cmp     MYSEAT
        bne     APWRET
        tax
        lda     MTMP2
        cmp     LWARP
        beq     APWRET
        sta     LWARP
        lda     SRVX
        sta     BX,x
        lda     SRVY
        sta     BY,x
APWRET: rts

; APWINS -- X = SP_WINS + seat: the HUD's trophy digit (the arena: row 0, col
; 5 + 2*seat) or the roster's (row 5 + 2*seat, col 11).
APWINS: txa
        sec
        sbc     #SP_WINS
        tax
        lda     MSCR
        cmp     #SC_ARENA
        bne     AW1
        txa
        asl     a
        clc
        adc     #5
        jmp     AW9
AW1:    cmp     #SC_WAIT
        bcc     AWRET
        cmp     #SC_OVER+1
        bcs     AWRET
        txa                     ; (5 + 2*seat)*12 + 11
        asl     a
        clc
        adc     #5
        tax
        lda     TIMES12,x
        clc
        adc     #11
AW9:    sta     CBD
        lda     #0
        sta     CBD+1
        sta     CBC
        lda     MTMP2
        clc
        adc     #'0'
        ldx     #0
        ldy     #FB_TCELL
        jmp     FBLIT
AWRET:  rts

; APTIMER -- the timer's three digits: the arena's HUD (row 0, cols 0-2) or the
; round-end screens' countdown (row 15, cols 3-5).
APTIMER:
        lda     MSCR
        cmp     #SC_ARENA
        beq     AT0
        cmp     #SC_ROUND
        beq     AT0
        cmp     #SC_OVER
        beq     AT0
        rts
AT0:    lda     MSCR
        cmp     #SC_ARENA
        bne     AT00
        lda     PHASE
        cmp     #PH_COUNT
        bne     AT00
        lda     #SND_BEEP       ; the countdown ticks
        sta     SND
AT00:   lda     MTMP2
        ldx     #'0'-1
AT1:    inx
        sec
        sbc     #100
        bcs     AT1
        adc     #100
        sta     KT1
        txa
        ldx     #0
        jsr     HUDCH
        ldx     #'0'-1
        lda     KT1
AT2:    inx
        sec
        sbc     #10
        bcs     AT2
        adc     #10
        clc
        adc     #'0'
        sta     KT1
        txa
        ldx     #1
        jsr     HUDCH
        lda     KT1
        ldx     #2
; HUDCH -- A = character, X = 0-2: a digit of the timer.
HUDCH:  sta     KT2
        lda     MSCR
        cmp     #SC_ARENA
        beq     HC1
        txa
        clc
        adc     #15*12+3
        tax
HC1:    stx     CBD
        ldx     #0
        stx     CBD+1
        stx     CBC
        lda     KT2
        ldy     #FB_TCELL
        jmp     FBLIT

; APNAME -- Y = the offset (56..87), MTMP2 = the byte: this seat's name byte
; against mine, case blind; MISM[seat] bit i = they differ.
APNAME: tya
        sec
        sbc     #56
        sta     KT0
        lda     MSCR            ; on a roster screen the name is also drawn
        cmp     #SC_WAIT
        bcc     APN0
        cmp     #SC_OVER+1
        bcs     APN0
        lda     KT0             ; row 5 + 2*seat, col 2 + i
        lsr     a
        lsr     a
        lsr     a
        asl     a
        clc
        adc     #5
        tax
        lda     TIMES12,x
        sta     CBD
        lda     KT0
        and     #7
        clc
        adc     CBD
        adc     #2
        sta     CBD
        lda     #0
        sta     CBD+1
        sta     CBC
        lda     MTMP2
        bne     APN00
        lda     #' '
APN00:  ldx     #0
        ldy     #FB_TCELL
        jsr     FBLIT
APN0:   lda     KT0
        and     #7
        tax                     ; i
        lda     KT0
        lsr     a
        lsr     a
        lsr     a
        tay                     ; seat
        lda     MTMP2
        ora     #$20
        sta     KT1
        lda     NAME,x
        ora     #$20
        cmp     KT1
        beq     APN1
        lda     BITS8,x
        ora     MISM,y
        sta     MISM,y
        rts
APN1:   lda     BITS8,x
        eor     #$FF
        and     MISM,y
        sta     MISM,y
        rts

; APPLY's special codes arrive with A = the byte: keep it in MTMP2.
APPLY:  sta     MTMP2
        ldx     SDEST,y
        beq     APRET
        bmi     APZP
        cpx     #SP_ALIVE
        bne     APX1
        jmp     APALIVE
APX1:
        cpx     #SP_MINE
        bcs     APMINE
        cpx     #SP_NAME
        bne     AP0
        jmp     APNAME
AP0:    cpx     #SP_X
        bcc     AP2
        jmp     APPLAYER
AP2:    cpx     #SP_WARP
        bcs     AP3
        cpx     #SP_WINS
        bcs     AP4
        jmp     APTIMER
AP3:    jmp     APWARP
AP4:    jmp     APWINS
APZP:   sta     $00,x
APRET:  rts

; APALIVE -- the alive mask: a seat that goes from alive to dead is a death.
APALIVE: lda    MTMP2
        eor     #$FF
        and     ALIVE
        beq     APAL1
        ldx     PHASE
        cpx     #PH_PLAY
        bne     APAL1
        lda     #SND_DIE
        sta     SND
APAL1:  lda     MTMP2
        sta     ALIVE
        rts

; APMINE -- X = SP_MINE + 4*field + seat: only my own seat's flags, bombs,
; speed and out are kept (MFLG, MBOMB, MSPD, MOUT, contiguous).
APMINE: txa
        sec
        sbc     #SP_MINE
        tax
        and     #3
        cmp     MYSEAT
        bne     APMNR
        txa
        lsr     a
        lsr     a
        tax
        lda     MTMP2
        cmp     MFLG,x
        beq     APMNR
        sta     MFLG,x
        lda     PHASE
        cmp     #PH_PLAY
        bne     APMNR
        lda     #SND_PICK       ; (a power-up taken)
        sta     SND
APMNR:   rts

; APMAP -- A = a map byte at state offset Y (88..230).
APMAP:  sta     KT0
        lda     DMROW
        cmp     #$FF
        bne     APM2
        tya                     ; a run starts here: its row and column
        sec
        sbc     #88
        ldx     #0
APM1:   cmp     #COLS
        bcc     APM1D
        sbc     #COLS
        inx
        bne     APM1
APM1D:  sta     DMCOL
        sta     DMSEG
        stx     DMROW
APM2:   lda     KT0
        cmp     #T_FLAME_C
        bne     APM3
        lda     SND             ; an explosion (unless something is sounding)
        bne     APM3
        lda     #SND_BOOM
        sta     SND
APM3:   lda     KT0
        sta     FNRSEL+FH_PATHC
        inc     DMCOL
        lda     DMCOL
        cmp     #COLS
        bne     APMR
        jsr     FLUSHSEG        ; the row is full: land it, start the next
        inc     DMROW
        lda     #0
        sta     DMCOL
        sta     DMSEG
APMR:   rts

; FLUSHSEG -- land the buffered map segment (DMSEG..DMCOL-1 of row DMROW):
; one PATHPOKE into the row's bytes, the row dirty.
FLUSHSEG:
        lda     DMROW
        cmp     #$FF
        beq     FSR
        lda     DMCOL
        sec
        sbc     DMSEG
        beq     FSR
        sta     CBC
        ldx     DMROW
        lda     MROFFL,x
        clc
        adc     DMSEG
        sta     CBD
        lda     MROFFH,x
        adc     #0
        sta     CBD+1
        lda     #0
        tax
        ldy     #FB_PPOKE
        jsr     FBLIT
        ldx     DMROW
        cpx     #8
        bcs     FS1
        lda     BITS8,x
        ora     DIRTYL
        sta     DIRTYL
        jmp     FS2
FS1:    lda     BITS8-8,x
        ora     DIRTYH
        sta     DIRTYH
FS2:    lda     DMCOL
        sta     DMSEG
        lda     #FP_RST
        sta     FNRSEL+FH_PATHO
FSR:    rts

; COMMIT -- a state is whole: who am I?
COMMIT: lda     LFLAGS
        ora     #LF_GOT
        sta     LFLAGS
        lda     MYSEAT
        pha
        lda     #$FF
        sta     MYSEAT
        ldx     #3
CM1:    lda     MISM,x
        bne     CM2
        lda     SEATED
        and     BITS8,x
        beq     CM2
        stx     MYSEAT
CM2:    dex
        bpl     CM1
        pla
        bpl     CM3             ; I was known already
        lda     MYSEAT
        bmi     CM3
        lda     LFLAGS          ; just found my seat: the state's fields for it
        ora     #LF_RESYNC      ;   went by unkept; ask for all of it again
        sta     LFLAGS
CM3:    rts

        INCLUDE "blit.inc"

DSTART:
TIMES12: DB     0,12,24,36,48,60,72,84,96,108,120,132,144
        INCLUDE "../../build/a26/netdata.inc"
        END

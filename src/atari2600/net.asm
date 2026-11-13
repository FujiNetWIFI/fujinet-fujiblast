; net.asm -- bank 3: the link to fujiblastd and the frame decoder.
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

RDMAX   EQU     192             ; a READ takes at most this
UMINB   EQU     12              ; ticks the decode leaves in the overscan
TXMOVE  EQU     3               ; M every (TXMOVE+1) frames
TXPING  EQU     150             ; keepalive: 2-frame units (300 frames)
CREDIT  EQU     48
LTMOMAX EQU     240

WT_H    EQU     'H'
WT_M    EQU     'M'
WT_A    EQU     'A'
WT_R    EQU     'R'
WT_T    EQU     'T'
WT_C    EQU     'C'
WT_P    EQU     'P'
WT_B    EQU     'B'
PROTOV  EQU     1

        ORG     $1000
NENTRY: jsr     LINK
        lda     #ENWORK         ; on to the decode bank, which hands back
        sta     MENT
        lda     #BKDEC
        jmp     MGOTO

NEXIT:  lda     MRET            ; back to the display bank: low nibble = the
        and     #$0F            ;   entry code, high nibble = its bank
        sta     MENT
        lda     MRET
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        jmp     MGOTO

; ---------------------------------------------------------------------------
; LINK -- one hop.
LINK:   lda     LREQ
        cmp     #$FF            ; held: no name or room chosen yet
        bne     LK00
        rts
LK00:   lda     LFLAGS
        and     #LF_LOST
        beq     LK0
        rts
LK0:    lda     LREQ
        bne     LKFLY
        jmp     LKIDLE
; ---- a transaction is out ----
LKFLY:  lda     FNACKS
        cmp     FNSEQ
        beq     LKDONE
        inc     LTMO
        lda     LTMO
        cmp     #LTMOMAX
        bcc     LKRET
LKFAIL: lda     LFLAGS
        ora     #LF_LOST
        sta     LFLAGS
        lda     #LQ_NONE
        sta     LREQ
LKRET:  rts
LKDONE: lda     FNERR
        bne     LKFAIL
        jsr     FNACK
        bne     LKFAIL
        ldx     LREQ
        lda     #LQ_NONE
        sta     LREQ
        cpx     #LQ_OPEN
        bne     LKD1
        lda     LFLAGS          ; connected: say HELLO (or, choosing a room, ask
        ora     #LF_CONN        ;   for the list of them)
        ldx     MSCR
        cpx     #SC_ROOMS
        bne     LKD0A
        ora     #LF_LIST
        jmp     LKD0B
LKD0A:  ora     #LF_HELLO
LKD0B:  sta     LFLAGS
        jmp     LKIDLE
LKD1:   cpx     #LQ_STAT
        bne     LKD2
        lda     FNRPLY+NSDEVST  ; 1 = SUCCESS: anything else the socket is gone
        cmp     #1
        bne     LKFAIL
        lda     FNRPLY+NSAVHI
        bne     LKD1A
        lda     FNRPLY+NSAVLO
        beq     LKD1Z
        cmp     #RDMAX
        bcc     LKD1B
LKD1A:  lda     #RDMAX
LKD1B:  sta     KT0             ; READ this many
        jmp     POSTREAD
LKD1Z:  lda     #2              ; nothing waiting: look again in 2 frames
        sta     LPOLL
        jmp     LKIDLE
LKD2:   cpx     #LQ_READ
        bne     LKIDLE          ; a WRITE: done
        lda     FNRXLO          ; the bytes the READ returned
        cmp     #RDMAX+1
        bcc     LKD2A
        lda     #RDMAX
LKD2A:  sta     RLEFT
        lda     #0
        sta     RPOS
        lda     #1              ; ask again soon: there may be more
        sta     LPOLL
        rts

; ---- idle: decode what is left, else post the next transaction ----
LKIDLE: lda     RLEFT           ; a reply is still being decoded (the decode bank,
        beq     LKPOST          ;   next hop): post nothing until it is whole
        rts
LKPOST: inc     LPING           ; the keepalive clock: 2-frame units
        lda     MFRAME
        lsr     a
        bcc     LKP0
        dec     LPING
LKP0:   lda     LFLAGS
        and     #LF_CONN
        bne     LKP1
        jmp     POSTOPEN        ; (first time only)
LKP1:   jsr     WANTW           ; anything to say? Z clear: yes
        beq     LKP2
        jmp     POSTWRITE
LKP2:   lda     LPOLL
        beq     LKP3
        dec     LPOLL
        rts
LKP3:   jmp     POSTSTAT

; ---------------------------------------------------------------------------
; POSTOPEN -- N: OPEN "N:TCP://host:port/" (the host is in path buffer 1).
POSTOPEN:
        lda     #NETDEV
        sta     FNDEV
        lda     #NCOPEN
        sta     FNCMD
        lda     #2
        sta     FNNPR
        jsr     FNBEG
        lda     #NMRDWR
        jsr     FNPB
        lda     #NTRNONE
        jsr     FNPB
        ldx     #0
PO1:    lda     OPENPFX,x
        beq     PO2
        sta     FNTX
        inx
        bne     PO1
PO2:    lda     #PB_HOST
        sta     FNRSEL+FH_PATHO
        lda     #FP_TXRAW
        sta     FNRSEL+FH_PATHO
        lda     #LQ_OPEN
        jmp     LAUNCH

OPENPFX: DB     "N:TCP://",0

; LAUNCH -- A = the request kind: commit the transaction set up in the TX.
LAUNCH: sta     LREQ
        lda     #0
        sta     LTMO
        jmp     FNLNCH

; POSTSTAT -- N: STATUS.
POSTSTAT:
        lda     #NETDEV
        sta     FNDEV
        lda     #NCSTAT
        sta     FNCMD
        lda     #2
        sta     FNNPR
        jsr     FNBEG
        lda     #0
        jsr     FNPB
        lda     #0
        jsr     FNPB
        lda     #LQ_STAT
        jmp     LAUNCH

; POSTREAD -- N: READ KT0 bytes.
POSTREAD:
        lda     #NETDEV
        sta     FNDEV
        lda     #NCREAD
        sta     FNCMD
        lda     #1
        sta     FNNPR
        jsr     FNBEG
        lda     KT0
        ldx     #0
        jsr     FNPW
        lda     #LQ_READ
        jmp     LAUNCH

; ---------------------------------------------------------------------------
; WANTW -- Z clear if there is a frame to send. Sets the per-frame choices in
; KT3 (a bit mask, WM_*) and the encoded size in KT2.
WM_H    EQU     $01
WM_R    EQU     $02
WM_T    EQU     $04
WM_B    EQU     $08
WM_C    EQU     $10
WM_A    EQU     $20
WM_M    EQU     $40
WM_P    EQU     $80

WANTW:  lda     #0
        sta     KT3
        sta     KT2
        lda     LFLAGS          ; connected but no state yet: the HELLO may have
        and     #LF_GOT|LF_LOST ;   been lost, so say it again every 64 frames
        bne     WW0
        lda     LPING           ; (reset by every WRITE, counts 2-frame units)
        cmp     #32
        bcc     WW0
        lda     LFLAGS
        ora     #LF_HELLO
        sta     LFLAGS
WW0:
        lda     LFLAGS
        and     #LF_HELLO
        beq     WW1
        lda     #WM_H
        jsr     WADD
        lda     #23
        jsr     WSIZE
WW1:    lda     LFLAGS
        and     #LF_RESYNC
        beq     WW2
        lda     #WM_R
        jsr     WADD
        lda     #5
        jsr     WSIZE
WW2:    lda     LFLAGS
        and     #LF_LIST
        beq     WW3
        lda     #WM_T
        jsr     WADD
        lda     #6
        jsr     WSIZE
WW3:    lda     LFLAGS
        and     #LF_BYE
        beq     WW4
        lda     #WM_B
        jsr     WADD
        lda     #5
        jsr     WSIZE
WW4:    lda     LCRED
        sec
        sbc     LCRSNT
        cmp     #CREDIT
        bcc     WW5
        lda     #WM_C
        jsr     WADD
        lda     #6
        jsr     WSIZE
WW5:    lda     LACT
        beq     WW6
        lda     #WM_A
        jsr     WADD
        lda     #9
        jsr     WSIZE
WW6:    lda     LFLAGS          ; M: while in the game, every few frames
        and     #LF_PLAY
        beq     WW7
        lda     MFRAME
        and     #TXMOVE
        bne     WW7
        lda     MYSEAT
        bmi     WW7
        lda     #WM_M
        jsr     WADD
        lda     #9
        jsr     WSIZE
WW7:    lda     KT3             ; a keepalive if nothing else is going and
        bne     WW8             ;   the link has been quiet
        lda     LPING
        cmp     #TXPING
        bcc     WW8
        lda     #WM_P
        jsr     WADD
        lda     #5
        jsr     WSIZE
WW8:    lda     KT3
        rts
WADD:   ora     KT3
        sta     KT3
        rts
WSIZE:  clc
        adc     KT2
        sta     KT2
        rts

; POSTWRITE -- N: WRITE the frames WANTW chose (KT3), total KT2 bytes.
POSTWRITE:
        lda     #NETDEV
        sta     FNDEV
        lda     #NCWRITE
        sta     FNCMD
        lda     #1
        sta     FNNPR
        jsr     FNBEG
        lda     KT2
        ldx     #0
        jsr     FNPW
; the frames: each is built in RAW (over FNDEV.. which FNBEG is done with) and
; encoded into the TX. KT3 is the mask; it lives across the encodes in KT3.
        lda     KT3
        and     #WM_H
        beq     PW1
        ldx     #0              ; 'H', version, flags, table[8], name[8]
        lda     #WT_H
        sta     RAW
        lda     #PROTOV
        sta     RAW+1
        lda     #0
        sta     RAW+2
PWH1:   lda     TABLE,x
        sta     RAW+3,x
        lda     NAME,x
        sta     RAW+11,x
        inx
        cpx     #8
        bne     PWH1
        ldx     #19
        jsr     ENCODE
        lda     LFLAGS
        and     #(~LF_HELLO)&$FF
        sta     LFLAGS
PW1:    lda     KT3
        and     #WM_R
        beq     PW2
        lda     #WT_R
        sta     RAW
        ldx     #1
        jsr     ENCODE
        lda     LFLAGS
        and     #(~LF_RESYNC)&$FF
        sta     LFLAGS
PW2:    lda     KT3
        and     #WM_T
        beq     PW3
        lda     #WT_T
        sta     RAW
        lda     #1
        sta     RAW+1
        ldx     #2
        jsr     ENCODE
        lda     LFLAGS
        and     #(~LF_LIST)&$FF
        sta     LFLAGS
PW3:    lda     KT3
        and     #WM_B
        beq     PW4
        lda     #WT_B
        sta     RAW
        ldx     #1
        jsr     ENCODE
        lda     LFLAGS
        and     #(~LF_BYE)&$FF
        sta     LFLAGS
PW4:    lda     KT3
        and     #WM_C
        beq     PW5
        lda     #WT_C
        sta     RAW
        lda     LCRED
        sta     RAW+1
        sta     LCRSNT
        ldx     #2
        jsr     ENCODE
PW5:    lda     KT3
        and     #WM_A
        beq     PW6
        lda     #WT_A
        sta     RAW
        lda     MFRAME          ; the seq: never 0
        ora     #1
        sta     RAW+1
        lda     LACT
        sta     RAW+2
        lda     LA1
        sta     RAW+3
        lda     LA2
        sta     RAW+4
        ldx     #5
        jsr     ENCODE
        lda     #0
        sta     LACT
PW6:    lda     KT3
        and     #WM_M
        beq     PW7
        lda     #WT_M
        sta     RAW
        ldx     MYSEAT
        lda     BX,x
        sta     RAW+1
        lda     BY,x
        sta     RAW+2
        lda     BDIR,x
        sta     RAW+3
        lda     LWARP
        sta     RAW+4
        ldx     #5
        jsr     ENCODE
PW7:    lda     KT3
        and     #WM_P
        beq     PW8
        lda     #WT_P
        sta     RAW
        ldx     #1
        jsr     ENCODE
        lda     #0
        sta     LPING
PW8:    lda     #0              ; anything sent counts as being heard
        sta     LPING
        lda     #LQ_WRITE
        jmp     LAUNCH

; ---------------------------------------------------------------------------
; ENCODE -- the frame in RAW[0..X-1]: append the Fletcher sums, COBS-encode,
; terminate with a zero, into the TX. (KT3 survives: the scratch used is
; KT0-KT2, and X, Y.)
ENCODE: stx     KT0             ; n
        lda     #0
        sta     KT1             ; s1
        sta     KT2             ; s2
        ldy     #0
EN1:    lda     RAW,y
        clc
        adc     KT1
        bcs     EN1S
        cmp     #255
        bcc     EN1A
EN1S:   sbc     #255            ; carry is set on both paths here
EN1A:   sta     KT1
        clc
        adc     KT2
        bcs     EN2S
        cmp     #255
        bcc     EN2A
EN2S:   sbc     #255
EN2A:   sta     KT2
        iny
        cpy     KT0
        bne     EN1
        lda     KT1
        sta     RAW,y
        iny
        lda     KT2
        sta     RAW,y
        iny
        sty     KT0             ; m = n + 2: the encoded length
; COBS: blocks of non-zero bytes, each led by its length + 1
        ldy     #0              ; pos
EC1:    sty     KT1             ; the block's start
        ldx     #0              ; its length
EC2:    cpy     KT0
        beq     EC3             ; end of data
        lda     RAW,y
        beq     EC3             ; a zero ends the block
        iny
        inx
        bne     EC2
EC3:    stx     KT2
        inx
        stx     FNTX            ; the code
        sty     MTMP            ; the block's end
        ldx     KT1
EC4:    cpx     MTMP
        beq     EC5
        lda     RAW,x
        sta     FNTX
        inx
        bne     EC4
EC5:    ldy     MTMP
        cpy     KT0
        beq     ECEND           ; the data ended here, not on a zero
        iny                     ; step over the zero
        jmp     EC1
ECEND:  lda     #0
        sta     FNTX
        rts

        END

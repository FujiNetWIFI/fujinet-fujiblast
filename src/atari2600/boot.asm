; boot.asm -- bank 0: cold start. Checks the cartridge, empties the path
; buffers, draws the HUD text and (for this milestone) loads a canned arena:
; the map into the planes through a path buffer, the bombers into RAM.
        CPU     6502
        INCLUDE "vcs.inc"
        INCLUDE "fujinet.inc"
        INCLUDE "fbdefs.inc"
        INCLUDE "../../build/a26/tail.inc"

        ORG     $1000
COLD:   sei
        cld
        ldx     #$FF
        txs
        lda     #0
        ldx     #0
CLR:    sta     $80,x
        inx
        cpx     #$76            ; $80-$F5; the stack ($F6-$FF) is left alone
        bne     CLR
        sta     VSYNC
        sta     VBLANK
        jsr     FNARM
        jsr     FNCHK
        beq     COLD1
NOCART: lda     #$44            ; no cartridge answering: a red screen
        sta     COLUBK
        jmp     NOCART
; the socket survives a console RESET too: close it (a failure is fine: none open)
        lda     #NETDEV
        sta     FNDEV
        lda     #NCCLOSE
        sta     FNCMD
        lda     #0
        sta     FNNPR
        jsr     FNBEG
        jsr     FNGO
; the path buffers survive a console RESET: empty all four, leave 1 selected
COLD1:  ldx     #FP_SEL0
COLD2:  txa
        sta     FNRSEL+FH_PATHO
        lda     #FP_RST
        sta     FNRSEL+FH_PATHO
        inx
        cpx     #FP_SEL3+1
        bne     COLD2
        lda     #FP_SEL1
        sta     FNRSEL+FH_PATHO

; the HUD (text row 0) and the status row (20)
        lda     #0
        jsr     FNROWA
        ldx     #0
H1:     lda     HUDS,x
        beq     H2
        jsr     FNCHR
        inx
        bne     H1
H2:     lda     #0
        sta     FNRSEL+FH_TEND
        lda     #20
        jsr     FNROWA
        ldx     #0
H3:     lda     STAS,x
        beq     H4
        jsr     FNCHR
        inx
        bne     H3
H4:     lda     #0
        sta     FNRSEL+FH_TEND
        lda     #$0E
        sta     HTXT

; identity: the player's name (appkey 0) and the room (appkey 0x10)
        lda     #AKNAMEK
        jsr     AKNAME
ID1:    lda     #AKROOMK
        jsr     AKURL
        lda     FNPLNL          ; PB_HOST is selected: did we get a host?
        bne     ID2
        lda     #PB_HOST
        sta     FNRSEL+FH_PATHO
        ldx     #0
ID1A:   lda     DEFHOST,x
        beq     ID1B
        sta     FNRSEL+FH_PATHC
        inx
        bne     ID1A
ID1B:
ID2:
ID3:    lda     #0              ; MISM: a name byte not yet sent is a zero
        ldx     #7
ID3A:   ldy     NAME,x
        beq     ID3B
        ora     BITS,x
ID3B:   dex
        bpl     ID3A
        ldx     #3
ID3C:   sta     MISM,x
        dex
        bpl     ID3C
        lda     #$FF
        sta     MYSEAT
        sta     CROW
        sta     DMROW
        lda     #PB_STREAM
        sta     FNRSEL+FH_PATHO
        lda     #FP_RST
        sta     FNRSEL+FH_PATHO

        lda     #$FF            ; every row is dirty until the map arrives
        sta     DIRTYL
        lda     #$07
        sta     DIRTYH
        lda     NAME            ; no name: the name wheel; no room: the room list
        bne     BN1
        lda     #SC_NAME
        sta     MSCR
        lda     #$FF            ; (the link waits until a name is picked)
        sta     LREQ
        jmp     BN3
BN1:    lda     TABLE
        bne     BN2
        lda     #SC_ROOMS
        jmp     BN4
BN2:    lda     #SC_CONN
BN4:    sta     MSCR
BN3:    lda     #ENENTER
        sta     MENT
        lda     #BKSCR
        jmp     MGOTO

        INCLUDE "blit.inc"

        INCLUDE "appkey.inc"

DSTART:
BITS:   DB      1,2,4,8,16,32,64,128
HUDS:   DB      "000  0 0 0 0",0
STAS:   DB      "FUJIBLAST",0
        INCLUDE "../../build/a26/endpoint.inc"
        END

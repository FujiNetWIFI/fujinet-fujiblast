; fixb.inc -- fixed B, $2000-$28FF (2650 page 1, with the mailbox and RAM-H):
; the same in every page's assembly and always mapped.
;
; What needs the mailbox close at hand: its transactions, cart RAM's
; writes, the tick; and the tables fixed A reads through pointer words
; (sound, keys, the build's server). Page-1 code reaches console RAM
; through the pointer words at the end.

; ---- start-up ---------------------------------------------------------------------
; STBAD: no FujiNet cart, or one too old for APPBANK images: say so, stop.
STBAD:  DAT     5,0
        TXTAT   SNEEDFW
        DAT     7,0
        TXTAT   SNEEDF2
STHALT: BCTR,UN STHALT
SNEEDFW: DB     "NEEDS A FUJINET",$FF
SNEEDF2: DB     "CART, PROTO 2",$FF

; ---- cart RAM ---------------------------------------------------------------------
; RAMPUT: cart RAM = R0 at pointer register R1 (FRRAMP0-3) + R3 (see CPUT).
; Leaf. Clobbers R0, R2.
RAMPUT: STRZ    R2
        LODA,R0 FNRSEL,R1       ; the pointer
        LODA,R0 FNRDAT,R3
        LODA,R0 FNRSEL+FRRAMPK  ; one byte
        LODA,R0 FNRDAT,R2
        LODA,R0 FNRSEL+FRRAMP0  ; closed: a stray REGDATA can't store
        RETC,UN
; RAMSET: the pointer at R1 + R3, a burst open (CPTR). RAMPUT1: R0 there,
; the pointer on. RAMPEND: the burst closed. Leaves. Clobber R0 (RAMPUT1 R2).
RAMSET: LODA,R0 FNRSEL,R1
        LODA,R0 FNRDAT,R3
        LODA,R0 FNRSEL+FRRAMPK
        RETC,UN
RAMPUT1: STRZ   R2
        LODA,R0 FNRDAT,R2
        RETC,UN
RAMPEND: LODA,R0 FNRSEL+FRRAMP0
        RETC,UN

; ---- the mailbox ------------------------------------------------------------------------
; MSTARTN: an N: transaction: TX rewound, command R3, R2 parameters.
; Leaf. Clobbers R0.
MSTARTN: LODA,R0 FNRSEL+FRDATRS
        LODA,R0 FNRDAT
        LODA,R0 FNRSEL+FRDEVIC
        LODA,R0 FNRDAT+NETDEV
MST1:   LODA,R0 FNRSEL+FRCMD
        LODA,R0 FNRDAT,R3
        LODA,R0 FNRSEL+FRNPARM
        LODA,R0 FNRDAT,R2
        RETC,UN
; MSTARTF: the same on the Fuji device.
MSTARTF: LODA,R0 FNRSEL+FRDATRS
        LODA,R0 FNRDAT
        LODA,R0 FNRSEL+FRDEVIC
        LODA,R0 FNRDAT+FNDFUJI
        BCTR,UN MST1
; MPARB: a 1-byte parameter, value R1. MPARW: a 2-byte one, R1 low, R2
; high. Leaves. Clobber R0.
MPARB:  LODA,R0 FNDATA+1
        LODA,R0 FNDATA,R1
        RETC,UN
MPARW:  LODA,R0 FNDATA+2
        LODA,R0 FNDATA,R1
        LODA,R0 FNDATA,R2
        RETC,UN
; MPOST: SEQ = ACKSEQ + 1 (255 wraps to 1): the transaction goes. The
; SEQ, which ACKSEQ echoes when it is done, is kept. Leaf. Clobbers R0, R2.
MPOST:  LODA,R0 FNACKSQ
        ADDI,R0 1
        BCFR,EQ MPO1
        LODI,R0 1
MPO1:   STRZ    R2
        LODA,R0 FNRSEL+FRSEQ
        LODA,R0 FNRDAT,R2
        LODZ    R2
        STRA,R0 *PNSEQ
        RETC,UN
; MDONE: CC EQ when the posted transaction has answered. Leaf. Clobbers R0.
MDONE:  LODA,R0 FNACKSQ
        COMA,R0 *PNSEQ
        RETC,UN
; MACK: CC EQ when it worked and was ACKed. Leaf. Clobbers R0.
MACK:   LODA,R0 FNERR
        RETC,GT                 ; (not EQ)
        RETC,LT
        LODA,R0 FNREPLY
        COMI,R0 FCACK
        RETC,UN
; MIDLE: the link's transaction, if one is out, answered first (a new one
; is never posted over it): up to 4 s of frames. Calls two down. Clobbers
; R0-R3.
MIDLE:  LODA,R0 *PNKIND
        RETC,EQ
        LODI,R0 240
        STRA,R0 *PMGOL
MID1:   BSTR,UN MDONE
        BCTR,EQ MID2
        BSTA,UN VBSYNC
        LODA,R0 *PMGOL
        SUBI,R0 1
        STRA,R0 *PMGOL
        BCFR,EQ MID1
MID2:   EORZ    R0
        STRA,R0 *PNKIND
        RETC,UN
; MGO: post, then wait for the answer a frame at a time (the sound and the
; sprites running), about 21 s at most (an OPEN may dial for seconds; the
; same SEQ is polled all along). CC EQ when it was ACKed. Calls two down.
; Clobbers R0-R3.
MGO:    BSTA,UN MPOST
        LODI,R0 5
        STRA,R0 *PMGOH
MGO1:   EORZ    R0
        STRA,R0 *PMGOL
MGO2:   BSTR,UN MDONE
        BCTR,EQ MGO4
        BSTA,UN VBSYNC
        LODA,R0 *PMGOL
        SUBI,R0 1
        STRA,R0 *PMGOL
        BCFR,EQ MGO2
        LODA,R0 *PMGOH
        SUBI,R0 1
        STRA,R0 *PMGOH
        BCFR,EQ MGO1
        LODI,R0 1               ; never came: (not EQ)
        RETC,UN
MGO4:   BCTA,UN MACK            ; tail call

; ---- the clock ----------------------------------------------------------------
; TICKRD: DT = the cart's ticks since the last loop (1-8), added to our
; frame clock. The tick and the vblank are not locked together, so a frame
; can see 0 or 2: 0 counts as 1. Leaf. Clobbers R0-R2.
TICKRD: LODA,R0 FNTICKH         ; HI, LO, HI: the bytes change together
        STRZ    R1
        LODA,R2 FNTICKL
        LODA,R0 FNTICKH
        COMZ    R1
        BCFR,EQ TICKRD
        LODZ    R2
        SUBA,R0 *PTICKL
        STRA,R1 *PTICKH
        STRA,R2 *PTICKL
        BCFR,EQ TKR1
        LODI,R0 1
TKR1:   COMI,R0 8
        BCTR,LT TKR2
        LODI,R0 8
TKR2:   STRA,R0 *PDT
        ADDA,R0 *PFRML          ; (C: the carry)
        STRA,R0 *PFRML
        PPSL    PSLWC
        LODA,R0 *PFRMH
        ADDI,R0 0
        STRA,R0 *PFRMH
        CPSL    PSLWC
        RETC,UN

; ---- sprites ---------------------------------------------------------------------
; SPRCOL: sprite R1's colour R2 (0-7), 16 lines (each row twice). Leaf.
; Clobbers R0-R3.
SPRCOL: LODZ    R2
        ANDI,R0 7
        STRZ    R3
        LODZ    R1
        ANDI,R0 1
        BCFR,EQ SPC1
        LODZ    R3              ; even sprites: colour in b3-5 (b7 size: 0)
        RRL,R0
        RRL,R0
        RRL,R0
        STRZ    R3
        LODI,R0 $47             ; keep the odd sprite's bits
        BCTR,UN SPC2
SPC1:   LODI,R0 $B8             ; odd sprites: colour b0-2 (b6 size: 0)
SPC2:   STRA,R0 *PT3            ; bits to keep
        LODZ    R1
        COMI,R0 2
        BCTR,LT SPC5
        LODA,R0 *PSC23SH
        ANDA,R0 *PT3
        IORZ    R3
        STRA,R0 *PSC23SH
        STRA,R0 *PSPRC23
        RETC,UN
SPC5:   LODA,R0 *PSC01SH
        ANDA,R0 *PT3
        IORZ    R3
        STRA,R0 *PSC01SH
        STRA,R0 *PSPRC01
        RETC,UN

; ---- tables fixed A reads ---------------------------------------------------------
        INCLUDE "snd.inc"       ; SNDIDX, SNDNP, SNDNF, SNDNV (tools/arc/mksnd_arc.py)

; the lowest set bit wins, so two keys at once never read as a third
KT0:    DB      K_NONE,K_CLR,7,K_CLR,4,K_CLR,7,K_CLR
        DB      1,K_CLR,7,K_CLR,4,K_CLR,7,K_CLR
KT1:    DB      K_NONE,0,8,0,5,0,8,0
KT2:    DB      K_NONE,K_ENT,9,K_ENT,6,K_ENT,9,K_ENT
        DB      3,K_ENT,9,K_ENT,6,K_ENT,9,K_ENT
KTP:    DB      K_NONE,K_START,K_OPT,K_START,K_SEL,K_START,K_OPT,K_START

        ASCII
        INCLUDE "config.inc"    ; DEFSPEC, DEFURL: the build's server (build.sh)
        GLYPHS

; ---- pointer words: page-1 code's way into page 0 ----------------------------------------
PNSEQ:  DWBE    NSEQ
PNKIND: DWBE    NKIND
PMGOH:  DWBE    T2
PMGOL:  DWBE    T3
PTICKH: DWBE    TICKH
PTICKL: DWBE    TICKL
PFRMH:  DWBE    FRMH
PFRML:  DWBE    FRML
PDT:    DWBE    DT
PT3:    DWBE    T3
PSC01SH: DWBE   SC01SH
PSC23SH: DWBE   SC23SH
PSPRC01: DWBE   SPRC01
PSPRC23: DWBE   SPRC23

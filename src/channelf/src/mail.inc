; ---------------------------------------------------------------------------
; mail.inc -- the cart's mailbox (M.U.L.E.'s clients/channelf net.inc
; primitives). Resident.
;
; A transaction: FNBEG (device, command, the parameters' count: three
; register stores, the TX stream rewound, DC0 left on the TX page), its
; parameters and payload stored through DC0 (each ST appends), then FNPOST:
; SEQ = the cart's ACKSEQ + 1 (255 wraps to 1). The cart echoes the SEQ when
; it has answered (FNDONE); the reply is FNREPLY on, FNRXLO/HI long, FNRCMD
; the ACK or NAK. Every answer repaints the 1K window, so a READ's bytes are
; decoded before anything else is posted. The link (net.inc) never waits;
; the boot page's transactions go through FNGO, which does.
; ---------------------------------------------------------------------------

; FNBEG -- r0 = device, r1 = command, r2 = parameters.  Leaf.  Clobbers A;
;   DC0 = the TX page.
FNBEG:  DCI FNREG+FR_DEV
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        DCI FNREG+FR_DRST
        CLR
        ST
        DCI FNTX
        POP

; FNPOST -- launch it.  Leaf.  Clobbers A, DC0.
FNPOST: DCI FNACKSQ
        LM
        INC
        BNZ FNPS1
        LIS 1
FNPS1:  DCI NSEQ
        ST
        DCI FNREG+FR_SEQ
        ST
        POP

; FNDONE -- Z once the cart has echoed NSEQ.  Leaf.  Clobbers A, DC0.
FNDONE: DCI NSEQ
        LM
        DCI FNACKSQ
        CM
        POP

; FNACK -- A = 0 (Z) if the last transaction worked and was ACKed.  Leaf.
;   Clobbers A, DC0.
FNACK:  DCI FNERR
        LM
        NI 0FFH
        BNZ FNACKX
        DCI FNRCMD
        LM
        CI 006H
        BZ FNACKO
        LI 0EEH
        NI 0FFH
        POP
FNACKO: CLR
        NI 0FFH
FNACKX: POP

; FNGO -- post it and wait (the boot page, the menus): up to ~21 s, the
;   sound still ticking.  A = 0 if it was ACKed (test it: LEAVE keeps A,
;   not the flags).  Clobbers r0-r3, DC0.
FNGO:   ENTER
        PI FNPOST
        LI 0
        STB GT+30               ; frames waited (x 5: 1280)
        CLR
        STB GT+31
FNG1:   PI FNDONE
        BZ FNG2
        PI FRWAIT
        PI SNDTICK
        INCB GT+30
        BNZ FNG1
        INCB GT+31
        CI 5
        BNZ FNG1
        LIS 1                   ; never came
        LEAVE
FNG2:   PI FNACK
        LEAVE

; MIDLE -- the link's transaction, if one is out, answered first (a new one
;   is never posted over it): up to 4 s.  Clobbers A, r0-r3, DC0.
MIDLE:  ENTER
        LDB NKIND
        NI 0FFH
        BZ MID9
        LI 240
        STB GT+30
MID1:   PI FNDONE
        BZ MID9
        PI FRWAIT
        DECB GT+30
        BNZ MID1
MID9:   CLR
        STB NKIND
        LEAVE

; FNTXS -- the string at DC1 (NUL ends it) appended to the TX.  Leaf.
;   Clobbers A, DC0, DC1 (DC0 on the TX page, past it).
FNTXS:  DCI FNTX
FNTS1:  XDC
        LM
        XDC
        NI 0FFH
        BZ FNTSX
        ST
        BR FNTS1
FNTSX:  POP

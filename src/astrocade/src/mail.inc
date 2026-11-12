; mail.inc -- the mailbox's transactions (the Arcadia's fixb.inc, for
; fujilib.inc's Astrocade mailbox). Resident: the reply and status bytes
; are always mapped (3B00H-3CFFH), so any page may call these.
;
; A transaction: MSTARTN/MSTARTF (TX rewound, device, command, the
; parameters' count), its parameters and payload streamed (FNPARB, MPARW,
; FNTXBYT), then MPOST; ACKSEQ echoes the SEQ when the cart has answered
; (MDONE). The link (p_net.inc) never waits; the boot page's blocking
; ones go through MGO.

; MSTARTN: an N: transaction: command E, L parameters. MSTARTF: the same on
; the Fuji device. Clobber A, BC, DE, HL.
MSTARTN: LD     A,NETDEV
        JP      FNBEGIN
MSTARTF: LD     A,FNDFUJI
        JP      FNBEGIN

; MPARW: a 2-byte parameter, HL (low first). Clobbers A, DE.
MPARW:  LD      A,2
        CALL    FNTXBYT
        LD      A,L
        CALL    FNTXBYT
        LD      A,H
        JP      FNTXBYT

; MPOST: SEQ = ACKSEQ + 1 (255 wraps to 1): the transaction goes. The SEQ,
; which ACKSEQ echoes when it is done, is kept. Clobbers A, BC, DE.
MPOST:  LD      A,(FNACKSQ)
        INC     A
        JR      NZ,MPO1
        INC     A
MPO1:   LD      (NSEQ),A
        LD      C,FRSEQ
        JP      FNREGWR

; MDONE: Z when the posted transaction has answered. Clobbers A.
MDONE:  LD      A,(NSEQ)
        PUSH    HL
        LD      HL,FNACKSQ
        CP      (HL)
        POP     HL
        RET

; MACK: Z when it worked and was ACKed. Clobbers A.
MACK:   LD      A,(FNERR)
        OR      A
        RET     NZ
        LD      A,(FNREPLY)
        CP      FCACK
        RET

; MIDLE: the link's transaction, if one is out, answered first (a new one is
; never posted over it): up to 4 s of frames. Clobbers A, BC, DE, HL.
MIDLE:  LD      A,(NKIND)
        OR      A
        RET     Z
        LD      B,240
MID1:   CALL    MDONE
        JR      Z,MID2
        PUSH    BC
        CALL    WAIT1
        POP     BC
        DJNZ    MID1
MID2:   XOR     A
        LD      (NKIND),A
        RET

; MGO: post, then wait for the answer a frame at a time (the raster and the
; sound running), about 21 s at most (an OPEN may dial for seconds; the
; same SEQ is polled all along). Z when it was ACKed. Clobbers A, BC, DE, HL.
MGO:    CALL    MPOST
        LD      BC,1280
MGO1:   CALL    MDONE
        JP      Z,MACK
        PUSH    BC
        CALL    WAIT1
        POP     BC
        DEC     BC
        LD      A,B
        OR      C
        JR      NZ,MGO1
        INC     A               ; never came: NZ
        RET

; WAIT1: the next frame, its sound tick. Clobbers A, BC, DE, HL.
WAIT1:  LD      HL,FRAMES
        LD      A,(HL)
W1A:    HALT
        CP      (HL)
        JR      Z,W1A
        FAR     PG_SND,SNDTICK
        RET

; SNDQUE: start sound or jingle A at the next tick (none while muted). An
; effect goes on its own channels and cuts only what is there (p_snd.inc).
; Two can wait; a third replaces the second. Clobbers A.
SNDQUE: PUSH    HL
        LD      HL,FLAGS
        BIT     2,(HL)          ; FL_MUTE
        JR      NZ,SQ9
        LD      HL,SNDQ
        INC     (HL)            ; (0FFH: free)
        JR      Z,SQ1
        DEC     (HL)
        LD      HL,SNDQ2
SQ1:    LD      (HL),A
SQ9:    POP     HL
        RET

; SNDOFF: every voice off now. Clobbers A, B, C, HL.
SNDOFF: LD      HL,SNDSH
        LD      B,8
SOF1:   LD      (HL),0
        INC     HL
        DJNZ    SOF1
        LD      A,0FFH
        LD      (SNDQ),A
        LD      (SNDQ2),A
        LD      HL,SNDSH
        LD      BC,8*256+SNDBX  ; registers 7 down to 0 (B after OUTI's DEC)
        OTIR
        RET

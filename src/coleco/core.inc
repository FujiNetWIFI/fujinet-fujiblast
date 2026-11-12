; core.inc -- the frame interrupt, the VDP's guard, paging, the frame,
; input. Fixed (8000H-BFFFH).
;
; The M.U.L.E. ColecoVision client's core (clients/coleco/core.inc) with the
; Astrocade port's frame, far calls and queues (src/astrocade/src/core.inc).

; ---- the frame: the VDP's interrupt is the Z80's NMI ----------------------
; NMI: the BIOS jumps here (0066H: JP 8021H). Reading the VDP's status
; acknowledges the frame, but it also resets the VDP's address latch, which
; would spoil an address being set or a burst under way: while the game is
; at the VDP (VDPBUSY) the status is left and VDPPEND set. The VDP holds its
; interrupt line down meanwhile, so no other NMI comes; VFREE catches up.
; It must never read F800H or above (the mailbox's hotspots).
NMI:    PUSH    AF
        LD      A,(VDPBUSY)
        OR      A
        JR      NZ,NMIDEF
        CALL    NMISAVE
        POP     AF
        RETN
NMIDEF: LD      A,1
        LD      (VDPPEND),A
        POP     AF
        RETN

; NMISAVE: NMIWORK with every register kept but AF.
NMISAVE: PUSH   BC
        PUSH    DE
        PUSH    HL
        PUSH    IX
        PUSH    IY
        EX      AF,AF'
        PUSH    AF
        EXX
        PUSH    BC
        PUSH    DE
        PUSH    HL
        CALL    NMIWORK
        POP     HL
        POP     DE
        POP     BC
        EXX
        POP     AF
        EX      AF,AF'
        POP     IY
        POP     IX
        POP     HL
        POP     DE
        POP     BC
        RET

; NMIWORK: one frame: the status (acknowledged), the clock, the controllers
; (OS7's POLLER) and the sound's step.
NMIWORK: IN     A,(VDPC)
        LD      HL,VDPSEEN
        OR      (HL)
        LD      (HL),A
        LD      HL,(FRAMES)
        INC     HL
        LD      (FRAMES),HL
        CALL    POLLER
        JP      SNDNMI

; VLOCK / VFREE: around every address and data burst. Both keep every
; register (a guard that clobbers what it guards is worse than none).
VLOCK:  PUSH    AF
        LD      A,1
        LD      (VDPBUSY),A
        POP     AF
        RET

VFREE:  PUSH    AF
VFR0:   XOR     A
        LD      (VDPBUSY),A
        LD      A,(VDPPEND)
        OR      A
        JR      NZ,VFR1
        POP     AF
        RET
VFR1:   LD      A,1             ; a frame came meanwhile: take it now, with
        LD      (VDPBUSY),A     ; the next one kept out until it is done
        XOR     A
        LD      (VDPPEND),A
        CALL    NMISAVE
        JR      VFR0

; ---- start-up ----------------------------------------------------------------
; START: a cold start, power-on or RESET. RESET restarts here but never
; resets the cart: the page, the socket and ACKSEQ are as they were.
START:  LD      A,1             ; first: the NMI waits until we are set up
        LD      (VDPBUSY),A
        DI
        LD      SP,STACK
        LD      A,(FNBKSEL+0)   ; page 0 (RESET leaves the page as it was)
        XOR     A
        LD      (CURPG),A
        LD      (DEFERWR),A     ; OS7: VRAM writes now, not queued
        LD      (MUXSPRT),A
        LD      HL,RAMBASE      ; our variables: zero
        LD      DE,RAMBASE+1
        LD      BC,STKLOW-RAMBASE-1
        LD      (HL),0
        LDIR
        LD      HL,FRAMES       ; and the low ones
        LD      DE,FRAMES+1
        LD      BC,SPARE1D-FRAMES
        LD      (HL),0
        LDIR
        XOR     A
        LD      (VDPPEND),A
        LD      (VDPSEEN),A
        LD      HL,(RANDNUM)    ; the seed: never 0 (RAND's)
        LD      A,H
        OR      L
        JR      NZ,ST1
        LD      HL,0A55AH
        LD      (RANDNUM),HL
ST1:    LD      A,0FFH
        LD      (MYSEAT),A
        LD      (KRAW),A
        CALL    SNDINI          ; the chip silent
        CALL    ININIT
        CALL    GFXINIT         ; the screen; the frame interrupt on
        XOR     A
        LD      (VDPBUSY),A
        LD      B,120           ; the cart: 'F','N', a protocol (it may
ST2:    PUSH    BC              ;  take a moment to paint its mailbox)
        LD      A,(FNMAGF)
        CP      'F'
        JR      NZ,ST3
        LD      A,(FNMAGN)
        CP      'N'
        JR      NZ,ST3
        LD      A,(FNPVER)
        CP      1               ; (the Coleco's: 1 serves flat and APPBANK images)
        JR      C,ST3
        POP     BC
        JP      MAININIT
ST3:    CALL    FRWAIT1
        POP     BC
        DJNZ    ST2
; STBAD: no FujiNet cart: say so, stop.
STBAD:  CALL    GFXBEGIN
        LD      HL,SNEEDFW
        LD      DE,10*256+8
        LD      C,SC_WHITE*16+SC_BLUE
        CALL    TXTAT
        LD      HL,SNEEDF2
        LD      DE,12*256+8
        LD      C,SC_WHITE*16+SC_BLUE
        CALL    TXTAT
        CALL    GFXSHOW
STHALT: HALT
        JR      STHALT
SNEEDFW: DB     "NEEDS A FUJINET",0
SNEEDF2: DB     "CART",0

; ---- paging ----------------------------------------------------------------
; SELPG: map page A (bit 7, the upper 4K, is not the cart's: dropped) at
; C000H. Preserves BC,DE,HL.
SELPG:  AND     7FH
        LD      (CURPG),A
        PUSH    HL
        ADD     A,FNBKLO
        LD      L,A
        LD      H,FNRSELP
        LD      A,(HL)
        POP     HL
        RET

; FARCALL: call into a page; the page and address follow the CALL:
;       CALL    FARCALL
;       DB      page
;       DW      routine
; A, BC, DE, HL go in and come back as the routine left them (and F). The
; caller's page is mapped again on return, so a page may call another
; this way. IX and IY are the routine's to keep. (M.U.L.E.'s.)
FARCALL: LD     (FCA),A
        LD      (FCHL),HL
        POP     HL
        LD      A,(HL)          ; page
        INC     HL
        PUSH    DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      (FCTGT),DE
        POP     DE
        PUSH    HL              ; the real return
        LD      H,A
        LD      A,(CURPG)
        PUSH    AF              ; the caller's page
        LD      A,H
        CALL    SELPG
        LD      HL,(FCHL)
        LD      A,(FCA)
        CALL    FCJMP
FCBACK: LD      (FCHL),HL
        POP     HL              ; H = the caller's page
        PUSH    AF
        LD      A,H
        CALL    SELPG
        POP     AF
        LD      HL,(FCHL)
        RET
FCJMP:  PUSH    HL
        LD      HL,(FCTGT)
        EX      (SP),HL
        RET

; PGCALL: entry HL of page A (E_*), the caller's page back after. BC, DE
; go in; A, F, BC, DE, HL come back as the entry left them. A page is 4K
; at C000H or (bit 7 of A) at D000H: two share an image page.
PGCALL: BIT     7,A
        JR      Z,PGC1
        PUSH    DE
        LD      DE,1000H
        ADD     HL,DE
        POP     DE
PGC1:   LD      (FCTGT),HL
        LD      H,A
        LD      A,(CURPG)
        PUSH    AF
        LD      A,H
        CALL    SELPG
        CALL    FCJMP
        JR      FCBACK

; ---- the frame ----------------------------------------------------------------------
; FRAME: the next frame: wait for the frame interrupt to count one (HALT:
; every band's interrupt wakes it, and the bands start on time while it
; waits), DT and our clock, the controls; MR turns the sound off and on.
; Clobbers A, BC, DE, HL.
FRAME:  LD      HL,(LASTF)
FR1:    LD      DE,(FRAMES)
        LD      A,E
        CP      L
        JR      NZ,FR2
        HALT
        JR      FR1
FR2:    LD      (LASTF),DE
        EX      DE,HL           ; frames since the last
        OR      A
        SBC     HL,DE
        LD      A,H
        OR      A
        JR      NZ,FR3
        LD      A,L
        CP      9
        JR      C,FR4
FR3:    LD      A,8
FR4:    LD      (DT),A
        LD      HL,(FRML)
        LD      E,A
        LD      D,0
        ADD     HL,DE
        LD      (FRML),HL
        LD      A,(INDIR)
        LD      (PRVDIR),A
        CALL    INPOLL
        LD      A,(EVKEY)
        CP      K_MUTE
        JR      NZ,FR5
        LD      A,K_NONE        ; (never the game's)
        LD      (EVKEY),A
        LD      A,(FLAGS)
        XOR     FL_MUTE
        LD      (FLAGS),A
        AND     FL_MUTE         ; sound off now: stop what plays
        CALL    NZ,SNDOFF
FR5:    RET

; ---- input --------------------------------------------------------------------------
; OS7's POLLER reads controller 1 in the frame interrupt into CTLDAT (the
; header points it there); INPOLL turns that into the game's input once a
; frame. INDIR: directions held (the handle's own bits, D_*). INBTN: a fire
; button held, EVBTN: it went down. EVKEY: a key that went down (K_*, digits).
; A harness may drive (INPHK, tools/coleco/colsim.py).
INPOLL: LD      A,(INPHK)
        OR      A
        JR      Z,IP0
        LD      A,(INPHKD)
        LD      (INDIR),A
        LD      A,(INPHKB)
        LD      B,A
        LD      A,(INPHKK)
        LD      C,A
        XOR     A
        LD      (INPHKK),A
        LD      A,C             ; (the key + 1: 0 is none)
        OR      A
        JR      Z,IPNK
        DEC     A
        JR      IP3
IPNK:   LD      A,K_NONE
        JR      IP3
IP0:    LD      A,(CTLDAT+CT_STICK)
        LD      C,A
        XOR     A
        BIT     0,C
        JR      Z,IP0A
        OR      D_UP
IP0A:   BIT     1,C
        JR      Z,IP0B
        OR      D_RIGHT
IP0B:   BIT     2,C
        JR      Z,IP0C
        OR      D_DOWN
IP0C:   BIT     3,C
        JR      Z,IP0D
        OR      D_LEFT
IP0D:   LD      (INDIR),A
        LD      A,(CTLDAT+CT_LFIRE)
        LD      B,A
        LD      A,(CTLDAT+CT_RFIRE)
        OR      B
        AND     40H
        LD      B,A
        LD      A,(CTLDAT+CT_KEY)
        AND     0FH
        CP      0CH
        JR      C,IP1
        LD      A,K_NONE        ; none (0FH) or a stray code
        JR      IP2
IP1:    CALL    KEYMAP
IP2:    LD      HL,KRAW
        LD      C,(HL)
        LD      (HL),A
        CP      C
        JR      NZ,IP3
        LD      A,K_NONE        ; held: no event
IP3:    LD      (EVKEY),A
        LD      A,B             ; B = the button's level
        OR      A
        JR      Z,IP4
        LD      A,1
IP4:    LD      B,A
        LD      HL,INBTN
        LD      A,(HL)
        LD      (HL),B
        XOR     1               ; it was up, is down: an event
        AND     B
        LD      (EVBTN),A
        RET

; KEYMAP: keypad code A (0-0BH) -> K_*: '#' is K_ENT; '*' leaves, or rubs
; out while a name is entered (INNAME); '9' is K_MUTE (the sound's), but a
; digit in a name. Preserves B.
KEYMAP: CP      0AH
        JR      Z,KMSTAR
        JR      NC,KMHASH
        CP      9
        RET     NZ
        LD      A,(INNAME)
        OR      A
        LD      A,9
        RET     NZ
        LD      A,K_MUTE
        RET
KMSTAR: LD      A,(INNAME)
        OR      A
        LD      A,K_CLR
        RET     Z
        LD      A,K_BACK
        RET
KMHASH: LD      A,K_ENT
        RET

; ININIT: controller 1 on (POLLER reads only what is enabled).
ININIT: LD      A,0FFH
        LD      (CTLDAT+CT_EN1),A
        XOR     A
        LD      (CTLDAT+CT_EN2),A
        LD      A,0FH
        LD      (CTLDAT+CT_KEY),A
        RET

; ---- odds and ends --------------------------------------------------------------------
; RAND: A = a random byte: a 16-bit Galois LFSR, seeded from the clock
; (M.U.L.E.'s). Preserves everything else.
RAND:   PUSH    HL
        LD      HL,(RNDS)
        LD      A,H
        OR      L
        JR      NZ,RND1
        LD      HL,(FRAMES)
        SET     0,L
RND1:   LD      A,8
RND2:   SRL     H
        RR      L
        JR      NC,RND3
        PUSH    AF
        LD      A,H
        XOR     0B4H
        LD      H,A
        POP     AF
RND3:   DEC     A
        JR      NZ,RND2
        LD      (RNDS),HL
        LD      A,L
        POP     HL
        RET

; ACTQ: an action to send: B its kind (ACT_*), D E its arguments; the next
; seq (never 0). One waits at a time: a second replaces it. Clobbers A.
ACTQ:   LD      A,B
        LD      (AK),A
        LD      A,D
        LD      (A1),A
        LD      A,E
        LD      (A2),A
        LD      A,(TXF)
        OR      TX_ACT
        LD      (TXF),A
        LD      A,(ASEQ)
        INC     A
        JR      NZ,ACQ1
        INC     A
ACQ1:   LD      (ASEQ),A
        RET

; MARKT: tile A (0-142) to be redrawn: its bit in MAPD (the draw page draws
; the marked ones, a few a frame). Preserves BC, DE.
MARKT:  PUSH    BC
        PUSH    HL
        LD      C,A
        RRCA
        RRCA
        RRCA
        AND     1FH
        LD      L,A
        LD      H,0
        LD      A,C
        LD      BC,MAPD
        ADD     HL,BC
        AND     7
        LD      B,A
        INC     B
        LD      A,80H
MK1:    RLCA
        DJNZ    MK1
        OR      (HL)
        LD      (HL),A
        POP     HL
        POP     BC
        RET

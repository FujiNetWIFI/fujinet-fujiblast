; core.inc -- start-up, the raster, paging, the frame, input. Resident.
;
; Interrupts are ON: IM 2, I = 4FH (refresh cycles land in screen RAM, never
; on the cart's hotspot pages; fuji_mailbox.h names 4FH as the safe value),
; the vector word in RAM. The handlers read no cart address at all, so an
; interrupt between a mailbox REGSEL and its REGDATA is harmless (the cart
; pairs reads, not bus cycles), and they never touch MAGIC or XPAND (the
; text and the compositor own those). The ISS tracker's raster engine
; (fujinet-iss-tracker/astrocade/raster.inc) and M.U.L.E.'s frame counter.

; ---- start-up --------------------------------------------------------------------
; RESET restarts here (RSTENT) but never resets the cart: the page, the
; socket and ACKSEQ are as they were. So everything is set up afresh (the
; socket is closed before it is opened again, p_boot.inc).
PRGSTR: DI
        LD      SP,STACK
        LD      HL,NORMEM       ; the picture and every variable: zero (the BIOS's
        LD      DE,NORMEM+1     ;  menu leaves its text in the variables)
        LD      BC,STKLOW-NORMEM-1
        LD      (HL),0
        LDIR
        CALL    SNDOFF          ; the chip silent (SNDSH is zero)
        LD      A,VERBLV        ; the picture ends at line LINES
        OUT     (VERBL),A
        LD      A,0FFH
        LD      (MYSEAT),A
        LD      (KRAW),A
        LD      (SNDQ),A
        LD      (SNDQ2),A
        LD      HL,B_PLAIN
        CALL    SETBANDS
        LD      HL,P_PLAIN
        CALL    SETPAL
        CALL    INTON
        LD      A,(FNMAGF)      ; the cart: 'F','N', protocol 2 or later
        CP      'F'
        JR      NZ,STBAD
        LD      A,(FNMAGN)
        CP      'N'
        JR      NZ,STBAD
        LD      A,(FNPVER)
        CP      2
        JP      NC,MAININIT
; STBAD: no FujiNet cart, or one too old for APPBANK images: say so, stop.
STBAD:  LD      HL,SNEEDFW
        LD      DE,5*TXH*256+12
        LD      C,3*4
        CALL    TXTAT
        LD      HL,SNEEDF2
        LD      DE,6*TXH*256+12
        LD      C,3*4
        CALL    TXTAT
STHALT: HALT
        JR      STHALT
SNEEDFW: DB     "NEEDS A FUJINET",0
SNEEDF2: DB     "CART, PROTO 2",0

; ---- the raster ------------------------------------------------------------------
; INTON: IM 2 through VECTOR; the frame interrupt starts the chain.
INTON:  LD      HL,VBISR
        LD      (VECTOR),HL
        LD      A,4FH           ; I: refresh cycles stay in screen RAM
        LD      I,A
        IM      2
        LD      A,VECTOR AND 0FFH
        OUT     (INFBK),A
        LD      A,VBLLN
        OUT     (INLIN),A
        LD      A,08H           ; the screen's interrupt on, held until taken
        OUT     (INMOD),A
        EI
        RET

; BANDISR: the next band's colours. Timed: its five writes must start inside
; the band's first scanline (113.75 cycles), and MAME draws a line from the
; registers as they are when the next begins (an instruction counts if it
; STARTS before then). The interrupt comes a scanline early (BANDOFS) and
; BPAD NOPs place the first write past that scanline's end; the five span
; about 85 cycles. raster.inc's measure: BPAD 4-13 with the program idle
; (the idle loop HALTs: no latency), 5 leaves room for a busy one's worst
; instruction.
BANDISR: PUSH   AF
        PUSH    BC
        PUSH    HL
        LD      HL,(BANDP)
        LD      A,(HL)          ; HORCB
        INC     HL
        LD      BC,4*256+COLBX  ; OUTI: registers 3, 2, 1 (B after its DEC)
        DS      BPAD            ; NOPs
        OUT     (HORCB),A
        OUTI                    ; R3
        OUTI                    ; R2
        OUTI                    ; R1
        LD      A,(HL)
        INC     HL
        OUT     (COL1L),A       ; L1
        LD      A,(HL)          ; the next interrupt's scanline
        INC     HL
        LD      (BANDP),HL
        OUT     (INLIN),A
        CP      VBLLN
        JR      NZ,BI1
        LD      HL,VBISR        ; that was the last band: the frame next
        LD      (VECTOR),HL
BI1:    POP     HL
        POP     BC
        POP     AF
        EI
        RETI

; VBISR: the frame interrupt, below the picture: the table's first entry
; for the top of the next frame (nothing is drawn now: no timing), the
; frame counted.
VBISR:  PUSH    AF
        PUSH    BC
        PUSH    HL
        LD      HL,(BANDTB)
        LD      A,(HL)
        INC     HL
        LD      BC,4*256+COLBX
        OUT     (HORCB),A
        OUTI
        OUTI
        OUTI
        LD      A,(HL)
        INC     HL
        OUT     (COL1L),A
        LD      A,(HL)
        INC     HL
        LD      (BANDP),HL
        OUT     (INLIN),A
        CP      VBLLN
        JR      Z,VB1           ; one band only: the frame again
        LD      HL,BANDISR
        LD      (VECTOR),HL
VB1:    LD      HL,(FRAMES)
        INC     HL
        LD      (FRAMES),HL
        POP     HL
        POP     BC
        POP     AF
        EI
        RETI

; SETBANDS: this screen's band table HL (resident: the handler reads it
; whatever page is mapped). Takes effect at the next frame. Clobbers A.
SETBANDS: DI
        LD      (BANDTB),HL
        EI
        RET

; SETPAL: the colours a screen keeps: HL = L0, L2, L3, R0 (the frame's and
; the ground's). Clobbers A, HL.
SETPAL: LD      A,(HL)
        OUT     (COL0L),A
        INC     HL
        LD      A,(HL)
        OUT     (COL2L),A
        INC     HL
        LD      A,(HL)
        OUT     (COL3L),A
        INC     HL
        LD      A,(HL)
        OUT     (COL0R),A
        RET

; ---- paging ----------------------------------------------------------------------
; SELPG: map page A at 2000H (one read; the next read sees it). Preserves
; BC, DE, HL. Resident code only: the next fetch from 2000H-2FFFH is the
; new page's.
SELPG:  LD      (CURPG),A
        PUSH    HL
        ADD     A,80H
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
; go in; A, F, BC, DE, HL come back as the entry left them.
PGCALL: LD      (FCTGT),HL
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
FR5:    FAR     PG_SND,SNDTICK  ; the sound's frames
        RET

; ---- input --------------------------------------------------------------------------
; INPOLL: this frame's controls. INDIR: directions held (the handle's own
; bits, diagonals too). INBTN: the trigger held, EVBTN: it went down.
; EVKEY: a key that went down (K_*, digits). Clobbers A, BC, DE, HL.
INPOLL: IN      A,(SW0)
        LD      C,A
        AND     0FH
        LD      (INDIR),A
        LD      A,C
        AND     10H
        LD      HL,INBTN
        JR      Z,IP1
        LD      A,(HL)          ; down: an event if it was up
        XOR     1
        LD      (EVBTN),A
        LD      (HL),1
        JR      IP2
IP1:    LD      (EVBTN),A
        LD      (HL),A
IP2:    CALL    KEYRAW          ; A = the key held, K_NONE none
        LD      HL,KRAW
        LD      C,(HL)
        LD      (HL),A
        CP      C
        JR      NZ,IP3
        LD      A,K_NONE        ; held: no event
IP3:    LD      (EVKEY),A
        RET

; KEYRAW: A = the key held on the keypad now (KPTAB), K_NONE if none: the
; first found. Clobbers A, BC, DE, HL.
KEYRAW: LD      HL,KPTAB
        LD      C,KEY0
KRC:    IN      A,(C)
        LD      D,A
        LD      E,6
KRB:    SRL     D
        JR      NC,KRB2
        LD      A,(HL)
        CP      K_NONE
        RET     NZ
KRB2:   INC     HL
        DEC     E
        JR      NZ,KRB
        INC     C
        LD      A,C
        CP      KEY3+1
        JR      NZ,KRC
        LD      A,K_NONE
        RET
; per column 14H..17H, bit 0 (the bottom key) first (MAME's astrohome.cpp)
KPTAB:  DB      K_NONE,K_NONE,K_NONE,K_NONE,K_NONE,K_ENT        ; % / x - + =
        DB      K_NONE,K_NONE,9,6,3,K_NONE                      ; v CH 9 6 3 .
        DB      K_NONE,K_NONE,8,5,2,0                           ; ^ MS 8 5 2 0
        DB      K_CLR,K_MUTE,7,4,1,K_BACK                       ; C MR 7 4 1 CE

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

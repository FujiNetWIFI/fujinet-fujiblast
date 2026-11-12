;; ===========================================================================
;; seats.asm -- the bombers, every jiffy and every frame, which IntyBASIC is
;; too slow for (four seats cost it a frame): the others walked toward
;; where the server has them (play.c's toward), and the eight MOBs written.
;; ===========================================================================

;; SEATS_STEP(skip, n): n jiffies of the seats in play but skip (ours,
;; walked by walk.asm; 255: none): disp_x, disp_y toward ST's x, y (3 units
;; a jiffy at most; at once if more than a tile off); a falling seat's die_t
;; on by n (to 48).
SEATS_STEP: PROC
        PSHR    R5
        MVO     R0, var_SK_SEAT
        MOVR    R1, R0
        SLL     R0, 1
        ADDR    R1, R0
        MVO     R0, var_SK_LIM          ; 3 n
        MVO     R1, var_SK_N
        CLRR    R2
@@s:    MVII    #label_TBIT, R4
        ADDR    R2, R4
        MVI@    R4, R3
        MVI     const_ST + const_OFF_INPLAY, R0
        ANDR    R3, R0
        BEQ     @@next
        MVII    #array_DIE_T, R4
        ADDR    R2, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@nd
        CMPI    #48, R0
        BC      @@nd
        ADD     var_SK_N, R0
        CMPI    #48, R0
        BNC     @@d48
        MVII    #48, R0
@@d48:  DECR    R4
        MVO@    R0, R4
@@nd:   CMP     var_SK_SEAT, R2
        BEQ     @@next
        MOVR    R2, R1                  ; ST's player: OFF_PLAYERS + 10 s
        SLL     R1, 1
        MOVR    R1, R0
        SLL     R1, 2
        ADDR    R0, R1
        ADDI    #const_ST + const_OFF_PLAYERS, R1
        MVI@    R1, R0
        ANDI    #255, R0
        MVII    #array_DISP_X, R4
        ADDR    R2, R4
        CALL    @@tw
        INCR    R1
        MVI@    R1, R0
        ANDI    #255, R0
        MVII    #array_DISP_Y, R4
        ADDR    R2, R4
        CALL    @@tw
@@next: INCR    R2
        CMPI    #4, R2
        BNEQ    @@s
        PULR    PC

        ; the byte at R4 toward R0, sk_lim at most (R1 kept, R3 used)
@@tw:   PSHR    R1
        MVI@    R4, R1                  ; at
        DECR    R4
        MOVR    R1, R3
        SUBR    R0, R3                  ; at - to
        BEQ     @@tz
        BC      @@dn                    ; at > to
        NEGR    R3
        CMPI    #17, R3
        BC      @@far
        CMP     var_SK_LIM, R3
        BNC     @@u1
        MVI     var_SK_LIM, R3
@@u1:   ADDR    R3, R1
        B       @@put
@@dn:   CMPI    #17, R3
        BC      @@far
        CMP     var_SK_LIM, R3
        BNC     @@d1
        MVI     var_SK_LIM, R3
@@d1:   SUBR    R3, R1
        B       @@put
@@far:  MOVR    R0, R1
@@put:  MVO@    R1, R4
@@tz:   PULR    R1
        JR      R5
        ENDP

;; SEATS_DRAW: the four bombers into IntyBASIC's MOB buffer (_mobs: 8 X
;; words, 8 Y, 8 A; the frame handler copies it to the STIC): seat s's
;; visor MOB 2s, its body 2s + 1, at disp_x, disp_y (arena units) in the
;; frame FRAME_OF picks, from gfx.bas's frame tables; only a seat whose
;; frame or place changed since (sd_lf, sd_lx, sd_ly; sd_lf 254: draw it
;; anyway). A seat's words are written with interrupts off, so a frame
;; never shows a body moved without its visor.
SEATS_DRAW: PROC
        PSHR    R5
        MVI     var_&FRAME_COUNT, R0    ; the walk's step: fr_walk((frame / 8) AND 3)
        SLR     R0, 2
        SLR     R0, 1
        ANDI    #3, R0
        ADDI    #label_FR_WALK, R0
        MOVR    R0, R4
        MVI@    R4, R0
        MVO     R0, var_SD_WALK
        CLRR    R2
@@s:    CALL    FRAME_OF                ; R1: the frame
        ; as last drawn (frame, x, y)? then nothing to write
        MVII    #array_SD_LF, R4
        ADDR    R2, R4
        MVI@    R4, R0
        CMPR    R1, R0
        BNEQ    @@new
        MVII    #array_SD_LX, R4
        ADDR    R2, R4
        MVI@    R4, R0
        MVII    #array_DISP_X, R4
        ADDR    R2, R4
        CMP@    R4, R0
        BNEQ    @@new
        MVII    #array_SD_LY, R4
        ADDR    R2, R4
        MVI@    R4, R0
        MVII    #array_DISP_Y, R4
        ADDR    R2, R4
        CMP@    R4, R0
        BEQ     @@next
@@new:  MVII    #array_SD_LF, R4
        ADDR    R2, R4
        MVO@    R1, R4
        MVII    #array_DISP_X, R4
        ADDR    R2, R4
        MVI@    R4, R0
        MVII    #array_SD_LX, R4
        ADDR    R2, R4
        MVO@    R0, R4
        MVII    #array_DISP_Y, R4
        ADDR    R2, R4
        MVI@    R4, R0
        MVII    #array_SD_LY, R4
        ADDR    R2, R4
        MVO@    R0, R4
        MOVR    R2, R3
        SLL     R3, 1
        ADDI    #_mobs, R3
        MOVR    R3, R5                  ; X of MOB 2s
        CMPI    #255, R1
        BNEQ    @@show
        CLRR    R0
        MVO@    R0, R5
        MVO@    R0, R5
        B       @@next
@@show: MVII    #array_DISP_X, R4       ; X: 8 + 32 + x / 2, shown
        ADDR    R2, R4
        MVI@    R4, R0
        SLR     R0, 1
        ADDI    #40 + $200, R0
        MVII    #label_FR_VISOR, R4
        ADDR    R1, R4
        MVI@    R4, R3                  ; the visor's card (0: none)
        PSHR    R3
        TSTR    R3
        BEQ     @@nv
        MOVR    R0, R3
@@nv:   DIS
        MVO@    R3, R5                  ; the visor's X (0: hidden)
        MVO@    R0, R5                  ; the body's
        ADDI    #6, R5
        MVII    #array_DISP_Y, R4       ; Y: 8 + 8 + y / 2, and the frame's flags
        ADDR    R2, R4
        MVI@    R4, R0
        SLR     R0, 1
        ADDI    #16, R0
        MVII    #label_FR_FLAGS, R4
        ADDR    R1, R4
        MVI@    R4, R3
        PSHR    R3
        ANDI    #$400, R3               ; the visor: mirrored with it, rows down
        ADDR    R0, R3
        MVII    #label_FR_VDY, R4
        ADDR    R1, R4
        ADD@    R4, R3
        MVO@    R3, R5
        PULR    R3
        ADDR    R0, R3
        MVO@    R3, R5
        ADDI    #6, R5
        PULR    R3                      ; A: the visor's card in yellow
        ADDI    #const_VISOR_COL, R3
        MVO@    R3, R5
        MVII    #label_FR_BODY, R4      ; the body's in the seat's colour
        ADDR    R1, R4
        MVI@    R4, R3
        MVII    #label_SEAT_COL, R4
        ADDR    R2, R4
        ADD@    R4, R3
        MVO@    R3, R5
        EIS
@@next: INCR    R2
        CMPI    #4, R2
        BNEQ    @@s
        PULR    PC
        ENDP
;; FRAME_OF: R1 = seat R2's frame (FR_*), 255 if hidden: out of play, or
;; down and done falling (die_t: FR_DIE0-3, 12 jiffies each); else its way
;; (ours: as walked; the others': the server's) and, walking, its step.
;; R2 kept; R0, R3, R4 used.
FRAME_OF: PROC
        MVII    #label_TBIT, R4
        ADDR    R2, R4
        MVI@    R4, R3                  ; the seat's bit
        MVI     const_ST + const_OFF_INPLAY, R0
        ANDR    R3, R0
        BEQ     @@hide
        MVI     const_ST + const_OFF_ALIVE, R0
        ANDR    R3, R0
        BNEQ    @@up
        MVII    #array_DIE_T, R4
        ADDR    R2, R4
        MVI@    R4, R0
        TSTR    R0
        BEQ     @@hide
        CMPI    #48, R0
        BC      @@hide
        MVII    #const_FR_DIE0, R1
@@d:    CMPI    #12, R0
        BNC     @@dd
        SUBI    #12, R0
        INCR    R1
        B       @@d
@@dd:   JR      R5
@@hide: MVII    #255, R1
        JR      R5
@@up:   CMP     var_MY_SEAT, R2
        BNEQ    @@other
        MVI     var_ME, R0
        TSTR    R0
        BEQ     @@other
        MVI     var_MV_DIR, R0
        MVI     var_ME_MOVED, R3
        B       @@f
@@other: MOVR   R2, R1                  ; the server's: OFF_PLAYERS + 10 s + P_DIR
        SLL     R1, 1
        MOVR    R1, R4
        SLL     R1, 2
        ADDR    R4, R1
        ADDI    #const_ST + const_OFF_PLAYERS + const_P_DIR, R1
        MOVR    R1, R4
        MVI@    R4, R0
        ANDI    #255, R0
        MOVR    R0, R3
        ANDI    #const_P_DIR_MOVING, R3
@@f:    ANDI    #3, R0
        ADDI    #label_FR_BASE, R0
        MOVR    R0, R4
        MVI@    R4, R1
        TSTR    R3
        BEQ     @@ff
        ADD     var_SD_WALK, R1
@@ff:   JR      R5
        ENDP

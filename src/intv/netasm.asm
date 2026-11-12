;; ===========================================================================
;; netasm.asm -- the link's hot loops, which IntyBASIC is too slow for:
;; COBS frames in and out (docs/PROTOCOL.md: COBS of type, payload,
;; Fletcher-16, then 0), the patches into the state, and the arena drawn
;; whole.
;;
;; Cartridge RAM is 8 bits wide and reads back with other bits set, so
;; every byte read from it (and from the mailbox) is masked. The routines
;; are called from IntyBASIC (CALL / USR: arguments in R0-R3, result in
;; R0) and share its variables (var_X 8-bit, var_&X 16-bit).
;; ===========================================================================

;; COPYB(src, dst, count): copy count bytes, masked to 8 bits. From
;; syn-ack-wire's net.bas (M.U.L.E.'s before it).
COPYB:  PROC
        PSHR    R5
        MOVR    R0, R4
        MOVR    R1, R5
        TSTR    R2
        BEQ     @@done
@@loop: MVI@    R4, R3
        ANDI    #255, R3
        MVO@    R3, R5
        DECR    R2
        BNEQ    @@loop
@@done: PULR    PC
        ENDP

;; DECODE: n = USR DECODE. Bytes from #rx_p up to #rx_e (the mailbox's
;; receive buffer), dec_left of them at most, into the frame being decoded
;; (#dec_p: where its next byte goes in NBUF; dec_code: the bytes left of
;; its COBS block; dec_bad: it went wrong). Stops at a frame's end: n = its
;; length (type and payload) if its sums are right, 255 if not; 0: no
;; whole frame yet. taken counts the bytes taken, mod 256; dec_left is what
;; is left of the pass's.
;;
;; Fletcher-16's sums are kept as the bytes come (#dec_s1, #dec_s2: mod
;; 65535, an end-around carry, which 255 divides), the two sums sent at the
;; end too. With c1, c2 those two, a right frame's come to 2 c1 + c2 and
;; twice that, mod 255. A block is never 255 long (a frame is at most 130
;; bytes), so every block but a frame's first stands for a 0 before it.
DECODE: PROC
        PSHR    R5
@@again:
        MVI     var_&RX_P, R4
        MVI     var_&RX_E, R0
        MOVR    R4, R3
        ADD     var_DEC_LEFT, R3
        CMPR    R3, R0                  ; R0 - R3
        BNC     @@e                     ; fewer there than the pass may take
        MOVR    R3, R0
@@e:    MVO     R0, var_&DEC_E          ; where this call stops
        MVI     var_&DEC_P, R5
        MVI     var_&DEC_S1, R1
        MVI     var_&DEC_S2, R2
@@top:  CMP     var_&DEC_E, R4
        BC      @@out
        MVI     var_DEC_CODE, R3
        TSTR    R3
        BEQ     @@code
        MVI     var_&DEC_E, R0          ; a run of the block's bytes: k = min(code, there)
        SUBR    R4, R0
        CMPR    R0, R3                  ; R3 - R0
        BNC     @@k
        MOVR    R0, R3
@@k:    MVI     var_DEC_CODE, R0
        SUBR    R3, R0
        MVO     R0, var_DEC_CODE
@@d:    MVI@    R4, R0
        ANDI    #255, R0
        BEQ     @@zin
        MVO@    R0, R5
        ADDR    R0, R1
        ADCR    R1
        ADDR    R1, R2
        ADCR    R2
        DECR    R3
        BNEQ    @@d
        B       @@top
@@zin:  MVII    #1, R3                  ; a 0 inside a block: the frame ends, bad
        MVO     R3, var_DEC_BAD
        B       @@end
@@code: MVI@    R4, R0
        ANDI    #255, R0
        BEQ     @@end                   ; 0: the frame's end
        CMPI    #255, R0
        BNEQ    @@c1
        MVII    #1, R3                  ; (254 bytes on: no frame is that long)
        MVO     R3, var_DEC_BAD
@@c1:   CMPI    #const_NBUF, R5
        BEQ     @@c2
        CLRR    R3                      ; the 0 the last block stood for
        MVO@    R3, R5
        ADDR    R1, R2                  ; (it adds nothing to s1, s1 to s2)
        ADCR    R2
@@c2:   DECR    R0
        MVO     R0, var_DEC_CODE
        CMPI    #const_NBUF_END, R5
        BNC     @@top
        MVII    #const_NBUF_END, R5     ; too long: bad, and the slack takes the rest
        MVII    #1, R3
        MVO     R3, var_DEC_BAD
        B       @@top

@@out:  MVO     R5, var_&DEC_P          ; no whole frame yet
        MVO     R1, var_&DEC_S1
        MVO     R2, var_&DEC_S2
        CALL    @@took
        CLRR    R0
        PULR    PC

@@end:  MOVR    R5, R3                  ; a frame, R5 its end (CALL takes R5)
        CALL    @@took
        MOVR    R3, R5
        SUBI    #const_NBUF, R3         ; its length, with the sums
        PSHR    R3
        MVII    #const_NBUF, R0         ; the next starts afresh
        MVO     R0, var_&DEC_P
        CLRR    R0
        MVO     R0, var_DEC_CODE
        MVO     R0, var_&DEC_S1
        MVO     R0, var_&DEC_S2
        MVI     var_DEC_BAD, R0
        CLRR    R4
        MVO     R4, var_DEC_BAD
        TSTR    R0
        BNEQ    @@bad1
        TSTR    R3
        BNEQ    @@some
        PULR    R3                      ; 0 0: nothing; on with the rest
        B       @@again
@@some: CMPI    #3, R3
        BNC     @@bad1                  ; type and two sums at least
        MOVR    R5, R4
        SUBI    #2, R4
        MVI@    R4, R3
        ANDI    #255, R3
        ADDR    R3, R3                  ; 2 c1
        MVI@    R4, R0
        ANDI    #255, R0
        ADDR    R3, R0                  ; E = 2 c1 + c2
        PSHR    R2                      ; s2
        PSHR    R0                      ; E
        MOVR    R1, R0
        CALL    MOD255                  ; s1 mod 255
        PULR    R0                      ; E
        PSHR    R1                      ; [n, s2, s1']
        PSHR    R0                      ; [n, s2, s1', E]
        CALL    MOD255                  ; E mod 255
        PULR    R0
        PULR    R3                      ; s1'
        CMPR    R1, R3
        BNEQ    @@bad2
        ADDR    R0, R0                  ; 2 E
        CALL    MOD255
        PULR    R0                      ; s2
        PSHR    R1                      ; [n, 2E']
        CALL    MOD255                  ; s2 mod 255
        PULR    R3
        CMPR    R1, R3
        BNEQ    @@bad1
        PULR    R0
        SUBI    #2, R0                  ; type and payload
        PULR    PC
@@bad2: PULR    R0
@@bad1: PULR    R0
        MVII    #255, R0
        PULR    PC

        ; the bytes taken this call (R4 less #rx_p): onto taken, off
        ; dec_left; #rx_p = R4 (R0 used)
@@took: MOVR    R4, R0
        SUB     var_&RX_P, R0
        PSHR    R1
        MVI     var_TAKEN, R1
        ADDR    R0, R1
        MVO     R1, var_TAKEN
        MVI     var_DEC_LEFT, R1
        SUBR    R0, R1
        MVO     R1, var_DEC_LEFT
        PULR    R1
        MVO     R4, var_&RX_P
        JR      R5
        ENDP

;; MOD255: R1 = R0 mod 255 (R0 a 16-bit number; 256 is 1, mod 255).
MOD255: PROC
        MOVR    R0, R1
        SWAP    R1
        ANDI    #255, R1
        ANDI    #255, R0
        ADDR    R0, R1
        CMPI    #255, R1
        BNC     @@ok
        SUBI    #255, R1
        CMPI    #255, R1
        BNC     @@ok
        SUBI    #255, R1
@@ok:   JR      R5
        ENDP

;; SEND(type, n): a frame of type and the n payload bytes at RAWB + 1, COBS
;; encoded onto the end of TXQ (txq_len bytes so far). USR: 1 if queued, 0
;; if TXQ hadn't room.
SEND:   PROC
        PSHR    R5
        MVO     R0, const_RAWB
        INCR    R1                      ; type and payload
        MVI     var_TXQ_LEN, R3         ; out: m + 2 sums + a code + the 0
        ADDR    R1, R3
        ADDI    #4, R3
        CMPI    #65, R3
        BC      @@drop
        MVII    #const_RAWB, R4
        MOVR    R1, R3
        CLRR    R0                      ; s1
        CLRR    R2                      ; s2
@@sum:  MVI@    R4, R5
        ANDI    #255, R5
        ADDR    R5, R0
        CMPI    #255, R0
        BNC     @@s1
        SUBI    #255, R0
@@s1:   ADDR    R0, R2
        CMPI    #255, R2
        BNC     @@s2
        SUBI    #255, R2
@@s2:   DECR    R3
        BNEQ    @@sum
        MVO@    R0, R4                  ; the sums after the payload
        MVO@    R2, R4
        ADDI    #2, R1
        ; COBS: each 0 becomes the count to the next (a code byte before
        ; each run of others)
        MVII    #const_RAWB, R4
        MVI     var_TXQ_LEN, R5
        ADDI    #const_TXQ, R5
        MOVR    R5, R3                  ; where the code byte goes
        INCR    R5
        MVII    #1, R2
@@enc:  MVI@    R4, R0
        ANDI    #255, R0
        BEQ     @@zero
        MVO@    R0, R5
        INCR    R2
        B       @@more
@@zero: MVO@    R2, R3
        MOVR    R5, R3
        INCR    R5
        MVII    #1, R2
@@more: DECR    R1
        BNEQ    @@enc
        MVO@    R2, R3
        CLRR    R0
        MVO@    R0, R5
        SUBI    #const_TXQ, R5
        MVO     R5, var_TXQ_LEN
        MVII    #1, R0
        PULR    PC
@@drop: CLRR    R0
        PULR    PC
        ENDP

;; PATCH(n): the patch frame in NBUF (n bytes: type, then runs of off,
;; len, bytes) into ST. While arena_on, each map tile that changes is drawn
;; at once (label_TPOS: its BACKTAB address, label_TWORD: each tile's card
;; word), cm_boom set when a tile turns to a blast's centre from no flame,
;; cm_fizz when a bomb turns into a dud. Stops at a malformed run.
PATCH:  PROC
        PSHR    R5
        MOVR    R0, R3
        DECR    R3                      ; the bytes after the type
        MVII    #const_NBUF + 1, R4
@@run:  CMPI    #2, R3
        BNC     @@done
        MVI@    R4, R1                  ; off
        ANDI    #255, R1
        MVI@    R4, R2                  ; len
        ANDI    #255, R2
        SUBI    #2, R3
        CMPR    R2, R3                  ; R3 - R2
        BNC     @@done                  ; runs past the frame
        MOVR    R1, R0
        ADDR    R2, R0                  ; off + len
        CMPI    #const_STATE_SIZE + 1, R0
        BC      @@done                  ; past the state
        SUBR    R2, R3
        TSTR    R2
        BEQ     @@run
        PSHR    R3
        MVI     var_ARENA_ON, R5        ; the map isn't shown, or the run
        TSTR    R5                      ; misses it: just copied
        BEQ     @@fast
        CMPI    #const_OFF_MAP + 1, R0
        BNC     @@fast
        CMPI    #const_OFF_MAP + const_TILES, R1
        BC      @@fast
@@b:    MVI@    R4, R0
        ANDI    #255, R0                ; new
        MOVR    R1, R5
        ADDI    #const_ST, R5
        MVI@    R5, R3
        ANDI    #255, R3                ; old
        CMPR    R0, R3
        BEQ     @@same
        DECR    R5
        MVO@    R0, R5
        CMPI    #const_OFF_MAP, R1
        BNC     @@same
        CMPI    #const_OFF_MAP + const_TILES, R1
        BC      @@same
        CMPI    #const_T_FLAME_C, R0
        BNEQ    @@nb
        CMPI    #const_T_FLAME_C, R3
        BNC     @@boom
        CMPI    #const_T_FLAME_D + 1, R3
        BNC     @@nb
@@boom: MVII    #1, R5
        MVO     R5, var_CM_BOOM
@@nb:   CMPI    #const_T_DUD, R0
        BNEQ    @@nf
        CMPI    #const_T_BOMB, R3
        BNEQ    @@nf
        MVII    #1, R5
        MVO     R5, var_CM_FIZZ
@@nf:   MOVR    R1, R5
        ADDI    #label_TPOS - const_OFF_MAP, R5
        MVI@    R5, R3                  ; the BACKTAB address
        MOVR    R0, R5
        ADDI    #label_TWORD, R5
        MVI@    R5, R0                  ; the tile's card
        MVO@    R0, R3
@@same: INCR    R1
        DECR    R2
        BNEQ    @@b
        PULR    R3
        B       @@run
@@fast: MOVR    R1, R5
        ADDI    #const_ST, R5
@@cp:   MVI@    R4, R0
        MVO@    R0, R5
        DECR    R2
        BNEQ    @@cp
        PULR    R3
        B       @@run
@@done: PULR    PC
        ENDP

;; ARENA_DRAW: every tile of ST's map onto the screen.
ARENA_DRAW: PROC
        PSHR    R5
        MVII    #const_ST + const_OFF_MAP, R4
        MVII    #label_TPOS, R5
        MVII    #const_TILES, R3
@@t:    MVI@    R4, R0
        ANDI    #255, R0
        ADDI    #label_TWORD, R0
        MOVR    R0, R1
        MVI@    R1, R0
        MVI@    R5, R2
        MVO@    R0, R2
        DECR    R3
        BNEQ    @@t
        PULR    PC
        ENDP

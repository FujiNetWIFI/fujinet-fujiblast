;; ===========================================================================
;; io.asm -- the controllers and the mailbox, every frame: input.bas's and
;; nstrans.bas's per-frame work, which costs IntyBASIC a tenth of a frame.
;; ===========================================================================

;; READ_INPUT (the frame handler's): input.bas says how the byte is read.
;; inp_dir, inp_btn: held; l_btn, l_key: pressed (input_take takes them).
READ_INPUT: PROC
        MVI     const_TEST_HOOK, R0
        ANDI    #255, R0
        CMPI    #$A5, R0
        BNEQ    @@cont
        MVI     const_TEST_HOOK + 1, R0
        B       @@got
@@cont: MVI     $1FE, R0                ; CONT: both controllers
        XOR     $1FF, R0
@@got:  ANDI    #255, R0
        CMP     var_IN_LAST, R0
        BEQ     @@held
        MVO     R0, var_IN_LAST         ; a frame to settle
        JR      R5
@@held: MOVR    R0, R1
        ANDI    #$E0, R1                ; the row lines
        BEQ     @@disc
        CMPI    #$20, R1
        BEQ     @@key
        CMPI    #$40, R1
        BEQ     @@key
        CMPI    #$80, R1
        BEQ     @@key
        MVII    #1, R2                  ; a button, the disc beside it
        B       @@dir
@@disc: CLRR    R2
@@dir:  ANDI    #31, R0
        ADDI    #label_DISC_TAB, R0
        MOVR    R0, R4
        MVI@    R4, R0
        MVO     R0, var_INP_DIR
        CLRR    R3
        B       @@latch
@@key:  CLRR    R2
        MVO     R2, var_INP_DIR
        MOVR    R1, R3                  ; (row / 32) * 16 + column
        SLR     R3, 1
        ANDI    #15, R0
        ADDR    R0, R3
        ADDI    #label_KEY_TAB, R3
        MOVR    R3, R4
        MVI@    R4, R3
@@latch:
        MVI     var_IN_B0, R0           ; R2: a button; R3: a key
        MVO     R2, var_IN_B0
        MVO     R2, var_INP_BTN
        TSTR    R2
        BEQ     @@nb
        TSTR    R0
        BNEQ    @@nb
        MVII    #1, R0
        MVO     R0, var_L_BTN
@@nb:   MVI     var_IN_K0, R0
        MVO     R3, var_IN_K0
        TSTR    R3
        BEQ     @@nk
        CMPR    R0, R3
        BEQ     @@nk
        MVO     R3, var_L_KEY
@@nk:   JR      R5
        ENDP

;; MB_CHECK: R0 = the transaction out's answer, as fujinet.bas's fn_check:
;; 255 not yet (900 looks at most, then 0), 1 ACKed, 0 refused.
MB_CHECK: PROC
        MVI     const_FN_ACKSEQ, R0
        ANDI    #255, R0
        CMP     var_MB_SEQ, R0
        BEQ     @@ans
        MVI     var_&FN_T, R0
        INCR    R0
        MVO     R0, var_&FN_T
        CMPI    #900, R0
        BC      @@no
        MVII    #255, R0
        JR      R5
@@ans:  MVI     const_FN_REPLY_CMD, R0
        ANDI    #255, R0
        CMPI    #const_FUJICMD_ACK, R0
        BNEQ    @@no
        MVII    #1, R0
        JR      R5
@@no:   CLRR    R0
        JR      R5
        ENDP

;; MB_POST: a transaction for N1: R0 its command, R1 its parameters' count,
;; R2 the TX buffer's bytes (the parameters and the buffer staged). Its
;; SEQ follows the cartridge's ACKSEQ, as fn_post's.
MB_POST: PROC
        MVII    #const_NET_DEVICEID, R3
        MVO     R3, const_FN_DEVICE
        MVO     R0, const_FN_CMD
        MVO     R1, const_FN_NPARAM
        MVO     R2, const_FN_TXLEN_LO
        SWAP    R2
        MVO     R2, const_FN_TXLEN_HI
        MVI     const_FN_ACKSEQ, R0
        INCR    R0
        ANDI    #255, R0
        BNEQ    @@seq
        INCR    R0
@@seq:  MVO     R0, var_MB_SEQ
        MVO     R0, const_FN_SEQ
        CLRR    R0
        MVO     R0, var_&FN_T
        JR      R5
        ENDP

;; POST_STATUS: N1's STATUS (its answer: bytes waiting, 2; connected; error).
POST_STATUS: PROC
        MVII    #1, R0
        MVO     R0, const_FN_PARAM_SIZE
        MVO     R0, const_FN_PARAM_SIZE + 1
        CLRR    R0
        MVO     R0, const_FN_PARAM_VAL
        MVO     R0, const_FN_PARAM_VAL + 4
        MVII    #const_NETCMD_STATUS, R0
        MVII    #2, R1
        CLRR    R2
        B       MB_POST
        ENDP

;; POST_READ(n): N1's READ of n bytes.
POST_READ: PROC
        MVII    #2, R1
        MVO     R1, const_FN_PARAM_SIZE
        MVO     R0, const_FN_PARAM_VAL
        SWAP    R0
        MVO     R0, const_FN_PARAM_VAL + 1
        MVII    #const_NETCMD_READ, R0
        MVII    #1, R1
        CLRR    R2
        B       MB_POST
        ENDP

;; POST_WRITE(n): N1's WRITE of TXQ's first n bytes.
POST_WRITE: PROC
        PSHR    R5
        PSHR    R0
        MOVR    R0, R2
        MVII    #const_TXQ, R0
        MVII    #const_FN_TX, R1
        CALL    COPYB
        PULR    R2
        MVII    #2, R1
        MVO     R1, const_FN_PARAM_SIZE
        MVO     R2, const_FN_PARAM_VAL
        MOVR    R2, R0
        SWAP    R0
        MVO     R0, const_FN_PARAM_VAL + 1
        MVII    #const_NETCMD_WRITE, R0
        MVII    #1, R1
        CALL    MB_POST
        PULR    PC
        ENDP

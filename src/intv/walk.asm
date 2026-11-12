;; ===========================================================================
;; walk.asm -- our player's walking, a jiffy at a time: src/play.c's
;; try_dirs and src/move.c's mv_step, unit_step, walkable and bumped, step
;; for step (the server checks every move against internal/game/move.go,
;; which they copy). IntyBASIC took a third of a frame for it.
;;
;; The player is a tile-sized box at mv_x, mv_y (16 units a tile); one of
;; the two is always on a lane (a multiple of 16). Pushing into a cross lane
;; from up to LANE units off it slides onto it.
;; ===========================================================================

;; WALK(lv): the disc (inp_dir) a jiffy at speed level lv: a straight push
;; goes that way; on a diagonal, the turn first (the way we aren't going),
;; and if that's walled, on as we were. moved = 1 if it moved.
WALK:   PROC
        PSHR    R5
        MVO     R0, var_WK_LV
        MVI     var_INP_DIR, R0
        MVII    #255, R1                ; v
        MVII    #255, R2                ; h
        MOVR    R0, R3
        ANDI    #const_IN_UP, R3
        BEQ     @@nu
        MVII    #const_DIR_UP, R1
        B       @@hz
@@nu:   MOVR    R0, R3
        ANDI    #const_IN_DOWN, R3
        BEQ     @@hz
        MVII    #const_DIR_DOWN, R1
@@hz:   MOVR    R0, R3
        ANDI    #const_IN_LEFT, R3
        BEQ     @@nl
        MVII    #const_DIR_LEFT, R2
        B       @@pick
@@nl:   ANDI    #const_IN_RIGHT, R0
        BEQ     @@pick
        MVII    #const_DIR_RIGHT, R2
@@pick: CMPI    #255, R2
        BEQ     @@onev                  ; no h: v (NONE too)
        CMPI    #255, R1
        BEQ     @@oneh
        MVI     var_MV_DIR, R0          ; a diagonal: a, then b
        CMPI    #const_DIR_LEFT, R0
        BEQ     @@goh
        CMPI    #const_DIR_RIGHT, R0
        BEQ     @@goh
        MOVR    R1, R3                  ; going up or down: a = h, b = v
        MOVR    R2, R0
        B       @@two
@@goh:  MOVR    R2, R3                  ; going left or right: a = v, b = h
        MOVR    R1, R0
@@two:  PSHR    R3
        MVI     var_MV_X, R1
        PSHR    R1
        MVI     var_MV_Y, R1
        PSHR    R1
        MVI     var_MV_ACC, R1
        PSHR    R1
        CALL    STEP
        MVI     var_MOVED, R0
        TSTR    R0
        BEQ     @@back
        SUBI    #4, R6
        PULR    PC
@@back: PULR    R1
        MVO     R1, var_MV_ACC
        PULR    R1
        MVO     R1, var_MV_Y
        PULR    R1
        MVO     R1, var_MV_X
        PULR    R0
        CALL    STEP
        PULR    PC
@@onev: MOVR    R1, R0
        CALL    STEP
        PULR    PC
@@oneh: MOVR    R2, R0
        CALL    STEP
        PULR    PC
        ENDP

;; STEP(R0 = d): mv_step: a jiffy toward d (255: still) at speed level
;; wk_lv; moved = 1 if it moved; mv_bump the bomb or dud walked into.
STEP:   PROC
        PSHR    R5
        MVII    #255, R1
        MVO     R1, var_MV_BUMP
        CLRR    R1
        MVO     R1, var_MOVED
        CMPI    #255, R0
        BNEQ    @@go
        MVO     R1, var_MV_ACC
        PULR    PC
@@go:   MVO     R0, var_MV_DIR
        MVO     R0, var_WK_D
        MVI     var_WK_LV, R1
        SLL     R1, 2
        ADDI    #const_SPEED_BASE, R1   ; (SPEED_STEP is 4)
        ADD     var_MV_ACC, R1
        MOVR    R1, R2
        ANDI    #15, R1
        MVO     R1, var_MV_ACC
        SLR     R2, 2
        SLR     R2, 2                   ; the units this jiffy
        TSTR    R2
        BEQ     @@done
@@u:    PSHR    R2
        CALL    UNIT
        PULR    R2
        TSTR    R0
        BEQ     @@stop
        MVII    #1, R0
        MVO     R0, var_MOVED
        DECR    R2
        BNEQ    @@u
@@done: PULR    PC
@@stop: CLRR    R0
        MVO     R0, var_MV_ACC
        PULR    PC
        ENDP

;; UNIT: unit_step: a unit toward wk_d; R0 = 1 if it went.
UNIT:   PROC
        PSHR    R5
        MVI     var_WK_D, R0
        CMPI    #const_DIR_LEFT, R0
        BEQ     @@h
        CMPI    #const_DIR_RIGHT, R0
        BEQ     @@h
        MVII    #1, R3                  ; up or down: dy
        CMPI    #const_DIR_DOWN, R0
        BEQ     @@v1
        MVII    #$FFFF, R3
@@v1:   MVI     var_MV_X, R0
        MOVR    R0, R1
        ANDI    #15, R1                 ; off = x & 15
        SLR     R0, 2
        SLR     R0, 2                   ; c = x >> 4
        MVI     var_MV_Y, R2
        SLR     R2, 2
        SLR     R2, 2
        ADDR    R3, R2
        ANDI    #255, R2                ; r = (y >> 4) + dy
        TSTR    R1
        BEQ     @@von
        CMPI    #const_LANE + 1, R1
        BC      @@vhi
        CALL    WALKABLE                ; off <= LANE: slide back to the lane
        TSTR    R1
        BEQ     @@no
        MVI     var_MV_X, R0
        DECR    R0
        MVO     R0, var_MV_X
        B       @@yes
@@vhi:  CMPI    #const_UNIT - const_LANE, R1
        BNC     @@no
        INCR    R0                      ; off >= UNIT - LANE: on to the next
        CALL    WALKABLE
        TSTR    R1
        BEQ     @@no
        MVI     var_MV_X, R0
        INCR    R0
        MVO     R0, var_MV_X
        B       @@yes
@@von:  MVI     var_MV_Y, R1
        ANDI    #15, R1
        BNEQ    @@vgo
        CALL    WALKABLE
        TSTR    R1
        BNEQ    @@vgo
        CALL    BUMPED
        B       @@no
@@vgo:  MVI     var_MV_Y, R0
        ADDR    R3, R0
        MVO     R0, var_MV_Y
        B       @@yes

@@h:    MVII    #1, R3                  ; left or right: dx
        CMPI    #const_DIR_RIGHT, R0
        BEQ     @@h1
        MVII    #$FFFF, R3
@@h1:   MVI     var_MV_Y, R2
        MOVR    R2, R1
        ANDI    #15, R1                 ; off = y & 15
        SLR     R2, 2
        SLR     R2, 2                   ; r = y >> 4
        MVI     var_MV_X, R0
        SLR     R0, 2
        SLR     R0, 2
        ADDR    R3, R0
        ANDI    #255, R0                ; c = (x >> 4) + dx
        TSTR    R1
        BEQ     @@hon
        CMPI    #const_LANE + 1, R1
        BC      @@hhi
        CALL    WALKABLE
        TSTR    R1
        BEQ     @@no
        MVI     var_MV_Y, R0
        DECR    R0
        MVO     R0, var_MV_Y
        B       @@yes
@@hhi:  CMPI    #const_UNIT - const_LANE, R1
        BNC     @@no
        INCR    R2
        CALL    WALKABLE
        TSTR    R1
        BEQ     @@no
        MVI     var_MV_Y, R0
        INCR    R0
        MVO     R0, var_MV_Y
        B       @@yes
@@hon:  MVI     var_MV_X, R1
        ANDI    #15, R1
        BNEQ    @@hgo
        CALL    WALKABLE
        TSTR    R1
        BNEQ    @@hgo
        CALL    BUMPED
        B       @@no
@@hgo:  MVI     var_MV_X, R0
        ADDR    R3, R0
        MVO     R0, var_MV_X
@@yes:  MVII    #1, R0
        PULR    PC
@@no:   CLRR    R0
        PULR    PC
        ENDP

;; WALKABLE: R1 = 1 if tile R0, R2 (column, row; 255 is off the arena) can
;; be walked on: not our bomb that is coming (pend_tile), nor a pillar,
;; block, burning block, bomb or dud. R0, R2, R3 kept.
WALKABLE: PROC
        CLRR    R1
        CMPI    #const_ARENA_W, R0
        BC      @@out
        CMPI    #const_ARENA_H, R2
        BC      @@out
        MOVR    R2, R1                  ; t = r * 13 + c
        MOVR    R2, R4
        SLL     R1, 2
        ADDR    R1, R4
        SLL     R1, 1
        ADDR    R4, R1
        ADDR    R0, R1
        CMP     var_PEND_TILE, R1
        BEQ     @@no
        ADDI    #const_ST + const_OFF_MAP, R1
        MOVR    R1, R4
        MVI@    R4, R1
        ANDI    #255, R1
        DECR    R1                      ; T_PILLAR..T_DUD are 1-5
        CMPI    #5, R1
        BNC     @@no
        MVII    #1, R1
        JR      R5
@@no:   CLRR    R1
@@out:  JR      R5
        ENDP

;; BUMPED: walked into tile R0, R2: a bomb or a dud there is mv_bump. (R3
;; used.)
BUMPED: PROC
        CMPI    #const_ARENA_W, R0
        BC      @@out
        CMPI    #const_ARENA_H, R2
        BC      @@out
        MOVR    R2, R1
        MOVR    R2, R4
        SLL     R1, 2
        ADDR    R1, R4
        SLL     R1, 1
        ADDR    R4, R1
        ADDR    R0, R1                  ; t
        MOVR    R1, R4
        ADDI    #const_ST + const_OFF_MAP, R4
        MVI@    R4, R3
        ANDI    #255, R3
        CMPI    #const_T_BOMB, R3
        BEQ     @@b
        CMPI    #const_T_DUD, R3
        BNEQ    @@out
@@b:    MVO     R1, var_MV_BUMP
@@out:  JR      R5
        ENDP

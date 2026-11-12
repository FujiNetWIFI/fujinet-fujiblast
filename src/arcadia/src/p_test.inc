; p_test.inc -- the rigs' offline scenes (src/test.c). At start-up a rig
; pokes TESTHK (the boot page waits 20 frames for it): $5A in [0], the
; scene in [2], its argument in [3]; we answer $A5 in [0]. The state is
; test.c's canned one (canned.inc); the link is quiet (FL_TEST: p_net's
; step brings nothing), and the main loop runs the table's screens:
;   1  the arena, seat 0 (ANN) walked by the disc; [3]: b0 kick, b1 glove,
;      b4-6 speed; a blast in the top lane every 4 seconds, and back
;   2  the waiting room       3  a round's end       4  the game's end

        BCTA,UN TSTART          ; E_ENTER (never comes back: the table's loop)
        RETC,UN                 ; E_STATE
        NOP
        NOP
        BCTA,UN TTICK           ; E_TICK: the main loop's, each frame

TSTART: CPTR    ST              ; the canned state
        LODI,R3 $FF
TS1:    LODA,R0 CANNED,R3,+
        BSTA,UN RAMPUT1
        COMI,R3 STATE_SIZE-1
        BCFR,EQ TS1
        BSTA,UN RAMPEND
        LODI,R3 TABLE_LEN+NAME_LEN+2    ; ANN at blast1
TS2:    LODA,R0 TNAMES-1,R3
        STRA,R0 MYNAME-1,R3
        BDRR,R3 TS2
        LODA,R0 FLAGS
        IORI,R0 FL_TEST
        STRA,R0 FLAGS
        LODI,R0 NL_LINK|NL_JOINED|NL_HEARD
        STRA,R0 NLINK
        EORZ    R0              ; seat 0 is ours (ANN)
        STRA,R0 MYSEAT
        STRA,R0 TSTCH
        STRA,R0 TSTCL
        LODA,R0 TESTHK+2
        COMI,R0 1
        BCFR,EQ TS4
        LODA,R0 TESTHK+3        ; 1: the arena, our powers
        ANDI,R0 PF_KICK|PF_GLOVE
        CPUT    ST+OFF_PLAYERS+P_FLAGS
        LODA,R0 TESTHK+3
        RRR,R0
        RRR,R0
        RRR,R0
        RRR,R0
        ANDI,R0 7
        CPUT    ST+OFF_PLAYERS+P_SPEED
        BCTA,UN TABLE2
TS4:    COMI,R0 2
        BCFR,EQ TS5
        LODI,R0 PH_WAIT         ; 2: the waiting room
        CPUT    ST+OFF_PHASE
        BCTA,UN TABLE2
TS5:    LODI,R1 PH_ROUNDEND     ; 3, 4: the trophies
        COMI,R0 3
        BCTR,EQ TS6
        LODI,R1 PH_GAMEOVER
TS6:    LODZ    R1
        CPUT    ST+OFF_PHASE
        LODI,R0 5
        CPUT    ST+OFF_TIMER
        BCTA,UN TABLE2

; TTICK: a frame of the scene: the first brings "a state"; in the arena,
; every 240 frames the top lane's blocks blast, or come back.
TTICK:  LODA,R0 TSTCL
        ADDI,R0 1
        STRA,R0 TSTCL
        COMI,R0 1
        BCFR,EQ TT1
        LODA,R0 TSTCH
        BCFR,EQ TT1
        LODA,R0 NEVT
        IORI,R0 EV_STATE
        STRA,R0 NEVT
        RETC,UN
TT1:    LODA,R0 TESTHK+2
        COMI,R0 1
        RETC,LT
        RETC,GT
        LODA,R0 TSTCL           ; 240 frames
        COMI,R0 240
        RETC,LT
        EORZ    R0
        STRA,R0 TSTCL
        LODA,R0 TSTCH           ; (TSTCH: blasts so far, its b0 on or off)
        ADDI,R0 1
        STRA,R0 TSTCH
        CPTR    ST+OFF_MAP+2
        LODI,R3 2
TT2:    LODA,R0 TSTCH
        ANDI,R0 1
        BCTR,EQ TT3
        LODI,R0 T_FLAME_H       ; on: a blast across
        BCTR,UN TT5
TT3:    LODI,R0 T_BLOCK         ; off: the blocks back, the lane's ends floor
        COMI,R3 2
        BCTR,EQ TT4
        COMI,R3 10
        BCFR,EQ TT5
TT4:    EORZ    R0
TT5:    BSTA,UN RAMPUT1
        ADDI,R3 1
        COMI,R3 11
        BCFR,EQ TT2
        BSTA,UN RAMPEND
        LODA,R0 ST+OFF_TIMER
        SUBI,R0 1
        CPUT    ST+OFF_TIMER
        LODA,R0 MAPDL           ; the top row redrawn; a blast's boom
        IORI,R0 1
        STRA,R0 MAPDL
        LODA,R0 TSTCH
        ANDI,R0 1
        BCTR,EQ TT6
        LODI,R0 MF_MAP|MF_BOOM
        BCTR,UN TT7
TT6:    LODI,R0 MF_MAP
TT7:    IORA,R0 MFLAGS
        STRA,R0 MFLAGS
        LODA,R0 NEVT
        IORI,R0 EV_STATE
        STRA,R0 NEVT
        RETC,UN

        ASCII
TNAMES: DB      "ANN",0,0,0,0,0,0, "blast1",0,0,0
        GLYPHS
        INCLUDE "canned.inc"

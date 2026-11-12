; p_test.inc -- the rigs' offline scenes (src/test.c). At start-up a rig
; pokes TESTHK (the boot page waits 20 frames for it): 5AH in [0], the
; scene in [2], its argument in [3]; we answer A5H in [0]. The state is
; test.c's canned one (canned.inc); the link is quiet (FL_TEST: p_net's
; step brings nothing), and the main loop runs the table's screens. The
; Arcadia's (src/arcadia/src/p_test.inc), transcribed:
;   1  the arena, seat 0 (ANN) walked by the handle; [3]: b0 kick, b1 glove,
;      b4-6 speed; a blast in the top lane every 4 seconds, and back
;   2  the waiting room       3  a round's end       4  the game's end
;   5  the title (and the name wheel after it if [3] = 1)

        JP      TSTART          ; E_ENTER (never comes back)
        RET                     ; E_STATE
        NOP
        NOP
        JP      TTICK           ; E_TICK: the main loop's, each frame

TSTART: LD      A,(TESTHK+2)
        CP      6
        JP      Z,TSND
        CP      5
        JR      NZ,TS0
        LD      A,(TESTHK+3)    ; 5: the title (1: no name, so the wheel)
        OR      A
        JR      NZ,TS5A
        LD      HL,TNAMES
        LD      DE,MYNAME
        LD      BC,4
        LDIR
TS5A:   GOPAGE  PG_MENU,E_ENTER
TS5B:   CALL    FRAME
        JR      TS5B
TS0:    LD      HL,CANNED       ; the canned state
        LD      DE,ST
        LD      BC,STATE_SIZE
        LDIR
        LD      HL,TNAMES       ; ANN at blast1
        LD      DE,MYNAME
        LD      BC,TABLE_LEN+NAME_LEN+2
        LDIR
        LD      A,(FLAGS)
        OR      FL_TEST
        LD      (FLAGS),A
        LD      A,NL_LINK|NL_JOINED|NL_HEARD
        LD      (NLINK),A
        XOR     A               ; seat 0 is ours (ANN)
        LD      (MYSEAT),A
        LD      HL,0
        LD      (TSTC),HL
        LD      A,(TESTHK+2)
        CP      1
        JR      NZ,TS4
        LD      A,(TESTHK+3)    ; 1: the arena, our powers
        LD      B,A
        AND     PF_KICK|PF_GLOVE
        LD      (ST+OFF_PLAYERS+P_FLAGS),A
        LD      A,B
        RRCA
        RRCA
        RRCA
        RRCA
        AND     7
        LD      (ST+OFF_PLAYERS+P_SPEED),A
        JP      TABLE2
TS4:    CP      2
        JR      NZ,TS5
        LD      A,PH_WAIT       ; 2: the waiting room
        LD      (ST+OFF_PHASE),A
        JP      TABLE2
TS5:    LD      B,PH_ROUNDEND   ; 3, 4: the trophies
        CP      3
        JR      Z,TS6
        LD      B,PH_GAMEOVER
TS6:    LD      A,B
        LD      (ST+OFF_PHASE),A
        LD      A,5
        LD      (ST+OFF_TIMER),A
        JP      TABLE2

; TTICK: a frame of the scene: the first brings "a state"; in the arena,
; every 240 frames the top lane's blocks blast, or come back.
TTICK:  LD      HL,(TSTC)
        INC     L               ; (L: frames, H: blasts so far)
        LD      (TSTC),HL
        LD      A,H
        OR      A
        JR      NZ,TT1
        LD      A,L
        CP      1
        JR      NZ,TT1
        LD      A,(NEVT)
        OR      EV_STATE
        LD      (NEVT),A
        RET
TT1:    LD      A,(TESTHK+2)
        CP      1
        RET     NZ
        LD      A,L             ; 240 frames
        CP      240
        RET     C
        INC     H
        LD      L,0
        LD      (TSTC),HL
        LD      C,2             ; tiles 2-10 of the top row
TT2:    LD      A,H
        AND     1
        LD      A,T_FLAME_H     ; on: a blast across
        JR      NZ,TT5
        LD      A,C             ; off: the blocks back, the lane's ends floor
        CP      2
        JR      Z,TT4
        CP      10
        LD      A,T_BLOCK
        JR      NZ,TT5
TT4:    XOR     A
TT5:    PUSH    HL
        LD      B,0
        LD      HL,ST+OFF_MAP
        ADD     HL,BC
        CP      (HL)
        LD      (HL),A
        LD      A,C
        CALL    NZ,MARKT
        POP     HL
        INC     C
        LD      A,C
        CP      11
        JR      NZ,TT2
        LD      A,(ST+OFF_TIMER)
        DEC     A
        LD      (ST+OFF_TIMER),A
        LD      A,H             ; a blast's boom
        AND     1
        LD      A,MF_MAP
        JR      Z,TT7
        LD      A,MF_MAP|MF_BOOM
TT7:    LD      HL,MFLAGS
        OR      (HL)
        LD      (HL),A
        LD      A,(NEVT)
        OR      EV_STATE
        LD      (NEVT),A
        RET

; TSND: scene 6, the sound board: every sound and jingle in turn, 150
; frames apart (the rigs record it: ASTROSIM_WAV), then quiet.
TSND:   LD      HL,B_MSG
        CALL    SETBANDS
        LD      HL,P_TEXT
        CALL    SETPAL
        LD      HL,SBOARD
        LD      D,35
        LD      C,3*4+0
        CALL    CENTRE
        XOR     A
TSN1:   PUSH    AF
        CALL    SNDQUE
        LD      B,150
TSN2:   PUSH    BC
        CALL    FRAME
        POP     BC
        DJNZ    TSN2
        POP     AF
        INC     A
        CP      MUS_GAME+1
        JR      NZ,TSN1
TSN3:   CALL    FRAME
        JR      TSN3
SBOARD: DB      "THE SOUND BOARD",0

TNAMES: DB      "ANN",0,0,0,0,0,0, "blast1",0,0,0

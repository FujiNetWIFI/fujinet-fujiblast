; p_test.inc -- the rigs' offline scenes (src/test.c): the screens on
; canned states, no network. At the start TESTHK is 3C C3 for 20 frames;
; tools/s2sim's `test N ARG` pokes TESTHK+1 = N + 1, +2 = ARG, then $5A,
; and we answer $A5:
;   1  the arena, seat 0 ours (the keypad walks it), a blast every 4 s
;   2  the waiting room       3  a round's end       4  the game's end
;   5  the bench: four walking, two rows blowing up and clearing a second
;      (src/test.c walkers)
;   6  the sound board: every cue in turn, 90 jiffies apart (S2SIM_WAV)

; t_check: D = the scene asked for (0: none).
        RFIT    52
t_check:
        SETV    TESTHK, 3CH
        SETV    TESTHK+1, 0C3H
        LDI     20
        PLO     R9
qtc1:    GETV    TESTHK
        XRI     5AH
        BZ      qtc2
        FCALL   frame
        DEC     R9
        GLO     R9
        BNZ     qtc1
        SETV    TESTHK, 0
        LDI     0
        FRET
qtc2:    SETV    TESTHK, 0A5H
        GETV    TESTHK+1
        SMI     1
        FRET

; t_canned: src/test.c's canned state into ST.
        RFIT    64
t_canned:
        POKEAT  ST
        LDR     R8, CANNED
        LDI     STATE_SIZE
        PLO     R9
qtcn1:   LDA     R8
        PKD
        DEC     R9
        GLO     R9
        BNZ     qtcn1
        PKEND
        SETV    MYSEAT, 0
        FRET

; t_poke: ST[R9.0] = R9.1, and its tile queued if it is a map byte.
        RFIT    60
t_poke: GLO     R9
        POKEST
        GHI     R9
        PKD
        PKEND
        GLO     R9
        SMI     OFF_MAP
        BNF     qtpk9
        SMI     TILES
        BDF     qtpk9
        ADI     TILES
        FJMP    dq_push
qtpk9:   FRET

; ---- scene 1: the arena ------------------------------------------------------
        RFIT    95
t_arena:
        FCALL   t_canned
        GETV    FLAGS
        ORI     FL_TEST
        STR     RA
        SETV    SHOWS, S_ARENA
        FCALL   ar_enter
        SETV    TBON, 0                   ; TBT:TBON jiffies to the blast
        SETV    TBT, 240
qta1:    FCALL   frame
        GETV    DT
        STR     R2
        GETV    TBT
        SM
        STR     RA
        BZ      qta2
        BDF     qta3
qta2:    SETV    TBT, 240                 ; the top lane blasts, then clears
        GETV    TBON
        XRI     1
        STR     RA
        FCALL   t_lane
        FCALL   arena_state
qta3:    FCALL   in_poll
        FCALL   ar_pass
        FCALL   snd_tick
        INCV    PASSES
        BNZ     qta1
        INCV    PASSES+1
        BR      qta1

; t_lane: the top lane's tiles 2-10: flames (TBON = 1, a blast), else back.
        RFIT    58
t_lane: LDI     2
        PLO     R9
qtl1:    GETV    TBON
        BZ      qtl2
        LDI     T_FLAME_H
        BR      qtl4
qtl2:    GLO     R9
        XRI     2
        BZ      qtl3
        GLO     R9
        XRI     10
        BZ      qtl3
        LDI     T_BLOCK
        BR      qtl4
qtl3:    LDI     T_FLOOR
qtl4:    PHI     R9
        GLO     R9
        ADI     OFF_MAP
        PLO     R9
        FCALL   t_poke
        GLO     R9
        SMI     OFF_MAP-1
        PLO     R9
        XRI     11
        BNZ     qtl1
        GETV    TBON
        BZ      qtl9
        SETV    MFLAGS, MF_BOOM
qtl9:    FRET

; ---- scene 5: the bench --------------------------------------------------------
; seat 0 (ours) walks the left lane down and up; seats 1-3 go back and forth
; on theirs (the bottom row, the right lane, row 2); rows 0 and 8 blow up
; and clear every second. TBW the walk (0-160), TBB its way (0 out, 1 back),
; TBT jiffies to the blasts, TBON on.
        RFIT    171
t_bench:
        FCALL   t_canned
        LDI     0                       ; the outer lanes, rows 2 and 10: floor
        PLO     R9
qtb1:    GLO     R9                      ; col 0 and col 12 of row r
        PLO     R8
        LDI     0
        PHI     R9
        GLO     R9
        ADI     LO(TROW)
        PLO     RC
        LDI     HI(TROW)
        PHI     RC
        LDN     RC
        ADI     OFF_MAP
        PLO     R9
        FCALL   t_poke
        GLO     R9
        ADI     ARENA_W-1
        PLO     R9
        FCALL   t_poke
        GLO     R8
        ADI     1
        PLO     R9
        XRI     ARENA_H
        BNZ     qtb1
        LDI     OFF_MAP+2*ARENA_W       ; row 2
        FCALL   t_row0
        LDI     OFF_MAP+10*ARENA_W      ; row 10
        FCALL   t_row0
        LDI     OFF_ALIVE
        PLO     R9
        LDI     0FH
        PHI     R9
        FCALL   t_poke
        SETV    WX, 0
        SETV    WY, 0
        SETV    WDIR, DIR_DOWN
        SETV    TBW, 0
        SETV    TBB, 0
        SETV    TBON, 0
        SETV    TBT, 60
        FCALL   arena_enter
        FCALL   arena_state
qtbl:    FCALL   frame
        FCALL   t_walk                  ; the walks, DT jiffies on
        GETV    DT                      ; the blasts, every 60
        STR     R2
        GETV    TBT
        SM
        STR     RA
        BZ      qtb3
        BDF     qtb4
qtb3:    SETV    TBT, 60
        GETV    TBON
        XRI     1
        STR     RA
        FCALL   t_rows
        FCALL   arena_state
qtb4:    FCALL   arena_tick
        FCALL   snd_tick
        INCV    PASSES
        BNZ     qtbl
        INCV    PASSES+1
        BR      qtbl

; t_row0: the 13 tiles of the row at ST offset D: floor.
        RFIT    16
t_row0: PLO     R9
        LDI     T_FLOOR
        PHI     R9
        LDI     ARENA_W
        PLO     R8
qtr1:    FCALL   t_poke
        INC     R9
        DEC     R8
        GLO     R8
        BNZ     qtr1
        FRET

        PFIT    ARENA_H
TROW:
tt      SET     0
        WHILE   tt < ARENA_H
        DB      tt * ARENA_W
tt      SET     tt + 1
        ENDM

; t_walk: a jiffy of the bench's walks.
        RFIT    170
t_walk: GETV    DT                      ; TBW += DT out, -= DT back; turn at the
        STR     R2                      ;  ends (0, 160)
        GETV    TBB
        BNZ     qtw1
        GETV    TBW
        ADD
        STR     RA
        SMI     160
        BNF     qtw4
        LDI     160
        STR     RA
        BR      qtw3
qtw1:    GETV    TBW
        SM
        STR     RA
        BZ      qtw3
        BDF     qtw4
        LDI     0
        STR     RA
qtw3:    GETV    TBB
        XRI     1
        STR     RA
qtw4:    GETV    TBW                      ; ours: down the left lane and up
        PUTV    WY
        GETV    TBB
        SHL
        XRI     DIR_DOWN
        PUTV    WDIR
        SETV    WMOVED, 1
        LDI     OFF_PLAYERS+PLAYER_SIZE ; seat 1: x falls on the bottom row
        PLO     R9
        GETV    TBW
        SDI     MAX_X
        PHI     R9
        FCALL   t_poke
        INC     R9
        LDI     MAX_Y
        PHI     R9
        FCALL   t_poke
        INC     R9
        FCALL   t_dir
        XRI     DIR_LEFT
        PHI     R9
        FCALL   t_poke
        LDI     OFF_PLAYERS+2*PLAYER_SIZE ; seat 2: y rises in the right lane
        PLO     R9
        LDI     MAX_X
        PHI     R9
        FCALL   t_poke
        INC     R9
        GETV    TBW
        PHI     R9
        FCALL   t_poke
        INC     R9
        FCALL   t_dir
        XRI     DIR_DOWN
        PHI     R9
        FCALL   t_poke
        LDI     OFF_PLAYERS+3*PLAYER_SIZE ; seat 3: x rises on row 2
        PLO     R9
        GETV    TBW
        PHI     R9
        FCALL   t_poke
        INC     R9
        LDI     2*UNIT
        PHI     R9
        FCALL   t_poke
        INC     R9
        FCALL   t_dir
        XRI     DIR_RIGHT
        PHI     R9
        FCALL   t_poke
        FRET

; t_dir: D = (the way back << 1) | moving, to be XORed with the way out.
t_dir:  GETV    TBB
        SHL
        ORI     P_DIR_MOVING
        FRET

; t_rows: rows 0 and 8, tiles 1-11: flames (TBON), else as they were.
        RFIT    89
t_rows: LDI     1
        PLO     R8
qtrw1:   GETV    TBON                      ; row 0
        BZ      qtrw2
        LDI     T_FLAME_H
        BR      qtrw4
qtrw2:   GLO     R8
        SMI     3
        BNF     qtrw3
        SMI     7
        BDF     qtrw3
        LDI     T_BLOCK
        BR      qtrw4
qtrw3:   LDI     T_FLOOR
qtrw4:   PHI     R9
        GLO     R8
        ADI     OFF_MAP
        PLO     R9
        FCALL   t_poke
        GETV    TBON                      ; row 8
        BZ      qtrw5
        LDI     T_FLAME_H
        BR      qtrw7
qtrw5:   GLO     R8
        XRI     4
        BZ      qtrw6
        GLO     R8
        XRI     8
        BZ      qtrw6
        LDI     T_FLOOR
        BR      qtrw7
qtrw6:   LDI     T_BLOCK
qtrw7:   PHI     R9
        GLO     R8
        ADI     OFF_MAP+8*ARENA_W
        PLO     R9
        FCALL   t_poke
        INC     R8
        GLO     R8
        XRI     ARENA_W-1
        BNZ     qtrw1
        GETV    TBON
        BZ      qtrw9
        SETV    MFLAGS, MF_BOOM
qtrw9:   FRET

; ---- scenes 2, 3, 4: the waiting room, a round's end, the game's end -------------
        RFIT    137
t_screens:
        GETV    T7                      ; (the scene: 2, 3, 4)
        PUTV    TBW
        FCALL   t_canned
        GETV    FLAGS
        ORI     FL_TEST
        STR     RA
        SETV    SRVTBL, 'b'
        SETV    SRVTBL+1, 0
        GETV    TBW
        XRI     2
        BNZ     qtsc1
        LDI     PH_WAIT                 ; the room
        PHI     R9
        LDI     OFF_PHASE
        PLO     R9
        FCALL   t_poke
        SETV    SHOWS, S_WAIT
        BR      qtsc3
qtsc1:   GETV    TBW                      ; the trophies: 5 s to the next
        XRI     3
        BZ      qtsc2
        LDI     PH_GAMEOVER
        BR      qtsc2b
qtsc2:  LDI     PH_ROUNDEND
qtsc2b:
        PHI     R9
        LDI     OFF_PHASE
        PLO     R9
        FCALL   t_poke
        LDI     5
        PHI     R9
        LDI     OFF_TIMER
        PLO     R9
        FCALL   t_poke
        GETV    TBW
        XRI     3
        BZ      qtsc2a
        SETV    SHOWS, S_GAME
        BR      qtsc3
qtsc2a:  SETV    SHOWS, S_ROUND
qtsc3:   LDI     0
        FCALL   scr_call
qtsc4:   FCALL   frame
        FCALL   in_poll
        FCALL   snd_tick
        LDI     2
        FCALL   scr_call
        INCV    PASSES
        BR      qtsc4

; ---- scene 6: the sound board ---------------------------------------------------
        RFIT    66
t_sounds:
        OPN     TOP_CLS
        SETV    TBW, 0                  ; the cue
qts1:   GETV    TBW
        FCALL   snd_play
        SETV    TBT, 90
qts2:   FCALL   frame
        FCALL   snd_tick
        GETV    DT
        STR     R2
        GETV    TBT
        SM
        STR     RA
        BZ      qts3
        BDF     qts2
qts3:   INCV    TBW
        XRI     MUS_GAME+1
        BNZ     qts1
qts4:   FCALL   frame
        BR      qts4

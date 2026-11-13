; main.inc -- the start and the main loop (src/main.c; the Channel F's
; main.inc, transcribed).

; ---- start --------------------------------------------------------------------
        RFIT    33
main:   LDI     HI(RAMVARS)
        PHI     RA
        LDI     HI(ST)
        PHI     RB
        LDR     R8, V_APP               ; our RAM: zeros, up to the stack
mn1:    LDI     0
        STR     R8
        INC     R8
        GHI     R8
        XRI     HI(STKLOW)
        BNZ     mn1
mn2:    LDI     0
        STR     R8
        INC     R8
        GLO     R8
        XRI     LO(STKLOW)
        BNZ     mn2
        FJMP    start

        RFIT    147
start:  PEEK    FN_MAGIC                ; a FujiNet cart, protocol 2?
        XRI     'F'
        BNZ     st_nocart
        PEEK    FN_MAGIC+1
        XRI     'N'
        BNZ     st_nocart
        PEEK    FN_PROTOVER
        SMI     2
        BNF     st_nocart
        GLO     RE
        PUTV    LASTRE
        FJMP    boot
st_nocart:
        OPN     TOP_CLS
        PARS
        P       4
        P       50
        P       TM_SET
        P       'N'
        P       'E'
        P       'E'
        P       'D'
        P       'S'
        P       ' '
        P       'F'
        P       'U'
        P       'J'
        P       'I'
        P       'N'
        P       'E'
        P       'T'
        GOP     TOP_PTEXT
st4:    BR      st4

; ---------------------------------------------------------------------------
; boot -- the pictures into the cart, the rigs' scenes; the appkeys, the
;   title, a name if there is none (kept); then the game (main.c).
; ---------------------------------------------------------------------------
S_NONE  EQU     0
S_WAIT  EQU     1
S_ARENA EQU     2
S_ROUND EQU     3
S_GAME  EQU     4

        RFIT    98
boot:   OPN     TOP_CLS
        SETV    KDIG, NONE
        SETV    KEV, NONE
        SETV    MYSEAT, NONE
        FCALL   gfx_boot
        FCALL   t_check
        PUTV    T7                      ; (the scene: t_screens reads it)
        BZ      bt5
        XRI     1
        BNZ     bt1
        FJMP    t_arena
bt1:    GETV    T7
        XRI     5
        BNZ     bt1a
        FJMP    t_bench
bt1a:   GETV    T7
        XRI     6
        BNZ     bt2
        FJMP    t_sounds
bt2:    FJMP    t_screens               ; 2, 3, 4: the room, the trophies
bt5:    FCALL   ak_load
        FCALL   title
        GETV    MYNAME
        BZ      bt6
        FJMP    top
bt6:    FCALL   name_entry
        FCALL   ak_save_name
        FJMP    top

; ---- the rooms, when no table is known (menus.c table_list) ------------------
        RFIT    53
top:    OPN     TOP_SPRHIDE
        GETV    SRVTBL
        BZ      tp1
        FJMP    table
tp1:    SETV    MSGID, M_CONN
        FCALL   msg_show
        FCALL   sock_open
        BNZ     tpna
        FCALL   ninit
        FCALL   rooms
        BZ      tpna
        FJMP    table
tpna:   SETV    MSGID, M_NOANS          ; (no room to forget)
        FJMP    tfail

; ---- at a table (main.c play_table) --------------------------------------------
        RFIT    40
table:  FCALL   ak_save_room
        SETV    MSGID, M_JOIN
        FCALL   msg_show
        GETV    NLINK
        ANI     NL_LINK
        BNZ     tb1
        FCALL   sock_open
        BZ      tb1
        FJMP    tnoans
tb1:    FCALL   nhello                  ; the link's counters, ST zeroed, HELLO
table2: SETV    SHOWS, S_NONE           ; (the test scenes come in here)
        FJMP    mainlp

; the pass at a table: the frame, the keys, the link's step; the screen the
; state's phase wants, picked only at a commit; its pass.
        RFIT    165
mainlp: FCALL   frame
        FCALL   in_poll
        FCALL   snd_tick
        FCALL   nstep                   ; NEVT
        GETV    NEVT
        ANI     EV_ERROR+EV_LOST+EV_NOANS
        BZ      ml1
        LDN     RA
        ANI     EV_ERROR
        BZ      ml0
        FJMP    terr
ml0:    LDN     RA
        ANI     EV_LOST
        BZ      ml0a
        FJMP    tlost
ml0a:   FJMP    tnoans
ml1:    GETV    KNEW                    ; 0: leave
        ANI     KB_MENU
        BZ      ml2
        LDN     RA
        XRI     KB_MENU
        STR     RA
        FJMP    tleave
ml2:    GETV    NLINK                   ; no state yet: nothing to show
        ANI     NL_HEARD
        BZ      mlend
        GETV    NEVT                    ; a screen is picked only at a commit
        ANI     EV_STATE
        BZ      ml7
        GETS    OFF_PHASE               ; the screen it wants
        SMI     PH_GAMEOVER+1
        BNF     ml3
        LDI     0FFH                    ; (past the last: the room)
ml3:    ADI     PH_GAMEOVER+1
        ADI     LO(SCRFOR)
        PLO     R8
        LDI     HI(SCRFOR)
        PHI     R8
        LDN     R8
        PLO     R9                      ; R9.0 the screen
        STR     R2
        GETV    SHOWS
        XOR
        BNZ     ml5
        GLO     R9                      ; the arena again: a new round's?
        XRI     S_ARENA
        BNZ     ml6
        GETS    OFF_SEQ
        STR     R2
        GETV    SHSEQ
        XOR
        BZ      ml6
ml5:    GLO     R9                      ; another screen: drawn afresh
        PUTV    SHOWS
        GETS    OFF_SEQ
        PUTV    SHSEQ
        LDI     0
        BR      ml6a
ml6:    LDI     1                       ; the same: the new state on it
ml6a:   FCALL   scr_call
ml7:    LDI     2                       ; and its pass
        FCALL   scr_call
mlend:  INCV    PASSES                  ; (the rigs count the passes)
        BNZ     ml9
        INCV    PASSES+1
ml9:    FJMP    mainlp

; scr_call: entry D (0 enter, 1 state, 2 pass) of the screen SHOWS.
        RFIT    27
scr_call:
        PLO     R9
        GETV    SHOWS
        SHL
        STR     R2
        SHL
        ADD                             ; 6 SHOWS
        STR     R2
        GLO     R9
        SHL
        ADD                             ; + 2 entry
        ADI     LO(SCRTAB)
        PLO     R8
        LDI     HI(SCRTAB)
        PHI     R8
        LDA     R8
        PHI     R9
        LDN     R8
        PLO     R9
        ICALL
        FRET

        PFIT    40
; by phase: the screen; by screen: its enter, state, pass
SCRFOR: DB      S_WAIT, S_ARENA, S_ARENA, S_ROUND, S_GAME, S_WAIT
SCRTAB: DW      no_op, no_op, no_op
        DW      wait_enter, wait_state, wait_tick
        DW      ar_enter, ar_state, ar_pass
        DW      res_round, res_state, res_tick
        DW      res_game, res_state, res_tick

no_op:  FRET
res_round:
        LDI     0
        FJMP    res_enter
res_game:
        LDI     1
        FJMP    res_enter

; the arena's entries: ours (p_play) and the drawing's (p_arena)
        RFIT    35
ar_enter:
        FCALL   play_reset
        FCALL   arena_enter
        FCALL   arena_state
        GETS    OFF_PHASE
        XRI     PH_COUNTDOWN
        BNZ     are9
        SND     MUS_ROUND
are9:   FRET
ar_state:
        FCALL   play_state
        FJMP    arena_state
ar_pass:
        FCALL   play_pass
        FJMP    arena_tick

; ---- leaving and failing --------------------------------------------------------
; the server said no
        RFIT    142
terr:   GETV    NERR
        XRI     ERR_VERSION
        BZ      te1
        SETV    MSGID, M_NOROOM+80H
        BR      tfail
te1:    SETV    MSGID, M_VERSION+80H
        BR      tfail
; the link went: say so, then join again (the room kept)
tlost:  SETV    MSGID, M_LOST
        BR      tfail
; 0: give the seat up, BYE (a few passes to get it out), close
tleave: GETV    NLINK
        ANI     NL_JOINED
        BZ      tlv1
        LDI     0
        PLO     R8
        PHI     R8
        LDI     ACT_LEAVE
        FCALL   act_q
tlv1:   GETV    TXF
        ORI     TX_BYE
        STR     RA
        SETV    MRC, 60
tlv2:   FCALL   frame
        FCALL   nstep
        GETV    TXF
        BNZ     tlv3
        GETV    NKIND
        BZ      tlv4
tlv3:   VAR     MRC
        LDN     RA
        SMI     1
        STR     RA
        BNZ     tlv2
tlv4:   SETV    MSGID, M_QUIET+80H
        BR      tfail
; no answer: give the table up
tnoans: SETV    MSGID, M_NOANS+80H
; tfail: the socket closed, the message (b7: the room forgotten too), then the
; rooms again.
tfail:  GETV    FLAGS                   ; (a test scene is over too)
        ANI     0FFH-FL_TEST
        STR     RA
        OPN     TOP_SPRHIDE
        FCALL   sock_close
        LDI     120
        FCALL   msg_wait
        GETV    MSGID
        ANI     80H
        BZ      tf1
        FCALL   ak_clear_room
tf1:    FJMP    top

; ---------------------------------------------------------------------------
; frame -- wait for the ISR's frame count (RE) to move; DT = the jiffies
;   since the last pass (at most 8: a longer stall is not caught up),
;   JIFFY += DT.
; ---------------------------------------------------------------------------
        RFIT    47
frame:  VAR     LASTRE
frm1:   GLO     RE
        SEX     RA
        SM                              ; RE.0 - LASTRE
        SEX     R2
        BZ      frm1
        SMI     9
        BNF     frm2
        LDI     0FFH                    ; more than 8: 8
frm2:   ADI     9
        PUTV    DT
        GLO     RE
        PUTV    LASTRE
        GETV    DT
        STR     R2
        GETV    JIFFY
        ADD
        STR     RA
        INC     RA
        LDN     RA
        ADCI    0
        STR     RA
        FRET

; icall (ICALL): the routine at R9, as if the caller had FCALLed it: it
; takes over icall's frame, so its FRET goes straight back (M.U.L.E.'s).
        RFIT    12
jmp_r9: GHI     R9
        PHI     R3
        GLO     R9
        PLO     R3
        SEP     R3
icall:  LDR     R8, jmp_r9
        SEP     R8

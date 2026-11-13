; ---------------------------------------------------------------------------
; main.inc -- the start and the main loop (src/main.c; the Astrocade's
; main.inc, transcribed). Resident.
;
; The lobby's appkeys say who we are and where to play; else we ask, and
; list the rooms. At a table each pass runs FRAME (the tick, input, sound),
; the link's step (net.inc), then the screen the state's phase wants: the
; waiting room and the trophies (PG_ROOM), the arena (PG_ARENA: E_STATE on
; a new state, E_TICK each pass).
; ---------------------------------------------------------------------------

S_NONE  EQU 0
S_WAIT  EQU 1
S_ARENA EQU 2
S_ROUND EQU 3
S_GAME  EQU 4

; ---- the start --------------------------------------------------------------
MAIN:   GOPAGE PG_BOOT, E_ENTER         ; the rigs' test hook, the appkeys
        LR A,0
        NI 0FFH
        BZ MI1
        GOPAGE PG_TEST, E_ENTER         ; an offline scene: its state
        LDB FLAGS
        OI FL_TEST
        STB FLAGS
        LIS NL_LINK+NL_JOINED+NL_HEARD
        STB NLINK
        JMP TABLE2
MI1:    GOPAGE PG_MENU, E_ENTER         ; the title, and a name if none
        LR A,0
        NI 0FFH
        BZ TOP
        GOPAGE PG_BOOT, E_SAVENM        ; (a new one: kept)

; ---- the rooms, when no table is known (menus.c table_list) ------------------
TOP:    LDB SRVTBL
        NI 0FFH
        BZ TOP1
        JMP TABLE
TOP1:   GOPAGE PG_BOOT, E_CONN          ; "CONNECTING", the socket: r0 = 1 open
        LR A,0
        NI 0FFH
        BZ TOPNA
        PI NINIT
        GOPAGE PG_MENU, E_STATE         ; the list asked for
TOPL:   PI FRAME
        PI NSTEP
        LDB NEVT
        NI EV_LOST
        BNZ TOPNA
        GOPAGE PG_MENU, E_TICK          ; r0: 0 on, 1 picked, 2 no list
        LR A,0
        NI 0FFH
        BZ TOPL
        CI 1
        BZ TABLE
TOPNA:  LIS M_NOANS                     ; (no room to forget)
        LR 0,A
        JMP TFAIL

; ---- at a table (main.c play_table) ----------------------------------------
TABLE:  GOPAGE PG_BOOT, E_JOIN          ; the room kept, "JOINING", the socket
        LR A,0
        NI 0FFH
        BNZ TAB1
        JMP TNOANS
TAB1:   PI NHELLO                       ; the link's counters, ST zeroed, HELLO
TABLE2: CLR                             ; (the test scenes come in here)
        STB SHOWS
MAINLP: LISU 7                          ; the K-stack empty again
        LISL 3
        LI KSTK0
        LR S,A
        PI FRAME
        PI NSTEP                        ; the link's step: NEVT
        LDB FLAGS
        NI FL_TEST
        BZ ML0
        GOPAGE PG_TEST, E_TICK          ; a scene's own (the blasts)
ML0:    LDB NEVT
        NI EV_ERROR
        BZ ML0A
        JMP TERR
ML0A:   LDB NEVT
        NI EV_LOST
        BZ ML0B
        JMP TLOST
ML0B:   LDB NEVT
        NI EV_NOANS
        BZ ML0C
        JMP TNOANS
ML0C:   LDB EVKEY
        CI KEY_LEAVE
        BNZ ML0D
        JMP TLEAVE
ML0D:   LDB NLINK                       ; no state yet: nothing to show
        NI NL_HEARD
        BZ MLEND
        LDB NEVT                        ; a screen is picked only at a commit
        NI EV_STATE
        BZ ML7
        LDB ST+OFF_PHASE                ; the screen it wants
        CI PH_GAMEOVER
        BC ML4
        LIS PH_WAIT
ML4:    DCI SCRFOR
        ADC
        LM
        LR 4,A
        LDB SHOWS
        XS 4
        BNZ ML5
        LR A,4                          ; the arena again: a new round's
        CI S_ARENA
        BNZ ML6
        LDB ST+OFF_SEQ
        LR 5,A
        LDB SHSEQ
        XS 5
        BZ ML6
ML5:    LR A,4                          ; another screen: drawn afresh
        STB SHOWS
        LDB ST+OFF_SEQ
        STB SHSEQ
        LIS E_ENTER
        LR 6,A
        DCI SCRPG
        BR ML6A
ML6:    LIS E_STATE                     ; the same one: the new state on it
        LR 6,A
        DCI SCRPG
ML6A:   PI SCRCALL
ML7:    LIS E_TICK                      ; and its pass
        LR 6,A
        DCI SCRTK
        PI SCRCALL
MLEND:  INCB PASSES+1                   ; (the rigs count the passes)
        BNZ MLE1
        INCB PASSES
MLE1:   JMP MAINLP

; SCRCALL -- entry r6 of the page table at DC0 for the screen SHOWS.
;   Clobbers everything.
SCRCALL: ENTER
        LR Q,DC
        LDB SHOWS
        LR DC,Q
        ADC
        LM
        LR 9,A                          ; the page
        PI SELPG
        LI PGWIN>>8                     ; its entry: PGWIN + r6 (PK: PC1 = here,
        LR KU,A                         ;  so its POP comes back; the K-stack
        LR A,6                          ;  keeps our own return)
        LR KL,A
        PK
        LEAVE

; by phase: the screen; by screen: the pages that draw it and run it
SCRFOR: DB S_WAIT, S_ARENA, S_ARENA, S_ROUND, S_GAME
SCRPG:  DB PG_ROOM, PG_ROOM, PG_ARENA, PG_ROOM, PG_ROOM
SCRTK:  DB PG_BOOT, PG_ROOM, PG_ARENA, PG_ROOM, PG_ROOM ; (S_NONE: the boot's
                                        ;  E_TICK, a return)

; the server said no
TERR:   LDB NERR
        CI ERR_VERSION
        LI M_NOROOM+80H
        BNZ TERR1
        LI M_VERSION+80H
TERR1:  LR 0,A
        BR TFAIL
; the link went: say so, then join again (the room kept)
TLOST:  LIS M_LOST
        LR 0,A
        BR TFAIL
; MODE: give the seat up, BYE (a few passes to get it out), close
TLEAVE: LDB NLINK
        NI NL_JOINED
        BZ TLV1
        LIS ACT_LEAVE
        LR 1,A
        CLR
        LR 2,A
        LR 3,A
        PI ACTQ
TLV1:   LDB TXF
        OI TX_BYE
        STB TXF
        LI 30
        STB RCOUNT
TLV2:   PI FRAME
        PI NSTEP
        LDB TXF
        NI 0FFH
        BNZ TLV3
        LDB NKIND
        NI 0FFH
        BZ TLV4
TLV3:   DECB RCOUNT
        BNZ TLV2
TLV4:   LI M_QUIET+80H
        LR 0,A
        BR TFAIL
; no answer: give the table up
TNOANS: LI M_NOANS+80H
        LR 0,A
; TFAIL: the socket closed, message r0 (b7: the room forgotten too), then the
; rooms again.
TFAIL:  LR A,0
        STB MSGID
        LDB FLAGS                       ; (a test scene is over too)
        NI 0FFH-FL_TEST
        STB FLAGS
        GOPAGE PG_BOOT, E_FAIL
        JMP TOP

; ACTQ -- an action queued: r1 the kind, r2, r3 its arguments; its seq never
;   0.  Leaf.  Clobbers A, DC0.
ACTQ:   DCI ASEQ
        LM
        INC
        BNZ ACQ1
        LIS 1
ACQ1:   DCI ASEQ
        ST
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        LDB TXF
        OI TX_ACT
        STB TXF
        POP

; ---------------------------------------------------------------------------
; FRAME -- wait for the cart's tick to move; DT = the jiffies since the last
;   pass (at most 8: a longer stall is not caught up), JIFFY += DT; the
;   input read; the sound's tick.
; ---------------------------------------------------------------------------
FRAME:  ENTER
FRM1:   PI TICKRD                       ; r0:r1 the tick
        DCI FRAMES
        LM
        COM
        INC
        AS 0
        LR 2,A                          ; DT (the low bytes do)
        NI 0FFH
        BZ FRM1
        LR A,2
        CI 8
        BC FRM2
        LIS 8
        LR 2,A
FRM2:   LR A,2
        STB DT
        DCI FRAMES
        LR A,0
        ST
        LR A,1
        ST
        DCI JIFFY
        LM
        AS 2
        LR 3,A
        LM
        LNK
        DCI JIFFY+1
        ST
        DCI JIFFY
        LR A,3
        ST
        PI INPOLL
        PI INTAKE
        PI SNDTICK
        LEAVE

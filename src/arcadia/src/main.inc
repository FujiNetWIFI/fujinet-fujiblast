; main.inc -- src/main.c: the start, the rooms, a table (fixed A).
;
; The lobby's appkeys say who we are and where to play; else we ask, and
; list the rooms. At a table each frame runs the link's step (p_net), then
; the screen the state's phase wants: the waiting room and the trophies
; (p_room), the arena (p_arena draws it on a new state, p_play runs each
; frame).

; ---- the start ----------------------------------------------------------------
MAININIT:
        GOPAGE  PG_BOOT,E_ENTER         ; the rigs' test hook, the appkeys
        COMI,R0 0
        BCTR,EQ MI1
        GOPAGE  PG_TEST,E_ENTER         ; an offline scene (it never comes back)
MI1:    GOPAGE  PG_MENU,E_ENTER         ; the title, and a name if none
        COMI,R0 0
        BCTR,EQ TOP
        GOPAGE  PG_BOOT,E_SAVENM        ; (a new one: kept)

; ---- the rooms, when no table is known (menus.c table_list) ---------------------
TOP:    LODA,R0 SRVTBL
        BCFA,EQ TABLE
        GOPAGE  PG_BOOT,E_CONN          ; "CONNECTING", the socket: R0 = 1 open
        COMI,R0 0
        BCTA,EQ TOPNA
        GOPAGE  PG_NET,E_INIT
        GOPAGE  PG_MENU,E_STATE         ; the list asked for
TOPL:   BSTA,UN FRAME
        GOPAGE  PG_NET,E_ENTER          ; (the link's step)
        LODA,R0 NEVT
        TMI,R0  EV_LOST
        BCTA,EQ TOPNA
        GOPAGE  PG_MENU,E_TICK          ; R0: 0 on, 1 picked, 2 no list
        COMI,R0 0
        BCTR,EQ TOPL
        COMI,R0 1
        BCTR,EQ TABLE
TOPNA:  LODI,R0 M_NOANS         ; (no room to forget)
        BCTA,UN TFAIL

; ---- at a table (main.c play_table) ----------------------------------------------
TABLE:  GOPAGE  PG_BOOT,E_JOIN          ; the room kept, "JOINING", the socket
        COMI,R0 0
        BCTA,EQ TNOANS
        GOPAGE  PG_NET,E_HELLO          ; the link's counters, the state zeroed, HELLO
TABLE2: EORZ    R0              ; (the test scenes come in here)
        STRA,R0 SHOWN
MAINLP: BSTA,UN FRAME
        GOPAGE  PG_NET,E_ENTER          ; the link's step: NEVT
        LODA,R0 $18FF           ; the display seen (a late pass: VBSYNC)
        ANDI,R0 $0F
        COMI,R0 13
        BCTR,EQ ML00
        STRA,R0 VSEEN
ML00:
        LODA,R0 FLAGS
        TMI,R0  FL_TEST
        BCFR,EQ ML0
        GOPAGE  PG_TEST,E_TICK          ; a scene's own (the blasts)
ML0:    LODA,R0 NEVT
        TMI,R0  EV_ERROR
        BCTA,EQ TERR
        TMI,R0  EV_LOST
        BCTA,EQ TLOST
        TMI,R0  EV_NOANS        ; (p_net: no state in JOIN_WAIT frames)
        BCTA,EQ TNOANS
        LODA,R0 EVKEY
        COMI,R0 K_CLR
        BCTA,EQ TLEAVE
        LODA,R0 NLINK           ; no state yet: nothing to show
        TMI,R0  NL_HEARD
        BCFA,EQ MAINLP
ML2:    LODA,R0 NEVT            ; a screen is picked only at a commit (ST is
        TMI,R0  EV_STATE        ;  patched as frames come: half a state between)
        BCFA,EQ ML7
        LODA,R0 ST+OFF_PHASE    ; the screen it wants (p_net found our seat)
        STRZ    R2
        COMI,R2 PH_GAMEOVER+1
        BCTR,LT ML4
        LODI,R2 PH_WAIT
ML4:    LODA,R0 SCRFOR,R2
        COMA,R0 SHOWN
        BCFR,EQ ML5
        COMI,R0 S_ARENA         ; the arena again: a new round's
        BCFR,EQ ML6
        LODA,R0 ST+OFF_SEQ
        COMA,R0 SHSEQ
        BCTR,EQ ML6
        LODI,R0 S_ARENA
ML5:    STRA,R0 SHOWN           ; another screen: drawn afresh
        LODA,R0 ST+OFF_SEQ
        STRA,R0 SHSEQ
        LODA,R2 SHOWN
        LODA,R0 SCRPG,R2
        STRZ    R1
        LODI,R3 E_ENTER
        BSTA,UN PGCALL
        BCTR,UN ML7
ML6:    LODA,R2 SHOWN           ; the same one: the new state on it
        LODA,R0 SCRPG,R2
        STRZ    R1
        LODI,R3 E_STATE
        BSTA,UN PGCALL
ML7:    LODA,R2 SHOWN           ; and its frame
        LODA,R0 SCRTK,R2
        STRZ    R1
        LODI,R3 E_TICK
        BSTA,UN PGCALL
        BCTA,UN MAINLP
; by phase: the screen; by screen: the pages that draw it and run it
SCRFOR: DB      S_WAIT,S_ARENA,S_ARENA,S_ROUND,S_GAME
SCRPG:  DB      PG_ROOM,PG_ROOM,PG_ARENA,PG_ROOM,PG_ROOM
SCRTK:  DB      PG_BOOT,PG_ROOM,PG_PLAY,PG_ROOM,PG_ROOM   ; (S_NONE: the boot's
                                ;  E_TICK, a return)

; the server said no
TERR:   LODI,R0 M_NOROOM|$80
        LODA,R1 NERR
        COMI,R1 ERR_VERSION
        BCFA,EQ TFAIL
        LODI,R0 M_VERSION|$80
        BCTA,UN TFAIL
; the link went: say so, then join again (the room kept)
TLOST:  LODI,R0 M_LOST
        BCTA,UN TFAIL
; Clear: give the seat up, BYE (a few frames to get it out), close
TLEAVE: LODA,R0 NLINK
        TMI,R0  NL_JOINED
        BCFR,EQ TLV1
        LODI,R1 ACT_LEAVE
        LODI,R2 0
        LODI,R3 0
        BSTA,UN ACTQ
TLV1:   LODA,R0 TXF
        IORI,R0 TX_BYE
        STRA,R0 TXF
        LODI,R0 30
        STRA,R0 RCOUNT
TLV2:   BSTA,UN FRAME
        GOPAGE  PG_NET,E_ENTER
        LODA,R0 TXF
        BCFR,EQ TLV3
        LODA,R0 NKIND
        BCTR,EQ TLV4
TLV3:   LODA,R0 RCOUNT
        SUBI,R0 1
        STRA,R0 RCOUNT
        BCFR,EQ TLV2
TLV4:   LODI,R0 M_QUIET|$80
        BCTR,UN TFAIL
; no answer: give the table up
TNOANS: LODI,R0 M_NOANS|$80
; TFAIL: the socket closed, message R0 (b7: the room forgotten too), then
; the rooms again.
TFAIL:  STRA,R0 MSGID
        GOPAGE  PG_BOOT,E_FAIL
        BCTA,UN TOP

SEATBIT: DB     1,2,4,8
SEATOFF: DB     0,PLAYER_SIZE,2*PLAYER_SIZE,3*PLAYER_SIZE

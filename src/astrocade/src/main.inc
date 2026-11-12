; main.inc -- src/main.c: the start, the rooms, a table. Resident. The
; Arcadia's (src/arcadia/src/main.inc), transcribed.
;
; The lobby's appkeys say who we are and where to play; else we ask, and
; list the rooms. At a table each frame runs the sound's tick and the
; link's step (p_net), then the screen the state's phase wants: the
; waiting room and the trophies (p_room), the arena (p_arena draws it on a
; new state, p_play runs each frame).

; ---- the start ----------------------------------------------------------------
MAININIT:
        GOPAGE  PG_BOOT,E_ENTER         ; the rigs' test hook, the appkeys
        OR      A
        JR      Z,MI1
        GOPAGE  PG_TEST,E_ENTER         ; an offline scene (it never comes back)
MI1:    GOPAGE  PG_MENU,E_ENTER         ; the title, and a name if none
        OR      A
        JR      Z,TOP
        GOPAGE  PG_BOOT,E_SAVENM        ; (a new one: kept)

; ---- the rooms, when no table is known (menus.c table_list) ---------------------
TOP:    LD      A,(SRVTBL)
        OR      A
        JR      NZ,TABLE
        GOPAGE  PG_BOOT,E_CONN          ; "CONNECTING", the socket: A = 1 open
        OR      A
        JR      Z,TOPNA
        GOPAGE  PG_NET,E_INIT
        GOPAGE  PG_MENU,E_STATE         ; the list asked for
TOPL:   CALL    FRAME
        GOPAGE  PG_NET,E_ENTER          ; (the link's step)
        LD      A,(NEVT)
        AND     EV_LOST
        JR      NZ,TOPNA
        GOPAGE  PG_MENU,E_TICK          ; A: 0 on, 1 picked, 2 no list
        OR      A
        JR      Z,TOPL
        DEC     A
        JR      Z,TABLE
TOPNA:  LD      A,M_NOANS               ; (no room to forget)
        JP      TFAIL

; ---- at a table (main.c play_table) ----------------------------------------------
TABLE:  GOPAGE  PG_BOOT,E_JOIN          ; the room kept, "JOINING", the socket
        OR      A
        JP      Z,TNOANS
        GOPAGE  PG_NET,E_HELLO          ; the link's counters, the state zeroed, HELLO
TABLE2: XOR     A                       ; (the test scenes come in here)
        LD      (SHOWN),A
MAINLP: CALL    FRAME
        GOPAGE  PG_NET,E_ENTER          ; the link's step: NEVT
        LD      A,(FLAGS)
        AND     FL_TEST
        JR      Z,ML0
        GOPAGE  PG_TEST,E_TICK          ; a scene's own (the blasts)
ML0:    LD      A,(NEVT)
        BIT     1,A                     ; EV_ERROR
        JP      NZ,TERR
        BIT     2,A                     ; EV_LOST
        JP      NZ,TLOST
        BIT     3,A                     ; EV_NOANS (no state in JOIN_WAIT frames)
        JP      NZ,TNOANS
        LD      A,(EVKEY)
        CP      K_CLR
        JP      Z,TLEAVE
        LD      A,(NLINK)               ; no state yet: nothing to show
        AND     NL_HEARD
        JR      Z,MAINLP
        LD      A,(NEVT)                ; a screen is picked only at a commit (ST is
        AND     EV_STATE                ;  patched as frames come: half a state between)
        JR      Z,ML7
        LD      A,(ST+OFF_PHASE)        ; the screen it wants (p_net found our seat)
        CP      PH_GAMEOVER+1
        JR      C,ML4
        LD      A,PH_WAIT
ML4:    LD      C,A
        LD      B,0
        LD      HL,SCRFOR
        ADD     HL,BC
        LD      A,(HL)
        LD      HL,SHOWN
        CP      (HL)
        JR      NZ,ML5
        CP      S_ARENA                 ; the arena again: a new round's
        JR      NZ,ML6
        LD      A,(ST+OFF_SEQ)
        LD      HL,SHSEQ
        CP      (HL)
        JR      Z,ML6
        LD      A,S_ARENA
ML5:    LD      (SHOWN),A               ; another screen: drawn afresh
        LD      A,(ST+OFF_SEQ)
        LD      (SHSEQ),A
        LD      HL,E_ENTER
        LD      DE,SCRPG
        JR      ML6A
ML6:    LD      HL,E_STATE              ; the same one: the new state on it
        LD      DE,SCRPG
ML6A:   CALL    SCRCALL
ML7:    LD      HL,E_TICK               ; and its frame
        LD      DE,SCRTK
        CALL    SCRCALL
        JP      MAINLP

; SCRCALL: entry HL of the page table DE names for the screen SHOWN.
SCRCALL: LD     A,(SHOWN)
        ADD     A,E
        LD      E,A
        JR      NC,SCC1
        INC     D
SCC1:   LD      A,(DE)
        JP      PGCALL

; by phase: the screen; by screen: the pages that draw it and run it
SCRFOR: DB      S_WAIT,S_ARENA,S_ARENA,S_ROUND,S_GAME
SCRPG:  DB      PG_ROOM,PG_ROOM,PG_ARENA,PG_ROOM,PG_ROOM
SCRTK:  DB      PG_BOOT,PG_ROOM,PG_PLAY,PG_ROOM,PG_ROOM ; (S_NONE: the boot's
                                ;  E_TICK, a return)

; the server said no
TERR:   LD      A,(NERR)
        CP      ERR_VERSION
        LD      A,M_NOROOM+80H
        JR      NZ,TFAIL
        LD      A,M_VERSION+80H
        JR      TFAIL
; the link went: say so, then join again (the room kept)
TLOST:  LD      A,M_LOST
        JR      TFAIL
; C: give the seat up, BYE (a few frames to get it out), close
TLEAVE: LD      A,(NLINK)
        AND     NL_JOINED
        JR      Z,TLV1
        LD      B,ACT_LEAVE
        LD      DE,0
        CALL    ACTQ
TLV1:   LD      A,(TXF)
        OR      TX_BYE
        LD      (TXF),A
        LD      A,30
        LD      (RCOUNT),A
TLV2:   CALL    FRAME
        GOPAGE  PG_NET,E_ENTER
        LD      A,(TXF)
        OR      A
        JR      NZ,TLV3
        LD      A,(NKIND)
        OR      A
        JR      Z,TLV4
TLV3:   LD      HL,RCOUNT
        DEC     (HL)
        JR      NZ,TLV2
TLV4:   LD      A,M_QUIET+80H
        JR      TFAIL
; no answer: give the table up
TNOANS: LD      A,M_NOANS+80H
; TFAIL: the socket closed, message A (b7: the room forgotten too), then
; the rooms again.
TFAIL:  LD      (MSGID),A
        GOPAGE  PG_BOOT,E_FAIL
        JP      TOP

; p_boot.inc -- the boot page: the rigs' test hook (src/test.c test_check),
; the lobby's appkeys (src/appkey.c), the socket (net_open, net_close) and
; the messages (screens.c message). The Arcadia's (src/arcadia/src/
; p_boot.inc), transcribed. Each transaction here is waited for (mail.inc
; MGO: frames, the raster and the sound running); the link's own, if one
; is out, is let finish first (MIDLE).
;
; Key 0 is the player's name, key 0x10 FujiBlast's server
; ("tcp://host:port/?table=id"). We keep only the table's id (SRVTBL) and
; FL_ROOM (the key holds our server); the OPEN reads the key again and
; streams "N:TCP://HOST:PORT/" from its reply straight into the
; transaction. Without the key, the server is the build's (gen/server:
; DEFSPEC), and the table one picked from its list (E_JOIN then writes the
; key).

        JP      BOOT            ; E_ENTER: A = 1 a test scene, else the keys read
        RET                     ; E_STATE
        NOP
        NOP
        RET                     ; E_TICK
        NOP
        NOP
        JP      SAVENM          ; E_SAVENM
        JP      CONN            ; E_CONN
        JP      JOIN            ; E_JOIN
        JP      SCLOSE          ; E_CLOSE
        JP      CLRROOM         ; E_CLRRM
        JP      MSG             ; E_MSG
        JP      FAIL            ; E_FAIL

AKNAME  EQU     0
AKROOM  EQU     10H

; ---- test_check, then the keys -------------------------------------------------
; A rig pokes TESTHK[0] = 5AH (scene in [2], its argument in [3]) within
; 20 frames of seeing [1] = C3H.
BOOT:   LD      A,0C3H
        LD      (TESTHK+1),A
        LD      A,20
        LD      (RCOUNT),A
BT1:    CALL    FRAME
        LD      A,(TESTHK)
        CP      5AH
        JR      Z,BT2
        LD      HL,RCOUNT
        DEC     (HL)
        JR      NZ,BT1
        CALL    AKLOAD
        XOR     A
        RET
BT2:    LD      A,0A5H
        LD      (TESTHK),A
        LD      A,1
        RET

; ---- the appkeys ------------------------------------------------------------------
; AKOPEN: open key B (C = 0 read, 1 write): creator 1, app 1. Z when it
; opened. OPEN_APPKEY's payload is six raw bytes: creator lo, hi, app,
; key, mode, a reserved 0.
AKOPEN: PUSH    BC
        CALL    MIDLE
        LD      E,FCOKEY
        LD      L,0
        CALL    MSTARTF
        POP     BC
        LD      A,1
        CALL    FNTXBYT
        XOR     A
        CALL    FNTXBYT
        LD      A,1
        CALL    FNTXBYT
        LD      A,B
        CALL    FNTXBYT
        LD      A,C
        CALL    FNTXBYT
        XOR     A
        CALL    FNTXBYT
        JP      MGO
; AKCLOSE: the key closed.
AKCLOSE: LD     E,FCCKEY
        LD      L,0
        CALL    MSTARTF
        JP      MGO
; AKREAD: open key B and read it: the reply's first two bytes are its
; length (LE), the data follows (and stays in the window until the next
; transaction: the caller closes the key after). A = the length (0, Z:
; none or it failed: closed).
AKREAD: LD      C,0
        CALL    AKOPEN
        JR      NZ,AKR8
        LD      E,FCRKEY
        LD      L,0
        CALL    MSTARTF
        CALL    MGO
        JR      NZ,AKR8
        LD      A,(FNRDATA+1)   ; its length's high byte: 0, or it isn't a key
        OR      A
        JR      NZ,AKR8
        LD      A,(FNRDATA)
        CP      65
        JR      C,AKR9
        LD      A,64
AKR9:   OR      A
        RET
AKR8:   CALL    AKCLOSE
        XOR     A
        RET
; AKWRITE: open key B to write, the WRITE begun (the caller streams the
; data into the TX, then AKGO). Z when it opened.
AKWRITE: LD     C,1
        CALL    AKOPEN
        RET     NZ
        LD      E,FCWKEY
        LD      L,0
        CALL    MSTARTF
        XOR     A
        RET
; AKGO: the WRITE sent, the key closed.
AKGO:   CALL    MGO
        JP      AKCLOSE

; AKLOAD: MYNAME = key 0, letters and digits up to 8, upper-cased (NUL
; padded); SRVTBL and FL_ROOM from key 0x10 (appkey_load).
AKLOAD: LD      HL,MYNAME       ; (MYNAME, then SRVTBL: ram.inc)
        LD      B,NAME_LEN+TABLE_LEN+2
AKL0:   LD      (HL),0
        INC     HL
        DJNZ    AKL0
        LD      A,(FLAGS)
        AND     0FFH-FL_ROOM
        LD      (FLAGS),A
        LD      B,AKNAME        ; the name
        CALL    AKREAD
        JR      Z,AKL3
        LD      B,A             ; B: the key's bytes left
        LD      HL,FNRDATA+2
        LD      DE,MYNAME
        LD      C,NAME_LEN
AKL1:   LD      A,(HL)
        INC     HL
        CALL    UPALNUM
        JR      Z,AKL2
        LD      (DE),A
        INC     DE
        DEC     C
        JR      Z,AKL2A
AKL2:   DJNZ    AKL1
AKL2A:  CALL    AKCLOSE
AKL3:   LD      B,AKROOM        ; the server: its table, if it has one
        CALL    AKREAD
        CP      5
        RET     C               ; (4 bytes or fewer: none; closed)
        LD      B,A
        LD      HL,FNRDATA+2
AKL4:   LD      A,(HL)          ; to the "="
        INC     HL
        CP      3DH
        JR      Z,AKL5
        DJNZ    AKL4
        JR      AKL8
AKL5:   LD      DE,SRVTBL
        LD      C,TABLE_LEN
AKL6:   DEC     B
        JR      Z,AKL7
        LD      A,(HL)
        INC     HL
        CALL    UPALNUM
        JR      Z,AKL7
        CP      41H             ; a letter: lower case
        JR      C,AKL6A
        ADD     A,20H
AKL6A:  LD      (DE),A
        INC     DE
        DEC     C
        JR      NZ,AKL6
AKL7:   LD      A,(SRVTBL)
        OR      A
        JR      Z,AKL8
        LD      A,(FLAGS)
        OR      FL_ROOM
        LD      (FLAGS),A
AKL8:   JP      AKCLOSE

; UPALNUM: A upper-cased if a letter; 0 (Z) if not a letter or a digit.
UPALNUM: CP     61H
        JR      C,UPA1
        CP      7BH
        JR      NC,UPA9
        SUB     20H             ; (NZ)
        RET
UPA1:   CP      41H
        JR      C,UPA2
        CP      5BH
        JR      NC,UPA9
        OR      A
        RET
UPA2:   CP      30H
        JR      C,UPA9
        CP      3AH
        JR      NC,UPA9
        OR      A
        RET
UPA9:   XOR     A
        RET

; SAVENM: key 0 = MYNAME (appkey_save_name).
SAVENM: LD      B,AKNAME
        CALL    AKWRITE
        RET     NZ
        LD      HL,MYNAME
        LD      B,NAME_LEN
SVN1:   LD      A,(HL)
        OR      A
        JR      Z,SVN2
        CALL    FNTXBYT
        INC     HL
        DJNZ    SVN1
SVN2:   JP      AKGO

; SAVEROOM: key 0x10 = the build's server + SRVTBL, if the key isn't ours
; already (appkey_save_room).
SAVEROOM: LD    A,(FLAGS)
        AND     FL_ROOM
        RET     NZ
        LD      B,AKROOM
        CALL    AKWRITE
        RET     NZ
        LD      HL,DEFURL       ; "tcp://host:port/?table="
        CALL    FNTXSTR
        LD      HL,SRVTBL       ; the table
        LD      B,TABLE_LEN
SVR3:   LD      A,(HL)
        OR      A
        JR      Z,SVR4
        CALL    FNTXBYT
        INC     HL
        DJNZ    SVR3
SVR4:   CALL    AKGO
        LD      A,(FLAGS)
        OR      FL_ROOM
        LD      (FLAGS),A
        RET

; CLRROOM: key 0x10 emptied; no table (appkey_clear_room).
CLRROOM: LD     B,AKROOM
        CALL    AKWRITE
        CALL    Z,AKGO
        XOR     A
        LD      (SRVTBL),A
        LD      A,(FLAGS)
        AND     0FFH-FL_ROOM
        LD      (FLAGS),A
        RET

; ---- the socket ---------------------------------------------------------------------
; SCLOSE: CLOSE (a NAK is fine: nothing was open), the link down.
SCLOSE: CALL    MIDLE
        LD      E,NCCLOSE
        LD      L,0
        CALL    MSTARTN
        CALL    MGO
        XOR     A
        LD      (NLINK),A
        LD      (NKIND),A
        RET

; SOPEN: the socket: CLOSE first (a reset leaves the cart's open), then
; OPEN "N:TCP://host:port/" (read and write) -- from key 0x10 (FL_ROOM),
; else DEFSPEC. A = 1 open (NLINK), 0 not.
SOPEN:  CALL    SCLOSE
        LD      A,(FLAGS)
        AND     FL_ROOM
        JR      NZ,SOK
        CALL    SOBEGIN         ; the build's
        LD      HL,DEFSPEC
        CALL    FNTXSTR
        JR      SOGO
SOK:    LD      B,AKROOM        ; the key again: its reply holds the URL
        CALL    AKREAD
        JR      Z,SOFAIL
        LD      (T2),A          ; T2: its length
        CALL    SOBEGIN
        CALL    SPECKEY
SOGO:   CALL    MGO
        JR      NZ,SOFAIL1
        LD      A,(FLAGS)       ; (the key, if read, closed after)
        AND     FL_ROOM
        CALL    NZ,AKCLOSE
        LD      A,NL_LINK
        LD      (NLINK),A
        LD      A,1
        RET
SOFAIL1: LD     A,(FLAGS)
        AND     FL_ROOM
        CALL    NZ,AKCLOSE
SOFAIL: XOR     A
        LD      (NLINK),A
        RET
; SOBEGIN: OPEN: read and write, no translation.
SOBEGIN: LD     E,NCOPEN
        LD      L,2
        CALL    MSTARTN
        LD      A,NMRDWR
        CALL    FNPARB
        XOR     A
        JP      FNPARB

; SPECKEY: "N:" + the key's URL (in the reply from 2, T2 bytes) up to its
; "?" (a "/" before it dropped), upper-cased; ":9910" if it has no port;
; then "/".
SPECKEY: LD     HL,FNRDATA+2    ; its end: T2 bytes, or the "?"
        LD      A,(T2)
        LD      B,A
        LD      C,0
SK1:    LD      A,B
        OR      A
        JR      Z,SK2
        LD      A,(HL)
        OR      A
        JR      Z,SK2
        CP      3FH
        JR      Z,SK2
        INC     HL
        INC     C
        DEC     B
        JR      SK1
SK2:    LD      A,C             ; C: the URL's characters
        OR      A
        JR      Z,SK3
        DEC     HL              ; a "/" last: dropped
        LD      A,(HL)
        CP      2FH
        JR      NZ,SK3
        DEC     C
SK3:    LD      A,4EH           ; "N:"
        CALL    FNTXBYT
        LD      A,3AH
        CALL    FNTXBYT
        LD      B,0             ; B: its colons
        LD      HL,FNRDATA+2
        LD      A,C
        OR      A
        JR      Z,SK6
SK4:    LD      A,(HL)
        INC     HL
        CP      3AH
        JR      NZ,SK5
        INC     B
SK5:    CP      61H             ; a..z up
        JR      C,SK5A
        CP      7BH
        JR      NC,SK5A
        SUB     20H
SK5A:   CALL    FNTXBYT
        DEC     C
        JR      NZ,SK4
SK6:    LD      A,B             ; "tcp://host": one colon, no port
        CP      2
        JR      NC,SK8
        LD      HL,SPORT
        CALL    FNTXSTR
SK8:    LD      A,2FH           ; "/"
        JP      FNTXBYT
SPORT:  DB      ":9910",0

; CONN: "CONNECTING", the socket (the build's server: the table list).
; A = 1 open.
CONN:   LD      A,M_CONN
        LD      (MSGID),A
        CALL    MSG
        JP      SOPEN

; JOIN: the room kept, "JOINING <table>", the socket. A = 1 open.
JOIN:   CALL    SAVEROOM
        LD      A,M_JOIN
        LD      (MSGID),A
        CALL    MSG
        JP      SOPEN

; FAIL: the socket closed, message MSGID (M_QUIET: none); its b7: the room
; forgotten too.
FAIL:   CALL    SCLOSE
        LD      A,(MSGID)
        AND     80H
        LD      (FAILRM),A      ; b7: the room too
        LD      A,(MSGID)
        AND     7FH
        LD      (MSGID),A
        CP      M_QUIET
        CALL    NZ,MSG
        LD      A,(FAILRM)
        OR      A
        RET     Z
        JP      CLRROOM

; ---- message ------------------------------------------------------------------------
; MSG: message MSGID on a clear screen: its two lines (the second the
; table's id for M_JOIN); then its time (1: none at all) or FIRE. (src/
; screens.c message: shown before it waits, so a socket that blocks is seen.)
MSG:    CALL    GFXBEGIN
        LD      A,(MSGID)       ; its first line
        LD      C,A
        LD      B,0
        LD      HL,MSGA
        ADD     HL,BC
        ADD     HL,BC
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        LD      D,9
        LD      C,SC_WHITE*16+SC_BLUE
        CALL    CENTRE
        LD      A,(MSGID)
        CP      M_JOIN
        JR      Z,MSG2
        LD      C,A             ; its second
        LD      B,0
        LD      HL,MSGB
        ADD     HL,BC
        ADD     HL,BC
        LD      A,(HL)
        INC     HL
        LD      H,(HL)
        LD      L,A
        JR      MSG2A
MSG2:   LD      HL,SRVTBL       ; the table: its id
MSG2A:  LD      D,12
        LD      C,11*16+SC_BLUE
        CALL    CENTRE
        LD      A,(MSGID)
        LD      C,A
        LD      B,0
        LD      HL,MSGT         ; its time
        ADD     HL,BC
        LD      A,(HL)
        LD      (RCOUNT),A
        CP      1
        JR      NZ,MSG3
        JP      GFXSHOW
MSG3:   OR      A
        JR      NZ,MSG5
        LD      HL,SFIRE        ; ... or FIRE
        LD      D,17
        LD      C,7*16+SC_BLUE
        CALL    CENTRE
        CALL    GFXSHOW
MSG4:   CALL    FRAME
        LD      A,(EVBTN)
        OR      A
        RET     NZ
        LD      A,(EVKEY)
        CP      K_ENT
        JR      NZ,MSG4
        RET
MSG5:   CALL    GFXSHOW
MSG6:   CALL    FRAME
        LD      HL,RCOUNT
        DEC     (HL)
        JR      NZ,MSG6
        RET

; the messages: each one's lines and time (0: FIRE, 1: none)
MSGA:   DW      SJOIN,SNOANS,SLOST,SSAIDNO,SSAIDNO,SCONN
MSGB:   DW      SNONE,SFROM,SAGAIN,SNOROOM,SUPDATE,SNONE
MSGT:   DB      1,0,120,0,0,1
SJOIN:  DB      "JOINING",0
SNOANS: DB      "NO ANSWER",0
SFROM:  DB      "FROM THE SERVER",0
SLOST:  DB      "CONNECTION LOST",0
SAGAIN: DB      "TRYING AGAIN",0
SSAIDNO: DB     "SERVER SAID NO",0
SNOROOM: DB     "NO SUCH ROOM",0
SUPDATE: DB     "UPDATE FUJIBLAST",0
SCONN:  DB      "CONNECTING",0
SNONE:  DB      0
SFIRE:  DB      "PRESS FIRE",0

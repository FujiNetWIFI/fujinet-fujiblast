; p_boot.inc -- the boot page: the rigs' test hook (src/test.c test_check),
; the lobby's appkeys (src/appkey.c), the socket (net_open, net_close) and
; the messages (screens.c message). Each transaction here is waited for
; (fixb.inc MGO: frames, the sound running); the link's own, if one is out,
; is let finish first (MIDLE).
;
; Key 0 is the player's name, key 0x10 FujiBlast's server
; ("tcp://host:port/?table=id"). We keep only the table's id (SRVTBL) and
; FL_ROOM (the key holds our server); the OPEN reads the key again and
; streams "N:TCP://HOST:PORT/" from its reply straight into the
; transaction. Without the key, the server is the build's (gen/server:
; DEFSPEC), and the table one picked from its list (E_JOIN then writes the
; key).

        BCTA,UN BOOT            ; E_ENTER: R0 = 1 a test scene, else the keys read
        RETC,UN                 ; E_STATE
        NOP
        NOP
        RETC,UN                 ; E_TICK
        NOP
        NOP
        BCTA,UN SAVENM          ; E_SAVENM
        BCTA,UN CONN            ; E_CONN
        BCTA,UN JOIN            ; E_JOIN
        BCTA,UN SCLOSE          ; E_CLOSE
        BCTA,UN CLRROOM         ; E_CLRRM
        BCTA,UN MSG             ; E_MSG
        BCTA,UN FAIL            ; E_FAIL

AKNAME  EQU     0
AKROOM  EQU     $10

; ---- test_check, then the keys -------------------------------------------------
; A rig pokes TESTHK[0] = $5A (scene in [2], its argument in [3]) within
; 20 frames of seeing [1] = $C3.
BOOT:   LODI,R0 $C3
        STRA,R0 TESTHK+1
        LODI,R0 20
        STRA,R0 RCOUNT
BT1:    ZBSR    *ZFRAME
        LODA,R0 TESTHK
        COMI,R0 $5A
        BCTR,EQ BT2
        LODA,R0 RCOUNT
        SUBI,R0 1
        STRA,R0 RCOUNT
        BCFR,EQ BT1
        BSTA,UN AKLOAD
        EORZ    R0
        RETC,UN
BT2:    LODI,R0 $A5
        STRA,R0 TESTHK
        LODI,R0 1
        RETC,UN

; ---- the appkeys ------------------------------------------------------------------
; AKOPEN: open key R1 (R2 = 0 read, 1 write): creator 1, app 1. CC EQ when
; it opened. OPEN_APPKEY's payload is six raw bytes: creator lo, hi, app,
; key, mode, a reserved 0. Calls three down. Clobbers R0-R3, T0, T1.
AKOPEN: STRA,R1 T0
        STRA,R2 T1
        BSTA,UN MIDLE
        LODI,R3 FCOKEY
        LODI,R2 0
        BSTA,UN MSTARTF
        LODI,R1 1
        LODA,R0 *PTXD,R1
        LODI,R1 0
        LODA,R0 *PTXD,R1
        LODI,R1 1
        LODA,R0 *PTXD,R1
        LODA,R1 T0
        LODA,R0 *PTXD,R1
        LODA,R1 T1
        LODA,R0 *PTXD,R1
        LODI,R1 0
        LODA,R0 *PTXD,R1
        BCTA,UN MGO             ; tail call
; AKCLOSE: the key closed. Calls three down. Clobbers R0-R3.
AKCLOSE: LODI,R3 FCCKEY
        LODI,R2 0
        BSTA,UN MSTARTF
        BCTA,UN MGO
; AKREAD: open key R1 and read it: the reply's first two bytes are its
; length (LE), the data follows (and stays in the window until the next
; transaction: the caller closes the key after). R0 = the length (0: none
; or it failed: closed). Calls four down. Clobbers R0-R3, T0, T1.
AKREAD: LODI,R2 0
        BSTA,UN AKOPEN
        BCFR,EQ AKR8
        LODI,R3 FCRKEY
        LODI,R2 0
        BSTA,UN MSTARTF
        BSTA,UN MGO
        BCFR,EQ AKR8
        LODI,R3 1               ; its length's high byte: 0, or it isn't a key
        LODA,R0 *PRDATA,R3
        BCFR,EQ AKR8
        LODA,R0 *PRDATA
        COMI,R0 65
        BCTR,LT AKR9
        LODI,R0 64
AKR9:   COMI,R0 0               ; (CC for the caller)
        RETC,UN
AKR8:   BSTR,UN AKCLOSE
        EORZ    R0
        RETC,UN
; AKWRITE: open key R1 to write, the WRITE begun (the caller streams the
; data into the TX, then AKGO). CC EQ when it opened. Calls four down.
AKWRITE: LODI,R2 1
        BSTA,UN AKOPEN
        RETC,GT
        RETC,LT
        LODI,R3 FCWKEY
        LODI,R2 0
        BSTA,UN MSTARTF
        EORZ    R0
        RETC,UN
; AKGO: the WRITE sent, the key closed. Calls three down.
AKGO:   BSTA,UN MGO
        BCTA,UN AKCLOSE

; AKLOAD: MYNAME = key 0, letters and digits up to 8, upper-cased (NUL
; padded); SRVTBL and FL_ROOM from key 0x10 (appkey_load). Calls five down.
AKLOAD: EORZ    R0
        LODI,R3 TABLE_LEN+NAME_LEN+2
AKL0:   STRA,R0 MYNAME-1,R3     ; (MYNAME, then SRVTBL: ram.inc)
        BDRR,R3 AKL0
        LODA,R0 FLAGS
        ANDI,R0 ~FL_ROOM&$FF
        STRA,R0 FLAGS
        LODI,R1 AKNAME          ; the name
        BSTA,UN AKREAD
        BCTR,EQ AKL3
        STRA,R0 T2              ; T2: the key's bytes left
        LODI,R3 1               ; (reply offset - 1: the data from 2)
        LODI,R2 0               ; R2: the name's length
AKL1:   LODA,R0 *PRDATA,R3,+
        BSTA,UN UPALNUM
        BCTR,EQ AKL2
        STRA,R0 MYNAME,R2
        ADDI,R2 1
        COMI,R2 NAME_LEN
        BCTR,EQ AKL2A
AKL2:   LODA,R0 T2
        SUBI,R0 1
        STRA,R0 T2
        BCFR,EQ AKL1
AKL2A:  BSTA,UN AKCLOSE
AKL3:   LODI,R1 AKROOM          ; the server: its table, if it has one
        BSTA,UN AKREAD
        COMI,R0 5
        BCTA,LT AKL9            ; (4 bytes or fewer: none; closed)
        STRA,R0 T2
        LODI,R3 1
AKL4:   LODA,R0 *PRDATA,R3,+    ; to the "="
        COMI,R0 $3D
        BCTR,EQ AKL5
        LODA,R0 T2
        SUBI,R0 1
        STRA,R0 T2
        BCFR,EQ AKL4
        BCTR,UN AKL8
AKL5:   LODI,R2 0
AKL6:   LODA,R0 T2
        SUBI,R0 1
        STRA,R0 T2
        BCTR,EQ AKL7
        LODA,R0 *PRDATA,R3,+
        BSTA,UN UPALNUM
        BCTR,EQ AKL7
        COMI,R0 $41             ; a letter: lower case
        BCTR,LT AKL6A
        ADDI,R0 $20
AKL6A:  STRA,R0 SRVTBL,R2
        ADDI,R2 1
        COMI,R2 TABLE_LEN
        BCFR,EQ AKL6
AKL7:   LODA,R0 SRVTBL
        BCTR,EQ AKL8
        LODA,R0 FLAGS
        IORI,R0 FL_ROOM
        STRA,R0 FLAGS
AKL8:   BSTA,UN AKCLOSE
AKL9:   RETC,UN

; UPALNUM: R0 upper-cased if a letter; 0 (CC EQ) if not a letter or a
; digit. Leaf.
UPALNUM: COMI,R0 $61
        BCTR,LT UPA1
        COMI,R0 $7A
        BCTR,GT UPA9
        SUBI,R0 $20             ; (GT)
        RETC,UN
UPA1:   COMI,R0 $41
        BCTR,LT UPA2
        COMI,R0 $5A
        BCTR,GT UPA9
        COMI,R0 0               ; (GT)
        RETC,UN
UPA2:   COMI,R0 $30
        BCTR,LT UPA9
        COMI,R0 $39
        BCTR,GT UPA9
        COMI,R0 0
        RETC,UN
UPA9:   EORZ    R0
        RETC,UN

; SAVENM: key 0 = MYNAME (appkey_save_name). Calls five down.
SAVENM: LODI,R1 AKNAME
        BSTA,UN AKWRITE
        RETC,GT
        RETC,LT
        LODI,R3 $FF
SVN1:   LODA,R0 MYNAME,R3,+
        BCTR,EQ SVN2
        STRZ    R1
        LODA,R0 *PTXD,R1
        COMI,R3 NAME_LEN-1
        BCFR,EQ SVN1
SVN2:   BCTA,UN AKGO

; SAVEROOM: key 0x10 = the build's server + SRVTBL, if the key isn't ours
; already (appkey_save_room). Calls five down.
SAVEROOM:
        LODA,R0 FLAGS
        TMI,R0  FL_ROOM
        RETC,EQ
        LODI,R1 AKROOM
        BSTA,UN AKWRITE
        RETC,GT
        RETC,LT
        LODI,R3 $FF             ; "tcp://host:port/?table="
SVR1:   LODA,R0 *PDEFURL,R3,+
        BCTR,EQ SVR2
        STRZ    R1
        LODA,R0 *PTXD,R1
        BCTR,UN SVR1
SVR2:   LODI,R3 $FF             ; the table
SVR3:   LODA,R0 SRVTBL,R3,+
        BCTR,EQ SVR4
        STRZ    R1
        LODA,R0 *PTXD,R1
        COMI,R3 TABLE_LEN-1
        BCFR,EQ SVR3
SVR4:   BSTA,UN AKGO
        LODA,R0 FLAGS
        IORI,R0 FL_ROOM
        STRA,R0 FLAGS
        RETC,UN

; CLRROOM: key 0x10 emptied; no table (appkey_clear_room). Calls five down.
CLRROOM: LODI,R1 AKROOM
        BSTA,UN AKWRITE
        BCFR,EQ CLR1
        BSTA,UN AKGO
CLR1:   EORZ    R0
        STRA,R0 SRVTBL
        LODA,R0 FLAGS
        ANDI,R0 ~FL_ROOM&$FF
        STRA,R0 FLAGS
        RETC,UN

; ---- the socket ---------------------------------------------------------------------
; SCLOSE: CLOSE (a NAK is fine: nothing was open), the link down. Calls
; three down. Clobbers R0-R3.
SCLOSE: BSTA,UN MIDLE
        LODI,R3 NCCLOSE
        LODI,R2 0
        BSTA,UN MSTARTN
        BSTA,UN MGO
        EORZ    R0
        STRA,R0 NLINK
        STRA,R0 NKIND
        RETC,UN

; SOPEN: the socket: CLOSE first (a reset leaves the cart's open), then
; OPEN "N:TCP://host:port/" (read and write) -- from key 0x10 (FL_ROOM),
; else DEFSPEC. R0 = 1 open (NLINK), 0 not. Calls five down.
SOPEN:  BSTR,UN SCLOSE
        LODA,R0 FLAGS
        TMI,R0  FL_ROOM
        BCTR,EQ SOK
        BSTA,UN SOBEGIN         ; the build's
        LODI,R3 $FF
SOD1:   LODA,R0 *PDEFSP,R3,+
        BCTR,EQ SOGO
        STRZ    R1
        LODA,R0 *PTXD,R1
        BCTR,UN SOD1
SOK:    LODI,R1 AKROOM          ; the key again: its reply holds the URL
        BSTA,UN AKREAD
        BCTA,EQ SOFAIL
        STRA,R0 T2              ; T2: its length
        BSTR,UN SOBEGIN
        BSTA,UN SPECKEY
SOGO:   BSTA,UN MGO
        BCFR,EQ SOFAIL1
        LODA,R0 FLAGS           ; (the key, if read, closed after)
        TMI,R0  FL_ROOM
        BCFR,EQ SOG1
        BSTA,UN AKCLOSE
SOG1:   LODI,R0 NL_LINK
        STRA,R0 NLINK
        LODI,R0 1
        RETC,UN
SOFAIL1: LODA,R0 FLAGS
        TMI,R0  FL_ROOM
        BCFR,EQ SOFAIL
        BSTA,UN AKCLOSE
SOFAIL: EORZ    R0
        STRA,R0 NLINK
        RETC,UN
; SOBEGIN: OPEN: read and write, no translation. Leaf (fixed B's).
SOBEGIN: LODI,R3 NCOPEN
        LODI,R2 2
        BSTA,UN MSTARTN
        LODI,R1 NMRDWR
        BSTA,UN MPARB
        LODI,R1 0
        BCTA,UN MPARB

; SPECKEY: "N:" + the key's URL (in the reply from 2, T2 bytes) up to its
; "?" (a "/" before it dropped), upper-cased; ":9910" if it has no port;
; then "/". Leaf (fixed B's). Clobbers R0-R3, T0, T1.
SPECKEY: LODI,R3 1              ; its end: T2 bytes, or the "?"
        LODI,R2 0
SK1:    COMA,R2 T2
        BCTR,EQ SK2
        LODA,R0 *PRDATA,R3,+
        BCTR,EQ SK2
        COMI,R0 $3F
        BCTR,EQ SK2
        ADDI,R2 1
        BCTR,UN SK1
SK2:    STRA,R2 T0              ; T0: the URL's characters
        COMI,R2 0
        BCTR,EQ SK3
        LODZ    R2              ; a "/" last: dropped
        ADDI,R0 1
        STRZ    R3
        LODA,R0 *PRDATA,R3
        COMI,R0 $2F
        BCFR,EQ SK3
        LODA,R0 T0
        SUBI,R0 1
        STRA,R0 T0
SK3:    LODI,R1 $4E             ; "N:"
        LODA,R0 *PTXD,R1
        LODI,R1 $3A
        LODA,R0 *PTXD,R1
        EORZ    R0
        STRA,R0 T1              ; T1: its colons
        LODI,R3 1
        LODA,R2 T0
        BCTR,EQ SK6
SK4:    LODA,R0 *PRDATA,R3,+
        COMI,R0 $3A
        BCFR,EQ SK5
        LODA,R0 T1
        ADDI,R0 1
        STRA,R0 T1
        LODI,R0 $3A
SK5:    COMI,R0 $61             ; a..z up
        BCTR,LT SK5A
        COMI,R0 $7A
        BCTR,GT SK5A
        SUBI,R0 $20
SK5A:   STRZ    R1
        LODA,R0 *PTXD,R1
        BDRR,R2 SK4
SK6:    LODA,R0 T1              ; "tcp://host": one colon, no port
        COMI,R0 2
        BCFR,LT SK8
        LODI,R3 $FF
SK7:    LODA,R0 SPORT,R3,+
        BCTR,EQ SK8
        STRZ    R1
        LODA,R0 *PTXD,R1
        BCTR,UN SK7
SK8:    LODI,R1 $2F             ; "/"
        LODA,R0 *PTXD,R1
        RETC,UN
        ASCII
SPORT:  DB      ":9910",0
        GLYPHS

; CONN: "CONNECTING", the socket (the build's server: the table list).
; R0 = 1 open.
CONN:   LODI,R0 M_CONN
        STRA,R0 MSGID
        BSTA,UN MSG
        BCTA,UN SOPEN           ; tail call

; JOIN: the room kept, "JOINING <table>", the socket. R0 = 1 open.
JOIN:   BSTA,UN SAVEROOM
        LODI,R0 M_JOIN
        STRA,R0 MSGID
        BSTA,UN MSG
        BCTA,UN SOPEN           ; tail call

; FAIL: the socket closed, the sprites off, message MSGID (M_QUIET: none);
; its b7: the room forgotten too.
FAIL:   BSTA,UN SCLOSE
        ZBSR    *ZSCRCLR
        LODA,R0 MSGID
        ANDI,R0 $80
        STRA,R0 FAILRM          ; b7: the room too
        LODA,R0 MSGID
        ANDI,R0 $7F
        STRA,R0 MSGID
        COMI,R0 M_QUIET
        BCTR,EQ FAL1
        BSTA,UN MSG
FAL1:   LODA,R0 FAILRM
        RETC,EQ
        BCTA,UN CLRROOM         ; tail call

; ---- message ------------------------------------------------------------------------
; MSG: message MSGID on a clear screen: its two lines (the second the
; table's id for M_JOIN); then its time (1: none at all) or FIRE.
; Calls three down. Clobbers R0-R3.
MSG:    ZBSR    *ZSCRCLR
        DCOL    TXTI
        LODA,R1 MSGID
        LODA,R0 MSGA,R1         ; its first line
        ADDI,R0 MSGS&$FF
        STRZ    R1
        LODI,R0 MSGS>>8
        LODI,R3 5
        ZBSR    *ZCENTR
        LODA,R1 MSGID
        COMI,R1 M_JOIN
        BCTR,EQ MSG2
        LODA,R0 MSGB,R1         ; its second
        ADDI,R0 MSGS&$FF
        STRZ    R1
        LODI,R0 MSGS>>8
        LODI,R3 7
        ZBSR    *ZCENTR
        BCTR,UN MSG3
MSG2:   DAT     7,4             ; the table: its id, ASCII
        LODI,R3 $FF
MSG2A:  LODA,R0 SRVTBL,R3,+
        BCTR,EQ MSG3
        BSTA,UN A2G
        ZBSR    *ZDPUTC
        COMI,R3 TABLE_LEN-1
        BCFR,EQ MSG2A
MSG3:   LODA,R1 MSGID
        LODA,R0 MSGT,R1         ; its time
        STRA,R0 RCOUNT
        COMI,R0 1
        RETC,EQ
        COMI,R0 0
        BCFR,EQ MSG5
        DAT     10,3            ; ... or FIRE
        TXTAT   SFIRE
MSG4:   ZBSR    *ZFRAME
        LODA,R0 EVBTN
        RETC,GT
        LODA,R0 EVKEY
        COMI,R0 K_ENT
        BCFR,EQ MSG4
        RETC,UN
MSG5:   ZBSR    *ZFRAME
        LODA,R0 RCOUNT
        SUBI,R0 1
        STRA,R0 RCOUNT
        BCFR,EQ MSG5
        RETC,UN

; A2G: R0 an ASCII letter or digit to its glyph (anything else a space).
; Leaf.
A2G:    COMI,R0 $61
        BCTR,LT A2G0
        SUBI,R0 $20
A2G0:   COMI,R0 $41
        BCTR,LT A2G1
        COMI,R0 $5A
        BCTR,GT A2G9
        SUBI,R0 $41-GALPHA
        RETC,UN
A2G1:   COMI,R0 $30
        BCTR,LT A2G9
        COMI,R0 $39
        BCTR,GT A2G9
        SUBI,R0 $30-GDIGIT
        RETC,UN
A2G9:   EORZ    R0
        RETC,UN

; the messages: each one's lines (offsets in MSGS) and time (0: FIRE, 1: none)
        IF      ($&$FF) > $FF-$80
        ORG     ($+$FF)&$FF00   ; (MSG adds an offset to the low byte only)
        ENDIF
MSGA:   DB      SJOIN-MSGS,SNOANS-MSGS,SLOST-MSGS,SSAIDNO-MSGS,SSAIDNO-MSGS,SCONN-MSGS
MSGB:   DB      SNONE-MSGS,SFROM-MSGS,SAGAIN-MSGS,SNOROOM-MSGS,SUPDATE-MSGS,SNONE-MSGS
MSGT:   DB      1,0,120,0,0,1
MSGS:
SJOIN:  DB      "JOINING",$FF
SNOANS: DB      "NO ANSWER",$FF
SFROM:  DB      "FROM THE SERVER",$FF
SLOST:  DB      "CONNECTION LOST",$FF
SAGAIN: DB      "TRYING AGAIN",$FF
SSAIDNO: DB     "SERVER SAID NO",$FF
SNOROOM: DB     "NO SUCH ROOM",$FF
SUPDATE: DB     "UPDATE FUJIBLAST",$FF
SCONN:  DB      "CONNECTING",$FF
SNONE:  DB      $FF
SFIRE:  DB      "PRESS FIRE",$FF
        IF      (MSGS&$FF)+(SFIRE-MSGS) > $FF
        ERROR   "the messages must sit in one 256"
        ENDIF

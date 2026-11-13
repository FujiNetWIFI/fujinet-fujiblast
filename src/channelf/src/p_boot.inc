; ---------------------------------------------------------------------------
; p_boot.inc -- page BOOT: the rigs' test hook (src/test.c test_check), the
; lobby's appkeys (src/appkey.c), the socket (net_open, net_close) and the
; messages (screens.c message). The Astrocade's (src/astrocade/src/
; p_boot.inc), transcribed. Each transaction here is waited for (FNGO);
; the link's own, if one is out, is let finish first (MIDLE).
;
; Key 0 is the player's name, key 0x10 FujiBlast's server
; ("tcp://host:port/?table=id"). We keep only the table's id (SRVTBL) and
; FL_ROOM (the key holds our server); the OPEN reads the key again and
; stores "N:TCP://HOST:PORT/" from its reply straight into the transaction.
; Without the key, the server is the build's (DEFSPEC), and the table one
; picked from its list (E_JOIN then writes the key).
;
; Entries: E_ENTER the test hook, then the keys (r0 = the scene, 0: none);
; E_SAVENM; E_CONN and E_JOIN (r0 = 1: the socket is open); E_CLOSE;
; E_CLRRM; E_MSG (message MSGID); E_FAIL (the socket closed, message MSGID,
; b7 the room forgotten).
; ---------------------------------------------------------------------------

AKNAME  EQU 0
AKROOM  EQU 10H
FC_AKCLS EQU 0DBH               ; CLOSE_APPKEY

; the build's server (gen/server, or DEFSRV): DEFSPEC, DEFURL
        INCLUDE "config.inc"

; ---- test_check, then the keys ---------------------------------------------------
; A rig pokes TESTHK[0] = $5A (the scene in [2], its argument in [3]) within
; 20 frames of seeing [0], [1] = $3C, $C3.
BENTER: ENTER
        DCI TESTHK
        LI 03CH
        ST
        LI 0C3H
        ST
        LI 20
        STB RCOUNT
BEN1:   PI FRWAIT
        LDB TESTHK
        CI 05AH
        BZ BEN2
        DECB RCOUNT
        BNZ BEN1
        CLR
        STB TESTHK
        STB TESTHK+1
        PI AKLOAD
        CLR
        LR 0,A
        LEAVE
BEN2:   LI 0A5H
        STB TESTHK
        LDB TESTHK+2
        LR 0,A
        LEAVE

; ---- the appkeys -----------------------------------------------------------------
; Results come back in A (0: it worked): LEAVE keeps A but not the flags,
; so a caller tests A again (NI 0FFH).
;
; AKOPEN -- key r3 opened, mode r4 (0 read, 1 write): OPEN_APPKEY, no
;   parameters, a 6-byte payload (creator lo, hi, app, key, mode, 0).
;   A = 0 if it opened.
AKOPEN: ENTER
        PI MIDLE
        LI FD_FUJI
        LR 0,A
        LI FC_AKOPN
        LR 1,A
        CLR
        LR 2,A
        PI FNBEG
        LIS 1
        ST                      ; creator 1, LE
        CLR
        ST
        LIS 1
        ST                      ; app 1
        LR A,3
        ST                      ; the key
        LR A,4
        ST                      ; the mode
        CLR
        ST                      ; reserved: it must be sent
        PI FNGO
        LEAVE

; AKCLOSE -- the key closed.
AKCLOSE: ENTER
        LI FD_FUJI
        LR 0,A
        LI FC_AKCLS
        LR 1,A
        CLR
        LR 2,A
        PI FNBEG
        PI FNGO
        LEAVE

; AKREAD -- key r3 opened and read: the reply is its length (LE), then the
;   data (kept in the window until the next transaction: the caller closes
;   the key after).  r5 = the length, at most 64 (0, Z: none or it failed:
;   closed); A = r5.
AKREAD: ENTER
        CLR
        LR 4,A
        PI AKOPEN
        NI 0FFH
        BNZ AKR8
        LI FD_FUJI
        LR 0,A
        LI FC_AKRD
        LR 1,A
        CLR
        LR 2,A
        PI FNBEG
        PI FNGO
        NI 0FFH
        BNZ AKR8
        LDB FNREPLY+1           ; the length's high byte: 0, or it isn't a key
        NI 0FFH
        BNZ AKR8
        LDB FNREPLY
        CI 64
        BC AKR9
        LI 64
AKR9:   LR 5,A                  ; (A = the length: LEAVE keeps A, not the flags)
        LEAVE
AKR8:   PI AKCLOSE
        CLR
        LR 5,A
        LEAVE

; AKWRITE -- key r3 opened to write, the WRITE begun (DC0 on the TX: the
;   caller stores the data, then AKGO).  A = 0 if it opened.
AKWRITE: ENTER
        LIS 1
        LR 4,A
        PI AKOPEN
        NI 0FFH
        BNZ AKW9
        LI FD_FUJI
        LR 0,A
        LI FC_AKWR
        LR 1,A
        CLR
        LR 2,A
        PI FNBEG
        CLR                     ; A = 0: open
AKW9:   LEAVE

; AKGO -- the WRITE sent, the key closed.
AKGO:   ENTER
        PI FNGO
        PI AKCLOSE
        LEAVE

; AKLOAD -- MYNAME = key 0, letters and digits up to 8, upper-cased (NUL
;   padded); SRVTBL and FL_ROOM from key 0x10 (appkey_load).
AKLOAD: ENTER
        DCI MYNAME              ; both cleared
        LI NAME_LEN+1
        LR 0,A
AKL0:   CLR
        ST
        DS 0
        BNZ AKL0
        DCI SRVTBL
        LI TABLE_LEN+1
        LR 0,A
AKL0A:  CLR
        ST
        DS 0
        BNZ AKL0A
        LDB FLAGS
        NI 0FFH-FL_ROOM
        STB FLAGS
        LIS AKNAME              ; the name
        LR 3,A
        PI AKREAD
        NI 0FFH
        BZ AKL3
        DCI FNREPLY+2
        XDC
        DCI MYNAME
        LI NAME_LEN
        LR 6,A                  ; room left
AKL1:   XDC                     ; r5 the key's bytes left
        LM
        XDC
        LR 4,A
        PI UPALNUM
        BZ AKL2
        ST
        DS 6
        BZ AKL2A
AKL2:   DS 5
        BNZ AKL1
AKL2A:  PI AKCLOSE
AKL3:   LIS AKROOM>>4           ; the server: its table, if it has one
        SL 4
        LR 3,A
        PI AKREAD
        LR A,5
        CI 4
        BNC AKL3A
        LEAVE                   ; (4 bytes or fewer: none; closed)
AKL3A:  DCI FNREPLY+2
AKL4:   LM                      ; to the "="
        CI '='
        BZ AKL5
        DS 5
        BNZ AKL4
        BR AKL8
AKL5:   XDC
        DCI SRVTBL
        LI TABLE_LEN
        LR 6,A
AKL6:   DS 5
        BZ AKL7
        XDC
        LM
        XDC
        LR 4,A
        PI UPALNUM
        BZ AKL7
        CI 'A'-1                ; a letter: lower case
        BC AKL6A
        AI 20H
AKL6A:  ST
        DS 6
        BNZ AKL6
AKL7:   LDB SRVTBL
        NI 0FFH
        BZ AKL8
        LDB FLAGS
        OI FL_ROOM
        STB FLAGS
AKL8:   PI AKCLOSE
        LEAVE

; UPALNUM -- A = r4 upper-cased if a letter; 0 (Z) if not a letter or a
;   digit.  Leaf.
UPALNUM: LR A,4
        CI 'a'-1
        BC UPA1
        CI 'z'
        BNC UPA9
        AI -20H
        NI 0FFH
        POP
UPA1:   CI 'A'-1
        BC UPA2
        CI 'Z'
        BNC UPA9
        NI 0FFH
        POP
UPA2:   CI '0'-1
        BC UPA9
        CI '9'
        BNC UPA9
        NI 0FFH
        POP
UPA9:   CLR
        NI 0FFH
        POP

; SAVENM -- key 0 = MYNAME (appkey_save_name).
SAVENM: ENTER
        LIS AKNAME
        LR 3,A
        PI AKWRITE
        NI 0FFH
        BNZ SVN9
        XDC
        DCI MYNAME
        XDC
        LI NAME_LEN
        LR 6,A
SVN1:   XDC
        LM
        XDC
        NI 0FFH
        BZ SVN2
        ST
        DS 6
        BNZ SVN1
SVN2:   PI AKGO
SVN9:   LEAVE

; SAVEROOM -- key 0x10 = the build's server + SRVTBL, if the key isn't ours
;   already (appkey_save_room).
SAVEROOM: ENTER
        LDB FLAGS
        NI FL_ROOM
        BNZ SVR9
        LIS AKROOM>>4
        SL 4
        LR 3,A
        PI AKWRITE
        NI 0FFH
        BNZ SVR9
        DCI DEFURL              ; "tcp://host:port/?table="
        XDC
        PI FNTXS
        DCI SRVTBL
        XDC
        PI FNTXS
        PI AKGO
        LDB FLAGS
        OI FL_ROOM
        STB FLAGS
SVR9:   LEAVE

; CLRROOM -- key 0x10 emptied; no table (appkey_clear_room).
CLRROOM: ENTER
        LIS AKROOM>>4
        SL 4
        LR 3,A
        PI AKWRITE
        NI 0FFH
        BNZ CLR1
        PI AKGO
CLR1:   CLR
        STB SRVTBL
        LDB FLAGS
        NI 0FFH-FL_ROOM
        STB FLAGS
        LEAVE

; ---- the socket -------------------------------------------------------------------
; SCLOSE -- CLOSE (a NAK is fine: nothing was open), the link down.
SCLOSE: ENTER
        PI MIDLE
        LI FD_NET
        LR 0,A
        LI NC_CLOS
        LR 1,A
        CLR
        LR 2,A
        PI FNBEG
        PI FNGO
        CLR
        STB NLINK
        STB NKIND
        LEAVE

; SOPEN -- the socket: CLOSE first (a RESET leaves the cart's open), then
;   OPEN "N:TCP://host:port/" (read and write) -- from key 0x10 (FL_ROOM),
;   else DEFSPEC.  r0 = 1 open (NLINK), 0 not.
SOPEN:  ENTER
        PI SCLOSE
        LDB FLAGS
        NI FL_ROOM
        BNZ SOK
        PI SOBEGIN              ; the build's
        DCI DEFSPEC
        XDC
        PI FNTXS
        BR SOGO
SOK:    LIS AKROOM>>4           ; the key again: its reply holds the URL
        SL 4
        LR 3,A
        PI AKREAD
        NI 0FFH
        BZ SOFAIL
        LR A,5
        STB T2                  ; its length
        PI SOBEGIN
        PI SPECKEY
SOGO:   PI FNGO
        NI 0FFH
        BNZ SOFAIL1
        LDB FLAGS               ; (the key, if read, closed after)
        NI FL_ROOM
        BZ SOG1
        PI AKCLOSE
SOG1:   LIS NL_LINK
        STB NLINK
        LIS 1
        LR 0,A
        LEAVE
SOFAIL1: LDB FLAGS
        NI FL_ROOM
        BZ SOFAIL
        PI AKCLOSE
SOFAIL: CLR
        STB NLINK
        LR 0,A
        LEAVE

; SOBEGIN -- OPEN: read and write, no translation.  Leaf.  DC0 on the TX.
SOBEGIN: LI FD_NET
        LR 0,A
        LI NC_OPEN
        LR 1,A
        LIS 2
        LR 2,A
        DCI FNREG+FR_DEV        ; (FNBEG, inline: a leaf)
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        DCI FNREG+FR_DRST
        CLR
        ST
        DCI FNTX
        LIS 1
        ST
        LI NM_RW
        ST
        LIS 1
        ST
        CLR
        ST
        POP

; SPECKEY -- "N:" + the key's URL (in the reply from 2, T2 bytes) up to its
;   "?" (a "/" before it dropped), upper-cased; ":9910" if it has no port;
;   then "/".  Leaf.  Clobbers A, r0-r3, DCs; DC0 on the TX.
SPECKEY: DCI FNREPLY+2          ; r1: its characters, to T2 or the "?"
        LDB T2
        LR 0,A
        CLR
        LR 1,A
        DCI FNREPLY+2
SK1:    LR A,0
        NI 0FFH
        BZ SK2
        LM
        NI 0FFH
        BZ SK2
        CI '?'
        BZ SK2
        LR A,1
        INC
        LR 1,A
        DS 0
        BR SK1
SK2:    LR A,1                  ; a "/" last: dropped
        NI 0FFH
        BZ SK3
        DCI FNREPLY+1
        ADC
        LM
        CI '/'
        BNZ SK3
        DS 1
SK3:    DCI FNREPLY+2
        XDC
        DCI FNTX
        LI 'N'
        ST
        LI ':'
        ST
        CLR
        LR 2,A                  ; its colons
        LR A,1
        NI 0FFH
        BZ SK6
SK4:    XDC
        LM
        XDC
        CI ':'
        BNZ SK5
        LR 3,A
        LR A,2
        INC
        LR 2,A
        LR A,3
SK5:    CI 'a'-1                ; a..z up
        BC SK5A
        CI 'z'
        BNC SK5A
        AI -20H
SK5A:   ST
        DS 1
        BNZ SK4
SK6:    LR A,2                  ; "tcp://host": one colon, no port
        CI 1
        BNC SK8
        XDC
        DCI SPORT
        XDC
SK7:    XDC
        LM
        XDC
        NI 0FFH
        BZ SK8
        ST
        BR SK7
SK8:    LI '/'
        ST
        POP
SPORT:  DB ":9910",0

; CONN -- "CONNECTING", the socket (the build's server: the table list).
;   r0 = 1 open.
CONN:   ENTER
        LIS M_CONN
        STB MSGID
        PI MSG
        PI SOPEN
        LEAVE

; JOIN -- the room kept, "JOINING <table>", the socket.  r0 = 1 open.
JOIN:   ENTER
        PI SAVEROOM
        LIS M_JOIN
        STB MSGID
        PI MSG
        PI SOPEN
        LEAVE

; FAIL -- the socket closed, message MSGID (M_QUIET: none); its b7: the room
;   forgotten too.
FAIL:   ENTER
        PI SCLOSE
        LDB MSGID
        NI 080H
        STB FAILRM
        LDB MSGID
        NI 07FH
        STB MSGID
        CI M_QUIET
        BZ FAI1
        PI MSG
FAI1:   LDB FAILRM
        NI 0FFH
        BZ FAI9
        PI CLRROOM
FAI9:   LEAVE

; ---- the messages ------------------------------------------------------------------
; MSG -- message MSGID on a clear screen: its two lines (the second the
;   table's id for M_JOIN), white on black; then its time (1: none) or FIRE.
MSG:    ENTER
        CLR
        LR 0,A
        PI ECLS
        LDB MSGID               ; its first line
        SL 1
        DCI MSGA
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LI 22
        LR 1,A
        LI CV1
        LR 2,A
        LR 3,A
        PI ECENT
        LDB MSGID
        CI M_JOIN
        BZ MSG2
        SL 1                    ; its second
        DCI MSGB
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        BR MSG2A
MSG2:   DCI SRVTBL              ; the table: its id
MSG2A:  LI 30
        LR 1,A
        LI CV1
        LR 2,A
        LR 3,A
        PI ECENT
        LDB MSGID               ; its time
        DCI MSGT
        ADC
        LM
        STB RCOUNT
        CI 1
        BZ MSG9
        NI 0FFH
        BNZ MSG5
        DCI SFIRE               ; ... or FIRE
        LI 44
        LR 1,A
        LI CV1
        LR 2,A
        LR 3,A
        PI ECENT
MSG4:   PI FRWAIT
        PI INPOLL
        PI INTAKE
        LDB EVBTN
        NI 0FFH
        BNZ MSG9
        LDB EVKEY
        CI KEY_END
        BNZ MSG4
        BR MSG9
MSG5:   PI FRWAIT
        PI SNDTICK
        DECB RCOUNT
        BNZ MSG5
MSG9:   LEAVE

; the messages: each one's lines and time (0: FIRE, 1: none)
MSGA:   DW SJOIN, SNOANS, SLOST, SSAIDNO, SSAIDNO, SCONN
MSGB:   DW SNONE, SFROM, SAGAIN, SNOROOM, SUPDATE, SNONE
MSGT:   DB 1, 0, 120, 0, 0, 1
SJOIN:  DB "JOINING",0
SNOANS: DB "NO ANSWER",0
SFROM:  DB "FROM THE SERVER",0
SLOST:  DB "CONNECTION LOST",0
SAGAIN: DB "TRYING AGAIN",0
SSAIDNO: DB "SERVER SAID NO",0
SNOROOM: DB "NO SUCH ROOM",0
SUPDATE: DB "UPDATE FUJIBLAST",0
SCONN:  DB "CONNECTING",0
SNONE:  DB 0
SFIRE:  DB "PRESS FIRE",0

; p_boot.inc -- the lobby's appkeys (src/appkey.c) and the socket.
;
; Creator 1, app 1: key 0 our name, key 0x10 "tcp://host:port/?table=id"
; (the lobby's, or ours: kept when we join, emptied when the table refuses
; us). The server's N: spec ("N:TCP://host:port/") lives in the cart's RAM
; (CR_URL), SRVTBL the table (empty: the room list).
;
; These calls wait (fujilib's fn_go): they run between screens, never while
; the link steps.

AKREAD  EQU     0
AKWRITE EQU     1

; ak_open: OPEN_APPKEY key R9.0, mode D (the payload is all six bytes).
; D = 0 if it worked.
        RFIT    51
ak_open:
        PLO     RC
        GLO     R9
        PHI     RC
        LDI     FNDEVF
        PHI     R9
        LDI     FCOKEY
        PLO     R9
        LDI     0
        FCALL   fn_beg
        LDI     AK_CREATOR              ; creator (LE)
        FCALL   fn_txb
        LDI     0
        FCALL   fn_txb
        LDI     AK_APP
        FCALL   fn_txb
        GHI     RC                      ; the key
        FCALL   fn_txb
        GLO     RC                      ; the mode
        FCALL   fn_txb
        LDI     0
        FCALL   fn_txb
; ak_go: run it; D = 0 if it worked (and the server said yes).
ak_go:  FCALL   fn_go
        BNZ     akg9
        FJMP    fn_ack
akg9:   FRET

; ak_close: CLOSE_APPKEY.
        RFIT    14
ak_close:
        LDI     FNDEVF
        PHI     R9
        LDI     FCCKEY
        PLO     R9
        LDI     0
        FCALL   fn_beg
        FJMP    ak_go

; ak_read: READ_APPKEY key D: the reply window holds the u16 length, then
; the value (FN_REPLY+2). D = the length (0: none, or no appkeys; 64 at
; most). The key stays open: ak_close after reading the value.
        RFIT    47
ak_read:
        PLO     R9
        LDI     AKREAD
        FCALL   ak_open
        BNZ     akr0
        LDI     FNDEVF
        PHI     R9
        LDI     FCRKEY
        PLO     R9
        LDI     0
        FCALL   fn_beg
        FCALL   ak_go
        BNZ     akr0
        SLD     FN_REPLY+1              ; the length, at most 64
        BNZ     akr1
        DEC     R8
        LDN     R8
        SMI     65
        BDF     akr1
        LDN     R8
        FRET
akr1:   LDI     64
        FRET
akr0:   LDI     0
        FRET

; ak_write: WRITE_APPKEY key R9.0 = the RC.0 bytes at R8 (any page).
        RFIT    70
ak_write:
        GLO     RC
        PUTV    T7
        GHI     R8
        PUTV    T5
        GLO     R8
        PUTV    T6
        LDI     AKWRITE
        FCALL   ak_open
        BNZ     akw9
        LDI     FNDEVF
        PHI     R9
        LDI     FCWKEY
        PLO     R9
        LDI     0
        FCALL   fn_beg
        GETV    T5
        PHI     R8
        GETV    T6
        PLO     R8
        GETV    T7
        PLO     RC
akw1:   GLO     RC
        BZ      akw2
        DEC     RC
        LDA     R8
        FCALL   fn_txb
        BR      akw1
akw2:   FCALL   ak_go
akw9:   FJMP    ak_close

; ---------------------------------------------------------------------------
; ak_load -- our name from key 0 (letters, upper-cased, and digits, 8 at
;   most); the server and table from key 0x10, else the build's server and
;   no table.
; ---------------------------------------------------------------------------
        RFIT    91
ak_load:
        LDR     R8, MYNAME              ; no name yet
        LDI     9
        PLO     R9
akl0:   LDI     0
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     akl0
        SETV    SRVTBL, 0
        LDI     AK_NAME
        FCALL   ak_read
        BZ      akl3
        PLO     RC                      ; the value's bytes left
        LDR     R8, FN_REPLY+2
        LDR     R9, MYNAME
akl1:   GLO     RC
        BZ      akl3
        DEC     RC
        LDA     R8
        BZ      akl3
        FCALL   alnum                   ; a letter (upper) or a digit, else 0
        BZ      akl1
        SMI     'a'                     ; lower: upper
        BNF     akl2
        SMI     32
akl2:   ADI     'a'
        STR     R9
        INC     R9
        GLO     R9
        XRI     LO(MYNAME+NAME_LEN)
        BNZ     akl1
akl3:   FCALL   ak_close
        FCALL   spec_default
        LDI     AK_ROOM
        FCALL   ak_read
        SMI     6
        BNF     akl9                    ; too short to be a URL
        FCALL   ak_room
akl9:   FJMP    ak_close

; alnum: D if a letter or a digit (either case), else 0. Keeps R8, R9.
        RFIT    25
alnum:  PHI     RC
        SMI     '0'
        BNF     aln9
        SMI     10
        BNF     aln8
        GHI     RC
        ANI     0DFH
        SMI     'A'
        BNF     aln9
        SMI     26
        BDF     aln9
aln8:   GHI     RC
        FRET
aln9:   LDI     0
        FRET

; spec_default: CR_URL = the build's DEFSPEC.
        RFIT    55
spec_default:
        POKEAT  CR_URL
        LDR     R8, DEFSPEC
spd1:   LDA     R8
        PKD
        DEC     R8
        LDA     R8
        BNZ     spd1
        PKEND
        FRET

; ak_room: key 0x10's value (in the reply window): "...://host:port/..."
; becomes CR_URL "N:TCP://host:port/"; "table=id" SRVTBL (letters, lower,
; and digits; 8 at most). No "://": the default stays.
        RFIT    186
ak_room:
        LDR     R8, FN_REPLY+2          ; "://"
akm1:   LDA     R8
        BZ      akm9
        XRI     ':'
        BNZ     akm1
        LDA     R8
        XRI     '/'
        BNZ     akm1
        LDA     R8
        XRI     '/'
        BNZ     akm1
        LDN     R8                      ; no host: the default
        BZ      akm9
        XRI     '/'
        BZ      akm9
        POKEAT  CR_URL                  ; "N:TCP://", the host and port, "/"
        LDR     R9, u_ntcp
akm2:   LDA     R9
        BZ      akm3
        PKD
        BR      akm2
akm3:   LDA     R8
        BZ      akm4
        XRI     '/'
        BZ      akm4
        XRI     '/'!'?'
        BZ      akm4
        XRI     '?'
        PKD
        BR      akm3
akm4:   LDI     '/'
        PKD
        LDI     0
        PKD
        PKEND
        DEC     R8                      ; "table=" after it
akm5:   LDA     R8
        BZ      akm9
        XRI     't'
        BNZ     akm5
        LDR     R9, u_table
akm6:   LDA     R9
        BZ      akm7
        STR     R2
        LDA     R8
        XOR
        BNZ     akm5
        BR      akm6
akm7:   LDI     LO(SRVTBL)              ; the id
        PLO     RA
        LDI     TABLE_LEN
        PHI     R9
akm8:   LDA     R8
        FCALL   alnum
        BZ      akm8a
        SMI     'A'                     ; upper: lower
        BNF     akm8k
        SMI     26
        BDF     akm8k
        GHI     RC
        ADI     32
        BR      akm8b
akm8k:  GHI     RC
akm8b:  STR     RA
        INC     RA
        GHI     R9
        SMI     1
        PHI     R9
        BNZ     akm8
akm8a:  LDI     0
        STR     RA
akm9:   FRET
u_ntcp: DB      "N:TCP://", 0
u_table: DB     "able=", 0

; ak_save_name: key 0 = MYNAME.
        RFIT    30
ak_save_name:
        LDR     R8, MYNAME
        LDI     0
        PLO     RC
asn1:   LDA     R8
        BZ      asn2
        INC     RC
        GLO     RC
        XRI     NAME_LEN
        BNZ     asn1
asn2:   LDR     R8, MYNAME
        LDI     AK_NAME
        PLO     R9
        FJMP    ak_write

; ak_save_room: key 0x10 = "tcp://" host:port "/?table=" SRVTBL, built in
; FBUF (idle between tables).
        RFIT    83
ak_save_room:
        LDR     R9, FBUF
        LDR     R8, u_tcp
        FCALL   strcat
        LDR     R8, CR_URL+8            ; after "N:TCP://": to the '/'
asr1:   LDA     R8
        BZ      asr2
        XRI     '/'
        BZ      asr2
        XRI     '/'
        STR     R9
        INC     R9
        BR      asr1
asr2:   LDR     R8, u_room
        FCALL   strcat
        LDR     R8, SRVTBL
        FCALL   strcat
        GLO     R9                      ; its length
        SMI     LO(FBUF)
        PLO     RC
        LDR     R8, FBUF
        LDI     AK_ROOM
        PLO     R9
        FJMP    ak_write
u_tcp:  DB      "tcp://", 0
u_room: DB      "/?table=", 0

; strcat: the string at R8 copied to R9 (R9 left on its NUL).
        RFIT    8
strcat: LDA     R8
        STR     R9
        BZ      sct9
        INC     R9
        BR      strcat
sct9:   FRET

; ak_clear_room: key 0x10 written empty; no table.
        RFIT    15
ak_clear_room:
        SETV    SRVTBL, 0
        LDI     0
        PLO     RC
        LDI     AK_ROOM
        PLO     R9
        FJMP    ak_write

; ---------------------------------------------------------------------------
; sock_open -- the socket to CR_URL (read and write, no translation):
;   NLINK = NL_LINK if it opened (D = 0), else 0 (D != 0).
; sock_close -- closed (BYE sent by the caller first).
; ---------------------------------------------------------------------------
        RFIT    81
sock_open:
        LDI     NETDEV
        PHI     R9
        LDI     NCOPEN
        PLO     R9
        LDI     2
        FCALL   fn_beg
        LDI     NMRW                    ; 12: read and write
        FCALL   fn_pb
        LDI     0                       ; no translation
        FCALL   fn_pb
        LDR     R8, CR_URL
so1:    LDA     R8
        BZ      so2
        FCALL   fn_txb
        BR      so1
so2:    SETV    V_TMO, 30               ; (fn_go: 30 s)
        FCALL   fn_go
        BNZ     so9
        FCALL   fn_ack
        BNZ     so9
        SETV    NLINK, NL_LINK
        SETV    TAKEN, 0                ; a new socket: nothing taken yet
        SETV    CREDITED, 0
        LDI     0
        FRET
so9:    SETV    NLINK, 0
        LDI     1
        FRET

        RFIT    21
sock_close:
        SETV    NLINK, 0
        LDI     NETDEV
        PHI     R9
        LDI     NCCLOSE
        PLO     R9
        LDI     0
        FCALL   fn_beg
        FCALL   fn_go
        FRET

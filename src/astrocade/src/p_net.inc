; p_net.inc -- the link (src/net.c over the cart's mailbox): one step a
; frame that never waits. The Arcadia's (src/arcadia/src/p_net.inc, itself
; the Odyssey 2's), transcribed.
;
; The FujiNet's TCP socket is driven one transaction at a time, each posted
; in one frame and seen answered in a later one. The order: a WRITE of
; whatever is to be sent; else, with bytes known waiting (NBW), a READ of
; them, up to RD_MAX; else a STATUS (the bytes waiting; is the socket still
; up). A READ never asks for more than is waiting: firmware before
; afe7dddaa (2026-10-30; the Go Astrocade desktop's pin) takes a short
; read's bytes off the socket and drops them with an error. Every answer
; replaces the reply window -- an empty one too -- so a READ's bytes are
; decoded (a chunk a step) before anything else is posted.
;
; What is to be sent is kept as intents (TXF: HELLO, an action, our move,
; a credit...) and encoded only when a WRITE goes out: each frame built in
; RAWBUF (type, payload, Fletcher-16), its COBS streamed into the
; transaction, then a 0. The server's frames are decoded as they come:
; COBS, a running Fletcher, the last two bytes held back (they are the
; sums), and a patch's bytes written straight into ST (no staged copy: the
; screen is picked only at a commit, and a bad frame asks for the whole
; state again). Map bytes that change mark their tiles (MAPD) and the
; blasts and duds (MFLAGS) for the arena.

        JP      NSTEP           ; E_ENTER: a frame's step (NEVT: what came)
        JP      NHELLO          ; E_HELLO: at a table: the state zeroed, HELLO
        JP      NINIT           ; E_INIT: a fresh link's counters

PING_EVERY EQU  300             ; frames between keepalives (each is answered)
DEAD_AFTER EQU  900             ; frames without a frame: the link is gone
ASK_AGAIN  EQU  60              ; HELLO unanswered: again
RESYNC_GAP EQU  30              ; frames between RESYNCs
NTIMEOUT   EQU  240             ; frames a transaction may be out
RD_MAX     EQU  192             ; the most a READ asks for (slice 0 holds 256)
DCHUNK     EQU  24              ; encoded bytes decoded a step
DF_BLK     EQU  1               ; DFLAG: a COBS block began (a zero before the next)
DF_255     EQU  2               ;  a 254-byte one (no zero after it)
FNRPAGE    EQU  3BH             ; the reply's page (FNRDATA)

; WORK, while sending (ram.inc): the frame being built
RAWBUF  EQU     WORK            ; 24: type, payload, the two sums
RAWN    EQU     WORK+24         ; its length
BK      EQU     WORK+25         ; its TXF bit's number
CFROM   EQU     WORK+26         ; a COBS run's start
DRPOS0  EQU     WORK+27         ; (decoding) the chunk's start

; ---- entries ---------------------------------------------------------------------
; NINIT: a fresh link: the counters, the decoder, nothing out or owed.
NINIT:  XOR     A
        LD      (NKIND),A
        LD      (NWAIT),A
        LD      HL,NEVT
        LD      B,TBLHAVE-NEVT+1
NI1:    LD      (HL),A
        INC     HL
        DJNZ    NI1
        LD      A,0FFH
        LD      (TBLN),A
        RET

; NHELLO: sit at the table: the counters, ST zeroed (the state comes whole),
; HELLO due, no seat yet.
NHELLO: CALL    NINIT
        LD      HL,ST
        LD      B,STATE_SIZE
NH1:    LD      (HL),0
        INC     HL
        DJNZ    NH1
        LD      A,(NLINK)
        AND     NL_LINK
        OR      NL_JOINED
        LD      (NLINK),A
        LD      A,TX_HELLO
        LD      (TXF),A
        LD      A,NONE
        LD      (MYSEAT),A
        LD      HL,0
        LD      (JOINT),HL
        RET

; NSTEP: one frame of the link: the transaction out answered, a chunk
; decoded, the next posted, the clocks; our seat found at a commit; no
; state in JOIN_WAIT frames is EV_NOANS.
NSTEP:  XOR     A
        LD      (NEVT),A
        LD      A,(FLAGS)       ; a test scene: nothing comes or goes
        AND     FL_TEST
        JR      Z,NS0
        XOR     A
        LD      (TXF),A
        RET
NS0:    LD      A,(NLINK)
        AND     NL_LINK
        JR      NZ,NS1
        LD      A,EV_LOST
        LD      (NEVT),A
        RET
NS1:    CALL    NTXN            ; 1. the transaction out
        LD      A,(NEVT)
        OR      A
        RET     NZ              ; (lost)
        LD      A,(DLEFT)       ; 2. a chunk decoded
        OR      A
        JR      Z,NS2
        CALL    DECODE
        LD      A,(NEVT)
        AND     EV_ERROR
        RET     NZ              ; (an E frame: the caller's)
NS2:    CALL    NPOST           ; 3. the next transaction
        CALL    NTICK           ; 4. the clocks
        LD      A,(NEVT)        ; a commit: which seat is ours
        AND     EV_STATE
        CALL    NZ,FINDSEAT
        LD      A,(NLINK)       ; joined, nothing heard: JOIN_WAIT
        AND     NL_JOINED|NL_HEARD
        CP      NL_JOINED
        RET     NZ
        LD      HL,(JOINT)
        LD      A,(DT)
        LD      E,A
        LD      D,0
        ADD     HL,DE
        LD      (JOINT),HL
        LD      DE,JOIN_WAIT
        OR      A
        SBC     HL,DE
        RET     C
        LD      A,(NEVT)
        OR      EV_NOANS
        LD      (NEVT),A
        RET

; FINDSEAT: MYSEAT = the seat whose name is ours (NONE: watching).
FINDSEAT: LD    C,0
FS1:    LD      A,C
        CALL    SBITN           ; B = its bit
        LD      A,(ST+OFF_SEATED)
        AND     B
        JR      Z,FS4
        LD      A,C             ; its name: ST + OFF_NAMES + 8 C
        ADD     A,A
        ADD     A,A
        ADD     A,A
        LD      E,A
        LD      D,0
        LD      HL,ST+OFF_NAMES
        ADD     HL,DE
        LD      DE,MYNAME
        LD      B,NAME_LEN
FS2:    LD      A,(DE)
        CP      (HL)
        JR      NZ,FS4
        INC     HL
        INC     DE
        DJNZ    FS2
        LD      A,C
        LD      (MYSEAT),A
        RET
FS4:    INC     C
        LD      A,C
        CP      SEATS
        JR      NZ,FS1
        LD      A,NONE
        LD      (MYSEAT),A
        RET

; SBITN: B = seat A's bit (1, 2, 4, 8). Clobbers A.
SBITN:  LD      B,1
        OR      A
        RET     Z
SBN1:   SLA     B
        DEC     A
        JR      NZ,SBN1
        RET

; ---- the transactions ------------------------------------------------------------
; NTXN: 1. the transaction out: answered? Its answer taken (a READ's bytes
; to decode, a STATUS's bytes waiting and its word on the socket).
NTXN:   LD      A,(NKIND)
        OR      A
        RET     Z
        LD      HL,NWAIT
        INC     (HL)
        LD      A,(HL)
        CP      NTIMEOUT
        JR      Z,NLOST
        CALL    MDONE
        RET     NZ              ; still out
        LD      HL,NKIND
        LD      C,(HL)
        LD      (HL),0
        CALL    MACK
        JR      NZ,NT9          ; a NAK: nothing
        LD      A,C
        CP      NK_READ
        JR      NZ,NT2
        LD      A,(FNRXLL)      ; a READ's bytes: decode them
        LD      (DLEFT),A
        XOR     A
        LD      (DRPOS),A
        RET
NT2:    CP      NK_STATUS
        RET     NZ
        LD      HL,(FNRDATA)    ; STATUS: the bytes waiting, to be read
        LD      (NBW),HL
        LD      A,H
        OR      L
        RET     NZ
        LD      A,(FNRDATA+2)   ; ... or none, but connected
        OR      A
        RET     NZ
        JR      NLOST
NT9:    LD      HL,0            ; (what is waiting asked again: a STATUS)
        LD      (NBW),HL
        RET
NLOST:  XOR     A
        LD      (NLINK),A
        LD      A,(NEVT)
        OR      EV_LOST
        LD      (NEVT),A
        RET

; NPOST: 3. the next transaction, once nothing is out and the last READ is
; decoded: a WRITE, else a READ of what is waiting, else a STATUS.
NPOST:  LD      A,(NKIND)
        OR      A
        RET     NZ
        LD      A,(DLEFT)
        OR      A
        RET     NZ
        LD      A,(TXF)
        OR      A
        JP      NZ,POSTW
        LD      HL,(NBW)
        LD      A,H
        OR      L
        JR      NZ,POSTR
        LD      E,NCSTAT        ; none known waiting: STATUS
        LD      L,2
        CALL    MSTARTN
        XOR     A
        CALL    FNPARB
        XOR     A
        CALL    FNPARB
        LD      A,NK_STATUS
        JR      NPGO
POSTR:  LD      DE,RD_MAX       ; READ what is waiting, up to RD_MAX
        OR      A
        SBC     HL,DE
        JR      NC,NPR1         ; (RD_MAX of it, the rest kept)
        ADD     HL,DE           ; (all of it)
        EX      DE,HL
        LD      HL,0
NPR1:   LD      (NBW),HL        ; NBW: what is left
        PUSH    DE              ; DE: the READ's length
        LD      E,NCREAD
        LD      L,1
        CALL    MSTARTN
        POP     HL
        CALL    MPARW
        LD      A,NK_READ
NPGO:   LD      (NKIND),A       ; posted: kind A
        XOR     A
        LD      (NWAIT),A
        JP      MPOST

; POSTW: every frame TXF wants, in one WRITE: its length first (each
; frame's raw length + 2: COBS adds one, then the 0), then the frames.
POSTW:  LD      HL,FLEN
        LD      A,(TXF)
        LD      C,A
        LD      B,8
        XOR     A
PW1:    RR      C
        JR      NC,PW2
        ADD     A,(HL)
PW2:    INC     HL
        DJNZ    PW1
        PUSH    AF
        LD      E,NCWRITE
        LD      L,1
        CALL    MSTARTN
        POP     AF
        LD      L,A
        LD      H,0
        CALL    MPARW
        XOR     A               ; each frame, in bit order
PW3:    LD      (BK),A
        CALL    BITOF           ; C = its bit
        LD      A,(TXF)
        AND     C
        JR      Z,PW4
        CALL    BUILD
        CALL    COBSOUT
PW4:    LD      A,(BK)
        INC     A
        CP      8
        JR      NZ,PW3
        XOR     A
        LD      (TXF),A
        LD      A,NK_WRITE
        JR      NPGO

; BITOF: C = bit A (0-7). Clobbers A.
BITOF:  LD      C,1
        OR      A
        RET     Z
BO1:    SLA     C
        DEC     A
        JR      NZ,BO1
        RET

; the frames, by TXF bit: raw length + 2 (type, payload, 2 sums; COBS's 1;
; the 0), and the type
FLEN:   DB      1+18+2+2,1+4+2+2,1+4+2+2,1+1+2+2,1+0+2+2,1+0+2+2,1+0+2+2,1+0+2+2
FTYPES: DB      SC_HELLO,SC_ACT,SC_MOVE,SC_CREDIT,SC_PING,SC_RESYNC,SC_TABLES,SC_BYE

; BUILD: frame BK (a TXF bit's number) into RAWBUF: its type, its payload,
; then the two Fletcher sums; RAWN its length.
BUILD:  LD      A,(BK)
        LD      C,A
        LD      B,0
        LD      HL,FTYPES
        ADD     HL,BC
        LD      A,(HL)
        LD      DE,RAWBUF
        LD      (DE),A
        INC     DE
        LD      A,C
        OR      A
        JR      Z,BHELLO
        DEC     A
        JR      Z,BACT
        DEC     A
        JR      Z,BMOVE
        DEC     A
        JR      Z,BCRED
        JR      BSUM            ; the rest: no payload
BHELLO: LD      A,PROTO_VERSION ; version, flags, the table's id, our name
        LD      (DE),A
        INC     DE
        XOR     A
        LD      (DE),A
        INC     DE
        LD      HL,SRVTBL
        LD      BC,TABLE_LEN
        LDIR
        LD      HL,MYNAME
        LD      BC,NAME_LEN
        LDIR
        JR      BSUM
BACT:   LD      HL,ASEQ         ; seq, act, a1, a2
        JR      B4
BMOVE:  LD      HL,SENTX        ; x, y, dir|moving, warp
B4:     LD      BC,4
        LDIR
        JR      BSUM
BCRED:  LD      A,(TAKEN)       ; what we've taken (and credited with it)
        LD      (CREDITED),A
        LD      (DE),A
        INC     DE
BSUM:   EX      DE,HL           ; Fletcher-16 over RAWBUF up to DE
        LD      DE,RAWBUF
        OR      A
        SBC     HL,DE
        LD      B,L             ; its length
        LD      A,L
        ADD     A,2
        LD      (RAWN),A        ; and the sums
        EX      DE,HL
        LD      C,0             ; s1
        LD      D,0             ; s2
BS1:    LD      A,C
        ADD     A,(HL)
        CALL    MOD255
        LD      C,A
        ADD     A,D
        CALL    MOD255
        LD      D,A
        INC     HL
        DJNZ    BS1
        LD      (HL),C
        INC     HL
        LD      (HL),D
        RET

; MOD255: A, just added (CY its carry), mod 255: a carry is 1 (256 is 1
; mod 255), and 255 is 0.
MOD255: JR      NC,MOD1
        INC     A
MOD1:   CP      255
        RET     NZ
        XOR     A
        RET

; COBSOUT: RAWBUF's RAWN bytes as COBS into the TX stream, then a 0: each
; run up to a zero (or the end) is its length + 1, then its bytes.
COBSOUT: LD     HL,RAWBUF       ; HL: the run's start
        LD      A,(RAWN)
        LD      B,A             ; bytes left
CO1:    PUSH    HL              ; a run: up to a zero or the end
        LD      C,0
CO2:    LD      A,B
        OR      A
        JR      Z,CO3
        LD      A,(HL)
        OR      A
        JR      Z,CO3
        INC     HL
        INC     C
        DEC     B
        JR      CO2
CO3:    POP     HL
        LD      A,C             ; its code: length + 1
        INC     A
        CALL    FNTXBYT
        LD      A,C
        OR      A
        JR      Z,CO5
CO4:    LD      A,(HL)
        CALL    FNTXBYT
        INC     HL
        DEC     C
        JR      NZ,CO4
CO5:    LD      A,B             ; the end: the frame's 0
        OR      A
        JR      Z,CO6
        INC     HL              ; a zero: past it, the next run
        DEC     B
        JR      CO1
CO6:    XOR     A
        JP      FNTXBYT

; NTICK: 4. the clocks: a RESYNC owed, a credit, the keepalive, HELLO
; again, a silent link given up.
NTICK:  LD      HL,RHOLD        ; a RESYNC owed, once the gap is over
        LD      A,(HL)
        AND     7FH
        JR      Z,NTK2
        DEC     (HL)
        JR      NTK3
NTK2:   BIT     7,(HL)
        JR      Z,NTK3
        LD      (HL),RESYNC_GAP
        LD      A,(TXF)
        OR      TX_RESYNC
        LD      (TXF),A
NTK3:   LD      A,(CREDITED)    ; taken - credited >= 48: a credit
        LD      B,A
        LD      A,(TAKEN)
        SUB     B
        CP      48
        JR      C,NTK4
        LD      A,(TXF)
        OR      TX_CREDIT
        LD      (TXF),A
NTK4:   LD      HL,(QUIET)      ; quiet > DEAD_AFTER: gone
        INC     HL
        LD      (QUIET),HL
        LD      DE,DEAD_AFTER+1
        OR      A
        SBC     HL,DE
        JP      NC,NLOST
        LD      HL,(QPING)      ; every PING_EVERY: a credit and a PING
        INC     HL
        LD      (QPING),HL
        LD      DE,PING_EVERY
        OR      A
        SBC     HL,DE
        JR      NZ,NTK6
        LD      (QPING),HL
        LD      A,(TXF)
        OR      TX_CREDIT|TX_PING
        LD      (TXF),A
NTK6:   LD      HL,QASK         ; every ASK_AGAIN: HELLO, if unanswered
        INC     (HL)
        LD      A,(HL)
        CP      ASK_AGAIN
        RET     C
        LD      (HL),0
        LD      A,(NLINK)
        AND     NL_JOINED|NL_HEARD
        CP      NL_JOINED
        RET     NZ
        LD      A,(TXF)
        OR      TX_HELLO
        LD      (TXF),A
        RET

; ---- the decoder -----------------------------------------------------------------------
; DECODE: up to DCHUNK of the READ's bytes; TAKEN counts them. One loop, the
; common path (a patch's byte) inline. HL walks the reply (3B00H + DRPOS),
; B counts the chunk.
DECODE: LD      A,(DLEFT)       ; this chunk: DCHUNK at most
        CP      DCHUNK+1
        JR      C,DE1
        LD      A,DCHUNK
DE1:    LD      B,A
        LD      C,A
        LD      A,(DLEFT)
        SUB     C
        LD      (DLEFT),A
        LD      A,(DRPOS)
        LD      (DRPOS0),A
        LD      L,A
        LD      H,FNRPAGE
DC1:    LD      A,(HL)
        INC     L
        OR      A
        JP      Z,DCEND         ; 0: a frame's end
        LD      C,A
        LD      A,(DCODE)
        OR      A
        JP      Z,DCCODE
        DEC     A               ; a data byte
        LD      (DCODE),A
        LD      A,C
; emit: into the two held back (D1 the older, D2); the one pushed out is the
; frame's, once two are held
DEM:    LD      C,A
        LD      A,(DN)
        CP      2
        JP      C,DEMN
        LD      A,(D1)
        LD      E,A             ; E: the frame's byte
        LD      A,(D2)
        LD      (D1),A
        LD      A,C
        LD      (D2),A
        LD      A,(S1)          ; s1 += b, s2 += s1 (mod 255)
        ADD     A,E
        JR      NC,DP1
        INC     A
DP1:    CP      255
        JR      NZ,DP2
        XOR     A
DP2:    LD      (S1),A
        LD      C,A
        LD      A,(S2)
        ADD     A,C
        JR      NC,DP3
        INC     A
DP3:    CP      255
        JR      NZ,DP4
        XOR     A
DP4:    LD      (S2),A
        LD      A,(FIDX)
        OR      A
        JR      Z,DPTYPE
        LD      A,(FTYPE)
        CP      CS_PATCH
        JR      Z,DPATCH
        LD      A,(FIDX)        ; C, E, T: the payload's place + 1 (to 255)
        INC     A
        JR      Z,DPO
        LD      (FIDX),A
DPO:    PUSH    BC
        PUSH    HL
        CALL    PCOTHER
        POP     HL
        POP     BC
        JR      DCN
DPTYPE: LD      A,1             ; its type
        LD      (FIDX),A
        LD      A,E
        LD      (FTYPE),A
        XOR     A
        LD      (QST),A
        JR      DCN
DPATCH: LD      A,(QST)         ; a patch
        CP      2
        JR      Z,DPQ
        CALL    QHDR
        JR      DCN
DPQ:    PUSH    HL              ; its run's byte, into ST
        LD      A,(QOFF)
        LD      C,A
        PUSH    BC
        LD      B,0
        LD      HL,ST
        ADD     HL,BC
        POP     BC
        CP      OFF_MAP
        JR      C,QPUT
        CP      OFF_MAP+TILES
        JR      NC,QMOV
        LD      A,(HL)          ; a map byte: the old tile against it
        CP      E
        JR      Z,QPUT
        PUSH    BC
        PUSH    HL
        LD      D,A
        LD      A,C
        SUB     OFF_MAP
        CALL    MAPMARK         ; tile A from D to E
        POP     HL
        POP     BC
        JR      QPUT
QMOV:   LD      A,(MFLAGS)      ; the movers
        OR      MF_MOVERS
        LD      (MFLAGS),A
QPUT:   LD      (HL),E
        POP     HL
        LD      A,C
        INC     A
        LD      (QOFF),A
        LD      A,(QLEN)
        DEC     A
        LD      (QLEN),A
        JR      NZ,DCN
        LD      (QST),A         ; (the run's last: the next run's header)
DCN:    DEC     B               ; the chunk's next
        JP      NZ,DC1
DCOUT:  LD      A,L             ; taken += the bytes decoded
        LD      (DRPOS),A
        LD      HL,DRPOS0
        SUB     (HL)
        LD      HL,TAKEN
        ADD     A,(HL)
        LD      (HL),A
        RET
DEMN:   INC     A               ; (not two held yet)
        LD      (DN),A
        LD      A,(D2)          ; the delay line still steps
        LD      (D1),A
        LD      A,C
        LD      (D2),A
        JR      DCN
DCCODE: LD      A,C             ; a block's code (C): its data bytes
        DEC     A
        LD      (DCODE),A
        LD      A,(DFLAG)       ; (D: the last block's)
        LD      D,A
        LD      A,C
        CP      255
        LD      A,DF_BLK
        JR      NZ,DCC1
        LD      A,DF_BLK|DF_255
DCC1:   LD      (DFLAG),A
        LD      A,D             ; the zero before it, unless it's the frame's
        CP      DF_BLK          ;  first or after a 254-byte one
        JR      NZ,DCN
        XOR     A
        JP      DEM
DCEND:  PUSH    BC              ; a frame's end
        PUSH    HL
        CALL    FREND
        POP     HL
        POP     BC
        LD      A,(NEVT)        ; an error: stop
        AND     EV_ERROR
        JR      NZ,DCOUT
        JR      DCN

; QHDR: a run's header: its offset, its length (past the state: a bad run,
; ignored). A = QST, E the byte. Preserves BC, HL.
QHDR:   OR      A
        JR      Z,QH0
        CP      1
        RET     NZ              ; 3: a bad run
        LD      A,E             ; its length
        LD      (QLEN),A
        OR      A
        JR      Z,QHZ           ; (no bytes: the next run)
        LD      D,A
        LD      A,(QOFF)
        ADD     A,D
        JR      C,QHBAD
        CP      STATE_SIZE+1
        JR      NC,QHBAD
        LD      A,2
        LD      (QST),A
        RET
QH0:    LD      A,E             ; its offset
        LD      (QOFF),A
        LD      A,1
        LD      (QST),A
        RET
QHZ:    XOR     A
        LD      (QST),A
        RET
QHBAD:  LD      A,3
        LD      (QST),A
        RET

; PCOTHER: C, E: the first payload byte kept (QOFF); T: index (QLEN; 0FFH
; out of range), count (QOFF), the record into TBLS at the index's place.
; E the byte.
PCOTHER: LD     A,(FTYPE)
        CP      CS_TABLE
        JR      Z,PCT
        LD      A,(FIDX)
        CP      2
        RET     NZ
        LD      A,E
        LD      (QOFF),A
        RET
PCT:    LD      A,(FIDX)
        CP      2
        JR      NZ,PCT2
        LD      A,E             ; the index
        CP      MAX_TABLES
        JR      C,PCT1
        LD      A,0FFH
PCT1:   LD      (QLEN),A
        RET
PCT2:   CP      3
        JR      NZ,PCT3
        LD      A,E             ; the count, at most MAX_TABLES
        CP      MAX_TABLES+1
        JR      C,PCT2A
        LD      A,MAX_TABLES
PCT2A:  LD      (QOFF),A
        RET
PCT3:   SUB     4               ; a record's byte: TBLS + index x 24 + FIDX - 4
        CP      TABLE_REC
        RET     NC
        LD      C,A
        LD      A,(QLEN)
        CP      0FFH
        RET     Z
        LD      L,A             ; 24 index
        LD      H,0
        ADD     HL,HL
        ADD     HL,HL
        ADD     HL,HL
        LD      D,H
        LD      A,L
        ADD     HL,HL
        ADD     A,L
        LD      L,A
        LD      A,D
        ADC     A,H
        LD      H,A
        LD      B,0
        ADD     HL,BC
        LD      BC,TBLS
        ADD     HL,BC
        LD      (HL),E
        RET

; MAPMARK: tile A changes from D to E: its bit in MAPD; a blast's centre
; that wasn't a flame; a bomb become a dud. Clobbers A, BC, HL.
MAPMARK: LD     C,A
        LD      A,(MFLAGS)
        OR      MF_MAP
        LD      (MFLAGS),A
        LD      A,C             ; MAPD + tile / 8, bit tile % 8
        RRCA
        RRCA
        RRCA
        AND     1FH
        LD      L,A
        LD      H,0
        PUSH    DE
        LD      DE,MAPD
        ADD     HL,DE
        POP     DE
        LD      A,C
        AND     7
        CALL    BITOF           ; C = its bit
        LD      A,(HL)
        OR      C
        LD      (HL),A
        LD      A,E             ; a blast's centre, from not a flame
        CP      T_FLAME_C
        JR      NZ,MM6
        LD      A,D
        CP      T_FLAME_C
        JR      C,MM5
        CP      T_FLAME_D+1
        JR      C,MM6
MM5:    LD      A,(MFLAGS)
        OR      MF_BOOM
        LD      (MFLAGS),A
MM6:    LD      A,D             ; a bomb become a dud
        CP      T_BOMB
        RET     NZ
        LD      A,E
        CP      T_DUD
        RET     NZ
        LD      A,(MFLAGS)
        OR      MF_FIZZ
        LD      (MFLAGS),A
        RET

; FREND: a frame's end: its sums checked (S1, S2 against the two held
; back), then what its type does; a bad one asks for the whole state
; again. The frame's state reset.
FREND:  LD      A,(FIDX)
        OR      A
        JR      Z,FEBAD         ; nothing but the sums: malformed
        LD      A,(DN)
        CP      2
        JR      NZ,FEBAD
        LD      A,(D1)
        LD      HL,S1
        CP      (HL)
        JR      NZ,FEBAD
        LD      A,(D2)
        LD      HL,S2
        CP      (HL)
        JR      NZ,FEBAD
        LD      HL,0            ; a good frame: the link is alive
        LD      (QUIET),HL
        LD      (QPING),HL
        XOR     A
        LD      (QASK),A
        LD      A,(FTYPE)
        CP      CS_COMMIT
        JR      Z,FECOM
        CP      CS_ERROR
        JR      Z,FEERR
        CP      CS_TABLE
        JR      NZ,FERESET
        LD      A,(QOFF)        ; a table: the count, its record in
        LD      (TBLN),A
        LD      A,(QLEN)
        CP      MAX_TABLES
        JR      NC,FERESET
        CALL    BITOF
        LD      A,(TBLHAVE)
        OR      C
        LD      (TBLHAVE),A
        JR      FERESET
FECOM:  LD      A,(NLINK)       ; a commit: the state is whole
        BIT     1,A             ; NL_JOINED
        JR      Z,FERESET
        OR      NL_HEARD
        LD      (NLINK),A
        LD      A,(NEVT)
        OR      EV_STATE
        LD      (NEVT),A
        JR      FERESET
FEERR:  LD      A,(QOFF)
        LD      (NERR),A
        LD      A,(NEVT)
        OR      EV_ERROR
        LD      (NEVT),A
        JR      FERESET
FEBAD:  LD      HL,RHOLD        ; a RESYNC owed
        SET     7,(HL)
        LD      HL,NBAD         ; (counted, for the rigs)
        INC     (HL)
        JR      NZ,FERESET
        DEC     (HL)
FERESET: XOR    A
        LD      (DCODE),A
        LD      (DFLAG),A
        LD      (DN),A
        LD      (FIDX),A
        LD      (S1),A
        LD      (S2),A
        LD      (QST),A
        RET

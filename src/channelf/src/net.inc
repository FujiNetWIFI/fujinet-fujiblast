; ---------------------------------------------------------------------------
; net.inc -- the link (src/net.c over the cart's mailbox): one step a pass
; that never waits. The Astrocade's (src/astrocade/src/p_net.inc),
; transcribed; resident, the main loop's.
;
; The FujiNet's TCP socket is driven one transaction at a time, each posted
; in one pass and seen answered in a later one. The order: a WRITE of
; whatever is to be sent; else, with bytes known waiting (NBW), a READ of
; them, up to RD_MAX; else a STATUS (the bytes waiting; is the socket still
; up). A READ never asks for more than is waiting: firmware before
; afe7dddaa drops a short read's bytes.
;
; What is to be sent is kept as intents (TXF: HELLO, an action, our move, a
; credit...) and encoded only when a WRITE goes out: each frame built in
; RAWBUF (type, payload, Fletcher-16), its COBS stored into the
; transaction, then a 0.
;
; The server's frames are decoded in two steps (the Astrocade does it in
; one; the F8 has too few registers for that): a chunk of the READ's bytes
; a pass is COBS-decoded into FBUF; at a frame's 0 its sums are checked and
; only then is it applied (FREND): a patch's runs written into ST (map
; bytes that change marked for the arena: DMARK, BORN, MFLAGS), a commit,
; a table record, an error. A bad frame asks for the whole state again.
; ---------------------------------------------------------------------------

PING_EVERY EQU 300              ; jiffies between keepalives (each is answered)
DEAD_AFTER EQU 900              ; jiffies without a frame: the link is gone
ASK_AGAIN  EQU 60               ; HELLO unanswered: again
RESYNC_GAP EQU 30               ; jiffies between RESYNCs
NTIMEOUT   EQU 240              ; passes a transaction may be out
JOIN_WAIT  EQU 600              ; jiffies at a table with no state: no answer
RD_MAX     EQU 192              ; the most a READ asks for
DCHUNK     EQU 32               ; encoded bytes decoded a pass
FBMAX      EQU MAX_FRAME+4      ; FBUF: a decoded frame

NK_READ  EQU 1                  ; NKIND: the transaction out
NK_STATUS EQU 2
NK_WRITE EQU 3
NL_LINK  EQU 1                  ; NLINK: the socket is open
NL_JOINED EQU 2                 ;   HELLO sent: at a table
NL_HEARD EQU 4                  ;   a commit came since
EV_STATE EQU 1                  ; NEVT: what this pass's step brought
EV_ERROR EQU 2
EV_LOST  EQU 4
EV_NOANS EQU 8                  ;   no state in JOIN_WAIT jiffies
TX_HELLO EQU 01H                ; TXF: what the next WRITE carries
TX_ACT   EQU 02H
TX_MOVE  EQU 04H
TX_CREDIT EQU 08H
TX_PING  EQU 10H
TX_RESYNC EQU 20H
TX_TABLES EQU 40H
TX_BYE   EQU 80H
MF_MAP   EQU 01H                ; MFLAGS: what the patches did since the arena looked
MF_BOOM  EQU 02H
MF_FIZZ  EQU 04H
MF_MOVERS EQU 08H
FL_ROOM  EQU 01H                ; FLAGS: key 0x10 holds our server and table
FL_TEST  EQU 02H                ;   an offline test scene: the link is quiet
FL_MUTE  EQU 04H
MAX_TABLES EQU 8

; ---------------------------------------------------------------------------
; NINIT -- a fresh link: nothing out, owed or half-decoded.  Leaf.
;   Clobbers A, r0, DC0.
; ---------------------------------------------------------------------------
NINIT:  DCI NZERO
        LI NZEND-NZERO
        LR 0,A
NI1:    CLR
        ST
        DS 0
        BNZ NI1
        LI 0FFH
        STB TBLN
        POP

; NHELLO -- sit at the table: the link's counters, ST zeroed (the state comes
;   whole), HELLO due, no seat yet.  Clobbers A, r0, DC0.
NHELLO: ENTER
        PI NINIT
        DCI ST
        LI STATE_SIZE
        LR 0,A
NH1:    CLR
        ST
        DS 0
        BNZ NH1
        LDB NLINK
        NI NL_LINK
        OI NL_JOINED
        STB NLINK
        LIS TX_HELLO
        STB TXF
        LI 0FFH
        STB MYSEAT
        CLR
        STB JOINT
        STB JOINT+1
        LEAVE

; ---------------------------------------------------------------------------
; NSTEP -- a pass of the link: the transaction out answered, a chunk
;   decoded, the next posted, the clocks (DT jiffies); our seat found at a
;   commit; no state in JOIN_WAIT jiffies is EV_NOANS.  NEVT: what came.
; ---------------------------------------------------------------------------
NSTEP:  ENTER
        CLR
        STB NEVT
        LDB FLAGS               ; a test scene: nothing comes or goes
        NI FL_TEST
        BZ NS0
        CLR
        STB TXF
        LEAVE
NS0:    LDB NLINK
        NI NL_LINK
        BNZ NS1
        LIS EV_LOST
        STB NEVT
        LEAVE
NS1:    PI NTXN                 ; 1. the transaction out
        LDB NEVT
        NI 0FFH
        BZ NS1A
        LEAVE                   ; (lost)
NS1A:   LDB DLEFT               ; 2. a chunk decoded
        NI 0FFH
        BZ NS2
        PI DECODE
        LDB NEVT
        NI EV_ERROR
        BZ NS2
        LEAVE                   ; (an E frame: the caller's)
NS2:    PI NPOST                ; 3. the next transaction
        PI NTICK                ; 4. the clocks
        LDB NEVT                ; a commit: which seat is ours
        NI EV_STATE
        BZ NS3
        PI FINDSEAT
NS3:    LDB NLINK               ; joined, nothing heard: JOIN_WAIT
        NI NL_JOINED+NL_HEARD
        CI NL_JOINED
        BNZ NS9
        LDB DT
        LR 0,A
        DCI JOINT
        LM
        AS 0
        LR 1,A
        LM
        LNK
        LR 2,A
        DCI JOINT
        LR A,1
        ST
        LR A,2
        ST
        LR A,1                  ; JOINT >= JOIN_WAIT: no answer (low: C
        AI 256-(JOIN_WAIT&0FFH) ;  when no borrow; high: C when r2 + C >
        LR A,2                  ;  JOIN_WAIT's high)
        LNK
        AI 255-(JOIN_WAIT>>8)
        BNC NS9
        LDB NEVT
        OI EV_NOANS
        STB NEVT
NS9:    LEAVE

; FINDSEAT -- MYSEAT = the seated seat whose name is ours (FF: watching).
;   Leaf.  Clobbers A, r0-r4, DCs.
FINDSEAT: CLR
        LR 0,A                  ; seat
        LIS 1
        LR 1,A                  ; its bit
FS1:    LDB ST+OFF_SEATED
        NS 1
        BZ FS4
        LR A,0                  ; its name: ST + OFF_NAMES + 8 s
        SL 1
        SL 1
        SL 1
        DCI ST+OFF_NAMES
        ADC
        XDC
        DCI MYNAME
        LI NAME_LEN
        LR 2,A
FS2:    LM
        XDC
        CM
        XDC
        BNZ FS4
        DS 2
        BNZ FS2
        LR A,0
        STB MYSEAT
        POP
FS4:    LR A,1
        SL 1
        LR 1,A
        LR A,0
        INC
        LR 0,A
        CI SEATS
        BNZ FS1
        LI 0FFH
        STB MYSEAT
        POP

; ---------------------------------------------------------------------------
; NTXN -- 1. the transaction out: answered?  Its answer taken (a READ's
;   bytes to decode, a STATUS's bytes waiting and its word on the socket).
; ---------------------------------------------------------------------------
NTXN:   LDB NKIND               ; (nothing out: out before ENTER)
        NI 0FFH
        BNZ NT0
        POP
NT0:    ENTER
        INCB NWAIT
        CI NTIMEOUT
        BZ NLOST
        PI FNDONE
        BZ NT1
        LEAVE                   ; still out
NT1:    LDB NKIND
        LR 3,A
        CLR
        STB NKIND
        PI FNACK
        BNZ NT9                 ; a NAK: nothing
        LR A,3
        CI NK_READ
        BNZ NT2
        LDB FNRXLO              ; a READ's bytes: decode them
        STB DLEFT
        CLR
        STB DRPOS
        LEAVE
NT2:    CI NK_STATUS
        BZ NT3
        LEAVE
NT3:    DCI FNREPLY             ; STATUS: the bytes waiting, to be read
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        LR 2,A                  ; connected
        DCI NBW
        LR A,0
        ST
        LR A,1
        ST
        LR A,0                  ; some waiting
        NI 0FFH
        BNZ NTX
        LR A,1
        NI 0FFH
        BNZ NTX
        LR A,2                  ; none waiting, but connected
        NI 0FFH
        BNZ NTX
        BR NLOST
NT9:    CLR                     ; (what is waiting asked again: a STATUS)
        STB NBW
        STB NBW+1
NTX:    LEAVE
NLOST:  CLR
        STB NLINK
        LDB NEVT
        OI EV_LOST
        STB NEVT
        LEAVE

; ---------------------------------------------------------------------------
; NPOST -- 3. the next transaction, once nothing is out and the last READ
;   is decoded: a WRITE, else a READ of what is waiting (RD_MAX at most),
;   else a STATUS.
; ---------------------------------------------------------------------------
NPOST:  ENTER
        LDB NKIND
        NI 0FFH
        BNZ NPQ
        LDB DLEFT
        NI 0FFH
        BZ NP0
NPQ:    LEAVE
NP0:
        LDB TXF
        NI 0FFH
        BZ NP1
        PI POSTW
        LEAVE
NP1:    DCI NBW                 ; r4:r5 waiting
        LM
        LR 4,A
        LM
        LR 5,A
        NI 0FFH
        BNZ NPR
        LR A,4
        NI 0FFH
        BNZ NPR
        LI FD_NET               ; none known waiting: STATUS
        LR 0,A
        LI NC_STAT
        LR 1,A
        LIS 2
        LR 2,A
        PI FNBEG
        LIS 1                   ; two 1-byte parameters, 0
        ST
        CLR
        ST
        LIS 1
        ST
        CLR
        ST
        LIS NK_STATUS
        BR NPGO
NPR:    LR A,5                  ; READ what is waiting, up to RD_MAX
        NI 0FFH
        BNZ NPR1
        LR A,4
        CI RD_MAX
        BNC NPR1
        LR 6,A                  ; all of it
        CLR
        LR 4,A
        BR NPR2
NPR1:   LI RD_MAX               ; RD_MAX of it, the rest kept
        LR 6,A
        LR A,4
        AI 256-RD_MAX
        LR 4,A
        LR A,5
        LNK
        AI 255
        LR 5,A
NPR2:   DCI NBW
        LR A,4
        ST
        LR A,5
        ST
        LI FD_NET
        LR 0,A
        LI NC_READ
        LR 1,A
        LIS 1
        LR 2,A
        PI FNBEG
        LIS 2                   ; a 2-byte parameter: the length
        ST
        LR A,6
        ST
        CLR
        ST
        LIS NK_READ
NPGO:   STB NKIND               ; posted: kind A
        CLR
        STB NWAIT
        PI FNPOST
NPX:    LEAVE

; ---------------------------------------------------------------------------
; POSTW -- every frame TXF wants, in one WRITE: its length first (each
;   frame's raw length + 2: COBS adds one, then the 0), then the frames.
; ---------------------------------------------------------------------------
POSTW:  ENTER
        LDB TXF                 ; the length
        LR 3,A
        CLR
        LR 4,A
        DCI FLEN
        LIS 8
        LR 5,A
PW1:    LR A,3
        NI 1
        LM
        BZ PW2
        AS 4
        LR 4,A
PW2:    LR A,3
        SR 1
        LR 3,A
        DS 5
        BNZ PW1
        LI FD_NET
        LR 0,A
        LI NC_WRIT
        LR 1,A
        LIS 1
        LR 2,A
        PI FNBEG
        LIS 2
        ST
        LR A,4
        ST
        CLR
        ST
        CLR                     ; each frame, in bit order
        STB BK
PW3:    LDB BK
        DCI SLBIT
        ADC
        LM
        LR 3,A
        LDB TXF
        NS 3
        BZ PW4
        PI BUILD
        PI COBSOUT
PW4:    INCB BK
        CI 8
        BNZ PW3
        CLR
        STB TXF
        LIS NK_WRITE
        STB NKIND
        CLR
        STB NWAIT
        PI FNPOST
        LEAVE

; the frames, by TXF bit: raw length + 2 (type, payload, 2 sums; COBS's 1;
; the 0), and the type
FLEN:   DB 1+18+2+2, 1+4+2+2, 1+4+2+2, 1+1+2+2, 1+0+2+2, 1+0+2+2, 1+0+2+2, 1+0+2+2
FTYPES: DB SC_HELLO, SC_ACT, SC_MOVE, SC_CREDIT, SC_PING, SC_RESYNC, SC_TABLES, SC_BYE

; BUILD -- frame BK (a TXF bit's number) into RAWBUF: its type, its
;   payload, then the two Fletcher sums; RAWN its length.  Leaf.
;   Clobbers A, r0-r3, DCs.
BUILD:  LDB BK
        LR 3,A
        DCI FTYPES
        ADC
        LM
        DCI RAWBUF
        ST
        LR A,3
        NI 0FFH
        BZ BHELLO
        CI 1
        BZ BACT
        CI 2
        BZ BMOVE
        CI 3
        BZ BCRED
        BR BSUM                 ; the rest: no payload
BHELLO: LI PROTO_VERSION        ; version, flags, the table's id, our name
        ST
        CLR
        ST
        XDC
        DCI SRVTBL
        LI TABLE_LEN
        LR 0,A
BH1:    LM
        XDC
        ST
        XDC
        DS 0
        BNZ BH1
        DCI MYNAME
        LI NAME_LEN
        LR 0,A
BH2:    LM
        XDC
        ST
        XDC
        DS 0
        BNZ BH2
        XDC
        BR BSUM
BACT:   XDC                     ; seq, act, a1, a2
        DCI ASEQ
        BR B4
BMOVE:  XDC                     ; x, y, dir|moving, warp
        DCI SENTX
B4:     LIS 4
        LR 0,A
B41:    LM
        XDC
        ST
        XDC
        DS 0
        BNZ B41
        XDC
        BR BSUM
BCRED:  LDB TAKEN               ; what we've taken (and credited with it)
        STB CREDITED
        DCI RAWBUF+1
        ST
BSUM:   LR Q,DC                 ; Fletcher-16 over RAWBUF up to DC0
        LR A,QL
        AI -(RAWBUF&0FFH)
        LR 2,A                  ; its length
        INC
        INC
        STB RAWN                ; and the sums
        DCI RAWBUF
        CLR
        LR 0,A                  ; s1
        LR 1,A                  ; s2
BS1:    LR A,0
        AM
        LNK                     ; mod 255: a carry is 1 ...
        CI 0FEH
        BC BS2
        AI 1                    ; ... and 255 is 0
BS2:    LR 0,A
        AS 1
        LNK
        CI 0FEH
        BC BS3
        AI 1
BS3:    LR 1,A
        DS 2
        BNZ BS1
        LR A,0
        ST
        LR A,1
        ST
        POP

; COBSOUT -- RAWBUF's RAWN bytes as COBS into the TX, then a 0: each run up
;   to a zero (or the end) is its length + 1, then its bytes.  Leaf.
;   Clobbers A, r0-r4, Q, DCs.
COBSOUT: LDB RAWN
        LR 2,A                  ; bytes left
        DCI RAWBUF
        XDC                     ; DC1: the run's start
        DCI FNTX                ; DC0: the TX (it appends)
CO1:    XDC                     ; a run: up to a zero or the end
        LR Q,DC
        XDC
        CLR
        LR 3,A                  ; its length
CO2:    LR A,2
        NI 0FFH
        BZ CO3
        XDC
        LM
        XDC
        NI 0FFH
        BZ CO3
        LR A,3
        INC
        LR 3,A
        DS 2
        BR CO2
CO3:    LR A,3                  ; its code: length + 1
        INC
        ST
        XDC                     ; back to the run's start
        LR DC,Q
        XDC
        LR A,3
        NI 0FFH
        BZ CO5
CO4:    XDC
        LM
        XDC
        ST
        DS 3
        BNZ CO4
CO5:    LR A,2                  ; the end: the frame's 0
        NI 0FFH
        BZ CO6
        XDC                     ; a zero: past it, the next run
        LM
        XDC
        DS 2
        BR CO1
CO6:    CLR
        ST
        POP

; ---------------------------------------------------------------------------
; NTICK -- 4. the clocks, by DT: a RESYNC owed, a credit, the keepalive,
;   HELLO again, a silent link given up.
; ---------------------------------------------------------------------------
NTICK:  ENTER
        LDB DT
        LR 6,A
        LDB RHOLD               ; a RESYNC owed, once the gap is over
        LR 0,A
        NI 07FH
        BZ NTK2
        LR A,0                  ; (the gap counts passes)
        AI -1
        STB RHOLD
        BR NTK3
NTK2:   LR A,0
        NI 080H
        BZ NTK3
        LI RESYNC_GAP
        STB RHOLD
        LDB TXF
        OI TX_RESYNC
        STB TXF
NTK3:   LDB CREDITED            ; taken - credited >= 48: a credit
        COM
        INC
        LR 0,A
        LDB TAKEN
        AS 0
        CI 47
        BC NTK4
        LDB TXF
        OI TX_CREDIT
        STB TXF
NTK4:   DCI QUIET               ; quiet += DT; > DEAD_AFTER: gone
        LM
        AS 6
        LR 0,A
        LM
        LNK
        LR 1,A
        DCI QUIET
        LR A,0
        ST
        LR A,1
        ST
        LR A,0
        AI 256-((DEAD_AFTER+1)&0FFH)
        LR A,1
        LNK
        AI 255-((DEAD_AFTER+1)>>8)
        BNC NTK5
        JMP NLOSTT
NTK5:   DCI QPING               ; every PING_EVERY: a credit and a PING
        LM
        AS 6
        LR 0,A
        LM
        LNK
        LR 1,A
        LR A,0
        AI 256-(PING_EVERY&0FFH)
        LR 2,A
        LR A,1
        LNK
        AI 255-(PING_EVERY>>8)
        BNC NTK6
        CLR                     ; (reached: again from 0)
        LR 0,A
        LR 1,A
        LDB TXF
        OI TX_CREDIT+TX_PING
        STB TXF
NTK6:   DCI QPING
        LR A,0
        ST
        LR A,1
        ST
        LDB QASK                ; every ASK_AGAIN: HELLO, if unanswered
        AS 6
        STB QASK
        CI ASK_AGAIN-1
        BC NTK9
        CLR
        STB QASK
        LDB NLINK
        NI NL_JOINED+NL_HEARD
        CI NL_JOINED
        BNZ NTK9
        LDB TXF
        OI TX_HELLO
        STB TXF
NTK9:   LEAVE
NLOSTT: CLR
        STB NLINK
        LDB NEVT
        OI EV_LOST
        STB NEVT
        LEAVE

; ---------------------------------------------------------------------------
; DECODE -- up to DCHUNK of the READ's bytes COBS-decoded into FBUF; at a
;   frame's 0, FREND applies it.  TAKEN counts the bytes.
;   In the loop: r7 the chunk, r0 DCODE (the block's data bytes left), r1
;   FBN (FBUF's length), r2 DFLAG; DC1 the reply, DC0 FBUF.
; ---------------------------------------------------------------------------
DF_BLK  EQU 1                   ; DFLAG: a COBS block began (a zero before the next)
DF_255  EQU 2                   ;  a 254-byte one (no zero after it)
DECODE: ENTER
        LDB DLEFT               ; this chunk: DCHUNK at most
        CI DCHUNK
        BC DE1
        LI DCHUNK
DE1:    LR 7,A
        COM
        INC
        LR 0,A
        LDB DLEFT
        AS 0
        STB DLEFT
        LDB TAKEN               ; taken += the chunk
        AS 7
        STB TAKEN
        LDB DRPOS               ; DC1 = the reply + DRPOS
        DCI FNREPLY
        ADCU
        XDC
        LDB DRPOS
        AS 7
        STB DRPOS
DE2:    DCI DCODE               ; the decoder's state
        LM
        LR 0,A
        LM
        LR 1,A                  ; FBN
        LM
        LR 2,A                  ; DFLAG
        DCI FBUF
        LR A,1
        ADC
DCL:    XDC
        LM                      ; the next byte
        XDC
        NI 0FFH
        BZ DCZERO               ; 0: a frame's end
        LR 3,A
        LR A,0
        NI 0FFH
        BZ DCCODE
        DS 0                    ; a data byte
        LR A,3
DCEM:   LR 3,A                  ; emit A into FBUF (past FBMAX: dropped,
        LR A,1                  ;  and the frame will fail its sums)
        CI FBMAX-1
        BNC DCN
        LR A,3
        ST
        LR A,1
        INC
        LR 1,A
DCN:    DS 7
        BNZ DCL
DCOUT:  DCI DCODE               ; the state kept for the next chunk
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        LEAVE
DCCODE: LR A,3                  ; a block's code: its data bytes
        AI -1
        LR 0,A
        LR A,2                  ; the zero before it, unless it's the frame's
        LR 4,A                  ;  first or after a 254-byte one
        LR A,3
        CI 0FFH
        LIS DF_BLK
        BNZ DCC1
        LIS DF_BLK+DF_255
DCC1:   LR 2,A
        LR A,4
        CI DF_BLK
        BNZ DCN
        CLR
        BR DCEM
DCZERO: LR A,1                  ; a frame's end: applied, the state reset
        STB FBN
        XDC
        LR Q,DC
        LR A,QU                 ; (the reply's place, across FREND)
        STB DRP2
        LR A,QL
        STB DRP2+1
        LR A,7
        STB DRP2+2
        PI FREND
        DCI DRP2
        LM
        LR QU,A
        LM
        LR QL,A
        LM
        LR 7,A
        LR DC,Q
        XDC
        CLR
        LR 0,A
        LR 1,A
        LR 2,A
        DCI FBUF
        LDB NEVT                ; an error: stop
        NI EV_ERROR
        BNZ DCERR
        DCI FBUF
        BR DCN
DCERR:  CLR
        STB DLEFT
        BR DCOUT

; ---------------------------------------------------------------------------
; FREND -- a frame in FBUF (FBN bytes): its sums checked, then what its type
;   does; a bad one asks for the whole state again.
; ---------------------------------------------------------------------------
FREND:  ENTER
        LDB FBN
        CI 2
        JC FEBAD                ; the sums and nothing else: malformed
        AI -2
        LR 2,A                  ; the bytes summed
        STB FBN
        DCI FBUF
        CLR
        LR 0,A
        LR 1,A
FE1:    LR A,0                  ; s1 += b, s2 += s1 (mod 255)
        AM
        LNK
        CI 0FEH
        BC FE2
        AI 1
FE2:    LR 0,A
        AS 1
        LNK
        CI 0FEH
        BC FE3
        AI 1
FE3:    LR 1,A
        DS 2
        BNZ FE1
        LR A,0                  ; against the two that follow
        CM
        BNZ FEBAD
        LR A,1
        CM
        BNZ FEBAD
        CLR                     ; a good frame: the link is alive
        STB QUIET
        STB QUIET+1
        STB QPING
        STB QPING+1
        STB QASK
        LDB FBUF                ; its type
        CI CS_PATCH
        BNZ FE4
        PI FPATCH
        LEAVE
FE4:    CI CS_COMMIT
        BNZ FE5
        LDB NLINK               ; a commit: the state is whole
        NI NL_JOINED
        BZ FEX
        LDB NLINK
        OI NL_HEARD
        STB NLINK
        LDB NEVT
        OI EV_STATE
        STB NEVT
        LEAVE
FE5:    CI CS_ERROR
        BNZ FE6
        LDB FBUF+1
        STB NERR
        LDB NEVT
        OI EV_ERROR
        STB NEVT
        LEAVE
FE6:    CI CS_TABLE
        BNZ FEX
        PI FTABLE
FEX:    LEAVE
FEBAD:  LDB RHOLD               ; a RESYNC owed
        OI 080H
        STB RHOLD
        INCB NBAD               ; (counted, for the rigs)
        BNZ FEB1
        LI 0FFH
        STB NBAD
FEB1:   LEAVE

; FPATCH -- a patch's runs (offset, length, bytes) written into ST; a map
;   byte that changes marks its tile (MAPMARK); the movers' bytes MF_MOVERS.
;   A run past the state or the frame ends it.  (MAPMARK spends r1, r7,
;   r10: nothing lives there.)
FPATCH: ENTER
        DCI FBUF+1
        XDC                     ; DC1: the frame
        LDB FBN
        AI -1
        LR 8,A                  ; its bytes left after the type
FP1:    LR A,8
        CI 1
        BC FPX                  ; (a run needs two)
        XDC
        LM
        LR 9,A                  ; the offset
        LM
        LR 6,A                  ; the length
        XDC
        LR A,8
        AI -2
        LR 8,A
        LR A,6                  ; within the frame and the state
        NI 0FFH
        BZ FP1
        COM
        INC
        AS 8
        BNC FPX                 ; (more than the frame has)
        LR 8,A
        LR A,9
        AS 6
        BC FPX
        CI STATE_SIZE
        BNC FPX
        LR A,9                  ; DC0 = ST + offset
        DCI ST
        ADCU
FP2:    XDC
        LM                      ; the new byte
        XDC
        LR 3,A
        LM                      ; the old one
        LR 2,A
        XS 3
        BZ FP4                  ; the same: nothing
        LI 0FFH
        ADC
        LR A,3
        ST
        LR A,9                  ; a map byte: its tile
        CI OFF_MAP-1
        BC FP4
        CI OFF_MAP+TILES-1
        BNC FPMV
        AI -OFF_MAP
        LR 0,A
        LR Q,DC
        PI MAPMARK
        LR DC,Q
        BR FP4
FPMV:   LR Q,DC                 ; past the map: the movers
        LDB MFLAGS
        OI MF_MOVERS
        STB MFLAGS
        LR DC,Q
FP4:    LR A,9
        INC
        LR 9,A
        DS 6
        BNZ FP2
        BR FP1
FPX:    LEAVE

; MAPMARK -- tile r0 became r3 (r2 what it was): to be drawn again (DMARK),
;   born now (BORN: a flame's thinning), MF_MAP; a blast's centre (from no
;   flame) MF_BOOM, a bomb become a dud MF_FIZZ.  Clobbers A, r1, r7, r10,
;   DC0 (keeps Q).
MAPMARK: ENTER
        PI DMARK
        LDB JIFFY
        LR 1,A
        LR A,0
        DCI BORN
        ADCU
        LR A,1
        ST
        LDB MFLAGS
        OI MF_MAP
        STB MFLAGS
        LR A,3                  ; a blast's centre, from not a flame
        CI T_FLAME_C
        BNZ MM6
        LR A,2
        CI T_FLAME_C-1
        BC MM5
        CI T_FLAME_D
        BC MM6
MM5:    LDB MFLAGS
        OI MF_BOOM
        STB MFLAGS
        LR A,0
        STB BOOMT
MM6:    LR A,2                  ; a bomb become a dud
        CI T_BOMB
        BNZ MMX
        LR A,3
        CI T_DUD
        BNZ MMX
        LDB MFLAGS
        OI MF_FIZZ
        STB MFLAGS
MMX:    LEAVE

; FTABLE -- a table's record (index, count, TABLE_REC bytes) into TBLS;
;   TBLN the count (MAX_TABLES at most), TBLHAVE its bit.  Leaf.
;   Clobbers A, r0-r2, DCs.
FTABLE: LDB FBUF+2              ; the count
        CI MAX_TABLES
        BC FTB1
        LIS MAX_TABLES
FTB1:   STB TBLN
        LDB FBUF+1              ; the index
        CI MAX_TABLES-1
        BNC FTBX
        LR 0,A
        DCI SLBIT
        ADC
        LM
        LR 1,A
        LDB TBLHAVE             ; (no OR with a register: add it if clear)
        NS 1
        BNZ FTB2
        LDB TBLHAVE
        AS 1
        STB TBLHAVE
FTB2:   LR A,0                  ; TBLS + 24 index
        SL 1
        SL 1
        SL 1
        LR 1,A
        SL 1
        AS 1                    ; 24 index (< 192)
        DCI TBLS
        ADCU
        XDC
        DCI FBUF+3
        LI TABLE_REC
        LR 2,A
FTB3:   LM
        XDC
        ST
        XDC
        DS 2
        BNZ FTB3
FTBX:   POP

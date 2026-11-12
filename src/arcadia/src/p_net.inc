; p_net.inc -- the link (src/net.c over the cart's mailbox): one step a
; frame that never waits. The Odyssey 2's (src/o2/src/p5.inc, dec.inc),
; transcribed.
;
; The FujiNet's TCP socket is driven one transaction at a time, each posted
; in one frame and seen answered in a later one. The order: a WRITE of
; whatever is to be sent; else a STATUS every NSTAT_EVERY frames (is the
; socket still up); else a READ of up to RD_MAX bytes (with none waiting
; the answer is a NAK: nothing). Every answer replaces the reply window --
; an empty one too -- so a READ's bytes are decoded (a chunk a step) before
; anything else is posted.
;
; What is to be sent is kept as intents (TXF: HELLO, an action, our move,
; a credit...) and encoded only when a WRITE goes out: each frame built in
; RAWBUF (type, payload, Fletcher-16), its COBS streamed into the
; transaction, then a 0. The server's frames are decoded as they come:
; COBS, a running Fletcher, the last two bytes held back (they are the
; sums), and a patch's bytes streamed straight into ST in cart RAM (no
; staged copy: the screen is picked only at a commit, and a bad frame asks
; for the whole state again). Map bytes that change mark their rows
; (MAPDL/MAPDH) and the blasts and duds (MFLAGS) for the arena.

        BCTA,UN NSTEP           ; E_ENTER: a frame's step (NEVT: what came)
        BCTA,UN NHELLO          ; E_HELLO: at a table: the state zeroed, HELLO
        BCTA,UN NINIT           ; E_INIT: a fresh link's counters

PING_EVERY EQU  300             ; frames between keepalives (each is answered)
DEAD_AFTER EQU  900             ; frames without a frame: the link is gone
ASK_AGAIN  EQU  60              ; HELLO unanswered: again
RESYNC_GAP EQU  30              ; frames between RESYNCs
NSTAT_EVERY EQU 30              ; frames between STATUS checks
NTIMEOUT   EQU  240             ; frames a transaction may be out
RD_MAX     EQU  192             ; the most a READ asks for (slice 0 holds 256)
DCHUNK     EQU  8               ; encoded bytes decoded a step (a pass fits a frame)
DF_BLK     EQU  1               ; DFLAG: a COBS block began (a zero before the next)
DF_255     EQU  2               ;  a 254-byte one (no zero after it)

; ---- entries ---------------------------------------------------------------------
; NINIT: a fresh link: the counters, the decoder, nothing out or owed.
; Leaf. Clobbers R0, R3.
NINIT:  EORZ    R0
        STRA,R0 NKIND
        STRA,R0 NWAIT
        LODI,R3 TBLHAVE-NEVT+1
NI1:    STRA,R0 NEVT-1,R3
        BDRR,R3 NI1
        LODI,R0 $FF
        STRA,R0 TBLN
        RETC,UN

; NHELLO: sit at the table: the counters, ST zeroed (the state comes whole),
; HELLO due, no seat yet. Calls two down. Clobbers R0-R3.
NHELLO: BSTR,UN NINIT
        CPTR    ST
        LODI,R3 STATE_SIZE
NH1:    EORZ    R0
        BSTA,UN RAMPUT1
        BDRR,R3 NH1
        BSTA,UN RAMPEND
        LODA,R0 NLINK
        ANDI,R0 NL_LINK
        IORI,R0 NL_JOINED
        STRA,R0 NLINK
        LODI,R0 TX_HELLO
        STRA,R0 TXF
        LODI,R0 NONE
        STRA,R0 MYSEAT
        EORZ    R0
        STRA,R0 JOINTH
        STRA,R0 JOINTL
        RETC,UN

; NSTEP: one frame of the link: the transaction out answered, a chunk
; decoded, the next posted, the clocks; our seat found at a commit; no
; state in JOIN_WAIT frames is EV_NOANS. Calls four down. Clobbers R0-R3.
NSTEP:  EORZ    R0
        STRA,R0 NEVT
        LODA,R0 FLAGS           ; a test scene: nothing comes or goes
        TMI,R0  FL_TEST
        BCFR,EQ NS0
        EORZ    R0
        STRA,R0 TXF
        RETC,UN
NS0:    LODA,R0 NLINK
        TMI,R0  NL_LINK
        BCTR,EQ NS1
        LODI,R0 EV_LOST
        STRA,R0 NEVT
        RETC,UN
NS1:    BSTA,UN NTXN            ; 1. the transaction out
        LODA,R0 NEVT
        RETC,GT                 ; (lost)
        LODA,R0 DLEFT           ; 2. a chunk decoded
        BCTR,EQ NS2
        BSTA,UN DECODE
        LODA,R0 NEVT
        TMI,R0  EV_ERROR
        RETC,EQ                 ; (an E frame: the caller's)
NS2:    BSTA,UN NPOST           ; 3. the next transaction
        BSTA,UN NTICK           ; 4. the clocks
        LODA,R0 NEVT            ; a commit: which seat is ours
        TMI,R0  EV_STATE
        BCFR,EQ NS3
        BSTA,UN FINDSEAT
NS3:    LODA,R0 NLINK           ; joined, nothing heard: JOIN_WAIT
        ANDI,R0 NL_JOINED|NL_HEARD
        COMI,R0 NL_JOINED
        RETC,LT
        RETC,GT
        LODA,R0 JOINTL
        ADDA,R0 DT
        STRA,R0 JOINTL
        PPSL    PSLWC
        LODA,R0 JOINTH
        ADDI,R0 0
        STRA,R0 JOINTH
        CPSL    PSLWC
        COMI,R0 JOIN_WAIT>>8
        RETC,LT
        BCTR,GT NS4
        LODA,R0 JOINTL
        COMI,R0 JOIN_WAIT&$FF
        RETC,LT
NS4:    LODA,R0 NEVT
        IORI,R0 EV_NOANS
        STRA,R0 NEVT
        RETC,UN

; FINDSEAT: MYSEAT = the seat whose name is ours (NONE: watching).
; Leaf. Clobbers R0-R3.
FINDSEAT:
        LODI,R1 SEATS-1
FS1:    LODA,R0 ST+OFF_SEATED
        ANDA,R0 SEATBIT,R1
        BCTR,EQ FS4
        LODA,R0 SEATNM,R1       ; its name's offset, the last byte's
        STRZ    R2
        LODI,R3 NAME_LEN
FS2:    LODA,R0 ST+OFF_NAMES,R2
        COMA,R0 MYNAME-1,R3
        BCFR,EQ FS4
        SUBI,R2 1
        BDRR,R3 FS2
        STRA,R1 MYSEAT
        RETC,UN
FS4:    SUBI,R1 1
        BCFR,LT FS1
        LODI,R0 NONE
        STRA,R0 MYSEAT
        RETC,UN
SEATNM: DB      NAME_LEN-1,2*NAME_LEN-1,3*NAME_LEN-1,4*NAME_LEN-1

; ---- the transactions ------------------------------------------------------------
; NTXN: 1. the transaction out: answered? Its answer taken (a READ's bytes
; to decode, a STATUS's word on the socket). Calls one down. Clobbers R0-R3.
NTXN:   LODA,R0 NKIND
        RETC,EQ
        LODA,R0 NWAIT
        ADDI,R0 1
        STRA,R0 NWAIT
        COMI,R0 NTIMEOUT
        BCTR,EQ NLOST
        BSTA,UN MDONE
        RETC,LT                 ; still out
        RETC,GT
        LODA,R0 NKIND
        STRA,R0 T0
        EORZ    R0
        STRA,R0 NKIND
        BSTA,UN MACK
        RETC,LT                 ; a NAK: nothing (a READ with none waiting)
        RETC,GT
        LODA,R0 T0
        COMI,R0 NK_READ
        BCFR,EQ NT2
        LODA,R0 *PRXLL          ; a READ's bytes: decode them
        STRA,R0 DLEFT
        LODI,R0 $FF
        STRA,R0 DRPOS
        RETC,UN
NT2:    COMI,R0 NK_STATUS
        RETC,LT
        RETC,GT
        LODI,R3 2               ; STATUS: connected?
        LODA,R0 *PRDATA,R3
        RETC,GT
        RETC,LT
        LODA,R0 *PRDATA         ; ... or bytes still waiting
        RETC,GT
        RETC,LT
        LODI,R3 1
        LODA,R0 *PRDATA,R3
        RETC,GT
        RETC,LT
NLOST:  EORZ    R0
        STRA,R0 NLINK
        LODA,R0 NEVT
        IORI,R0 EV_LOST
        STRA,R0 NEVT
        RETC,UN

; NPOST: 3. the next transaction, once nothing is out and the last READ is
; decoded: a WRITE, else a STATUS (now and then), else a READ. Calls three
; down. Clobbers R0-R3.
NPOST:  LODA,R0 NKIND
        RETC,GT
        LODA,R0 DLEFT
        RETC,GT
        RETC,LT
        LODA,R0 TXF
        BCFA,EQ POSTW
        LODA,R0 NSTAT
        BCFR,EQ POSTR
        LODI,R0 NSTAT_EVERY     ; STATUS
        STRA,R0 NSTAT
        LODI,R3 NCSTAT
        LODI,R2 2
        BSTA,UN MSTARTN
        LODI,R1 0
        BSTA,UN MPARB
        LODI,R1 0
        BSTA,UN MPARB
        LODI,R0 NK_STATUS
        BCTR,UN NPGO
POSTR:  LODI,R3 NCREAD          ; READ up to RD_MAX
        LODI,R2 1
        BSTA,UN MSTARTN
        LODI,R1 RD_MAX
        LODI,R2 0
        BSTA,UN MPARW
        LODI,R0 NK_READ
NPGO:   STRA,R0 NKIND           ; posted: kind R0
        EORZ    R0
        STRA,R0 NWAIT
        BCTA,UN MPOST           ; tail call

; POSTW: every frame TXF wants, in one WRITE: its length first (each
; frame's raw length + 2: COBS adds one, then the 0), then the frames.
POSTW:  LODI,R1 0               ; the length
        LODI,R3 7
PW1:    LODA,R0 TXF
        ANDA,R0 BITS,R3
        BCTR,EQ PW2
        LODZ    R1
        ADDA,R0 FLEN,R3
        STRZ    R1
PW2:    SUBI,R3 1
        BCFR,LT PW1
        STRA,R1 T1
        LODI,R3 NCWRITE
        LODI,R2 1
        BSTA,UN MSTARTN
        LODA,R1 T1
        LODI,R2 0
        BSTA,UN MPARW
        LODI,R0 0               ; each frame, in bit order
PW3:    STRA,R0 BK
        STRZ    R3
        LODA,R0 TXF
        ANDA,R0 BITS,R3
        BCTR,EQ PW4
        BSTR,UN BUILD
        BSTA,UN COBSOUT
PW4:    LODA,R0 BK
        ADDI,R0 1
        COMI,R0 8
        BCFR,EQ PW3
        EORZ    R0
        STRA,R0 TXF
        LODI,R0 NK_WRITE
        BCTA,UN NPGO

; the frames, by TXF bit: raw length + 2 (type, payload, 2 sums; COBS's 1;
; the 0), and the type
FLEN:   DB      1+18+2+2,1+4+2+2,1+4+2+2,1+1+2+2,1+0+2+2,1+0+2+2,1+0+2+2,1+0+2+2
FTYPES: DB      SC_HELLO,SC_ACT,SC_MOVE,SC_CREDIT,SC_PING,SC_RESYNC,SC_TABLES,SC_BYE
BITS:   DB      1,2,4,8,$10,$20,$40,$80

; BUILD: frame BK (a TXF bit's number) into RAWBUF: its type, its payload,
; then the two Fletcher sums; RAWN its length. Leaf. Clobbers R0-R3.
BUILD:  LODA,R3 BK
        LODA,R0 FTYPES,R3
        STRA,R0 RAWBUF
        LODI,R2 0               ; (the last byte written: stores pre-increment)
        LODZ    R3
        BCTR,EQ BHELLO
        COMI,R0 1
        BCTR,EQ BACT
        COMI,R0 2
        BCTR,EQ BMOVE
        COMI,R0 3
        BCTA,EQ BCRED
        BCTA,UN BSUM            ; the rest: no payload
BHELLO: LODI,R0 PROTO_VERSION   ; version, flags, the table's id, our name
        STRA,R0 RAWBUF,R2,+
        EORZ    R0
        STRA,R0 RAWBUF,R2,+
        LODI,R3 $FF
BH1:    LODA,R0 SRVTBL,R3,+
        STRA,R0 RAWBUF,R2,+
        COMI,R3 TABLE_LEN-1
        BCFR,EQ BH1
        LODI,R3 $FF
BH2:    LODA,R0 MYNAME,R3,+
        STRA,R0 RAWBUF,R2,+
        COMI,R3 NAME_LEN-1
        BCFR,EQ BH2
        BCTR,UN BSUM
BACT:   LODI,R3 $FF             ; seq, act, a1, a2
BA1:    LODA,R0 ASEQ,R3,+
        STRA,R0 RAWBUF,R2,+
        COMI,R3 3
        BCFR,EQ BA1
        BCTR,UN BSUM
BMOVE:  LODI,R3 $FF             ; x, y, dir|moving, warp
BM1:    LODA,R0 SENTX,R3,+
        STRA,R0 RAWBUF,R2,+
        COMI,R3 3
        BCFR,EQ BM1
        BCTR,UN BSUM
BCRED:  LODA,R0 TAKEN           ; what we've taken (and credited with it)
        STRA,R0 CREDITED
        STRA,R0 RAWBUF,R2,+
BSUM:   STRA,R2 RAWN            ; Fletcher-16 over RAWBUF[0..R2]
        LODI,R3 $FF
        EORZ    R0
        STRA,R0 S1T
        STRA,R0 S2T
BS1:    LODA,R0 RAWBUF,R3,+
        ADDA,R0 S1T
        BSTR,UN MOD255
        STRA,R0 S1T
        ADDA,R0 S2T
        BSTR,UN MOD255
        STRA,R0 S2T
        COMA,R3 RAWN
        BCFR,EQ BS1
        LODA,R0 S1T
        STRA,R0 RAWBUF,R2,+
        LODA,R0 S2T
        STRA,R0 RAWBUF,R2,+
        ADDI,R2 1
        STRA,R2 RAWN
        RETC,UN

; MOD255: R0, just added (C its carry), mod 255: a carry is 1 (256 is 1
; mod 255), and 255 is 0. Leaf.
MOD255: TPSL    PSLC
        BCFR,EQ MOD1
        ADDI,R0 1
MOD1:   COMI,R0 255
        RETC,LT
        EORZ    R0
        RETC,UN

; COBSOUT: RAWBUF's RAWN bytes as COBS into the TX stream, then a 0: each
; run up to a zero (or the end) is its length + 1, then its bytes. Leaf.
; Clobbers R0, R1, R3.
COBSOUT: EORZ   R0
        STRA,R0 CFROM           ; the run's start
        LODI,R3 0
CO1:    COMA,R3 RAWN
        BCTR,EQ CO3
        LODA,R0 RAWBUF,R3
        BCTR,EQ CO2
        ADDI,R3 1
        BCTR,UN CO1
CO2:    BSTR,UN CORUN           ; a zero: the run, then on past it
        ADDI,R3 1
        STRA,R3 CFROM
        BCTR,UN CO1
CO3:    BSTR,UN CORUN           ; the end: the last run, then the 0
        LODI,R1 0
        LODA,R0 *PTXD,R1
        RETC,UN
CORUN:  STRA,R3 CI              ; length + 1, then RAWBUF[CFROM..R3)
        LODZ    R3
        SUBA,R0 CFROM
        ADDI,R0 1
        STRZ    R1
        LODA,R0 *PTXD,R1
        LODA,R3 CFROM
CR1:    COMA,R3 CI
        BCTR,EQ CR2
        LODA,R0 RAWBUF,R3
        STRZ    R1
        LODA,R0 *PTXD,R1
        ADDI,R3 1
        BCTR,UN CR1
CR2:    LODA,R3 CI
        RETC,UN

; NTICK: 4. the clocks: a RESYNC owed, a credit, the keepalive, HELLO
; again, a silent link given up. Leaf. Clobbers R0.
NTICK:  LODA,R0 NSTAT
        BCTR,EQ NTK1
        SUBI,R0 1
        STRA,R0 NSTAT
NTK1:   LODA,R0 RHOLD           ; a RESYNC owed, once the gap is over
        ANDI,R0 $7F
        BCTR,EQ NTK2
        LODA,R0 RHOLD
        SUBI,R0 1
        STRA,R0 RHOLD
        BCTR,UN NTK3
NTK2:   LODA,R0 RHOLD
        TMI,R0  $80
        BCFR,EQ NTK3
        LODI,R0 RESYNC_GAP
        STRA,R0 RHOLD
        LODA,R0 TXF
        IORI,R0 TX_RESYNC
        STRA,R0 TXF
NTK3:   LODA,R0 TAKEN           ; taken - credited >= 48: a credit
        SUBA,R0 CREDITED
        COMI,R0 48
        BCTR,LT NTK4
        LODA,R0 TXF
        IORI,R0 TX_CREDIT
        STRA,R0 TXF
NTK4:   LODA,R0 QUIETL          ; quiet > DEAD_AFTER: gone
        ADDI,R0 1
        STRA,R0 QUIETL
        PPSL    PSLWC
        LODA,R0 QUIETH
        ADDI,R0 0
        STRA,R0 QUIETH
        CPSL    PSLWC
        COMI,R0 DEAD_AFTER>>8
        BCTR,LT NTK5
        BCTA,GT NLOST
        LODA,R0 QUIETL
        COMI,R0 DEAD_AFTER&$FF
        BCTA,GT NLOST
NTK5:   LODA,R0 QPINGL          ; every PING_EVERY: a credit and a PING
        ADDI,R0 1
        STRA,R0 QPINGL
        PPSL    PSLWC
        LODA,R0 QPINGH
        ADDI,R0 0
        STRA,R0 QPINGH
        CPSL    PSLWC
        COMI,R0 PING_EVERY>>8
        BCFR,EQ NTK6
        LODA,R0 QPINGL
        COMI,R0 PING_EVERY&$FF
        BCFR,EQ NTK6
        EORZ    R0
        STRA,R0 QPINGL
        STRA,R0 QPINGH
        LODA,R0 TXF
        IORI,R0 TX_CREDIT|TX_PING
        STRA,R0 TXF
NTK6:   LODA,R0 QASK            ; every ASK_AGAIN: HELLO, if unanswered
        ADDI,R0 1
        STRA,R0 QASK
        COMI,R0 ASK_AGAIN
        RETC,LT
        EORZ    R0
        STRA,R0 QASK
        LODA,R0 NLINK
        ANDI,R0 NL_JOINED|NL_HEARD
        COMI,R0 NL_JOINED
        RETC,LT
        RETC,GT
        LODA,R0 TXF
        IORI,R0 TX_HELLO
        STRA,R0 TXF
        RETC,UN

; ---- the decoder (dec.inc) -----------------------------------------------------------
; DECODE: up to DCHUNK of the READ's bytes; TAKEN counts them. A patch's
; run going on gets the cart's RAM stream aimed at it again (a RAMPUT
; elsewhere may have moved it); the stream is closed after. One loop, the
; common path inline (a patch's byte, the profile's: about 180 cycles).
; Calls three down. Clobbers R0-R3.
DECODE: LODA,R0 DRPOS
        STRA,R0 DRPOS0
        LODA,R0 DLEFT           ; this chunk: DCHUNK at most
        COMI,R0 DCHUNK+1
        BCTR,LT DE1
        LODI,R0 DCHUNK
DE1:    STRA,R0 DCNT
        STRZ    R1
        LODA,R0 DLEFT
        SUBZ    R1
        STRA,R0 DLEFT
        LODA,R0 QST
        COMI,R0 2
        BCFR,EQ DC1
        LODA,R0 FTYPE
        COMI,R0 CS_PATCH
        BCFR,EQ DC1
        BSTA,UN AIM
DC1:    LODA,R3 DRPOS
        LODA,R0 *PRDATA,R3,+
        STRA,R3 DRPOS
        BCTA,EQ DCEND           ; 0: a frame's end
        LODA,R1 DCODE
        BCTA,EQ DCCODE
        SUBI,R1 1               ; a data byte
        STRA,R1 DCODE
; emit: into the two held back (D1 the older, D2); the one pushed out is the
; frame's, once two are held
DEM:    LODA,R1 D2
        STRA,R0 D2
        LODA,R0 D1
        STRA,R1 D1
        LODA,R1 DN
        COMI,R1 2
        BCTA,LT DEMN
; the frame's byte (R0): the sums run over it; its type, then its payload
; by type -- a patch's bytes streamed into ST
        STRA,R0 DOUT
        ADDA,R0 S1              ; s1 += b, s2 += s1 (mod 255: 256 is 1, 255 0)
        TPSL    PSLC
        BCFR,EQ DP1
        ADDI,R0 1
DP1:    COMI,R0 255
        BCTR,LT DP2
        EORZ    R0
DP2:    STRA,R0 S1
        ADDA,R0 S2
        TPSL    PSLC
        BCFR,EQ DP3
        ADDI,R0 1
DP3:    COMI,R0 255
        BCTR,LT DP4
        EORZ    R0
DP4:    STRA,R0 S2
        LODA,R0 FIDX
        BCTA,EQ DPTYPE
        LODA,R0 FTYPE
        COMI,R0 CS_PATCH
        BCTA,EQ DPATCH
        LODA,R0 FIDX            ; C, E, T: the payload's place + 1 (to 255)
        COMI,R0 255
        BCTR,EQ DPO
        ADDI,R0 1
        STRA,R0 FIDX
DPO:    BSTA,UN PCOTHER
        BCTA,UN DCN
DPTYPE: LODI,R0 1               ; its type
        STRA,R0 FIDX
        LODA,R0 DOUT
        STRA,R0 FTYPE
        EORZ    R0
        STRA,R0 QST
        BCTA,UN DCN
DPATCH: LODA,R0 QST             ; a patch
        COMI,R0 2
        BCTR,EQ DPQ
        BSTA,UN QHDR
        BCTA,UN DCN
DPQ:    LODA,R0 QOFF            ; its run's byte
        COMI,R0 OFF_MAP
        BCTR,LT QPUT
        COMI,R0 OFF_MAP+TILES
        BCFR,LT QMOV
        SUBI,R0 OFF_MAP         ; a map byte: the old tile against it
        STRA,R0 DTILE
        LODA,R1 QOFF
        LODA,R0 ST,R1
        COMA,R0 DOUT
        BCTR,EQ QPUT
        STRA,R0 DOLD
        BSTA,UN MAPMARK
        BCTR,UN QPUT
QMOV:   LODA,R0 MFLAGS          ; the movers
        IORI,R0 MF_MOVERS
        STRA,R0 MFLAGS
QPUT:   LODA,R1 DOUT            ; into ST: the stream stores, steps on
        LODA,R0 *PRDAT,R1
        LODA,R0 QOFF
        ADDI,R0 1
        STRA,R0 QOFF
        LODA,R0 QLEN
        SUBI,R0 1
        STRA,R0 QLEN
        BCFR,EQ DCN
        STRA,R0 QST             ; (the run's last: the next run's header)
DCN:    LODA,R0 DCNT            ; the chunk's next
        SUBI,R0 1
        STRA,R0 DCNT
        BCFA,EQ DC1
DCOUT:  LODA,R0 DRPOS           ; taken += the bytes decoded
        SUBA,R0 DRPOS0
        ADDA,R0 TAKEN
        STRA,R0 TAKEN
        BCTA,UN RAMPEND         ; tail call: the stream closed
DEMN:   ADDI,R1 1               ; (not two held yet)
        STRA,R1 DN
        BCTR,UN DCN
DCCODE: STRA,R0 DBYTE           ; a block's code: its data bytes
        SUBI,R0 1
        STRA,R0 DCODE
        LODA,R1 DFLAG           ; (R1: the last block's)
        LODI,R0 DF_BLK
        LODA,R2 DBYTE
        COMI,R2 255
        BCFR,EQ DCC1
        LODI,R0 DF_BLK|DF_255
DCC1:   STRA,R0 DFLAG
        COMI,R1 DF_BLK          ; the zero before it, unless it's the frame's
        BCFA,EQ DCN             ;  first or after a 254-byte one
        EORZ    R0
        BCTA,UN DEM
DCEND:  BSTA,UN FREND           ; a frame's end
        LODA,R0 NEVT            ; an error: stop
        TMI,R0  EV_ERROR
        BCTA,EQ DCOUT
        BCTA,UN DCN

; AIM: the cart's RAM stream at ST + QOFF. Leaf (fixed B's). Clobbers R0.
AIM:    LODI,R1 FRRAMP0
        LODA,R3 QOFF
        BCTA,UN RAMSET

; QHDR: a run's header: its offset, its length (past the state: a bad run,
; ignored); then the stream aimed at it. R0 = QST.
QHDR:   COMI,R0 0
        BCTR,EQ QH0
        COMI,R0 1
        RETC,GT                 ; 3: a bad run
        LODA,R0 DOUT            ; its length
        STRA,R0 QLEN
        BCTR,EQ QHZ             ; (no bytes: the next run)
        ADDA,R0 QOFF
        TPSL    PSLC
        BCTR,EQ QHBAD
        COMI,R0 STATE_SIZE+1
        BCFR,LT QHBAD
        LODI,R0 2
        STRA,R0 QST
        BCTA,UN AIM             ; tail call
QH0:    LODA,R0 DOUT            ; its offset
        STRA,R0 QOFF
        LODI,R0 1
        STRA,R0 QST
        RETC,UN
QHZ:    EORZ    R0
        STRA,R0 QST
        RETC,UN
QHBAD:  LODI,R0 3
        STRA,R0 QST
        RETC,UN

; PCOTHER: C, E: the first payload byte kept (QOFF); T: index (QLEN; $FF
; out of range), count (QOFF), the record into TBLS at the index's place.
PCOTHER: LODA,R0 FTYPE
        COMI,R0 CS_TABLE
        BCTA,EQ PCT
        LODA,R0 FIDX
        COMI,R0 2
        RETC,LT
        RETC,GT
        LODA,R0 DOUT
        STRA,R0 QOFF
        RETC,UN
PCT:    LODA,R0 FIDX
        COMI,R0 2
        BCFR,EQ PCT2
        LODA,R0 DOUT            ; the index
        COMI,R0 MAX_TABLES
        BCTR,LT PCT1
        LODI,R0 $FF
PCT1:   STRA,R0 QLEN
        RETC,UN
PCT2:   COMI,R0 3
        BCFR,EQ PCT3
        LODA,R0 DOUT            ; the count, at most MAX_TABLES
        COMI,R0 MAX_TABLES+1
        BCTR,LT PCT2A
        LODI,R0 MAX_TABLES
PCT2A:  STRA,R0 QOFF
        RETC,UN
PCT3:   SUBI,R0 4               ; a record's byte: TBLS + index x 24 + FIDX - 4
        COMI,R0 TABLE_REC
        RETC,GT
        RETC,EQ
        STRA,R0 DTILE
        LODA,R1 QLEN
        COMI,R1 $FF
        RETC,EQ
        LODA,R0 REC24,R1
        ADDA,R0 DTILE
        STRZ    R3
        LODA,R0 DOUT
        LODI,R1 FRRAMP0+((TBLS-ST)>>8)
        BCTA,UN RAMPUT          ; tail call
REC24:  DB      0,24,48,72,96,120,144,168

; MAPMARK: tile DTILE changes from DOLD to DOUT: its row in MAPDL/MAPDH; a
; blast's centre that wasn't a flame; a bomb become a dud. Leaf. Clobbers
; R0, R2.
MAPMARK: LODA,R0 MFLAGS
        IORI,R0 MF_MAP
        STRA,R0 MFLAGS
        LODA,R0 DTILE           ; its row: tile / 13
        LODI,R2 0
MM1:    COMI,R0 ARENA_W
        BCTR,LT MM2
        SUBI,R0 ARENA_W
        ADDI,R2 1
        BCTR,UN MM1
MM2:    COMI,R2 8
        BCFR,LT MM3
        LODA,R0 MAPDL
        IORA,R0 BITS,R2
        STRA,R0 MAPDL
        BCTR,UN MM4
MM3:    LODA,R0 MAPDH
        IORA,R0 BITS-8,R2
        STRA,R0 MAPDH
MM4:    LODA,R0 DOUT            ; a blast's centre, from not a flame
        COMI,R0 T_FLAME_C
        BCFR,EQ MM6
        LODA,R0 DOLD
        COMI,R0 T_FLAME_C
        BCTR,LT MM5
        COMI,R0 T_FLAME_D
        BCFR,GT MM6
MM5:    LODA,R0 MFLAGS
        IORI,R0 MF_BOOM
        STRA,R0 MFLAGS
MM6:    LODA,R0 DOLD            ; a bomb become a dud
        COMI,R0 T_BOMB
        RETC,LT
        RETC,GT
        LODA,R0 DOUT
        COMI,R0 T_DUD
        RETC,LT
        RETC,GT
        LODA,R0 MFLAGS
        IORI,R0 MF_FIZZ
        STRA,R0 MFLAGS
        RETC,UN

; FREND: a frame's end: its sums checked (S1, S2 against the two held
; back), then what its type does; a bad one asks for the whole state
; again. The frame's state reset. Leaf. Clobbers R0, R1.
FREND:  LODA,R0 FIDX
        BCTA,EQ FEBAD           ; nothing but the sums: malformed
        LODA,R0 DN
        COMI,R0 2
        BCFA,EQ FEBAD
        LODA,R0 S1
        COMA,R0 D1
        BCFA,EQ FEBAD
        LODA,R0 S2
        COMA,R0 D2
        BCFA,EQ FEBAD
        EORZ    R0              ; a good frame: the link is alive
        STRA,R0 QUIETH
        STRA,R0 QUIETL
        STRA,R0 QPINGH
        STRA,R0 QPINGL
        STRA,R0 QASK
        LODA,R0 FTYPE
        COMI,R0 CS_COMMIT
        BCTR,EQ FECOM
        COMI,R0 CS_ERROR
        BCTR,EQ FEERR
        COMI,R0 CS_TABLE
        BCFA,EQ FERESET
        LODA,R0 QOFF            ; a table: the count, its record in
        STRA,R0 TBLN
        LODA,R1 QLEN
        COMI,R1 MAX_TABLES
        BCFA,LT FERESET
        LODA,R0 TBLHAVE
        IORA,R0 BITS,R1
        STRA,R0 TBLHAVE
        BCTR,UN FERESET
FECOM:  LODA,R0 NLINK           ; a commit: the state is whole
        TMI,R0  NL_JOINED
        BCFR,EQ FERESET
        IORI,R0 NL_HEARD
        STRA,R0 NLINK
        LODA,R0 NEVT
        IORI,R0 EV_STATE
        STRA,R0 NEVT
        BCTR,UN FERESET
FEERR:  LODA,R0 QOFF
        STRA,R0 NERR
        LODA,R0 NEVT
        IORI,R0 EV_ERROR
        STRA,R0 NEVT
        BCTR,UN FERESET
FEBAD:  LODA,R0 RHOLD           ; a RESYNC owed
        IORI,R0 $80
        STRA,R0 RHOLD
        LODA,R0 NBAD            ; (counted, for the rigs)
        ADDI,R0 1
        BCTR,EQ FERESET
        STRA,R0 NBAD
FERESET: EORZ   R0
        STRA,R0 DCODE
        STRA,R0 DFLAG
        STRA,R0 DN
        STRA,R0 FIDX
        STRA,R0 S1
        STRA,R0 S2
        STRA,R0 QST
        RETC,UN

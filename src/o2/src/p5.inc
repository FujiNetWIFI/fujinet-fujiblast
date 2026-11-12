; p5.inc -- page 5: the link (src/net.c over the cartridge's mailbox), by
; farcall (N_*): one step a frame that never waits.
;
; The FujiNet's TCP socket is driven one transaction at a time, each posted
; in one frame and seen answered in a later one. The order: a WRITE of
; whatever is to be sent; else a STATUS every NSTAT_EVERY frames (is the
; socket still up); else a READ of up to RD_MAX bytes (with none waiting
; the answer is a NAK: nothing). Every answer replaces the cart's reply
; buffer -- an empty one too -- so a READ's bytes are decoded (a chunk a
; step) before anything else is posted.
;
; What is to be sent is kept as intents (TXF: HELLO, an action, our move, a
; credit...) and encoded only when a WRITE goes out: each frame built in
; RAWBUF (type, payload, Fletcher-16), its COBS streamed into the
; transaction, then a 0. The server's frames are decoded as they come: COBS,
; a running Fletcher, the last two bytes held back (they are the sums), and
; a patch's bytes POKEd straight into ST (no staged copy: the screen is
; picked only at a commit, and a bad frame asks for the whole state again).
; Map bytes that change mark their rows (MAPDL/H) and the blasts and duds
; (MFLAGS) for the arena.

PING_EVERY equ 300              ; frames between keepalives (each is answered)
DEAD_AFTER equ 900              ; frames without a frame: the link is gone
ASK_AGAIN  equ 60               ; HELLO unanswered: again
RESYNC_GAP equ 30               ; frames between RESYNCs
NSTAT_EVERY equ 30              ; frames between STATUS checks
NTIMEOUT   equ 240              ; frames a transaction may be out
RD_MAX     equ 192              ; the most a READ asks for (the window: 208)
DCHUNK     equ 16               ; encoded bytes decoded a step

    PAGE_HEAD
    db e_step&0FFh              ; N_STEP: a frame's step (NEVT: what came)
    db e_hello&0FFh             ; N_HELLO: sit at the table (ST zeroed)
    db e_act&0FFh               ; N_ACT: action r3, args r4, r5 (r2: its seq)
    db e_tables&0FFh            ; N_TABLES: ask for the table list
    db e_leave&0FFh             ; N_LEAVE: give the seat up
    db e_init&0FFh              ; N_INIT: the link just opened: counters
e_step:
    jmp n_step
e_hello:
    jmp n_hello
e_act:
    jmp n_act
e_tables:
    jmp n_tables
e_leave:
    jmp n_leave
e_init:
    jmp n_init

; n_init: a fresh link: the counters, the decoder, nothing out or owed.
; Clobbers A, r0, r2.
n_init:
    mov r0,#NKIND
    mov r2,#MFLAGS+1-NKIND
    clr a
ni_l:
    movx @r0,a
    inc r0
    djnz r2,ni_l
    mov a,#0FFh
    xset TBLN
    mov a,#NL_LINK
    xset NLINK
    ret

; n_hello: sit at the table: ST zeroed (the state comes whole), HELLO due.
; Clobbers A, r0-r2, r6.
n_hello:
    mov r2,#ST>>8
    mov a,#ST&0FFh
    FCALL poke_at
    clr a
    mov r6,#STATE_SIZE
    FCALL poke_n
    xget NLINK
    anl a,#NL_LINK
    orl a,#NL_JOINED
    movx @r0,a
    clr a
    xset NERR
    xset QUIETL
    xset QUIETH
    xset QPINGL
    xset QPINGH
    xset QASK
    xset RHOLD
    xorb TXF,TX_HELLO
    ret

; n_act: action r3 (ACT_*), arguments r4, r5: the next seq (never 0), in r2.
; One waits to be sent at a time: a second one in the same frames replaces
; it. Clobbers A, r0, r2.
n_act:
    xget ASEQ
    inc a
    jnz na_s
    inc a
na_s:
    movx @r0,a
    mov r2,a
    mov a,r3
    xset AK
    mov a,r4
    xset A1
    mov a,r5
    xset A2
    xorb TXF,TX_ACT
    ret

; n_tables: the table list asked for (TBLN 0FFh until the count comes).
n_tables:
    mov a,#0FFh
    xset TBLN
    clr a
    xset TBLHAVE
    xorb TXF,TX_TABLES
    ret

; n_leave: an action LEAVE, if seated; no longer joined.
n_leave:
    xget NLINK
    jb1 nl_act
    ret
nl_act:
    anl a,#(~NL_JOINED)&0FFh
    movx @r0,a
    mov r3,#ACT_LEAVE
    mov r4,#0
    mov r5,#0
    jmp n_act

; n_step: one frame of the link. NEVT says what came (EV_*). Clobbers A,
; r0-r7.
n_step:
    clr a
    xset NEVT
    xget FLAGS                  ; a test scene: nothing comes or goes
    anl a,#FL_TEST
    jz ns_live
    clr a
    xset TXF
    ret
ns_live:
    xget NLINK
    jb0 ns_up
    xorb NEVT,EV_LOST
    ret
ns_up:
    call ns_txn                 ; 1. the transaction out
    xget NEVT
    jnz ns_r                    ; (lost)
    xget DLEFT                  ; 2. decode a chunk
    jz ns_p
    call decode
    xget NEVT
    jb1 ns_r                    ; an E frame: the caller's
ns_p:
    call ns_post                ; 3. the next transaction
    jmp ns_tick                 ; 4. the clocks
ns_r:
    ret

    if $ > 900h
    error "page 5's page $8 has run out"
    endif

; ===========================================================================
    org 0900h
; ns_txn: 1. the transaction out: answered? Its answer taken (a READ's
; bytes to decode, a STATUS's word on the socket). Clobbers A, r0-r7.
ns_txn:
    xget NKIND
    jz nt_x
    xinc NWAIT
    xrl a,#NTIMEOUT
    jnz ns_chk
    jmp ns_lost
ns_chk:
    call fn_done
    jnz nt_x                    ; still out
    xget NKIND
    mov r7,a
    clr a
    xset NKIND
    call fn_ack
    jnz nt_x                    ; a NAK: nothing (a READ with none waiting)
    mov a,r7
    xrl a,#NK_READ
    jnz ns_nrd
    mov a,#FN_R_RXLEN_LO&0FFh   ; a READ's bytes: decode them
    call mb_get
    xset DLEFT
    clr a
    xset DPOS
    ret
ns_nrd:
    mov a,r7
    xrl a,#NK_STATUS
    jnz nt_x
    mov a,#(FN_R_DATA+2)&0FFh   ; STATUS: connected?
    call mb_get
    jnz nt_x
    mov a,#FN_R_DATA&0FFh       ; ... or bytes still waiting
    call mb_get
    jnz nt_x
    mov a,#(FN_R_DATA+1)&0FFh
    call mb_get
    jnz nt_x
    jmp ns_lost
nt_x:
    ret
ns_lost:
    clr a
    xset NLINK
    xorb NEVT,EV_LOST
    ret
; ns_post: 3. the next transaction, once nothing is out and the last READ
; is decoded: a WRITE, else a STATUS (now and then), else a READ.
ns_post:
    xget NKIND
    jnz np_x
    xget DLEFT
    jnz np_x
    xget TXF
    jz ns_nw
    jmp post_write
ns_nw:
    xget NSTAT
    jnz ns_rd
    mov a,#NSTAT_EVERY
    movx @r0,a
    jmp post_status
ns_rd:
    jmp post_read
np_x:
    ret
; post_read: READ up to RD_MAX. post_status: STATUS. Clobbers A, r0-r7.
post_read:
    mov r2,#NET_DEVICEID
    mov r3,#NC_READ
    mov r4,#2
    call fn_begin
    mov r6,#RD_MAX
    mov r5,#0
    call fn_par2                ; the length, then a 1-byte 0 (netcat's)
    clr a
    call fn_par1
    mov r7,#NK_READ
    jmp pw_go
post_status:
    mov r2,#NET_DEVICEID
    mov r3,#NC_STATUS
    mov r4,#2
    call fn_begin
    clr a
    call fn_par1
    clr a
    call fn_par1
    mov r7,#NK_STATUS
    jmp pw_go


; ---- the mailbox (mail.inc's, what the link uses) ----
; fn_reg: register r7 = A. Clobbers r0, r6.
fn_reg:
    mov r6,a
    mov r0,#FN_W_REGSEL
    mov a,r7
    movx @r0,a
    inc r0
    mov a,r6
    movx @r0,a
    ret
; fn_begin: device r2, command r3, r4 parameters; the TX stream rewound.
; Clobbers A, r0, r6, r7.
fn_begin:
    mov a,r2
    mov r7,#FN_REG_DEVICE
    call fn_reg
    mov a,r3
    mov r7,#FN_REG_CMD
    call fn_reg
    mov a,r4
    mov r7,#FN_REG_NPARAM
    call fn_reg
    mov r7,#FN_REG_DATA_RST
    jmp fn_reg
; fn_tx: A into the TX stream. Clobbers r0.
fn_tx:
    mov r0,#FN_W_DATA
    movx @r0,a
    ret
; fn_par1: a one-byte parameter, A. Clobbers r0, r6.
fn_par1:
    mov r6,a
    mov a,#1
    call fn_tx
    mov a,r6
    jmp fn_tx
; fn_par2: a two-byte parameter, r6 (low) r5 (high). Clobbers A, r0.
fn_par2:
    mov a,#2
    call fn_tx
    mov a,r6
    call fn_tx
    mov a,r5
    jmp fn_tx
; fn_post: launch it: SEQ = the cart's ACKSEQ + 1 (never 0), kept in NSEQ.
; Clobbers A, r0.
fn_post:
    mov a,#FN_R_ACKSEQ&0FFh
    call mb_get
    inc a
    jnz fp_ok
    inc a
fp_ok:
    mov r0,#NSEQ
    movx @r0,a
    mov r0,#FN_W_SEQ
    movx @r0,a
    ret
; fn_done: A = 0 once the cart has echoed NSEQ. Clobbers A, r0, r6.
fn_done:
    mov a,#FN_R_ACKSEQ&0FFh
    call mb_get
    mov r6,a
    mov r0,#NSEQ
    movx a,@r0
    xrl a,r6
    ret
; fn_ack: A = 0 if it worked and was ACKed. Clobbers A.
fn_ack:
    mov a,#FN_R_ERR&0FFh
    call mb_get
    jnz fa_bad
    mov a,#FN_R_REPLY_CMD&0FFh
    call mb_get
    xrl a,#FC_ACK
fa_bad:
    ret

    if $ > 0A00h
    error "page 5's page $9 has run out"
    endif

; ===========================================================================
    org 0A00h
; post_write: every frame TXF wants, in one WRITE: its length first (each
; frame's raw length + 2: COBS adds one, then the 0), then the frames.
; Clobbers A, r0-r7.
post_write:
    xget TXF
    mov r6,a
    mov r5,#0                   ; the length
    mov r4,#0                   ; the bit
pwl_l:
    mov a,r6
    rrc a
    mov r6,a
    jnc pwl_n
    mov a,r4
    add a,#flen&0FFh
    movp a,@a
    add a,r5
    mov r5,a
pwl_n:
    inc r4
    mov a,r4
    xrl a,#8
    jnz pwl_l
    mov a,r5
    mov r1,a                    ; (fn_begin keeps r1)
    mov r2,#NET_DEVICEID
    mov r3,#NC_WRITE
    mov r4,#1
    call fn_begin
    mov a,r1
    mov r6,a
    mov r5,#0
    call fn_par2
    mov r4,#0                   ; each frame, in bit order
pw_f:
    mov a,r4
    xset LTMP
    mov a,r4
    add a,#fbit&0FFh
    movp a,@a
    mov r6,a
    xget TXF
    anl a,r6
    jz pw_n
    mov a,r4
    call build                  ; RAWBUF: type, payload; r2 its length
    call cobs_out
pw_n:
    xget LTMP
    inc a
    mov r4,a
    xrl a,#8
    jnz pw_f
    clr a
    xset TXF
    mov r7,#NK_WRITE
pw_go:                          ; post it: r7 its kind
    call fn_post
    mov a,r7
    xset NKIND
    clr a
    xset NWAIT
    ret

; the frames, by TXF bit: raw length + 2 (type, payload, 2 sums; COBS's 1;
; the 0), and the bit
flen:   db 1+18+2+2, 1+4+2+2, 1+4+2+2, 1+1+2+2, 1+0+2+2, 1+0+2+2, 1+0+2+2, 1+0+2+2
fbit:   db TX_HELLO, TX_ACT, TX_MOVE, TX_CREDIT, TX_PING, TX_RESYNC, TX_TABLES, TX_BYE
ftype:  db SC_HELLO, SC_ACT, SC_MOVE, SC_CREDIT, SC_PING, SC_RESYNC, SC_TABLES, SC_BYE

; build: frame A (a TXF bit's number) into RAWBUF: its type, its payload,
; then the two Fletcher sums; r2 = its length. Clobbers A, r0-r3, r5-r7.
build:
    mov r7,a
    add a,#ftype&0FFh
    movp a,@a
    mov r1,#RAWBUF
    mov @r1,a
    inc r1
    mov a,r7
    jz bd_hello
    xrl a,#1
    jz bd_act
    xrl a,#1!2
    jz bd_move
    xrl a,#2!3
    jz bd_credit
    jmp bd_sum                  ; the rest: no payload
bd_hello:                       ; version, flags, the table's id, our name
    mov @r1,#PROTO_VERSION
    inc r1
    mov @r1,#0
    inc r1
    mov r0,#SRVTBL
    mov r2,#TABLE_LEN+NAME_LEN
bd_h:
    movx a,@r0
    mov @r1,a
    inc r0
    inc r1
    djnz r2,bd_h
    jmp bd_sum
bd_act:                         ; seq, act, a1, a2
    mov r0,#ASEQ
    jmp bd_4
bd_move:                        ; x, y, dir|moving, warp
    mov r0,#SENTX
    mov r2,#3
bd_m:
    movx a,@r0
    mov @r1,a
    inc r0
    inc r1
    djnz r2,bd_m
    mov r0,#MYWARP
    movx a,@r0
    mov @r1,a
    inc r1
    jmp bd_sum
bd_credit:                      ; what we've taken (and credited with it)
    xget TAKEN
    xset CREDITED
    mov @r1,a
    inc r1
    jmp bd_sum
bd_4:
    mov r2,#4
bd_4l:
    movx a,@r0
    mov @r1,a
    inc r0
    inc r1
    djnz r2,bd_4l
bd_sum:                         ; Fletcher-16 over RAWBUF..r1
    mov a,r1
    add a,#256-RAWBUF
    mov r2,a                    ; the length so far
    mov r3,a
    mov r0,#RAWBUF
    mov r5,#0                   ; s1
    mov r6,#0                   ; s2
bs_l:
    mov a,@r0
    mov r7,a
    mov a,r5
    call add255
    mov r5,a
    mov r7,a
    mov a,r6
    call add255
    mov r6,a
    inc r0
    djnz r3,bs_l
    mov a,r5
    mov @r1,a
    inc r1
    mov a,r6
    mov @r1,a
    inc r2
    inc r2
    ret

; add255: A + r7, mod 255 (Fletcher's: 256 is 1, and 255 is 0).
add255:
    add a,r7
    jnc ad_n
    inc a
ad_n:
    inc a
    jz ad_x
    dec a
ad_x:
    ret


    if $ > 0B00h
    error "page 5's page $A has run out"
    endif

; ===========================================================================
    org 0B00h
; cobs_out: RAWBUF's r2 bytes as COBS into the TX stream, then a 0: each
; run up to a zero (or the end) is its length + 1, then its bytes.
; Clobbers A, r0-r3, r5.
cobs_out:
    mov r1,#RAWBUF              ; the run's start
    mov a,r2
    mov r5,a                    ; bytes left
co_run:
    mov a,r1
    mov r3,a                    ; scan for a zero
    mov r2,#0                   ; the run's length
co_scan:
    mov a,r5
    jz co_end
    mov a,@r1
    jz co_zero
    inc r1
    inc r2
    dec r5
    jmp co_scan
co_zero:                        ; a zero: the run, then on past it
    call co_emit
    inc r1
    dec r5
    jmp co_run
co_end:                         ; the end: the last run, then the 0
    call co_emit
    clr a
    jmp fn_tx
co_emit:                        ; length + 1, then r2 bytes from r3
    mov a,r2
    inc a
    call fn_tx
    mov a,r2
    jz ce_x
ce_l:
    mov a,r3
    mov r0,a
    mov a,@r0
    call fn_tx
    inc r3
    djnz r2,ce_l
ce_x:
    ret

; ns_tick: 4. the clocks: a RESYNC owed, a credit, the keepalive, HELLO
; again, a silent link given up. Clobbers A, r0, r2.
ns_tick:
    xget NSTAT
    jz ns_rh
    dec a
    movx @r0,a
ns_rh:
    xget RHOLD                  ; a RESYNC owed, once the gap is over
    anl a,#7Fh
    jz ns_ro
    xget RHOLD
    dec a
    movx @r0,a
    jmp ns_cr
ns_ro:
    xget RHOLD
    jb7 ns_rs
    jmp ns_cr
ns_rs:
    mov a,#RESYNC_GAP
    movx @r0,a
    xorb TXF,TX_RESYNC
ns_cr:
    xget CREDITED               ; taken - credited >= 48: a credit
    cpl a
    inc a
    mov r2,a
    xget TAKEN
    add a,r2
    add a,#256-48
    jnc ns_q
    xorb TXF,TX_CREDIT
ns_q:
    xinc QUIETL                 ; quiet > DEAD_AFTER: gone
    jnz ns_q2
    xinc QUIETH
ns_q2:
    xget QUIETL
    add a,#(10000h-DEAD_AFTER-1)&0FFh
    xget QUIETH
    addc a,#(10000h-DEAD_AFTER-1)>>8
    jc ns_lost3
    xinc QPINGL                 ; every PING_EVERY: a credit and a PING
    jnz ns_p2
    xinc QPINGH
ns_p2:
    xget QPINGL
    xrl a,#PING_EVERY&0FFh
    jnz ns_a
    xget QPINGH
    xrl a,#PING_EVERY>>8
    jnz ns_a
    clr a
    xset QPINGL
    xset QPINGH
    xorb TXF,TX_CREDIT|TX_PING
ns_a:
    xinc QASK                   ; every ASK_AGAIN: HELLO, if unanswered
    xrl a,#ASK_AGAIN
    jnz ns_x
    movx @r0,a
    xget NLINK
    anl a,#NL_JOINED|NL_HEARD
    xrl a,#NL_JOINED
    jnz ns_x
    xorb TXF,TX_HELLO
ns_x:
    ret
ns_lost3:
    jmp ns_lost

    if $ > 0C00h
    error "page 5's page $B has run out"
    endif

    include "dec.inc"

    PAGE_TAIL

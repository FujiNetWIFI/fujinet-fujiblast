; p6.inc -- page 6: the lobby's appkeys and the socket, by farcall (K_*,
; C_*): src/appkey.c and net_open. Each transaction is waited for (fn_go:
; frames, the display running).
;
; Key 0 is the player's name, key 0x10 FujiBlast's server
; ("tcp://host:port/?table=id"). We keep only the table's id (SRVTBL) and
; FL_ROOM (the key holds our server); C_OPEN reads the key again and streams
; "N:TCP://host:port/" from its reply straight into the OPEN -- no host kept
; in RAM. Without the key, the server is the build's (gen/server: DEFSPEC),
; and the table one picked from its list (K_SAVEROOM then writes the key).

AK_SERVER equ 10h

    PAGE_HEAD
    db e_load&0FFh              ; K_LOAD: MYNAME, SRVTBL, FL_ROOM from the keys
    db e_savename&0FFh          ; K_SAVENAME: key 0 = MYNAME
    db e_saveroom&0FFh          ; K_SAVEROOM: key 0x10 = the build's server +
                                ; SRVTBL (if it isn't the key's already)
    db e_clearroom&0FFh         ; K_CLEARROOM: key 0x10 empty, no table
    db e_open&0FFh              ; C_OPEN: the socket: r2 = 1 open, 0 not
    db e_close&0FFh             ; C_CLOSE: the socket closed
e_load:
    jmp k_load
e_savename:
    jmp k_savename
e_saveroom:
    jmp k_saveroom
e_clearroom:
    jmp k_clearroom
e_open:
    jmp c_open
e_close:
    jmp c_close

; k_load: MYNAME = key 0, letters and digits up to 8, upper-cased (NUL
; padded); SRVTBL and FL_ROOM from key 0x10. Clobbers A, r0-r7.
k_load:
    mov r0,#SRVTBL              ; SRVTBL, MYNAME zeroed
    mov r2,#TABLE_LEN+NAME_LEN
    clr a
kl_z:
    movx @r0,a
    inc r0
    djnz r2,kl_z
    xget FLAGS
    anl a,#(~FL_ROOM)&0FFh
    movx @r0,a
    mov r5,#0                   ; the name
    call ak_read
    jz kl_room
    mov r6,a                    ; bytes
    mov r3,#2                   ; reply offset
    mov r1,#MYNAME
kl_n:
    mov a,r3
    call rx_get
    call up_alnum
    jz kl_nn
    movx @r1,a
    inc r1
    mov a,r1
    xrl a,#MYNAME+NAME_LEN
    jz kl_room
kl_nn:
    inc r3
    djnz r6,kl_n
kl_room:
    mov r5,#AK_SERVER           ; the server: its table, if it has one
    call ak_read
    add a,#256-5
    jnc kl_x                    ; (4 bytes or fewer: none)
    call url_table
    xget SRVTBL
    jz kl_x
    xget FLAGS
    orl a,#FL_ROOM
    movx @r0,a
kl_x:
    ret

; url_table: SRVTBL from the reply's key (at 2): what follows its "=",
; lower-cased letters and digits, up to 8. Clobbers A, r0-r3, r6.
url_table:
    mov a,#1                    ; the key's length (its high byte: 0)
    call rx_get
    jnz ut_x
    clr a
    call rx_get
    mov r2,a
    mov r3,#2
ut_eq:                          ; to the "="
    mov a,r2
    jz ut_x
    mov a,r3
    call rx_get
    inc r3
    dec r2
    xrl a,#'='
    jnz ut_eq
    mov r1,#SRVTBL
ut_c:
    mov a,r2
    jz ut_x
    mov a,r3
    call rx_get
    call up_alnum
    jz ut_x
    add a,#256-'A'              ; a letter: lower case
    jnc ut_d
    add a,#'a'
    jmp ut_s
ut_d:
    add a,#'A'
ut_s:
    movx @r1,a
    inc r1
    inc r3
    dec r2
    mov a,r1
    xrl a,#SRVTBL+TABLE_LEN
    jnz ut_c
ut_x:
    ret

; up_alnum: A upper-cased if a letter; 0 if not a letter or a digit.
; Clobbers r7.
up_alnum:
    mov r7,a
    add a,#256-'a'              ; 'a'..'z': up
    jnc ua_1
    add a,#256-26
    jc ua_1
    mov a,r7
    add a,#256-32
    ret
ua_1:
    mov a,r7                    ; 'A'..'Z'
    add a,#256-'A'
    jnc ua_2
    add a,#256-26
    jnc ua_ok
ua_2:
    mov a,r7                    ; '0'..'9'
    add a,#256-'0'
    jnc ua_no
    add a,#256-10
    jnc ua_ok
ua_no:
    clr a
    ret
ua_ok:
    mov a,r7
    ret

    if $ > 900h
    error "page 6's page $8 has run out"
    endif

; ===========================================================================
    org 900h

; c_open: the socket: CLOSE first (a reset leaves the cart's open), then
; OPEN "N:TCP://host:port/" -- from key 0x10 (FL_ROOM), else DEFSPEC.
; Then the link's counters fresh (p5 n_init's). r2 = 1 open, 0 not.
; Clobbers A, r0-r7.
c_open:
    call c_close
    xget FLAGS
    anl a,#FL_ROOM
    jnz co_key
    call co_begin
    mov a,#defspec&0FFh
    call tx_str
    jmp co_go
co_key:
    mov r5,#AK_SERVER           ; the key again: its reply holds the URL
    call ak_read
    jz co_fail
    call co_begin
    call spec_key
co_go:
    clr a                       ; the spec's NUL
    call fn_tx
    mov r5,#2                   ; (about 8 s: a dial can take 5)
    call fn_go_long
    jnz co_fail
    mov r0,#NKIND               ; p5's n_init: the counters, nothing out
    mov r2,#MFLAGS+1-NKIND
    clr a
coi_l:
    movx @r0,a
    inc r0
    djnz r2,coi_l
    mov a,#0FFh
    xset TBLN
    mov a,#NL_LINK
    xset NLINK
    mov r2,#1
    ret
co_fail:
    clr a
    xset NLINK
    mov r2,#0
    ret

co_begin:                       ; OPEN: read and write, no translation
    mov r2,#NET_DEVICEID
    mov r3,#NC_OPEN
    mov r4,#2
    call fn_begin
    mov a,#NM_RW
    call fn_par1
    clr a
    jmp fn_par1

; spec_key: "N:" + the key's URL up to its "?" (a "/" before it dropped),
; upper-cased; the port (":9910") if it has none; then "/". Clobbers A,
; r0-r3, r5-r7.
spec_key:
    clr a                       ; pass 1: r2 = the end (reply offset)
    call rx_get
    add a,#2
    mov r6,a                    ; the key's end
    mov r2,#2
sk_e:
    mov a,r2
    xrl a,r6
    jz sk_e2
    mov a,r2
    call rx_get
    jz sk_e2
    xrl a,#'?'
    jz sk_e2
    inc r2
    jmp sk_e
sk_e2:
    mov a,r2                    ; a "/" just before it: dropped
    dec a
    call rx_get
    xrl a,#'/'
    jnz sk_p
    dec r2
sk_p:
    mov a,#'N'                  ; pass 2: "N:", the URL, colons counted
    call fn_tx
    mov a,#':'
    call fn_tx
    mov r3,#2
    mov r5,#0
sk_l:
    mov a,r3
    xrl a,r2
    jz sk_port
    mov a,r3
    call rx_get
    mov r7,a
    xrl a,#':'
    jnz sk_up
    inc r5
sk_up:
    mov a,r7                    ; a..z: upper case
    add a,#256-'a'
    jnc sk_put
    add a,#256-26
    jc sk_put
    mov a,r7
    add a,#256-32
    mov r7,a
sk_put:
    mov a,r7
    call fn_tx
    inc r3
    jmp sk_l
sk_port:
    mov a,r5                    ; "tcp://host" has one colon: the port
    add a,#256-2
    jc sk_slash
    mov a,#portstr&0FFh
    call tx_str
sk_slash:
    mov a,#'/'
    jmp fn_tx

; tx_str: the NUL-ended string at offset A of page $9 into the TX stream.
; Clobbers A, r0, r1.
tx_str:
    mov r1,a
ts_l:
    mov a,r1
    movp a,@a
    jz ts_x
    call fn_tx
    inc r1
    jmp ts_l
ts_x:
    ret

    include "config.inc"        ; defspec, defurl (gen/server)
portstr: db ":9910",0

    if $ > 0A00h
    error "page 6's page $9 has run out"
    endif

; ===========================================================================
    org 0A00h

; ak_open: OPEN_APPKEY key r5, mode r1 (0 read, 1 write): creator 1, app 1.
; A = 0 if it worked. Clobbers A, r0, r2-r4, r6, r7.
ak_open:
    mov r2,#FUJI_DEVICEID
    mov r3,#FC_AKOPEN
    mov r4,#0
    call fn_begin
    mov a,#1                    ; creator, LE
    call fn_tx
    clr a
    call fn_tx
    mov a,#1                    ; app
    call fn_tx
    mov a,r5                    ; key
    call fn_tx
    mov a,r1                    ; mode
    call fn_tx
    clr a                       ; reserved
    call fn_tx
    jmp fn_go

; ak_read: key r5 into the reply: its length (2 bytes), then the key. A = the
; length, at most 64; 0: none. Clobbers A, r0-r7.
ak_read:
    mov r1,#0
    call ak_open
    jnz akr_none
    mov r2,#FUJI_DEVICEID
    mov r3,#FC_AKREAD
    mov r4,#0
    call fn_begin
    call fn_go
    jnz akr_none
    mov a,#1
    call rx_get
    jnz akr_64
    clr a
    call rx_get
    mov r6,a
    add a,#256-65
    jc akr_64
    mov a,r6
    ret
akr_64:
    mov a,#64
    ret
akr_none:
    clr a
    ret

; rx_get: A = reply byte A (the window at 0: the first 208). Clobbers
; nothing else.
rx_get:
    add a,#FN_R_DATA&0FFh
    jmp mb_get

; k_savename: key 0 = MYNAME (its letters, NUL-ended). Clobbers A, r0-r7.
k_savename:
    mov r5,#0
    mov r1,#1
    call ak_open
    mov r2,#FUJI_DEVICEID
    mov r3,#FC_AKWRITE
    mov r4,#0
    call fn_begin
    mov r1,#MYNAME
ksn_l:
    movx a,@r1
    jz ksn_go
    call fn_tx
    inc r1
    mov a,r1
    xrl a,#MYNAME+NAME_LEN
    jnz ksn_l
ksn_go:
    jmp fn_go

; k_saveroom: key 0x10 = the build's server and SRVTBL (a table picked from
; its list; the key's own table is left as it is). Clobbers A, r0-r7.
k_saveroom:
    xget FLAGS
    anl a,#FL_ROOM
    jnz ksr_x                   ; (the key has it)
    mov r5,#AK_SERVER
    mov r1,#1
    call ak_open
    mov r2,#FUJI_DEVICEID
    mov r3,#FC_AKWRITE
    mov r4,#0
    call fn_begin
    mov a,#defurl&0FFh
    call tx_str
    mov r1,#SRVTBL
ksr_t:
    movx a,@r1
    jz ksr_go
    call fn_tx
    inc r1
    mov a,r1
    xrl a,#SRVTBL+TABLE_LEN
    jnz ksr_t
ksr_go:
    call fn_go
    xget FLAGS
    orl a,#FL_ROOM
    movx @r0,a
ksr_x:
    ret

; k_clearroom: key 0x10 written empty; no table. Clobbers A, r0-r7.
k_clearroom:
    mov r5,#AK_SERVER
    mov r1,#1
    call ak_open
    mov r2,#FUJI_DEVICEID
    mov r3,#FC_AKWRITE
    mov r4,#0
    call fn_begin
    call fn_go
    clr a
    xset SRVTBL
    xget FLAGS
    anl a,#(~FL_ROOM)&0FFh
    movx @r0,a
    ret

; c_close: CLOSE the socket (a NAK is fine), the link down. Clobbers A,
; r0-r7.
c_close:
    mov r2,#NET_DEVICEID
    mov r3,#NC_CLOSE
    mov r4,#0
    call fn_begin
    call fn_go
    clr a
    xset NLINK
    ret


    if $ > 0B00h
    error "page 6's page $A has run out"
    endif

; ===========================================================================
    org 0B00h
    include "mail.inc"
frame_wait:                     ; (mail.inc's fn_go: until the next vblank)
    mov r1,#FRAMES
    mov a,@r1
    mov r2,a
fwc_l:
    mov a,@r1
    xrl a,r2
    jz fwc_l
    ret

    if $ > 0C00h
    error "page 6's page $B has run out"
    endif

    PAGE_TAIL

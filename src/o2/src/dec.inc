; dec.inc -- the link's decoder (p5, $C00-$EFF): the server's COBS frames
; out of a READ's reply, a chunk a step.
;
; The hot state lives in registers for the chunk -- r2 bytes left (DLEFT), r3
; the reply's offset (DPOS), r4 the COBS block's data bytes left (DCODE), r5
; and r6 the two bytes held back (D1, D2: the sums, at a frame's end) -- and
; the frame's in internal RAM (RAWBUF's: free in a net step until the WRITE
; is built), copied from external RAM in the same order and back. A patch's
; byte outside the map is about 75 cycles all told.

; internal RAM, mirroring external RAM DN..QLEN ($28-$2F)
I_DN    equ RAWBUF+0
I_FIDX  equ RAWBUF+1
I_FTYPE equ RAWBUF+2
I_S1    equ RAWBUF+3
I_S2    equ RAWBUF+4
I_QST   equ RAWBUF+5
I_QOFF  equ RAWBUF+6
I_QLEN  equ RAWBUF+7
I_FLAG  equ RAWBUF+8            ; DFLAG
I_T     equ RAWBUF+9            ; scratch
I_SAVE  equ RAWBUF+10           ; r2 across poke_at
I_C     equ RAWBUF+11           ; a block's code across emit
I_CNT   equ RAWBUF+12           ; the chunk's bytes left
I_T0    equ RAWBUF+13           ; DPOS at the chunk's start (TAKEN)
I_OLD   equ RAWBUF+14           ; map_mark: the old tile

    if DN+7 <> QLEN
    error "dec.inc: DN..QLEN must be contiguous"
    endif

; ===========================================================================
    org 0C00h

; decode: up to DCHUNK of the reply's bytes. Clobbers A, r0-r7.
decode:
    mov r0,#DN                  ; the frame's state in
    mov r1,#I_DN
    mov r2,#8
dc_in:
    movx a,@r0
    mov @r1,a
    inc r0
    inc r1
    djnz r2,dc_in
    xget DFLAG
    mov r1,#I_FLAG
    mov @r1,a
    xget D1
    mov r5,a
    xget D2
    mov r6,a
    xget DCODE
    mov r4,a
    xget DPOS
    mov r3,a
    xget DLEFT
    mov r2,a
    mov r0,#I_QST               ; a patch's run going on: POKE aimed at it
    mov a,@r0
    xrl a,#2
    jnz dc_c
    mov r0,#I_FTYPE
    mov a,@r0
    xrl a,#CS_PATCH
    jnz dc_c
    call aim
dc_c:
    mov r0,#I_CNT
    mov @r0,#DCHUNK
    mov r0,#I_T0
    mov a,r3
    mov @r0,a
dc_l:
    mov a,r2
    jz dc_out
    dec r2
    mov a,r3
    inc r3
    add a,#FN_R_DATA&0FFh
    call mb_get
    jz dc_end                   ; 0: a frame's end
    mov r7,a
    mov a,r4
    jz dc_code
    dec r4                      ; a data byte
    mov a,r7
    call emit
dc_n:
    xget NEVT                   ; an error: stop
    jb1 dc_out
    mov r0,#I_CNT
    mov a,@r0
    dec a
    mov @r0,a
    jnz dc_l
dc_out:                         ; the state back out
    mov a,r2
    xset DLEFT
    mov a,r3
    xset DPOS
    xget TAKEN                  ; taken += the bytes decoded
    mov r7,a
    mov r1,#I_T0
    mov a,@r1
    cpl a
    inc a
    add a,r3
    add a,r7
    xset TAKEN
    mov a,r4
    xset DCODE
    mov a,r5
    xset D1
    mov a,r6
    xset D2
    mov r1,#I_FLAG
    mov a,@r1
    xset DFLAG
    mov r0,#DN
    mov r1,#I_DN
    mov r2,#8
dc_ot:
    mov a,@r1
    movx @r0,a
    inc r0
    inc r1
    djnz r2,dc_ot
    ret
dc_code:                        ; a block's code: the zero before it, unless
    mov a,r7                    ; it's the frame's first or after a 255
    mov r0,#I_C
    mov @r0,a
    mov r0,#I_FLAG
    mov a,@r0
    jb0 dcc_w
    jmp dcc_s
dcc_w:
    jb1 dcc_s
    clr a
    call emit
dcc_s:
    mov r0,#I_C
    mov a,@r0
    mov r7,a
    dec a
    mov r4,a                    ; its data bytes
    mov a,r7
    inc a
    mov r0,#I_FLAG
    jz dcc_255
    mov @r0,#DF_BLK
    jmp dc_n
dcc_255:
    mov @r0,#DF_BLK|DF_255
    jmp dc_n
dc_end:
    call fr_end
    jmp dc_n

; emit: decoded byte A into the two held back (r5, r6); the one pushed out
; is the frame's (process), once two are held. Clobbers A, r0, r1, r7.
emit:
    xch a,r6
    xch a,r5
    mov r7,a
    mov r0,#I_DN
    mov a,@r0
    xrl a,#2
    jnz em_one
    jmp process
em_one:
    inc @r0
    ret

; aim: POKE at ST + I_QOFF (r2 kept). Clobbers A, r0, r1.
aim:
    mov a,r2
    mov r0,#I_SAVE
    mov @r0,a
    mov r0,#I_QOFF
    mov a,@r0
    mov r2,#ST>>8
    FCALL poke_at
    mov r0,#I_SAVE
    mov a,@r0
    mov r2,a
    ret

; sums: s1 += r7, s2 += s1 (mod 255: 256 is 1, 255 is 0). Clobbers A, r0.
sums:
    mov a,r7                    ; s1 += b, s2 += s1 (mod 255: 256 is 1, 255 0)
    mov r0,#I_S1
    add a,@r0
    jnc pc_1
    inc a
pc_1:
    inc a
    jz pc_2
    dec a
pc_2:
    mov @r0,a
    inc r0
    add a,@r0
    jnc pc_3
    inc a
pc_3:
    inc a
    jz pc_4
    dec a
pc_4:
    mov @r0,a
    ret

    if $ > 0D00h
    error "page 5's page $C has run out"
    endif

; ===========================================================================
    org 0D00h

; process: the frame's byte r7: the sums run over it; its type, then its
; payload by type -- a patch's bytes POKEd into ST. Clobbers A, r0, r1, r7.
process:
    call sums
    mov r0,#I_FIDX
    mov a,@r0
    jz pc_type
    inc a                       ; FIDX: the payload's place + 1 (to 255)
    jz pc_cap
    mov @r0,a
pc_cap:
    mov r1,a
    mov r0,#I_FTYPE
    mov a,@r0
    xrl a,#CS_PATCH
    jnz pc_other
    mov r0,#I_QST               ; a patch
    mov a,@r0
    xrl a,#2
    jnz q_hdr
    mov r0,#I_QOFF              ; its run's byte
    mov a,@r0
    add a,#256-OFF_MAP
    jc q_map
q_put:
    mov a,r7
    mov r1,#FN_W_REGDATA
    movx @r1,a
    mov r0,#I_QOFF
    inc @r0
    mov r0,#I_QLEN
    mov a,@r0
    dec a
    mov @r0,a
    jnz q_x
    mov r0,#I_QST
    mov @r0,a
q_x:
    ret
pc_type:
    inc a
    mov @r0,a                   ; FIDX 1
    mov r0,#I_FTYPE
    mov a,r7
    mov @r0,a
    mov r0,#I_QST
    mov @r0,#0
    ret

; q_map: a run's byte at the map (A = offset - OFF_MAP) or past it: a tile
; that changes marks its row, a blast's centre, a dud; the movers mark.
q_map:
    add a,#256-TILES
    jc q_mov
    add a,#TILES
    mov r0,#I_T
    mov @r0,a                   ; the tile
    mov r0,#I_QOFF
    mov a,@r0
    FCALL st_get                ; the old tile
    xrl a,r7
    jz q_put
    xrl a,r7
    call map_mark
    jmp q_put
q_mov:
    xorb MFLAGS,MF_MOVERS
    jmp q_put

; q_hdr: a run's header: its offset, its length (past the state: a bad run,
; ignored); then POKE aimed at it. A = QST ^ 2.
q_hdr:
    xrl a,#2
    jz qh_off
    xrl a,#1
    jz qh_len
    ret                         ; 3: a bad run
qh_off:
    mov r0,#I_QOFF
    mov a,r7
    mov @r0,a
    mov r0,#I_QST
    mov @r0,#1
    ret
qh_len:
    mov r0,#I_QLEN
    mov a,r7
    mov @r0,a
    mov r0,#I_QST
    jz qh_0                     ; (no bytes: the next run)
    mov r1,#I_QOFF
    add a,@r1
    jc qh_bad
    add a,#256-STATE_SIZE-1
    jc qh_bad
    mov @r0,#2
    jmp aim
qh_0:
    mov @r0,#0
    ret
qh_bad:
    mov @r0,#3
    ret

; pc_other: C, E: the first payload byte kept (QOFF); T: index (QLEN; 0FFh
; out of range), count (QOFF), the record POKEd into ST at the index's
; place. r1 = FIDX after this byte.
pc_other:
    mov r0,#I_FTYPE
    mov a,@r0
    xrl a,#CS_TABLE
    jz po_t
    mov a,r1
    xrl a,#2
    jnz po_x
    mov r0,#I_QOFF
    mov a,r7
    mov @r0,a
po_x:
    ret
po_t:
    mov a,r1
    xrl a,#2
    jnz po_t2
    mov a,r7                    ; the index
    add a,#256-MAX_TABLES
    mov a,r7
    jnc po_ti
    mov a,#0FFh
po_ti:
    mov r0,#I_QLEN
    mov @r0,a
    ret
po_t2:
    mov a,r1
    xrl a,#3
    jnz po_t3
    mov a,r7                    ; the count, at most MAX_TABLES
    add a,#256-MAX_TABLES-1
    mov a,r7
    jnc po_tc
    mov a,#MAX_TABLES
po_tc:
    mov r0,#I_QOFF
    mov @r0,a
    ret
po_t3:
    mov a,r1                    ; a record byte: ST + index x 24 + FIDX - 4
    add a,#256-4-TABLE_REC
    jc po_x
    mov r0,#I_QLEN
    mov a,@r0
    inc a
    jz po_x
    dec a
    add a,#rec24&0FFh
    movp a,@a
    add a,r1
    add a,#256-4
    mov r0,#I_T
    mov @r0,a                   ; its offset in ST
    mov a,r2                    ; aimed at (r2 kept)
    mov r0,#I_SAVE
    mov @r0,a
    mov r0,#I_T
    mov a,@r0
    mov r2,#ST>>8
    FCALL poke_at
    mov r0,#I_SAVE
    mov a,@r0
    mov r2,a
    mov a,r7
    mov r1,#FN_W_REGDATA
    movx @r1,a
    ret

rec24:
K   set 0
    rept MAX_TABLES
    db (ST&0FFh)+TABLE_REC*K
K   set K+1
    endm

    if $ > 0E00h
    error "page 5's page $D has run out"
    endif

; ===========================================================================
    org 0E00h

; map_mark: the tile I_T changes from A to r7: its row in MAPDL/H; a blast's
; centre that wasn't a flame; a bomb become a dud. Clobbers A, r0, r1.
map_mark:
    mov r0,#I_OLD
    mov @r0,a
    xorb MFLAGS,MF_MAP
    mov r0,#I_T                 ; row = tile / 13
    mov a,@r0
    mov r1,#0
mm_d:
    add a,#256-ARENA_W
    jnc mm_r
    inc r1
    jmp mm_d
mm_r:
    mov a,r1
    anl a,#7
    add a,#bitm&0FFh
    movp a,@a
    mov @r0,a                   ; I_T = the row's bit
    mov a,r1
    mov r1,#MAPDL
    jb3 mm_h
    jmp mm_s
mm_h:
    mov r1,#MAPDH
mm_s:
    movx a,@r1
    orl a,@r0
    movx @r1,a
    mov a,r7                    ; a blast's centre, from not a flame
    xrl a,#T_FLAME_C
    jnz mm_f
    mov r0,#I_OLD
    mov a,@r0
    add a,#256-T_FLAME_C
    jnc mm_boom
    add a,#256-(T_FLAME_D-T_FLAME_C+1)
    jnc mm_f
mm_boom:
    xorb MFLAGS,MF_BOOM
mm_f:
    mov r0,#I_OLD               ; a bomb become a dud
    mov a,@r0
    xrl a,#T_BOMB
    jnz mm_x
    mov a,r7
    xrl a,#T_DUD
    jnz mm_x
    xorb MFLAGS,MF_FIZZ
mm_x:
    ret

; fr_end: a frame's end: its sums checked (S1, S2 = the two held back), then
; what its type does; a bad one asks for the whole state again. The frame's
; state reset. Clobbers A, r0, r1, r7.
fr_end:
    mov r0,#I_FIDX
    mov a,@r0
    jz fe_bad                   ; nothing but the sums: malformed
    mov r0,#I_DN
    mov a,@r0
    xrl a,#2
    jnz fe_bad
    mov r0,#I_S1
    mov a,@r0
    xrl a,r5
    jnz fe_bad
    inc r0
    mov a,@r0
    xrl a,r6
    jnz fe_bad
    clr a                       ; a good frame: the link is alive
    xset QUIETL
    xset QUIETH
    xset QPINGL
    xset QPINGH
    xset QASK
    mov r0,#I_FTYPE
    mov a,@r0
    xrl a,#CS_COMMIT
    jz fe_commit
    xrl a,#CS_COMMIT!CS_ERROR
    jz fe_err
    xrl a,#CS_ERROR!CS_TABLE
    jnz fe_reset
    mov r0,#I_QOFF              ; a table: the count, the record in
    mov a,@r0
    xset TBLN
    mov r0,#I_QLEN
    mov a,@r0
    add a,#256-MAX_TABLES
    jc fe_reset
    mov a,@r0
    add a,#bitm&0FFh
    movp a,@a
    mov r7,a
    xget TBLHAVE
    orl a,r7
    movx @r0,a
    jmp fe_reset
fe_commit:
    xget NLINK
    jb1 fe_c2
    jmp fe_reset
fe_c2:
    orl a,#NL_HEARD
    movx @r0,a
    xorb NEVT,EV_STATE
    jmp fe_reset
fe_err:
    mov r0,#I_QOFF
    mov a,@r0
    xset NERR
    xorb NEVT,EV_ERROR
    jmp fe_reset
fe_bad:
    xorb RHOLD,80h              ; a RESYNC owed
fe_reset:
    clr a
    mov r4,a                    ; DCODE
    mov r0,#I_FLAG
    mov @r0,a
    mov r0,#I_DN
    mov @r0,a
    inc r0
    mov @r0,a                   ; FIDX
    mov r0,#I_S1
    mov @r0,a
    inc r0
    mov @r0,a                   ; S2
    inc r0
    mov @r0,a                   ; QST
    ret

bitm:   db 1, 2, 4, 8, 10h, 20h, 40h, 80h

    if $ > 0F00h
    error "page 5's page $E has run out"
    endif

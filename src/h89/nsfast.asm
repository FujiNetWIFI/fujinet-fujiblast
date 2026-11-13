; nsfast.asm -- the per-byte loops of src/h89/nstrans.c, which in C took
; most of the CPU once a game's state was streaming in:
;
;   uint8_t ns_collect(void)
;       A reply's bytes off the card's 82C55, as many as wait (255 at most a
;       call): SLIP undone, the header into ns_hdr, the data into
;       ns_dptr[6..ns_dend), the checksum folded into ns_sum. 1 when the
;       reply is to be judged (its header's length is in, or an END came in
;       the middle), 0 when more is to come.
;   void ns_request(void)
;       A request to the card: ns_rq[6] (the header, its checksum byte 0)
;       and ns_rq_data[0..ns_rq_n), checksummed, SLIP-framed and written
;       through the 82C55, each byte after the one before was taken; what
;       is left of an old reply is read away first. A card that stops
;       taking bytes (60 ms) sets ns_stuck, and the rest goes unwaited.
;   uint8_t ns_decode(void)
;       rbuf[rpos..rlen) into nst_buf, the COBS undone (ns_dec: the length
;       so far, the bytes to the next code, a 0 owed, too long), up to and
;       with a 0: 1 when one came (nstrans.c ends the frame), 0 when all
;       were taken. nst_taken counts them.
;   uint8_t ns_fletcher(uint8_t n) __z88dk_fastcall
;       Fletcher-16 (mod 255) of nst_buf[0..n): 1 if nst_buf[n], [n+1]
;       hold it.
;
; The state is nstrans.c's: ns_got (16 bits), ns_sum, ns_inframe, ns_esc.
; AF, BC, DE, HL and IY are ours to use (zsdcc).

        SECTION code_user

        PUBLIC  _ns_collect, _ns_fletcher, _ns_decode, _ns_request
        EXTERN  _ns_got, _ns_sum, _ns_inframe, _ns_esc, _ns_hdr, _ns_dptr, _ns_dend
        EXTERN  _nst_buf, _rbuf, _rpos, _rlen, _ns_dec, _nst_taken
        EXTERN  _ns_rq, _ns_rq_data, _ns_rq_n, _ns_stuck

        defc    PPI_A = $D0
        defc    PPI_C = $D2
        defc    IBF = $20
        defc    SL_END = $C0
        defc    SL_ESC = $DB
        defc    SL_ESCEND = $DC
        defc    SL_ESCESC = $DD

_ns_collect:
        ld      hl,(_ns_got)            ; HL = got
        ld      a,(_ns_sum)
        ld      b,a                     ; B = sum
        ld      c,0                     ; C = bytes this call
co_loop:
        in      a,(PPI_C)
        and     IBF
        jp      z,co_more
        in      a,(PPI_A)
        ld      e,a                     ; E = the byte
co_got:
        cp      SL_END
        jp      z,co_end
        ld      a,(_ns_inframe)
        or      a
        jr      z,co_next               ; (between replies: dropped)
        ld      a,(_ns_esc)
        or      a
        jr      nz,co_unesc
        ld      a,e
        cp      SL_ESC
        jr      nz,co_byte
        ld      a,1
        ld      (_ns_esc),a
        jr      co_next
co_unesc:
        xor     a
        ld      (_ns_esc),a
        ld      a,e
        cp      SL_ESCEND
        jr      nz,co_unesc2
        ld      e,SL_END
        jr      co_byte
co_unesc2:
        cp      SL_ESCESC
        jr      nz,co_byte
        ld      e,SL_ESC
co_byte:
        ld      a,e
        ld      (iy_byte),a
        push    hl
        ld      a,h                     ; got < 6: the header
        or      a
        jr      nz,co_data
        ld      a,l
        cp      6
        jr      nc,co_data
        ld      d,0
        ld      e,a                     ; (the byte: kept in A meanwhile)
        ld      a,(iy_byte)
        ld      hl,_ns_hdr
        add     hl,de
        jr      co_store
co_data:
        ld      de,(_ns_dend)           ; got < dend: dptr[got]
        or      a
        sbc     hl,de
        jr      nc,co_kept              ; (past the room: dropped)
        add     hl,de
        ld      de,(_ns_dptr)
        add     hl,de
        ld      a,(iy_byte)
co_store:
        ld      (hl),a
co_kept:
        pop     hl
        ld      a,l                     ; got 4 is the checksum: taken as 0
        cp      4
        jr      nz,co_sum
        ld      a,h
        or      a
        jr      z,co_count
co_sum:
        ld      a,(iy_byte)
        add     a,b
        adc     a,0
        ld      b,a
co_count:
        inc     hl
        ld      a,h                     ; got >= 6 and its length: judge
        or      a
        jr      nz,co_len
        ld      a,l
        cp      6
        jr      c,co_next
co_len:
        ld      de,(_ns_hdr + 2)
        ld      a,l
        cp      e
        jr      nz,co_fast
        ld      a,h
        cp      d
        jr      z,co_judge
        jr      co_fast
co_next:
        inc     c
        jp      nz,co_loop
co_more:
        ld      (_ns_got),hl
        ld      a,b
        ld      (_ns_sum),a
        ld      l,0
        ret

co_end:
        ld      a,(_ns_inframe)         ; the opening END (or none yet)
        or      a
        jr      z,co_open
        ld      a,h
        or      l
        jr      nz,co_judge             ; short of its length: judged bad
co_open:
        ld      a,1
        ld      (_ns_inframe),a
        jr      co_next
co_judge:
        ld      (_ns_got),hl
        ld      a,b
        ld      (_ns_sum),a
        ld      l,1
        ret

; The data's fast path: in a reply, past its header, short of its length
; (DE), no escape owed. Plain bytes go straight in (about 150 T-states
; each, where the path above takes some 450); an END or ESC goes back to
; it. HL = got, B = sum.
co_fast:
        ex      de,hl                   ; HL = len, DE = got
        or      a
        sbc     hl,de                   ; HL = bytes to the end
        jr      c,co_fast_no            ; (a length short of what came: never met)
        jr      z,co_fast_no
        ld      (cf_left),hl
        ld      hl,(_ns_dend)           ; C = room left for the data (255 at most)
        or      a
        sbc     hl,de
        ld      c,0
        jr      c,cf_room
        jr      z,cf_room
        ld      c,l
        ld      a,h
        or      a
        jr      z,cf_room
        ld      c,255
cf_room:
        ld      hl,(_ns_dptr)
        add     hl,de                   ; HL = where the next one goes
        ld      de,(cf_left)
cf_loop:
        in      a,(PPI_C)
        and     IBF
        jr      z,cf_out
        in      a,(PPI_A)
        cp      SL_END
        jr      z,cf_special
        cp      SL_ESC
        jr      z,cf_special
        inc     c
        dec     c
        jr      z,cf_skip               ; (no room: counted, not kept)
        ld      (hl),a
        inc     hl
        dec     c
cf_skip:
        add     a,b
        adc     a,0
        ld      b,a
        dec     de
        ld      a,d
        or      e
        jr      nz,cf_loop
        ld      hl,(_ns_hdr + 2)        ; all of it: judge
        jp      co_judge
cf_out:
        ld      hl,(_ns_hdr + 2)        ; got = len - left
        or      a
        sbc     hl,de
        jp      co_more
cf_special:
        ld      c,a
        ld      hl,(_ns_hdr + 2)
        or      a
        sbc     hl,de                   ; HL = got
        ld      a,c
        ld      e,a
        ld      c,0
        jp      co_got
co_fast_no:
        ex      de,hl                   ; (HL = got again)
        jp      co_next

_ns_fletcher:
        ld      b,l                     ; B = n
        ld      hl,_nst_buf
        ld      de,0                    ; D = s1, E = s2
        ld      a,b
        or      a
        jr      z,fl_check
fl_loop:
        ld      a,d
        add     a,(hl)
        adc     a,0                     ; (256 is 1, mod 255)
        cp      255
        jr      nz,fl_s1
        xor     a
fl_s1:
        ld      d,a
        add     a,e
        adc     a,0
        cp      255
        jr      nz,fl_s2
        xor     a
fl_s2:
        ld      e,a
        inc     hl
        djnz    fl_loop
fl_check:
        ld      a,(hl)
        cp      d
        jr      nz,fl_bad
        inc     hl
        ld      a,(hl)
        cp      e
        jr      nz,fl_bad
        ld      l,1
        ret
fl_bad:
        ld      l,0
        ret

        defc    FMAX = 136              ; nstrans.c's

_ns_decode:
        ld      a,(_rpos)
        ld      c,a
        ld      a,(_rlen)
        sub     c
        jr      z,de_none
        jr      c,de_none
        ld      b,a                     ; B = bytes left
        ld      (de_n),a
        ld      hl,_rbuf
        ld      a,l
        add     a,c
        ld      l,a
        jr      nc,de_src
        inc     h                       ; HL = rbuf + rpos
de_src:
        ld      iy,_ns_dec              ; (iy+0) dlen, +1 drem, +2 dzero, +3 dbad
        ld      a,(iy+0)
        ld      e,a
        ld      d,0
        push    hl
        ld      hl,_nst_buf
        add     hl,de
        ex      de,hl                   ; DE = nst_buf + dlen
        pop     hl
de_loop:
        ld      a,(hl)
        inc     hl
        or      a
        jr      z,de_zero
        ld      c,a                     ; C = the byte
        ld      a,(iy+0)
        cp      FMAX - 1
        jr      nc,de_bad
        ld      a,(iy+1)
        or      a
        jr      nz,de_data
        ld      a,(iy+2)                ; a code: the 0 owed first
        or      a
        jr      z,de_code
        xor     a
        ld      (de),a
        inc     de
        inc     (iy+0)
de_code:
        ld      a,c
        dec     a
        ld      (iy+1),a
        ld      a,c
        cp      $FF
        ld      a,0
        jr      nc,de_dz
        inc     a
de_dz:
        ld      (iy+2),a
        jr      de_next
de_data:
        dec     a
        ld      (iy+1),a
        ld      a,c
        ld      (de),a
        inc     de
        inc     (iy+0)
        jr      de_next
de_bad:
        ld      (iy+3),1
de_next:
        djnz    de_loop
        ld      a,(_rlen)               ; all taken
        ld      (_rpos),a
        ld      a,(de_n)
        ld      hl,_nst_taken
        add     a,(hl)
        ld      (hl),a
de_none:
        ld      l,0
        ret
de_zero:
        dec     b                       ; (the 0 taken too)
        ld      a,(_rlen)
        sub     b
        ld      (_rpos),a
        ld      a,(de_n)
        sub     b                       ; bytes taken
        ld      hl,_nst_taken
        add     a,(hl)
        ld      (hl),a
        ld      l,1
        ret

        defc    OBF_HIGH = $80
        defc    TAKE_WAIT = 2400        ; ~50 T-states a look: 60 ms

_ns_request:
        ld      hl,_ns_rq               ; the checksum: an end-around sum
        ld      b,6
        xor     a
        ld      c,a
        call    rq_sum
        ld      a,(_ns_rq_n)
        or      a
        jr      z,rq_sumd
        ld      b,a
        ld      hl,(_ns_rq_data)
        call    rq_sum
rq_sumd:
        ld      a,c
        ld      (_ns_rq + 4),a
        xor     a
        ld      (_ns_stuck),a
        ld      b,a                     ; what is left of an old reply: away
rq_drain:
        in      a,(PPI_C)
        and     IBF
        jr      z,rq_go
        in      a,(PPI_A)
        djnz    rq_drain
rq_go:
        ld      a,SL_END
        call    rq_put
        ld      hl,_ns_rq
        ld      b,6
        call    rq_send
        ld      a,(_ns_rq_n)
        or      a
        jr      z,rq_end
        ld      b,a
        ld      hl,(_ns_rq_data)
        call    rq_send
rq_end:
        ld      a,SL_END
        call    rq_put
        jp      rq_taken                ; (the last one taken too)

; C + (HL)..(HL+B-1), end around.
rq_sum:
        ld      a,c
        add     a,(hl)
        adc     a,0
        ld      c,a
        inc     hl
        djnz    rq_sum
        ret

; (HL)..(HL+B-1), SLIP-escaped.
rq_send:
        ld      a,(hl)
        cp      SL_END
        jr      z,rq_send_e
        cp      SL_ESC
        jr      z,rq_send_e
        call    rq_put
        jr      rq_send_n
rq_send_e:
        push    af
        ld      a,SL_ESC
        call    rq_put
        pop     af
        cp      SL_END
        ld      a,SL_ESCEND
        jr      z,rq_send_x
        ld      a,SL_ESCESC
rq_send_x:
        call    rq_put
rq_send_n:
        inc     hl
        djnz    rq_send
        ret

; A to the card once the byte before has been taken. Keeps BC, HL.
rq_put:
        push    af
        call    rq_taken
        pop     af
        out     (PPI_A),a
        ret

; Wait (60 ms at most) for the byte written to be taken. Keeps BC, HL.
rq_taken:
        ld      a,(_ns_stuck)
        or      a
        ret     nz
        ld      de,TAKE_WAIT
rq_wait:
        in      a,(PPI_C)
        and     OBF_HIGH
        ret     nz
        dec     de
        ld      a,d
        or      e
        jr      nz,rq_wait
        ld      a,1
        ld      (_ns_stuck),a
        ret

        SECTION bss_user

iy_byte: defs   1
de_n:   defs    1
cf_left: defs   2

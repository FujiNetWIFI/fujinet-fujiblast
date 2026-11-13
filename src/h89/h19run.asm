; h19run.asm -- the hot path of src/h89/h19.c's flush: sending a row.
;
;   uint8_t h19_run(void)
;       Row sw_y, columns sw_x..sw_hi-1: each cell where the row as it
;       should be (line) differs from what the terminal shows (*sw_h) is
;       sent -- the cursor moved there the cheapest way, reverse video
;       switched if it must be -- and put in *sw_h. 1 when the span is
;       done; 0 when the budget ran out first, sw_x then the cell left.
;   void h19_goto(uint8_t x) __z88dk_fastcall
;       The cursor to column x of row sw_y (h19.c's ESC K).
;
; The cursor moves as h19.c's did in C: across up to three cells by
; sending them again (they are in line from span_lo on, and must need no
; reverse switch), back by BS, down a row by LF (never from the last row:
; it would scroll), else ESC Y r c. cur_x/cur_y 0FFh: not known.
;
; Bytes go straight into the console's ring (con_emit, unchecked: h19.c's
; budget is what the ring has room for, and every byte here counts against
; it). About 100 T-states a cell that is the same, 300 one sent, against
; some 2000 as C calls.
;
; AF, BC, DE, HL and IY are ours to use (zsdcc).

        SECTION code_user

        PUBLIC  _h19_run, _h19_goto
        EXTERN  _line, _sw_x, _sw_hi, _sw_h, _sw_y, _span_lo
        EXTERN  _cur_x, _cur_y, _cur_rev, _budget, con_emit

        defc    ESC = $1B
        defc    REV = $80
        defc    SCR_W = 80
        defc    ROW25 = 24
        defc    OPMAX = 7

; A into the ring, the budget down one. Keeps BC, DE, HL.
emit:
        call    con_emit
        push    hl
        ld      hl,_budget
        dec     (hl)
        pop     hl
        ret

_h19_run:
        ld      a,(_sw_x)
        ld      c,a                     ; C = x
        ld      a,(_sw_hi)
        sub     c
        jr      z,run_done
        jr      c,run_done
        ld      b,a                     ; B = cells left
        ld      hl,(_sw_h)
        ld      e,c
        ld      d,0
        add     hl,de
        ex      de,hl                   ; DE = have + x
        ld      hl,_line
        ld      a,l
        add     a,c
        ld      l,a
        jr      nc,run_loop
        inc     h                       ; HL = line + x
run_loop:
        ld      a,(de)
        cp      (hl)
        jr      nz,run_diff
run_next:
        inc     hl
        inc     de
        inc     c
        djnz    run_loop
run_done:
        ld      l,1
        ret

run_diff:
        ld      a,(_budget)             ; (signed: it may be under 0)
        xor     $80
        cp      $80 + OPMAX
        jr      c,run_stop
        push    bc
        push    de
        push    hl
        call    goto_c                  ; the cursor to column C
        pop     hl
        pop     de
        pop     bc
        ld      a,(hl)                  ; reverse video as the cell wants
        and     REV
        push    hl
        ld      hl,_cur_rev
        cp      (hl)
        jr      z,run_rev_ok
        ld      (hl),a
        ld      a,ESC
        call    emit
        ld      a,(_cur_rev)
        or      a
        ld      a,'p'
        jr      nz,run_rev_set
        ld      a,'q'
run_rev_set:
        call    emit
run_rev_ok:
        pop     hl
        ld      a,(hl)
        ld      (de),a                  ; the terminal shows it
        and     $7F
        call    emit
        ld      a,c                     ; (discard at the end of a line: the
        cp      SCR_W - 1               ; last column keeps the cursor)
        ld      a,$FF
        jr      nc,run_cx
        ld      a,c
        inc     a
run_cx:
        ld      (_cur_x),a
        jr      run_next

run_stop:
        ld      a,c
        ld      (_sw_x),a
        ld      l,0
        ret

_h19_goto:
        ld      c,l
        ; (fall into goto_c)

; The cursor to column C of row sw_y. Keeps C.
goto_c:
        ld      a,(_cur_x)
        cp      $FF
        jr      z,go_escy
        ld      e,a                     ; E = cur_x
        ld      a,(_cur_y)
        cp      $FF
        jr      z,go_escy
        ld      d,a                     ; D = cur_y
        ld      a,(_sw_y)
        cp      d
        jr      z,go_along
        dec     a                       ; down a row?
        cp      d
        jr      nz,go_escy
        ld      a,(_sw_y)
        cp      ROW25
        jr      nc,go_escy
        ld      a,e
        sub     c
        jr      nc,go_lf_left
        neg
go_lf_left:
        cp      3
        jr      nc,go_escy
        ld      a,10                    ; LF: the column stays
        call    emit
        ld      a,(_sw_y)
        ld      (_cur_y),a
go_along:
        ld      a,e
        cp      c
        ret     z                       ; there
        jr      c,go_right
        sub     c                       ; back: BS (at most 2)
        cp      3
        jr      nc,go_escy
        ld      b,a
go_bs:
        ld      a,8
        call    emit
        djnz    go_bs
        ld      a,c
        ld      (_cur_x),a
        ret
go_right:
        ld      a,c                     ; on: the cells between again
        sub     e
        cp      4
        jr      nc,go_escy
        ld      b,a
        ld      a,(_span_lo)
        ld      d,a
        ld      a,e
        cp      d
        jr      c,go_escy               ; (before the span: not in line)
        ld      hl,_line
        ld      d,0
        add     hl,de                   ; HL = line + cur_x
        push    hl
        push    bc
        ld      a,(_cur_rev)
        ld      d,a
go_chk:
        ld      a,(hl)
        and     REV
        cp      d
        jr      nz,go_chk_no
        inc     hl
        djnz    go_chk
        pop     bc
        pop     hl
go_resend:
        ld      a,(hl)
        and     $7F
        call    emit
        inc     hl
        djnz    go_resend
        ld      a,c
        ld      (_cur_x),a
        ret
go_chk_no:
        pop     bc
        pop     hl
go_escy:
        ld      a,ESC
        call    emit
        ld      a,'Y'
        call    emit
        ld      a,(_sw_y)
        ld      (_cur_y),a
        add     a,32
        call    emit
        ld      a,c
        ld      (_cur_x),a
        add     a,32
        jp      emit

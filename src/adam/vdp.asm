; vdp.asm -- the VDP writers (adam.h). Arguments are variables, not stack.
; Each locks the NMI out (vdp_busy) and unlocks at the end, taking a frame
; the NMI held back (vdp_catch_up). A zero count does nothing (a count that
; counts down from zero would spray 64K of RAM over the VRAM).

        SECTION code_user

        PUBLIC  _va_addr, _va_len, _va_src, _va_val
        PUBLIC  _va_write, _va_writef, _va_fill
        EXTERN  _vdp_busy, _vdp_pend, _vdp_catch_up

; set the write address va_addr, lock; DE the count, HL the source
setup:
        ld      a,1
        ld      (_vdp_busy),a
        ld      hl,(_va_addr)
        ld      a,l
        out     ($BF),a
        ld      a,h
        or      $40
        out     ($BF),a
        ld      de,(_va_len)
        ld      hl,(_va_src)
        ret

unlock:
        xor     a
        ld      (_vdp_busy),a
        ld      a,(_vdp_pend)
        or      a
        ret     z
        jp      _vdp_catch_up

; 50 T-states a byte
_va_write:
        ld      de,(_va_len)
        ld      a,d
        or      e
        ret     z
        call    setup
wl:     ld      a,(hl)
        out     ($BE),a
        inc     hl
        dec     de
        ld      a,d
        or      e
        jr      nz,wl
        jr      unlock

; OTIR, 21 T-states a byte
_va_writef:
        ld      de,(_va_len)
        ld      a,d
        or      e
        ret     z
        call    setup
fl:     ld      a,d
        or      a
        jr      z,ftail
        ld      bc,$00BE        ; b = 0 is 256
        otir
        dec     d
        jr      fl
ftail:  ld      a,e
        or      a
        jr      z,unlock
        ld      b,a
        ld      c,$BE
        otir
        jr      unlock

; 41 T-states a byte
_va_fill:
        ld      de,(_va_len)
        ld      a,d
        or      e
        ret     z
        call    setup
        ld      a,(_va_val)
        ld      h,a
fi:     ld      a,h
        out     ($BE),a
        dec     de
        ld      a,d
        or      e
        jr      nz,fi
        jr      unlock

        SECTION bss_user
_va_addr:       defs    2
_va_len:        defs    2
_va_src:        defs    2
_va_val:        defs    1

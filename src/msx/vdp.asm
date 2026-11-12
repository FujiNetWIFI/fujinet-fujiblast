; vdp.asm -- the VDP writers (vdp.h), for the MSX: ports 98h and 99h.
; Arguments are variables, not stack. Interrupts are off for the whole
; burst: the BIOS's interrupt reads the status (which resets the address
; latch), and with it in the middle of a burst bytes were lost (the sprite
; table came out shifted). A burst is at most a screen's name table (768
; bytes, 10 ms), so no frame's interrupt is missed but the clear at
; start-up. A zero count does nothing (a count that counts down from zero
; would spray 64K of RAM over the VRAM).

        SECTION code_user

        PUBLIC  _va_addr, _va_len, _va_src, _va_val
        PUBLIC  _va_write, _va_writef, _va_fill

; set the write address va_addr, interrupts off; DE the count, HL the source
setup:
        di
        ld      hl,(_va_addr)
        ld      a,l
        out     ($99),a
        ld      a,h
        or      $40
        out     ($99),a
        ld      de,(_va_len)
        ld      hl,(_va_src)
        ret

; 50 T-states a byte
_va_write:
        ld      de,(_va_len)
        ld      a,d
        or      e
        ret     z
        call    setup
wl:     ld      a,(hl)
        out     ($98),a
        inc     hl
        dec     de
        ld      a,d
        or      e
        jr      nz,wl
        ei
        ret

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
        ld      bc,$0098        ; b = 0 is 256
        otir
        dec     d
        jr      fl
ftail:  ld      a,e
        or      a
        jr      z,fdone
        ld      b,a
        ld      c,$98
        otir
fdone:  ei
        ret

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
        out     ($98),a
        dec     de
        ld      a,d
        or      e
        jr      nz,fi
        ei
        ret

        SECTION bss_user
_va_addr:       defs    2
_va_len:        defs    2
_va_src:        defs    2
_va_val:        defs    1

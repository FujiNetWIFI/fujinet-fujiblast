; vdp.asm -- the SMS's VDP, written a block at a time (src/sms/gfx.c).
;
; The data port takes a byte every 26 cycles at the fastest with the display
; on, and these loops are 40-50: safe in the blank and out of it. The frame
; interrupt reads the status (which clears the address latch), so an
; address is set with interrupts off.
;
;   va_addr   the VRAM address (CRAM: va_cram), va_src the bytes, va_len
;   va_val    the byte va_fill writes va_len times

        SECTION code_user

        PUBLIC  _va_write, _va_fill, _va_cram, _va_reg
        PUBLIC  _va_addr, _va_src, _va_len, _va_val

        defc    VDP_DATA = $BE
        defc    VDP_CTRL = $BF

; the address in va_addr, with the command bits in B (40h write VRAM, C0h CRAM)
setaddr:
        ld      hl,(_va_addr)
        di
        ld      a,l
        out     (VDP_CTRL),a
        ld      a,h
        or      b
        out     (VDP_CTRL),a
        ei
        ret

_va_write:
        ld      b,$40
wr:     call    setaddr
        ld      hl,(_va_src)
        ld      bc,(_va_len)
        ld      a,b
        or      c
        ret     z
wr_l:   ld      a,(hl)
        out     (VDP_DATA),a
        inc     hl
        dec     bc
        ld      a,b
        or      c
        jr      nz,wr_l
        ret

_va_cram:
        ld      b,$C0
        jr      wr

_va_fill:
        ld      b,$40
        call    setaddr
        ld      bc,(_va_len)
        ld      a,b
        or      c
        ret     z
        ld      a,(_va_val)
        ld      e,a
fl_l:   ld      a,e
        out     (VDP_DATA),a
        dec     bc
        ld      a,b
        or      c
        jr      nz,fl_l
        ret

; the VDP register va_addr (low byte) gets va_val
_va_reg:
        ld      a,(_va_val)
        ld      hl,(_va_addr)
        di
        out     (VDP_CTRL),a
        ld      a,l
        or      $80
        out     (VDP_CTRL),a
        ei
        ret

        SECTION bss_user
_va_addr:       defs    2
_va_src:        defs    2
_va_len:        defs    2
_va_val:        defs    1

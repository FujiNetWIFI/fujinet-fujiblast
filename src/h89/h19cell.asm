; h19cell.asm -- the H89's hot cell writes (src/h89/h19.c, play.c):
;
;   void h19_put(uint8_t x, uint8_t y, uint8_t c)
;   void h19_fill(uint8_t x, uint8_t y, uint8_t w, uint8_t c)
;   void h19_putn(uint8_t x, uint8_t y, const uint8_t *s, uint8_t n)
;       Cells into bg; the columns that changed widen the row's span
;       (d_lo, d_hi: what h19_flush looks at). A cell the same as before
;       costs 55 T-states, against some 160 as zsdcc compiled the C (from
;       M.U.L.E.'s h19cell.asm, which kept dirty bits).
;   uint8_t map_next(const uint8_t *map, const uint8_t *drawn, uint8_t from)
;       (play.c, PLAT_MAP_NEXT) the first tile from `from` on where the
;       map and what is drawn differ, or TILES: the scan of a new state,
;       about 50 T-states a tile, against some 400.
;
; zsdcc's arguments, on the stack (a byte for each uint8_t), the caller's
; to take off; the result in L. AF, BC, DE, HL and IY are ours to use.

        SECTION code_user

        PUBLIC  _h19_put, _h19_fill, _h19_putn, _map_next
        EXTERN  _rowp, _d_lo, _d_hi, _any_dirty

        defc    SCR_W = 80
        defc    SCR_H = 25
        defc    TILES = 143

; HL = rowp[A] + C. Keeps BC, DE.
cell_at:
        add     a,a
        ld      l,a
        ld      h,0
        push    de
        ld      de,_rowp
        add     hl,de
        ld      e,(hl)
        inc     hl
        ld      d,(hl)
        ex      de,hl
        pop     de
        ld      a,l
        add     a,c
        ld      l,a
        ret     nc
        inc     h
        ret

; Columns (cfirst)..(clast) of row (cy) changed: the span widened.
widen:
        ld      a,(cy)
        ld      e,a
        ld      d,0
        ld      hl,_d_lo
        add     hl,de
        ld      a,(cfirst)
        cp      (hl)
        jr      nc,wd_hi
        ld      (hl),a
wd_hi:
        ld      hl,_d_hi
        add     hl,de
        ld      a,(clast)
        inc     a
        cp      (hl)
        jr      c,wd_done
        jr      z,wd_done
        ld      (hl),a
wd_done:
        ld      a,1
        ld      (_any_dirty),a
        ret

_h19_put:
        ld      hl,2
        add     hl,sp
        ld      c,(hl)                  ; x
        inc     hl
        ld      a,(hl)                  ; y
        inc     hl
        ld      b,(hl)                  ; c
        cp      SCR_H
        ret     nc
        ld      (cy),a
        ld      e,a
        ld      a,c
        cp      SCR_W
        ret     nc
        ld      a,e
        call    cell_at
        ld      a,b
        cp      (hl)
        ret     z
        ld      (hl),a
        ld      a,c
        ld      (cfirst),a
        ld      (clast),a
        jp      widen

_h19_fill:
        ld      hl,2
        add     hl,sp
        ld      c,(hl)                  ; x
        inc     hl
        ld      a,(hl)                  ; y
        inc     hl
        ld      b,(hl)                  ; w
        inc     hl
        ld      d,(hl)                  ; c
        cp      SCR_H
        ret     nc
        ld      (cy),a
        ld      e,a
        ld      a,c
        cp      SCR_W
        ret     nc
        ld      a,SCR_W                 ; w cut at the row's end
        sub     c
        cp      b
        jr      nc,fi_w
        ld      b,a
fi_w:
        ld      a,b
        or      a
        ret     z
        ld      a,e
        call    cell_at                 ; HL = the first cell, C = its column
        ld      a,$FF
        ld      (cfirst),a
fi_loop:
        ld      a,(hl)
        cp      d
        jr      z,fi_same
        ld      (hl),d
        ld      a,(cfirst)
        inc     a
        jr      nz,fi_last
        ld      a,c
        ld      (cfirst),a
fi_last:
        ld      a,c
        ld      (clast),a
fi_same:
        inc     hl
        inc     c
        djnz    fi_loop
        ld      a,(cfirst)
        inc     a
        ret     z
        jp      widen

_h19_putn:
        ld      hl,2
        add     hl,sp
        ld      c,(hl)                  ; x
        inc     hl
        ld      a,(hl)                  ; y
        inc     hl
        ld      e,(hl)
        inc     hl
        ld      d,(hl)                  ; s
        inc     hl
        ld      b,(hl)                  ; n
        cp      SCR_H
        ret     nc
        ld      (cy),a
        ld      a,c
        cp      SCR_W
        ret     nc
        ld      a,SCR_W
        sub     c
        cp      b
        jr      nc,pn_w
        ld      b,a
pn_w:
        ld      a,b
        or      a
        ret     z
        ld      a,(cy)
        call    cell_at                 ; HL = the first cell, C = its column
        ld      a,$FF
        ld      (cfirst),a
pn_loop:
        ld      a,(de)
        cp      (hl)
        jr      z,pn_same
        ld      (hl),a
        ld      a,(cfirst)
        inc     a
        jr      nz,pn_last
        ld      a,c
        ld      (cfirst),a
pn_last:
        ld      a,c
        ld      (clast),a
pn_same:
        inc     hl
        inc     de
        inc     c
        djnz    pn_loop
        ld      a,(cfirst)
        inc     a
        ret     z
        jp      widen

_map_next:
        ld      hl,2
        add     hl,sp
        ld      e,(hl)
        inc     hl
        ld      d,(hl)                  ; DE = map
        inc     hl
        ld      c,(hl)
        inc     hl
        ld      b,(hl)                  ; BC = drawn
        inc     hl
        ld      a,(hl)                  ; from
        cp      TILES
        jr      nc,mn_none
        ld      l,a
        ld      h,0
        add     hl,de
        ex      de,hl                   ; DE = map + from
        ld      l,a
        ld      h,0
        add     hl,bc                   ; HL = drawn + from
        neg
        add     a,TILES
        ld      b,a                     ; B = tiles left
mn_loop:
        ld      a,(de)
        cp      (hl)
        jr      nz,mn_found
        inc     de
        inc     hl
        djnz    mn_loop
mn_none:
        ld      l,TILES
        ret
mn_found:
        ld      a,TILES
        sub     b
        ld      l,a
        ret

        SECTION bss_user

cy:     defs    1
cfirst: defs    1
clast:  defs    1

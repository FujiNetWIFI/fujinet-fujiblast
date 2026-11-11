;
; gfxa.asm -- gfx.c's loops: copying B (the screen's copy in RAM) to the
; screen, art into B, and putting sprites together with B line by line.
;
; Small model: code in _TEXT, data in DGROUP (DS = SS = DGROUP when C calls
; here). B is its own segment (_gx_bseg); the screen is B800h. Both have
; CGA's layout, a line at _gx_rowoff[y] in either, so a rectangle has the
; same offsets in both. Arguments are globals (gfx.c sets them); C calls
; these through #pragma aux ... "*_" parm [] modify [ax bx cx dx si di es].
;
        .8086

DGROUP  GROUP   _DATA, _BSS

_DATA   SEGMENT WORD PUBLIC 'DATA'
_DATA   ENDS

_BSS    SEGMENT WORD PUBLIC 'BSS'
cy      DB      ?               ; the line being put together
cnt     DB      ?               ; lines left
jcnt    DB      ?               ; jobs left on this line
        EVEN
loff    DW      ?               ; the line's offset (B and the screen)
_BSS    ENDS

        EXTRN   _gx_rowoff:WORD
        EXTRN   _gx_bseg:WORD
        EXTRN   _gx_x:BYTE
        EXTRN   _gx_w:BYTE
        EXTRN   _gx_y:BYTE
        EXTRN   _gx_h:BYTE
        EXTRN   _gx_src:WORD
        EXTRN   _gx_lbuf:WORD
        EXTRN   _gx_jobs:BYTE
        EXTRN   _gx_njobs:BYTE
        EXTRN   _gx_tab:BYTE
        EXTRN   _gx_tog:BYTE
        EXTRN   _gx_tabk:WORD
        EXTRN   _gx_ctab:WORD
        EXTRN   _gx_both:BYTE

; a Job (gfx.c, packed: -zp1)
J_Y0    EQU     0               ; its first line here
J_Y1    EQU     1               ; and the line after its last
J_DST   EQU     2               ; where it starts in the line buffer
J_N     EQU     3               ; bytes a line
J_STRIDE EQU    4               ; its data's bytes a line (2 a byte)
J_TOG   EQU     5               ; what odd lines change in its colours' page
J_DATA  EQU     6               ; its data at J_Y0 (moved on a line at a time)
J_TAB   EQU     8               ; its colours: page in the high byte
J_SIZE  EQU     10

VIDSEG  EQU     0B800h

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT, DS:DGROUP, SS:DGROUP

; gx_blit: B to the screen, _gx_w bytes from byte _gx_x on _gx_h lines from
; line _gx_y.
        PUBLIC  gx_blit_
gx_blit_ PROC   NEAR
        push    bp
        push    ds
        cld
        mov     ax, VIDSEG
        mov     es, ax
        mov     bl, _gx_y
        xor     bh, bh
        shl     bx, 1
        mov     al, _gx_x
        xor     ah, ah
        mov     bp, ax
        mov     dl, _gx_h
        mov     dh, _gx_w
        mov     ds, _gx_bseg            ; (DGROUP is SS from here)
bl_line:
        mov     si, ss:_gx_rowoff[bx]
        add     si, bp
        mov     di, si
        mov     cl, dh
        xor     ch, ch
        shr     cx, 1
        rep     movsw
        adc     cx, cx
        rep     movsb
        add     bx, 2
        dec     dl
        jnz     bl_line
        pop     ds
        pop     bp
        ret
gx_blit_ ENDP

; gx_put: art (near, _gx_src, _gx_w bytes a line) into B at byte _gx_x,
; line _gx_y, for _gx_h lines.
        PUBLIC  gx_put_
gx_put_ PROC    NEAR
        cld
        mov     es, _gx_bseg
        mov     si, _gx_src
        mov     bl, _gx_y
        xor     bh, bh
        shl     bx, 1
        mov     al, _gx_x
        xor     ah, ah
        mov     dx, ax
        mov     ah, _gx_h
        mov     al, _gx_w
pt_line:
        mov     di, _gx_rowoff[bx]
        add     di, dx
        mov     cl, al
        xor     ch, ch
        shr     cx, 1
        rep     movsw
        adc     cx, cx
        rep     movsb
        add     bx, 2
        dec     ah
        jnz     pt_line
        ret
gx_put_ ENDP

; gx_put2: the same, into B and onto the screen at once (nothing being
; over it there).
        PUBLIC  gx_put2_
gx_put2_ PROC   NEAR
        push    bp
        cld
        mov     bl, _gx_y
        xor     bh, bh
        shl     bx, 1
        mov     al, _gx_x
        xor     ah, ah
        mov     dx, ax
        mov     ah, _gx_h
        mov     al, _gx_w
        mov     si, _gx_src
        mov     bp, VIDSEG
p2_line:
        mov     di, _gx_rowoff[bx]
        add     di, dx
        push    di
        mov     es, _gx_bseg
        mov     cl, al
        xor     ch, ch
        shr     cx, 1
        rep     movsw
        adc     cx, cx
        rep     movsb
        pop     di
        mov     es, bp
        mov     cl, al
        xor     ch, ch
        sub     si, cx                  ; the line again, to the screen
        shr     cx, 1
        rep     movsw
        adc     cx, cx
        rep     movsb
        add     bx, 2
        dec     ah
        jnz     p2_line
        pop     bp
        ret
gx_put2_ ENDP

; gx_tile: a 16x16 tile (64 bytes at _gx_src, near) into B and onto the
; screen at byte _gx_x, line _gx_y (even). The lines step through CGA's
; two banks by turns (2000h on, then back 2000h less 80), no table.
TSTEP0  EQU     2000h - 4               ; an even line's end to the odd one after
TSTEP1  EQU     80 - 2000h - 4          ; an odd line's end to the even one after
        PUBLIC  gx_tile_
gx_tile_ PROC   NEAR
        push    bp
        cld
        mov     bl, _gx_y
        xor     bh, bh
        shl     bx, 1
        mov     di, _gx_rowoff[bx]
        mov     al, _gx_x
        xor     ah, ah
        add     di, ax
        mov     dx, di
        mov     bx, TSTEP0 XOR TSTEP1
        mov     es, _gx_bseg
        mov     si, _gx_src
        mov     bp, TSTEP0
        mov     cx, 16
tl_b:   movsw
        movsw
        add     di, bp
        xor     bp, bx
        loop    tl_b
        mov     ax, VIDSEG
        mov     es, ax
        mov     si, _gx_src
        mov     di, dx
        mov     bp, TSTEP0
        mov     cx, 16
tl_v:   movsw
        movsw
        add     di, bp
        xor     bp, bx
        loop    tl_v
        pop     bp
        ret
gx_tile_ ENDP

; gx_char: a glyph (8 bytes at _gx_src, a row a byte, bit 7 left) into B
; and onto the screen (_gx_both; else B twice) at byte _gx_x, line _gx_y
; (a cell's top). Normal text (_gx_w 2): each nibble a byte through the
; table at _gx_ctab (16 bytes). BIG text (_gx_w 4): each nibble two bytes,
; through the table's two halves (16 + 16). Odd lines use _gx_ctab xor
; _gx_tabk (the fourth seat's stripes). The tables are DGROUP's (SS here:
; DS is B), so each byte is made once and stored twice.
SSXLAT  MACRO
        DB      36h                     ; ss: (xlat reads SS:BX + AL)
        xlat
        ENDM
        PUBLIC  gx_char_
gx_char_ PROC   NEAR
        push    bp
        cld
        mov     bl, _gx_y
        xor     bh, bh
        shl     bx, 1
        mov     di, _gx_rowoff[bx]
        mov     al, _gx_x
        xor     ah, ah
        add     di, ax
        mov     al, _gx_w
        mov     bp, 2000h               ; even line to odd: 2000h on, less w
        sub     bp, ax
        mov     cx, 80 - 2000h
        sub     cx, ax
        xor     cx, bp
        mov     cs:ch_k, cx             ; (and back: they take turns)
        mov     ax, _gx_tabk
        mov     cs:ch_tk, ax
        mov     si, _gx_src
        mov     bx, _gx_ctab
        mov     dh, _gx_w
        mov     ax, VIDSEG
        cmp     _gx_both, 0
        jne     ch_es
        mov     ax, _gx_bseg            ; (B only: written twice)
ch_es:  mov     es, ax
        push    ds
        mov     ds, _gx_bseg
        mov     cx, 8
ch_line:
        mov     al, ss:[si]
        inc     si
        mov     ah, al
        and     ah, 0Fh                 ; AH the low nibble
        shr     al, 1
        shr     al, 1
        shr     al, 1
        shr     al, 1                   ; AL the high one
        mov     dl, al
        SSXLAT
        mov     [di], al
        stosb
        cmp     dh, 4
        jne     ch_small
        mov     al, dl                  ; BIG: the high nibble's second byte
        add     al, 16
        SSXLAT
        mov     [di], al
        stosb
        mov     al, ah
        SSXLAT
        mov     [di], al
        stosb
        mov     al, ah
        add     al, 16
        SSXLAT
        mov     [di], al
        stosb
        jmp     ch_next
ch_small:
        mov     al, ah
        SSXLAT
        mov     [di], al
        stosb
ch_next:
        add     di, bp
        xor     bp, cs:ch_k
        xor     bx, cs:ch_tk
        loop    ch_line
        pop     ds
        pop     bp
        ret
ch_k    DW      0
ch_tk   DW      0
gx_char_ ENDP

; map_next(SI map, DI drawn, DX from): AL = the first tile from DX on whose
; map entry differs from what is drawn, or TILES (143). repe cmpsb skips
; the many that match ~10x faster than a C loop.
TILES   EQU     143
        PUBLIC  map_next_
map_next_ PROC  NEAR
        cld
        mov     ax, ds
        mov     es, ax
        xor     dh, dh
        add     si, dx
        add     di, dx
        mov     cx, TILES
        sub     cx, dx
        jbe     mn_none
        repe    cmpsb
        je      mn_none                 ; all the same
        mov     ax, TILES - 1
        sub     ax, cx
        ret
mn_none:
        mov     al, TILES
        ret
map_next_ ENDP

; gx_compose: lines _gx_y.._gx_y+_gx_h-1, bytes _gx_x.._gx_x+_gx_w-1: B,
; with the _gx_njobs sprites' lines over it, put together in a line buffer
; (_gx_lbuf, in B's segment), then to the screen. For sprites that overlap,
; or are cut by the screen's edge; gx_sprite does the usual ones. A
; sprite's data (B's segment) is a role and a mask byte for each 4 pixels:
; lodsw takes both, xlat colours the roles for the seat (its table's page
; in BH), and the mask keeps what of the line shows through.
        PUBLIC  gx_compose_
gx_compose_ PROC NEAR
        push    bp
        cld
        mov     al, _gx_y
        mov     cy, al
        mov     al, _gx_h
        mov     cnt, al
cp_line:
        mov     bl, cy
        xor     bh, bh
        shl     bx, 1
        mov     si, _gx_rowoff[bx]
        mov     al, _gx_x
        xor     ah, ah
        add     si, ax
        mov     loff, si
        mov     di, _gx_lbuf
        mov     cl, _gx_w
        xor     ch, ch
        mov     dl, _gx_njobs
        mov     dh, cy
        push    ds
        mov     es, _gx_bseg
        mov     ds, _gx_bseg            ; (DGROUP is SS from here)
        shr     cx, 1                   ; B's line into the buffer
        rep     movsw
        adc     cx, cx
        rep     movsb
        test    dl, dl
        jz      cp_out
        mov     bp, OFFSET DGROUP:_gx_jobs
cp_job:
        cmp     dh, [bp + J_Y0]
        jb      cp_next
        cmp     dh, [bp + J_Y1]
        jae     cp_next
        mov     bx, [bp + J_TAB]
        test    dh, 1
        jz      cp_even
        xor     bh, [bp + J_TOG]
cp_even:
        mov     di, ss:_gx_lbuf
        mov     al, [bp + J_DST]
        xor     ah, ah
        add     di, ax
        mov     si, [bp + J_DATA]
        mov     al, [bp + J_STRIDE]
        xor     ah, ah
        add     [bp + J_DATA], ax       ; (the next line's)
        mov     cl, [bp + J_N]
        xor     ch, ch
cp_byte:
        lodsw                           ; AL 4 roles, AH their mask
        xlat                            ; 4 colours
        and     ah, [di]                ; what of the line shows through
        or      al, ah
        stosb
        loop    cp_byte
cp_next:
        add     bp, J_SIZE
        dec     dl
        jnz     cp_job
cp_out:
        mov     ax, VIDSEG              ; the buffer to the screen
        mov     es, ax
        mov     si, ss:_gx_lbuf
        mov     di, ss:loff
        mov     cl, ss:_gx_w
        xor     ch, ch
        shr     cx, 1
        rep     movsw
        adc     cx, cx
        rep     movsb
        pop     ds
        inc     cy
        dec     cnt
        jnz     cp_line
        pop     bp
        ret
gx_compose_ ENDP

; gx_sprite: one sprite, 4 or 5 bytes wide (_gx_w), with nothing else over
; it, straight onto the screen: each byte B's (the same offset) masked and
; its colours put in. At _gx_x (bytes), _gx_y, for _gx_h lines; its data at
; _gx_src (B's segment), its colours' page _gx_tab, _gx_tog what odd lines
; change in it (the fourth seat's stripes). About 45 cycles a byte.
sp_k    DW      0                       ; even-line step xor odd-line step
SPBYTE  MACRO
        lodsw
        xlat
        and     ah, [di]
        or      al, ah
        stosb
        ENDM
        PUBLIC  gx_sprite_
gx_sprite_ PROC NEAR
        push    bp
        cld
        mov     bl, _gx_y
        xor     bh, bh
        shl     bx, 1
        mov     di, _gx_rowoff[bx]
        mov     al, _gx_x
        xor     ah, ah
        add     di, ax
        mov     cl, _gx_h
        xor     ch, ch
        mov     dl, _gx_tog
        mov     dh, _gx_w
        ; from a line's end to the next one's start: even to odd 2000h - w,
        ; odd to even 80 - 2000h - w; they take turns (xor sp_k)
        mov     al, dh
        xor     ah, ah
        mov     bp, 2000h
        sub     bp, ax
        mov     si, 80 - 2000h
        sub     si, ax
        mov     ax, bp
        xor     ax, si
        mov     cs:sp_k, ax
        mov     bh, _gx_tab
        xor     bl, bl
        test    _gx_y, 1
        jz      sp_go
        mov     bp, si                  ; an odd first line
        xor     bh, dl
sp_go:
        mov     si, _gx_src
        mov     ax, VIDSEG
        mov     es, ax
        push    ds
        mov     ds, _gx_bseg
        cmp     dh, 5
        je      sp5
sp4:
        SPBYTE
        SPBYTE
        SPBYTE
        SPBYTE
        add     di, bp
        xor     bp, cs:sp_k
        xor     bh, dl
        loop    sp4
        jmp     sp_done
sp5:
        SPBYTE
        SPBYTE
        SPBYTE
        SPBYTE
        SPBYTE
        add     di, bp
        xor     bp, cs:sp_k
        xor     bh, dl
        loop    sp5
sp_done:
        pop     ds
        pop     bp
        ret
gx_sprite_ ENDP

_TEXT   ENDS
        END

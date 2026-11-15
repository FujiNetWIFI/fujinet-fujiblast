;
; gfxa.asm -- the VGA build's gfx.c loops (mode 13h, a byte a pixel):
; copying B (the screen's copy in RAM) to the screen, art and text into B,
; and sprites put together with B on their way to the screen.
;
; Small model: code in _TEXT, data in DGROUP (DS = SS = DGROUP when C calls
; here). B is its own 64K segment (_gx_bseg), the art another (_gx_aseg),
; the screen A000h; a line is at _gx_rowoff[y] (y * 320) in B and on the
; screen, so a rectangle has the same offsets in both. Arguments are
; globals (gfx.c sets them); C calls these through
; #pragma aux ... "*_" parm [] modify [ax bx cx dx si di es]. 8086 only.
;
        .8086

DGROUP  GROUP   _DATA, _BSS

_DATA   SEGMENT WORD PUBLIC 'DATA'
_DATA   ENDS

_BSS    SEGMENT WORD PUBLIC 'BSS'
cy      DB      ?               ; gx_compose: the line being put together
cnt     DB      ?               ; lines left
        EVEN
loff    DW      ?               ; the line's offset (B and the screen)
_BSS    ENDS

        EXTRN   _gx_rowoff:WORD
        EXTRN   _gx_bseg:WORD
        EXTRN   _gx_aseg:WORD
        EXTRN   _gx_x:WORD
        EXTRN   _gx_w:WORD
        EXTRN   _gx_y:BYTE
        EXTRN   _gx_h:BYTE
        EXTRN   _gx_src:WORD
        EXTRN   _gx_bgl:WORD
        EXTRN   _gx_fg:WORD
        EXTRN   _gx_bg:WORD
        EXTRN   _gx_lbuf:WORD
        EXTRN   _gx_jobs:BYTE
        EXTRN   _gx_njobs:BYTE

; a Job (gfx.c, packed: -zp1)
J_Y0    EQU     0               ; its first line here
J_Y1    EQU     1               ; and the line after its last
J_DST   EQU     2               ; where it starts in the line buffer
J_N     EQU     4               ; pixels a line
J_STRIDE EQU    6               ; its image's bytes a line
J_DATA  EQU     8               ; its image at J_Y0 (DGROUP; moved on a line at a time)
J_SIZE  EQU     10

VIDSEG  EQU     0A000h

; an ss: prefix (lodsb then reads SS:SI: a sprite's image in DGROUP while DS
; is B)
SSPFX   MACRO
        DB      36h
        ENDM

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT, DS:DGROUP, SS:DGROUP

; gx_blit: B to the screen, _gx_w pixels from _gx_x on _gx_h lines from
; _gx_y.
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
        mov     bp, _gx_x
        mov     dx, _gx_w
        mov     al, _gx_h
        mov     ds, _gx_bseg            ; (DGROUP is SS from here)
bl_line:
        mov     si, ss:_gx_rowoff[bx]
        add     si, bp
        mov     di, si
        mov     cx, dx
        shr     cx, 1
        rep     movsw
        adc     cx, cx
        rep     movsb
        add     bx, 2
        dec     al
        jnz     bl_line
        pop     ds
        pop     bp
        ret
gx_blit_ ENDP

; gx_fill: B's rectangle (_gx_x, _gx_w, _gx_y, _gx_h), each line in its
; colour from the table at _gx_bgl (DGROUP, a byte a line from line 0).
        PUBLIC  gx_fill_
gx_fill_ PROC   NEAR
        push    bp
        cld
        mov     es, _gx_bseg
        mov     bl, _gx_y
        xor     bh, bh
        mov     si, _gx_bgl
        add     si, bx
        shl     bx, 1
        mov     dx, _gx_w
        mov     bp, _gx_x
        mov     ch, _gx_h
        mov     cs:fl_cnt, ch
fl_line:
        lodsb
        mov     ah, al
        mov     di, _gx_rowoff[bx]
        add     di, bp
        mov     cx, dx
        shr     cx, 1
        rep     stosw
        adc     cx, cx
        rep     stosb
        add     bx, 2
        dec     byte ptr cs:fl_cnt
        jnz     fl_line
        pop     bp
        ret
fl_cnt  DB      0
gx_fill_ ENDP

; gx_put: art (_gx_aseg:_gx_src, _gx_w bytes a line) into B at _gx_x,
; _gx_y, for _gx_h lines.
        PUBLIC  gx_put_
gx_put_ PROC    NEAR
        push    bp
        push    ds
        cld
        mov     es, _gx_bseg
        mov     bl, _gx_y
        xor     bh, bh
        shl     bx, 1
        mov     bp, _gx_x
        mov     dx, _gx_w
        mov     al, _gx_h
        mov     si, _gx_src
        mov     ds, _gx_aseg            ; (DGROUP is SS from here)
pt_line:
        mov     di, ss:_gx_rowoff[bx]
        add     di, bp
        mov     cx, dx
        shr     cx, 1
        rep     movsw
        adc     cx, cx
        rep     movsb
        add     bx, 2
        dec     al
        jnz     pt_line
        pop     ds
        pop     bp
        ret
gx_put_ ENDP

; gx_putk: the same, but where the art is 0 the line's colour from the
; table at _gx_bgl (the backdrop shows through).
        PUBLIC  gx_putk_
gx_putk_ PROC   NEAR
        push    bp
        push    ds
        cld
        mov     es, _gx_bseg
        mov     bl, _gx_y
        xor     bh, bh
        mov     bp, _gx_bgl             ; ([bp] reads SS: DGROUP)
        add     bp, bx
        shl     bx, 1
        mov     dx, _gx_w
        mov     al, _gx_h
        mov     cs:pk_cnt, al
        mov     ax, _gx_x
        mov     cs:pk_x, ax
        mov     si, _gx_src
        mov     ds, _gx_aseg            ; (DGROUP is SS from here)
pk_line:
        mov     di, ss:_gx_rowoff[bx]
        add     di, cs:pk_x
        mov     ah, [bp]                ; the line's colour
        inc     bp
        mov     cx, dx
pk_px:  lodsb
        or      al, al
        jnz     pk_put
        mov     al, ah
pk_put: stosb
        loop    pk_px
        add     bx, 2
        dec     byte ptr cs:pk_cnt
        jnz     pk_line
        pop     ds
        pop     bp
        ret
pk_cnt  DB      0
pk_x    DW      0
gx_putk_ ENDP

; gx_char: a glyph (8 bytes at _gx_src, DGROUP, a line a byte, bit 7 left)
; into B at _gx_x, _gx_y (a cell's top), _gx_w 8 (each bit a pixel) or 16
; (each bit two). Line n's lit pixels are _gx_fg[n], the rest _gx_bg[n]
; (8 bytes each, DGROUP).
        PUBLIC  gx_char_
gx_char_ PROC   NEAR
        push    bp
        cld
        mov     es, _gx_bseg
        mov     bl, _gx_y
        xor     bh, bh
        shl     bx, 1
        mov     di, _gx_rowoff[bx]
        add     di, _gx_x
        mov     ax, 320
        sub     ax, _gx_w
        mov     cs:ch_step, ax
        mov     si, _gx_src
        mov     bx, _gx_fg
        mov     bp, _gx_bg              ; ([bp] reads SS: DGROUP)
        mov     ah, 8
ch_line:
        mov     ch, [si]                ; the glyph's line
        inc     si
        mov     dl, [bx]                ; lit
        inc     bx
        mov     dh, [bp]                ; unlit
        inc     bp
        mov     cl, 8
        cmp     byte ptr _gx_w, 16
        je      ch_big
ch_bit: mov     al, dh
        shl     ch, 1
        jnc     ch_w
        mov     al, dl
ch_w:   stosb
        dec     cl
        jnz     ch_bit
        jmp     short ch_next
ch_big: mov     al, dh
        shl     ch, 1
        jnc     ch_w2
        mov     al, dl
ch_w2:  stosb
        stosb
        dec     cl
        jnz     ch_big
ch_next:
        add     di, cs:ch_step
        dec     ah
        jnz     ch_line
        pop     bp
        ret
ch_step DW      0
gx_char_ ENDP

; gx_sprite: one sprite (_gx_w 16 or 32 wide (a multiple of 8), all of it on the screen,
; nothing else over it) straight onto the screen: each pixel the image's
; (at _gx_src, DGROUP, 0 clear) or else B's under it. At _gx_x, _gx_y for
; _gx_h lines.
SPPX    MACRO
        LOCAL   put
        SSPFX
        lodsb
        or      al, al
        jnz     put
        mov     al, [di]
put:    stosb
        ENDM
        PUBLIC  gx_sprite_
gx_sprite_ PROC NEAR
        push    bp
        push    ds
        cld
        mov     ax, VIDSEG
        mov     es, ax
        mov     bl, _gx_y
        xor     bh, bh
        shl     bx, 1
        mov     di, _gx_rowoff[bx]
        add     di, _gx_x
        mov     si, _gx_src
        mov     dx, 320
        sub     dx, _gx_w               ; a line's end to the next one's start
        mov     bp, _gx_w
        mov     cl, 3
        shr     bp, cl                  ; 8 pixels at a time (loop reaches 127 bytes)
        mov     al, _gx_h
        mov     cs:sp_cnt, al
        mov     ds, _gx_bseg            ; (DGROUP is SS from here)
sp_line:
        mov     cx, bp
sp_8:   SPPX
        SPPX
        SPPX
        SPPX
        SPPX
        SPPX
        SPPX
        SPPX
        loop    sp_8
        add     di, dx
        dec     byte ptr cs:sp_cnt
        jnz     sp_line
        pop     ds
        pop     bp
        ret
sp_cnt  DB      0
gx_sprite_ ENDP

; gx_compose: lines _gx_y.._gx_y+_gx_h-1, pixels _gx_x.._gx_x+_gx_w-1: B,
; with the _gx_njobs sprites' lines over it, put together in a line buffer
; (_gx_lbuf, in B's segment), then to the screen. For sprites that overlap,
; or are cut by the screen's edge; gx_sprite does the usual ones.
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
        add     si, _gx_x
        mov     loff, si
        mov     di, _gx_lbuf
        mov     cx, _gx_w
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
        mov     bp, OFFSET DGROUP:_gx_jobs      ; ([bp] reads SS)
cp_job:
        cmp     dh, [bp + J_Y0]
        jb      cp_next
        cmp     dh, [bp + J_Y1]
        jae     cp_next
        mov     di, ss:_gx_lbuf
        add     di, [bp + J_DST]
        mov     si, [bp + J_DATA]
        mov     ax, [bp + J_STRIDE]
        add     [bp + J_DATA], ax       ; (the next line's)
        mov     cx, [bp + J_N]
cp_px:  SSPFX
        lodsb                           ; the image's pixel
        or      al, al
        jz      cp_skip
        mov     [di], al
cp_skip:
        inc     di
        loop    cp_px
cp_next:
        add     bp, J_SIZE
        dec     dl
        jnz     cp_job
cp_out:
        mov     ax, VIDSEG              ; the buffer to the screen
        mov     es, ax
        mov     si, ss:_gx_lbuf
        mov     di, ss:loff
        mov     cx, ss:_gx_w
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

_TEXT   ENDS
        END

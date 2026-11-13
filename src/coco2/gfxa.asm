*
* gfxa.asm -- the CoCo 1/2's blitters (src/coco/gfx.c calls them with its
* arguments in globals: CMOC would push each one).
*
* The screen is PMODE 4, 32 bytes a line, 4 pixels (2 bits) a byte.
*
*   tile_put    16 lines of 2 bytes, tp_src to tp_dst
*   spr_draw    a bomber: 16 lines, sd_src's role and mask bytes (3 + 3 a
*               line) onto sd_dst, the roles coloured by sd_tab (256 bytes)
*   spr_draw_s  the same with two tables (sd_tab, then the 256 after it) on
*               alternate lines: the fourth seat's stripes
*
        section code

        import  _tp_src,_tp_dst
        import  _sd_src,_sd_dst,_sd_tab,_sd_buf
        export  _spr_save
        export  _spr_restore
        export  _tile_put
        export  _spr_draw
        export  _spr_draw_s

TL      macro
        ldd     ,x++
        std     ,u
        leau    32,u
        endm

_tile_put
        pshs    u,y
        ldx     _tp_src
        ldu     _tp_dst
        TL
        TL
        TL
        TL
        TL
        TL
        TL
        TL
        TL
        TL
        TL
        TL
        TL
        TL
        TL
        TL
        puls    y,u,pc

* one line of a bomber: X the source (3 roles, 3 masks), U the screen, Y the colours
SL      macro
        ldb     3,x
        andb    ,u
        stb     ,u
        ldb     ,x
        lda     b,y
        ora     ,u
        sta     ,u+
        ldb     4,x
        andb    ,u
        stb     ,u
        ldb     1,x
        lda     b,y
        ora     ,u
        sta     ,u+
        ldb     5,x
        andb    ,u
        stb     ,u
        ldb     2,x
        lda     b,y
        ora     ,u
        sta     ,u+
        leax    6,x
        leau    29,u
        endm

_spr_draw
        pshs    u,y
        ldx     _sd_src
        ldu     _sd_dst
        ldy     _sd_tab
        ldb     #16
        pshs    b
@line
        SL
        dec     ,s
        bne     @line
        leas    1,s
        puls    y,u,pc

_spr_draw_s
        pshs    u,y
        ldx     _sd_src
        ldu     _sd_dst
        ldy     _sd_tab
        ldb     #8
        pshs    b
@pair
        SL
        leay    256,y
        SL
        leay    -256,y
        dec     ,s
        bne     @pair
        leas    1,s
        puls    y,u,pc

* a bomber's 3 bytes of 16 lines: the screen at sd_dst to sd_buf, and back
_spr_save
        pshs    u,y
        ldx     _sd_dst
        ldu     _sd_buf
        ldy     #16
@l      ldd     ,x
        std     ,u++
        lda     2,x
        sta     ,u+
        leax    32,x
        leay    -1,y
        bne     @l
        puls    y,u,pc

_spr_restore
        pshs    u,y
        ldx     _sd_dst
        ldu     _sd_buf
        ldy     #16
@l      ldd     ,u++
        std     ,x
        lda     ,u+
        sta     2,x
        leax    32,x
        leay    -1,y
        bne     @l
        puls    y,u,pc

        endsect

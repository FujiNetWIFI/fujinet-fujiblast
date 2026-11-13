*
* gfx3a.asm -- the CoCo 3's blitters (src/coco3/gfx3.c calls them with its
* arguments in globals: CMOC would push each one).
*
* The screen is 320x192 in 16 colours, 160 bytes a line, 2 pixels (a nibble
* each) a byte, but the lines are 256 bytes apart (the GIME's horizontal
* virtual screen), so an 8K block of the MMU holds 32 lines and the line
* after the one at U is at U + 256. The caller has the right block in the
* window and keeps within it.
*
*   tile_put3   tp_n lines of 8 bytes, tp_src (contiguous) to tp_dst
*   spr_draw3   a bomber: sd_n lines of 8 role bytes and 8 mask bytes from
*               sd_src onto sd_dst, the roles coloured by sd_tab (256 bytes)
*
        section code

        import  _tp_src,_tp_dst,_tp_n
        import  _sd_src,_sd_dst,_sd_tab,_sd_n,_sd_buf
        export  _tile_put3
        export  _spr_draw3
        export  _spr_save3
        export  _spr_restore3

_tile_put3
        pshs    u
        ldx     _tp_src
        ldu     _tp_dst
        ldb     _tp_n
        pshs    b
@line
        ldd     ,x++
        std     ,u
        ldd     ,x++
        std     2,u
        ldd     ,x++
        std     4,u
        ldd     ,x++
        std     6,u
        leau    256,u
        dec     ,s
        bne     @line
        leas    1,s
        puls    u,pc

* one byte of a bomber line: X the source, U the screen, Y the colours
SB      macro
        ldb     8+\1,x
        andb    \1,u
        stb     \1,u
        ldb     \1,x
        lda     b,y
        ora     \1,u
        sta     \1,u
        endm

_spr_draw3
        pshs    u,y
        ldx     _sd_src
        ldu     _sd_dst
        ldy     _sd_tab
        ldb     _sd_n
        pshs    b
@line
        SB      0
        SB      1
        SB      2
        SB      3
        SB      4
        SB      5
        SB      6
        SB      7
        leax    16,x
        leau    256,u
        dec     ,s
        bne     @line
        leas    1,s
        puls    y,u,pc

* sd_n lines of 8 bytes: the screen at sd_dst to sd_buf (contiguous), and back
_spr_save3
        pshs    u,y
        ldx     _sd_dst
        ldu     _sd_buf
        ldb     _sd_n
        pshs    b
@l      ldd     ,x
        std     ,u
        ldd     2,x
        std     2,u
        ldd     4,x
        std     4,u
        ldd     6,x
        std     6,u
        leau    8,u
        leax    256,x
        dec     ,s
        bne     @l
        leas    1,s
        puls    y,u,pc

_spr_restore3
        pshs    u,y
        ldx     _sd_dst
        ldu     _sd_buf
        ldb     _sd_n
        pshs    b
@l      ldd     ,u
        std     ,x
        ldd     2,u
        std     2,x
        ldd     4,u
        std     4,x
        ldd     6,u
        std     6,x
        leau    8,u
        leax    256,x
        dec     ,s
        bne     @l
        leas    1,s
        puls    y,u,pc

        endsect

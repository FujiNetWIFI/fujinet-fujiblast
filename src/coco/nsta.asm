*
* nsta.asm -- the CoCo link's frame decoder (src/coco/nstrans.c's nst_frame),
* in assembly: the ring's bytes up to a frame's end, the COBS undone as they
* come, then the Fletcher-16 sum checked. A 6809 at CMOC's -O1 spent a fifth
* of its time here.
*
*   uint8_t nst_scan(void)  the frame's length, or 255 (bad), or 0 (the ring
*                           ran out first). The state is the C's it replaced:
*                           nst_ring, nst_rhead, nst_rtail, nst_taken, nst_buf,
*                           and the frame being decoded: nst_dlen (bytes in
*                           nst_buf), nst_drem (data bytes still to come in
*                           this COBS block), nst_dzero (a zero follows the
*                           block), nst_dbad.
*
        section code

        import  _nst_ring,_nst_rhead,_nst_rtail,_nst_taken
        import  _nst_buf,_nst_dlen,_nst_drem,_nst_dzero,_nst_dbad
        export  _nst_scan

FMAX    equ     136

* The frame's end: its length in B, or 255. Keeps Y; uses U.
frame_end
        tst     _nst_dbad
        bne     @bad
        tst     _nst_drem
        bne     @bad
        ldb     _nst_dlen
        cmpb    #3
        blo     @bad
        subb    #2              B = o, the bytes the sum covers
        clra
        tfr     d,u             U counts them
        ldx     #_nst_buf
        clra                    A = s1
        clrb                    B = s2
@fl     adda    ,x+             s1 += byte, mod 255
        bcc     @n1
        inca
@n1     cmpa    #255
        bne     @n2
        clra
@n2     pshs    a
        addb    ,s+             s2 += s1, mod 255
        bcc     @n3
        incb
@n3     cmpb    #255
        bne     @n4
        clrb
@n4     leau    -1,u
        cmpu    #0
        bne     @fl
        cmpa    ,x+             X is at nst_buf + o
        bne     @bad
        cmpb    ,x
        bne     @bad
        ldb     _nst_dlen
        subb    #2
        rts
@bad    ldb     #255
        rts

_nst_scan
        pshs    y,u
        ldy     #_nst_ring
@next   ldb     _nst_rtail
        cmpb    _nst_rhead
        beq     @none
        clra
        lda     d,y             c
        incb
        stb     _nst_rtail
        inc     _nst_taken
        tsta
        bne     @nz
* a zero: the frame's end (an empty one is nothing)
        lda     _nst_dlen
        ora     _nst_drem
        ora     _nst_dbad
        beq     @clr0
        bsr     frame_end
        bra     @clr
@clr0   clrb
@clr    clr     _nst_dlen
        clr     _nst_drem
        clr     _nst_dzero
        clr     _nst_dbad
        tstb
        beq     @next
        puls    y,u,pc
@nz     ldb     _nst_dlen
        cmpb    #FMAX-1
        blo     @room
        lda     #1              too long: wait for the next 0
        sta     _nst_dbad
        bra     @next
@room   tst     _nst_drem
        bne     @data
* a code byte c: c-1 bytes follow, then a 0 (unless c is $FF)
        tst     _nst_dzero
        beq     @nz0
        ldx     #_nst_buf
        abx
        clr     ,x
        inc     _nst_dlen
@nz0    deca                    A = c-1
        sta     _nst_drem
        ldb     #1
        cmpa    #$FE            c < $FF
        blo     @dz
        clrb
@dz     stb     _nst_dzero
        bra     @next
@data   ldx     #_nst_buf
        abx
        sta     ,x
        inc     _nst_dlen
        dec     _nst_drem
        bra     @next
@none   clrb
        puls    y,u,pc

        endsect

;
; dhgr.s -- the double hi-res blitter and the 60 Hz clock, in the language
; card ($D400, bank 2: src/apple2/fujiblst.cfg), where RAMRD and RAMWRT
; don't reach, so the code runs while either bank is switched in.
;
; A line is 40 aux bytes and 40 main bytes; a group (7 colour pixels) is
; aux 2g, main 2g, aux 2g+1, main 2g+1 -- bytes k = 0..3 of the group. The
; art and the tables are in aux RAM (tools/mkgfx_apple2.py), read with RAMRD
; on; whatever the main-bank half of a draw needs is in zero page or here.
; Every routine leaves both banks on main, and calls frame_tick as it goes
; (the //e's blank is polled: no more than ~4 ms between looks).
;
; Parameters are in zero page (gfx.c sets them):
;   dh_page   $20 or $40: the page drawn on
;   dh_y      the first line          dh_n   lines
;   dh_g      the first group (dh_lines: byte column)
;   dh_g1     the last group (dh_frags, dh_fill; dh_lines: byte column)
;   dh_src    aux address of the source
;   dh_x      a sprite's pixel x       dh_big  1: twice the size
;   dh_bg     dh_frags: the background colour; dh_fill: the colour
;   dh_tb, dh_ts, dh_tv  a sprite's colour tables (ax_ctab entries)

        .exportzp       _dh_page, _dh_y, _dh_n, _dh_g, _dh_g1, _dh_src, _dh_x
        .exportzp       _dh_big, _dh_bg, _dh_tb, _dh_ts, _dh_tv
        .exportzp       _flip, _vbl_mode, _btn_latch, src2, shp
        .export         _frame_tick, _dh_tile, _dh_lines, _dh_frags, _dh_fill
        .export         _dh_sprite, _dh_mover
        .export         _rowlo, _rowhi
        .import         _frame_count, _colpat, _fragbuf, _fgbuf
        .import         _ax_exp, _ax_nexp, _ax_shl

RAMRDOFF  := $C002
RAMRDON   := $C003
RAMWRTOFF := $C004
RAMWRTON  := $C005
RDVBLBAR  := $C019
PAGE1     := $C054
PAGE2     := $C055
BUTN0     := $C061
BUTN1     := $C062
PTRIG     := $C070

        .zeropage
_dh_page: .res 1
_dh_y:    .res 1
_dh_n:    .res 1
_dh_g:    .res 1
_dh_g1:   .res 1
_dh_src:  .res 2
_dh_x:    .res 1
_dh_big:  .res 1
_dh_bg:   .res 1
_dh_tb:   .res 2
_dh_ts:   .res 2
_dh_tv:   .res 2
_flip:    .res 1        ; a page to show at the next blank (1 or 2), 0 none
_vbl_mode: .res 1       ; 0 //e, 1 IIgs, 2 //c
_btn_latch: .res 1      ; bit 7: a button went down (input.c clears it)
last:     .res 1
scr:      .res 2
src2:     .res 2
shp:      .res 2        ; the shift's tables: low at shp, high at shp2
shp2:     .res 2
tb2:      .res 2        ; tb, ts, tv + 256 (bytes 2, 3)
ts2:      .res 2
tv2:      .res 2
col:      .res 1
t1:       .res 1
t2:       .res 1
ng:       .res 1        ; groups a sprite covers
fm:       .res 3        ; a line's fragments by group: mask, body, shade, visor
fb:       .res 3
fs:       .res 3
fv:       .res 3
cm:       .res 1        ; one group's
cb:       .res 1
cs:       .res 1
cv:       .res 1
cb1:      .res 1        ; the same + 128 (bytes 1 and 3)
cs1:      .res 1
cv1:      .res 1
nm:       .res 12       ; the bytes to keep, by group and byte
co:       .res 12       ; the bytes to set

        .segment "LC"

; ---------------------------------------------------------------------------
; The 60 Hz clock: frame_count goes up as each vertical blank begins. A page
; waiting in flip is shown then. The buttons are latched each time.
;   //e   RDVBLBAR bit 7 is 0 in the blank       IIgs  the same, inverted
;   //c   the VBL flag (vbl_init enables it), reset by reading PTRIG

_frame_tick:
        lda     _vbl_mode
        cmp     #2
        beq     iic
        lda     RDVBLBAR
        ldx     _vbl_mode
        beq     :+
        eor     #$80
:       and     #$80            ; $00 in the blank
        cmp     last
        beq     buttons
        sta     last
        tax
        bne     buttons         ; drawing began
blank:  inc     _frame_count
        bne     :+
        inc     _frame_count+1
:       lda     _flip
        beq     buttons
        cmp     #2
        beq     :+
        sta     PAGE1
        jmp     shown
:       sta     PAGE2
shown:  lda     #0
        sta     _flip
buttons:
        lda     BUTN0
        ora     BUTN1
        and     #$80
        ora     _btn_latch
        sta     _btn_latch
        rts
iic:    bit     RDVBLBAR
        bpl     buttons
        bit     PTRIG
        jmp     blank

; scr = line dh_y's address on the page drawn on, + A (a byte column)
setscr:
        ldx     _dh_y
        clc
        adc     _rowlo,x
        sta     scr
        lda     _rowhi,x
        ora     _dh_page
        sta     scr+1
        rts

; ---------------------------------------------------------------------------
; dh_tile: dh_n lines of a tile (4 bytes a line at aux dh_src) at group dh_g
; from line dh_y.
_dh_tile:
        sta     RAMRDON
@line:  lda     _dh_g
        asl     a
        jsr     setscr
        ldy     #0
        sta     RAMWRTON
        lda     (_dh_src),y     ; aux 2g
        sta     (scr),y
        ldy     #2
        lda     (_dh_src),y     ; aux 2g+1
        ldy     #1
        sta     (scr),y
        sta     RAMWRTOFF
        lda     (_dh_src),y     ; main 2g
        dey
        sta     (scr),y
        ldy     #3
        lda     (_dh_src),y     ; main 2g+1
        ldy     #1
        sta     (scr),y
        lda     _dh_src
        clc
        adc     #4
        sta     _dh_src
        bcc     :+
        inc     _dh_src+1
:       inc     _dh_y
        dec     _dh_n
        bne     @line
        sta     RAMRDOFF
        jmp     _frame_tick

; ---------------------------------------------------------------------------
; dh_lines: dh_n whole lines from aux dh_src (each 40 aux bytes, 40 main),
; byte columns dh_g..dh_g1, from line dh_y.
_dh_lines:
        sta     RAMRDON
@line:  lda     _dh_src
        clc
        adc     #40
        sta     src2
        lda     _dh_src+1
        adc     #0
        sta     src2+1
        lda     #0
        jsr     setscr
        sta     RAMWRTON
        ldy     _dh_g
:       lda     (_dh_src),y
        sta     (scr),y
        cpy     _dh_g1
        iny
        bcc     :-
        sta     RAMWRTOFF
        ldy     _dh_g
:       lda     (src2),y
        sta     (scr),y
        cpy     _dh_g1
        iny
        bcc     :-
        lda     _dh_src
        clc
        adc     #80
        sta     _dh_src
        bcc     :+
        inc     _dh_src+1
:       inc     _dh_y
        sta     RAMRDOFF
        jsr     _frame_tick
        sta     RAMRDON
        dec     _dh_n
        bne     @line
        sta     RAMRDOFF
        rts

; ---------------------------------------------------------------------------
; dh_fill: dh_n lines from dh_y, groups dh_g..dh_g1, all colour dh_bg.
_dh_fill:
        lda     _dh_bg
        asl     a
        asl     a
        tax
        lda     _colpat,x
        sta     nm
        lda     _colpat+1,x
        sta     nm+1
        lda     _colpat+2,x
        sta     nm+2
        lda     _colpat+3,x
        sta     nm+3
@line:  lda     #0
        jsr     setscr
        lda     _dh_g
        asl     a
        tay
        sta     RAMWRTON
:       lda     nm
        sta     (scr),y
        iny
        lda     nm+2
        sta     (scr),y
        iny
        tya
        lsr     a
        cmp     _dh_g1
        beq     :-
        bcc     :-
        lda     _dh_g
        asl     a
        tay
        sta     RAMWRTOFF
:       lda     nm+1
        sta     (scr),y
        iny
        lda     nm+3
        sta     (scr),y
        iny
        tya
        lsr     a
        cmp     _dh_g1
        beq     :-
        bcc     :-
        inc     _dh_y
        jsr     _frame_tick
        dec     _dh_n
        bne     @line
        rts

; ---------------------------------------------------------------------------
; dh_frags: line dh_y, groups dh_g..dh_g1: each group's fragbuf pixels in
; its fgbuf colour, the rest in dh_bg.
_dh_frags:
        lda     #0
        jsr     setscr
        lda     _dh_bg
        asl     a
        asl     a
        tax
        lda     _colpat,x
        sta     nm+4
        lda     _colpat+1,x
        sta     nm+5
        lda     _colpat+2,x
        sta     nm+6
        lda     _colpat+3,x
        sta     nm+7
        ldx     _dh_g
@group: stx     ng
        sta     RAMRDOFF        ; fgbuf and fragbuf are in main RAM
        lda     _fgbuf,x
        asl     a
        asl     a
        tay
        lda     _colpat,y       ; the foreground, bytes 0..3
        sta     nm
        lda     _colpat+1,y
        sta     nm+1
        lda     _colpat+2,y
        sta     nm+2
        lda     _colpat+3,y
        sta     nm+3
        txa
        asl     a
        tay                     ; byte column 2g
        lda     _fragbuf,x
        tax                     ; the fragment
        sta     RAMRDON         ; the expansion tables
        sta     RAMWRTON
        lda     _ax_exp+0,x     ; byte 0: aux 2g
        and     nm
        sta     col
        lda     _ax_nexp+0,x
        and     nm+4
        ora     col
        sta     (scr),y
        iny
        lda     _ax_exp+256,x   ; byte 2: aux 2g+1
        and     nm+2
        sta     col
        lda     _ax_nexp+256,x
        and     nm+6
        ora     col
        sta     (scr),y
        dey
        sta     RAMWRTOFF
        lda     _ax_exp+128,x   ; byte 1: main 2g
        and     nm+1
        sta     col
        lda     _ax_nexp+128,x
        and     nm+5
        ora     col
        sta     (scr),y
        iny
        lda     _ax_exp+384,x   ; byte 3: main 2g+1
        and     nm+3
        sta     col
        lda     _ax_nexp+384,x
        and     nm+7
        ora     col
        sta     (scr),y
        ldx     ng
        cpx     _dh_g1
        inx
        bcc     @group
        sta     RAMRDOFF
        jmp     _frame_tick

; ---------------------------------------------------------------------------
; dh_sprite: a sprite at pixel dh_x, line dh_y: dh_n source lines from aux
; dh_src (mask, body, shade, visor: a byte each, or two if dh_big, when each
; line is drawn twice too), in the colour tables dh_tb, dh_ts, dh_tv.
_dh_sprite:
        lda     _dh_x           ; group and shift
        ldx     #0
:       cmp     #7
        bcc     :+
        sbc     #7
        inx
        bne     :-
:       stx     _dh_g
        clc
        adc     #>_ax_shl
        sta     shp+1
        sta     shp2+1
        lda     #<_ax_shl
        sta     shp
        clc
        adc     #$80
        sta     shp2
        bcc     :+
        inc     shp2+1
:
        lda     _dh_big         ; groups: 2, 3 if big; none past 19
        clc
        adc     #2
        sta     ng
        lda     #20
        sec
        sbc     _dh_g
        cmp     ng
        bcs     :+
        sta     ng
:       lda     ng
        asl     a
        asl     a
        sta     ng
        lda     _dh_tb
        sta     tb2
        lda     _dh_tb+1
        clc
        adc     #1
        sta     tb2+1
        lda     _dh_ts
        sta     ts2
        lda     _dh_ts+1
        clc
        adc     #1
        sta     ts2+1
        lda     _dh_tv
        sta     tv2
        lda     _dh_tv+1
        clc
        adc     #1
        sta     tv2+1
@line:  lda     _dh_y
        cmp     #192
        bcc     :+
        jmp     @done
:       sta     RAMRDON
        lda     _dh_big
        bne     @big
        ldy     #0              ; a plane a byte: its low and high fragments
        lda     (_dh_src),y
        tay
        lda     (shp),y
        sta     fm
        lda     (shp2),y
        sta     fm+1
        ldy     #1
        lda     (_dh_src),y
        tay
        lda     (shp),y
        sta     fb
        lda     (shp2),y
        sta     fb+1
        ldy     #2
        lda     (_dh_src),y
        tay
        lda     (shp),y
        sta     fs
        lda     (shp2),y
        sta     fs+1
        ldy     #3
        lda     (_dh_src),y
        tay
        lda     (shp),y
        sta     fv
        lda     (shp2),y
        sta     fv+1
        jmp     @bytes
@big:   ldx     #0              ; two halves a plane: lo(A), hi(A)|lo(B), hi(B)
        stx     t1
@pl2:   ldy     t1
        lda     (_dh_src),y     ; A
        tay
        lda     (shp),y
        sta     fm,x
        lda     (shp2),y
        sta     fm+1,x
        inc     t1
        ldy     t1
        lda     (_dh_src),y     ; B
        tay
        lda     (shp),y
        ora     fm+1,x
        sta     fm+1,x
        lda     (shp2),y
        sta     fm+2,x
        inc     t1
        inx
        inx
        inx
        cpx     #12
        bcc     @pl2
@bytes: ldx     #0              ; 4j + k
        stx     t1              ; j
@grp:   ldy     t1
        lda     fm,y
        bne     @some
        lda     #$7F            ; nothing in this group on this line
        sta     nm,x
        sta     nm+1,x
        sta     nm+2,x
        sta     nm+3,x
        jmp     @next
@some:  sta     cm
        lda     fb,y
        sta     cb
        ora     #$80
        sta     cb1
        lda     fs,y
        sta     cs
        ora     #$80
        sta     cs1
        lda     fv,y
        sta     cv
        ora     #$80
        sta     cv1
        ldy     cm              ; byte 0 (an untouched byte: no colour)
        lda     _ax_nexp+0,y
        sta     nm,x
        cmp     #$7F
        beq     @b1
        ldy     cb
        lda     (_dh_tb),y
        ldy     cs
        ora     (_dh_ts),y
        ldy     cv
        beq     :+
        ora     (_dh_tv),y
:       sta     co,x
@b1:    ldy     cm              ; byte 1
        lda     _ax_nexp+128,y
        sta     nm+1,x
        cmp     #$7F
        beq     @b2
        ldy     cb1
        lda     (_dh_tb),y
        ldy     cs1
        ora     (_dh_ts),y
        ldy     cv
        beq     :+
        ldy     cv1
        ora     (_dh_tv),y
:       sta     co+1,x
@b2:    ldy     cm              ; byte 2
        lda     _ax_nexp+256,y
        sta     nm+2,x
        cmp     #$7F
        beq     @b3
        ldy     cb
        lda     (tb2),y
        ldy     cs
        ora     (ts2),y
        ldy     cv
        beq     :+
        ora     (tv2),y
:       sta     co+2,x
@b3:    ldy     cm              ; byte 3
        lda     _ax_nexp+384,y
        sta     nm+3,x
        cmp     #$7F
        beq     @next
        ldy     cb1
        lda     (tb2),y
        ldy     cs1
        ora     (ts2),y
        ldy     cv
        beq     :+
        ldy     cv1
        ora     (tv2),y
:       sta     co+3,x
@next:  inc     t1
        txa
        clc
        adc     #4
        tax
        cpx     ng
        bcs     :+
        jmp     @grp
:       jsr     rmw
        lda     _dh_big
        beq     :+
        inc     _dh_y
        lda     _dh_y
        cmp     #192
        bcs     @done
        sta     RAMRDON
        jsr     rmw
:       inc     _dh_y
        lda     _dh_src
        clc
        adc     #4
        ldx     _dh_big
        beq     :+
        adc     #4
:       sta     _dh_src
        bcc     :+
        inc     _dh_src+1
:       jsr     _frame_tick
        dec     _dh_n
        beq     @done
        jmp     @line
@done:  rts

; Line dh_y from group dh_g, ng/4 groups: keep nm, set co. Enter with RAMRD
; on; leaves both banks on main.
rmw:    lda     _dh_g
        asl     a
        jsr     setscr
        sta     RAMWRTON
        ldx     #0
        ldy     #0
@a:     lda     nm,x            ; aux 2j
        cmp     #$7F
        beq     :+
        lda     (scr),y
        and     nm,x
        ora     co,x
        sta     (scr),y
:       iny
        lda     nm+2,x          ; aux 2j+1
        cmp     #$7F
        beq     :+
        lda     (scr),y
        and     nm+2,x
        ora     co+2,x
        sta     (scr),y
:       iny
        txa
        clc
        adc     #4
        tax
        cpx     ng
        bcc     @a
        sta     RAMRDOFF
        sta     RAMWRTOFF
        ldx     #0
        ldy     #0
@m:     lda     nm+1,x          ; main 2j
        cmp     #$7F
        beq     :+
        lda     (scr),y
        and     nm+1,x
        ora     co+1,x
        sta     (scr),y
:       iny
        lda     nm+3,x          ; main 2j+1
        cmp     #$7F
        beq     :+
        lda     (scr),y
        and     nm+3,x
        ora     co+3,x
        sta     (scr),y
:       iny
        txa
        clc
        adc     #4
        tax
        cpx     ng
        bcc     @m
        rts

; ---------------------------------------------------------------------------
; dh_mover: a bomb's image (aux dh_src: 16 lines of 8 bytes, each data then
; keep) at group dh_g, line dh_y.
_dh_mover:
        lda     #8
        sta     ng
        lda     #16
        sta     _dh_n
@line:  lda     _dh_y
        cmp     #192
        bcs     @done
        sta     RAMRDON
        ldy     #0
        ldx     #0
:       lda     (_dh_src),y
        sta     co,x
        iny
        lda     (_dh_src),y
        sta     nm,x
        iny
        inx
        cpx     #8
        bcc     :-
        jsr     rmw
        inc     _dh_y
        lda     _dh_src
        clc
        adc     #16
        sta     _dh_src
        bcc     :+
        inc     _dh_src+1
:       jsr     _frame_tick
        dec     _dh_n
        bne     @line
@done:  rts

; ---------------------------------------------------------------------------
; The lines' addresses within a page.
_rowlo:
        .repeat 192, LN
        .byte   <((LN .mod 8) * $400 + ((LN / 8) .mod 8) * $80 + (LN / 64) * $28)
        .endrepeat
_rowhi:
        .repeat 192, LN
        .byte   >((LN .mod 8) * $400 + ((LN / 8) .mod 8) * $80 + (LN / 64) * $28)
        .endrepeat

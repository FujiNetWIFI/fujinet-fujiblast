;
; hgr.s -- the high-res blitter and the 60 Hz clock, in the language card
; ($D400, bank 2: src/apple2/fujiblst.cfg), where RAMRD doesn't reach, so the
; code runs while it reads aux.
;
; A line is 40 bytes in main RAM; a colour pixel is two dots, so a group (7
; colour pixels, a tile's width) is bytes 2g and 2g+1. The art and its
; tables are in aux RAM (tools/mkgfx_hgr.py), read with RAMRD on while the
; screen is written (RAMWRT stays off); a sprite's line is copied to zero
; page first, since its merge reads the screen. Every routine leaves RAMRD
; off, and calls frame_tick as it goes (the //e's blank is polled: no more
; than ~4 ms between looks) -- always with RAMRD off: the clock is in main.
;
; Parameters are in zero page (gfx.c sets them):
;   hg_page   $20 or $40: the page drawn on
;   hg_y      the first line          hg_n   lines
;   hg_g      the first group (hg_lines: byte column)
;   hg_g1     the last group (hg_frags, hg_fill; hg_lines: byte column);
;             hg_bomber, hg_big: the frame; hg_mover: the image
;   hg_src    aux address of the source
;   hg_x      hg_bomber: the pixel x
;   hg_bg     hg_frags: the background colour; hg_fill: the colour;
;             hg_bomber, hg_big: the seat

        .exportzp       _hg_page, _hg_y, _hg_n, _hg_g, _hg_g1, _hg_src, _hg_x, _hg_bg
        .exportzp       _flip, _vbl_mode, _btn_latch, src2, shp
        .exportzp       _hg_c0, _hg_nb, _er_c, _er_n, _er_y0, _er_y1
        .export         _frame_tick, _hg_tile, _hg_lines, _hg_frags, _hg_fill
        .export         _hg_bomber, _hg_mover, _hg_big, _erase_box
        .export         _hg_bomber_c, _hg_mover_c, _px_box
        .import         _tile_want, _tile_first, _tile_n, _tm_phase, _ax_tiles
        .export         _rowlo, _rowhi
        .import         _frame_count, _colpat, _fexp0, _fexp1, _fragbuf, _fgbuf
        .import         _ax_bspr, _ax_bkeep, _ax_movd, _ax_movk, _ax_big, _ax_bigk

RAMRDOFF  := $C002
RAMRDON   := $C003
RDVBLBAR  := $C019
PAGE1     := $C054
PAGE2     := $C055
BUTN0     := $C061
BUTN1     := $C062
PTRIG     := $C070

        .zeropage
_hg_page: .res 1
_hg_y:    .res 1
_hg_n:    .res 1
_hg_g:    .res 1
_hg_g1:   .res 1
_hg_src:  .res 2
_hg_x:    .res 1
_hg_bg:   .res 1
_flip:    .res 1        ; a page to show at the next blank (1 or 2), 0 none
_vbl_mode: .res 1       ; 0 //e, 1 IIgs, 2 //c
_btn_latch: .res 1      ; bit 7: a button went down (input.c clears it)
last:     .res 1
scr:      .res 2
src2:     .res 2        ; (gfxa.s's, between our calls)
shp:      .res 2
bdp:      .res 2        ; a sprite's data and keep bytes (aux)
bkp:      .res 2
dd:       .res 4        ; a sprite line's data, keep
kk:       .res 4
col0:     .res 1        ; its first byte column
_hg_c0 = col0           ; (gfx.c keeps it, and how many bytes, to erase later)
_hg_nb:   .res 1
_er_c:    .res 1        ; erase_box: byte columns er_c.., er_n of them
_er_n:    .res 1
_er_y0:   .res 1        ; lines er_y0..er_y1
_er_y1:   .res 1
pp:       .res 6        ; its source pointers, a byte column each
sly:      .res 1        ; comp: the offset into a sprite's art
left:     .res 1        ; erase_box, comp: lines to go
cnt:      .res 1        ; and in this tile row
cnt0:     .res 1
w1:       .res 1        ; PUT3's offsets
w2:       .res 1
ly:       .res 1        ; its offset into the art
t1:       .res 1
t2:       .res 1

        .segment "LC"

; ---------------------------------------------------------------------------
; The 60 Hz clock: frame_count goes up as each vertical blank begins. A page
; waiting in flip is shown then. The buttons are latched at each edge.
;   //e   RDVBLBAR bit 7 is 0 in the blank       IIgs  the same, inverted
;   //c   the VBL flag (vbl_init enables it), reset by reading PTRIG
; Keeps Y.

_frame_tick:
        lda     _vbl_mode
        bne     others
        lda     RDVBLBAR        ; the //e, the usual case: no edge, out at once
        and     #$80
        cmp     last
        bne     :+
        rts
:       sta     last
        tax
        bne     buttons         ; drawing began
        jmp     blank
others: cmp     #2
        beq     iic
        lda     RDVBLBAR
        eor     #$80            ; (the IIgs)
        and     #$80            ; $00 in the blank
        cmp     last
        beq     buttons
        sta     last
        tax
        bne     buttons
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

; scr = line hg_y's address on the page drawn on, + A (a byte column)
setscr:
        ldx     _hg_y
        clc
        adc     _rowlo,x        ; (never carries: a line's low byte + 39 < 256)
        sta     scr
        lda     _rowhi,x
        ora     _hg_page
        sta     scr+1
        rts

; ---------------------------------------------------------------------------
; hg_tile: hg_n lines of a tile (2 bytes a line at aux hg_src) at group hg_g
; from line hg_y.
_hg_tile:
        lda     _hg_g
        asl     a
        sta     col0
        lda     #0
        sta     ly
        sta     RAMRDON
@line:  ldx     _hg_y
        lda     _rowlo,x
        clc
        adc     col0
        sta     scr
        lda     _rowhi,x
        ora     _hg_page
        sta     scr+1
        ldy     ly
        lda     (_hg_src),y
        tax
        iny
        lda     (_hg_src),y
        iny
        sty     ly
        ldy     #1
        sta     (scr),y
        txa
        dey
        sta     (scr),y
        inc     _hg_y
        dec     _hg_n
        bne     @line
        sta     RAMRDOFF
        jmp     _frame_tick

; ---------------------------------------------------------------------------
; hg_lines: hg_n whole lines from aux hg_src (40 bytes each), byte columns
; hg_g..hg_g1, from line hg_y.
_hg_lines:
@line:  lda     #0
        jsr     setscr
        sta     RAMRDON
        ldy     _hg_g
:       lda     (_hg_src),y
        sta     (scr),y
        cpy     _hg_g1
        iny
        bcc     :-
        sta     RAMRDOFF
        lda     _hg_src
        clc
        adc     #40
        sta     _hg_src
        bcc     :+
        inc     _hg_src+1
:       inc     _hg_y
        jsr     _frame_tick
        dec     _hg_n
        bne     @line
        rts

; ---------------------------------------------------------------------------
; hg_fill: hg_n lines from hg_y, groups hg_g..hg_g1, all colour hg_bg.
_hg_fill:
        lda     _hg_bg
        asl     a
        tax
        lda     _colpat,x
        sta     dd
        lda     _colpat+1,x
        sta     dd+1
@line:  lda     #0
        jsr     setscr
        lda     _hg_g
        asl     a
        tay
:       lda     dd
        sta     (scr),y
        iny
        lda     dd+1
        sta     (scr),y
        iny
        tya
        lsr     a
        cmp     _hg_g1
        beq     :-
        bcc     :-
        inc     _hg_y
        jsr     _frame_tick
        dec     _hg_n
        bne     @line
        rts

; ---------------------------------------------------------------------------
; hg_frags: line hg_y, groups hg_g..hg_g1: each group's fragbuf pixels in
; its fgbuf colour, the rest in hg_bg. A byte with any of the pixels in it
; takes the pixels' palette.
_hg_frags:
        lda     #0
        jsr     setscr
        lda     _hg_bg
        asl     a
        tax
        lda     _colpat,x       ; the background's 2 bytes
        sta     kk
        lda     _colpat+1,x
        sta     kk+1
        ldx     _hg_g
@group: stx     ly
        lda     _fgbuf,x
        asl     a
        tay
        lda     _colpat,y       ; the foreground's
        sta     dd
        lda     _colpat+1,y
        sta     dd+1
        lda     ly
        asl     a
        tay                     ; byte column 2g
        lda     _fragbuf,x
        tax                     ; the fragment
        lda     _fexp0,x
        beq     @bg0
        sta     t1
        eor     #$7F
        and     kk
        and     #$7F
        sta     t2
        lda     t1
        ora     #$80
        and     dd
        ora     t2
        jmp     :+
@bg0:   lda     kk
:       sta     (scr),y
        iny
        lda     _fexp1,x
        beq     @bg1
        sta     t1
        eor     #$7F
        and     kk+1
        and     #$7F
        sta     t2
        lda     t1
        ora     #$80
        and     dd+1
        ora     t2
        jmp     :+
@bg1:   lda     kk+1
:       sta     (scr),y
        ldx     ly
        cpx     _hg_g1
        inx
        bcc     @group
        jmp     _frame_tick

; ---------------------------------------------------------------------------
; Sprites: 3 bytes a line from byte column col0, screen = screen and keep,
; or data; the data at aux bdp, the keep bytes at aux bkp, hg_n lines from
; line hg_y (none past 191).
draw3:
        lda     #0
        sta     ly
@line:  ldx     _hg_y
        cpx     #192
        bcs     @done
        sta     RAMRDON
        ldy     ly
        lda     (bdp),y
        sta     dd
        lda     (bkp),y
        sta     kk
        iny
        lda     (bdp),y
        sta     dd+1
        lda     (bkp),y
        sta     kk+1
        iny
        lda     (bdp),y
        sta     dd+2
        lda     (bkp),y
        sta     kk+2
        iny
        sty     ly
        sta     RAMRDOFF
        lda     _rowlo,x
        clc
        adc     col0
        sta     scr
        lda     _rowhi,x
        ora     _hg_page
        sta     scr+1
        ldy     #0
        lda     (scr),y
        and     kk
        ora     dd
        sta     (scr),y
        iny
        lda     (scr),y
        and     kk+1
        ora     dd+1
        sta     (scr),y
        iny
        lda     (scr),y
        and     kk+2
        ora     dd+2
        sta     (scr),y
        inc     _hg_y
        dec     _hg_n
        bne     @line           ; (16 lines: ~2.5K cycles, inside the blank's 4.5K)
@done:  jmp     _frame_tick

; (t2:t1) *= 48
times48:
        lda     t1
        asl     a
        rol     t2
        asl     a
        rol     t2
        asl     a
        rol     t2
        asl     a
        rol     t2              ; x 16
        sta     t1
        asl     a
        sta     dd
        lda     t2
        rol     a
        sta     dd+1            ; x 32
        lda     dd
        clc
        adc     t1
        sta     t1
        lda     dd+1
        adc     t2
        sta     t2              ; x 48
        rts

; ---------------------------------------------------------------------------
; hg_bomber: a bomber, 16 lines, at pixel hg_x (below 133) from line hg_y:
; frame hg_g1, seat hg_bg. Its art comes shifted and coloured (ax_bspr: by
; seat, frame and shift; ax_bkeep: by frame and shift), 3 bytes a line from
; byte 2g, or 2g+1 for shifts 4-6.
_hg_bomber:
        jsr     bsetup
draw16: lda     #16
        sta     _hg_n
        jmp     draw3
_hg_bomber_c:                   ; the same, over the tiles (comp)
        jsr     bsetup
        jmp     compbox
bsetup: ldy     _hg_x           ; group and shift (tables: no dividing)
        lda     _px_box,y
        tax
        and     #$7F
        sta     col0            ; 2g, or 2g+1
        txa
        asl     a               ; C: 3 bytes
        lda     #2
        adc     #0
        sta     _hg_nb
        lda     px_g,y
        sta     _hg_g
        ldx     _hg_g1          ; the block: frame * 7 + shift, x 48
        lda     mul7,x
        clc
        adc     px_s,y
        tay
        lda     blk48lo,y
        sta     t1
        lda     blk48hi,y
        sta     t2
        lda     t1              ; keep: ax_bkeep + block
        clc
        adc     #<_ax_bkeep
        sta     bkp
        lda     t2
        adc     #>_ax_bkeep
        sta     bkp+1
        ldx     _hg_bg          ; data: ax_bspr + seat x 6048 + block
        lda     t1
        clc
        adc     seat48lo,x
        sta     bdp
        lda     t2
        adc     seat48hi,x
        sta     bdp+1
        lda     bdp
        clc
        adc     #<_ax_bspr
        sta     bdp
        lda     bdp+1
        adc     #>_ax_bspr
        sta     bdp+1
        rts

; bsetup's tables (main memory: read with RAMRD off)
        .rodata
px_g:
        .repeat 140, PX
        .byte   PX / 7
        .endrepeat
px_s:
        .repeat 140, PX
        .byte   PX .mod 7
        .endrepeat
mul7:
        .repeat 18, F
        .byte   F * 7
        .endrepeat
blk48lo:
        .repeat 126, B
        .byte   <(B * 48)
        .endrepeat
blk48hi:
        .repeat 126, B
        .byte   >(B * 48)
        .endrepeat
seat48lo:
        .repeat 4, S
        .byte   <(S * 18 * 7 * 48)
        .endrepeat
seat48hi:
        .repeat 4, S
        .byte   >(S * 18 * 7 * 48)
        .endrepeat
        .segment "LC"

; ---------------------------------------------------------------------------
; hg_mover: a moving bomb (image hg_g1: dud, half-tile offset, phase) at
; group hg_g, line hg_y.
_hg_mover:
        jsr     msetup
        jmp     draw16
_hg_mover_c:
        jsr     msetup
        jmp     compbox
msetup: lda     _hg_g
        asl     a
        sta     col0
        lda     _hg_g1          ; bytes touched: 2 at offset 0, 3 at offset 3 (bit 2)
        lsr     a
        lsr     a
        lsr     a               ; C: the offset bit
        lda     #2
        adc     #0
        sta     _hg_nb
        lda     _hg_g1
        sta     t1
        lda     #0
        sta     t2
        jsr     times48
        lda     t1
        clc
        adc     #<_ax_movd
        sta     bdp
        lda     t2
        adc     #>_ax_movd
        sta     bdp+1
        lda     t1
        clc
        adc     #<_ax_movk
        sta     bkp
        lda     t2
        adc     #>_ax_movk
        sta     bkp+1
        rts

; ---------------------------------------------------------------------------
; hg_big: the winner twice the size (frame hg_g1: 0 win0, 1 win1; seat
; hg_bg) at group hg_g, from line hg_y: 32 lines of 4 bytes.
_hg_big:
        lda     #4
        sta     _hg_nb
        lda     _hg_g
        asl     a
        sta     col0
        lda     _hg_bg          ; 128 bytes a (seat, frame)
        asl     a
        ora     _hg_g1
        lsr     a
        sta     t2
        lda     #0
        ror     a
        clc
        adc     #<_ax_big
        sta     bdp
        lda     t2
        adc     #>_ax_big
        sta     bdp+1
        lda     _hg_g1          ; and a frame's keep bytes
        lsr     a
        lda     #0
        ror     a
        clc
        adc     #<_ax_bigk
        sta     bkp
        lda     #>_ax_bigk
        adc     #0
        sta     bkp+1
        lda     #32
        sta     _hg_n
        lda     #0
        sta     ly
@line:  lda     _hg_y
        cmp     #192
        bcs     @done
        sta     RAMRDON
        ldy     ly
        ldx     #0
:       lda     (bdp),y
        sta     dd,x
        lda     (bkp),y
        sta     kk,x
        iny
        inx
        cpx     #4
        bcc     :-
        sty     ly
        sta     RAMRDOFF
        lda     col0
        jsr     setscr
        ldy     #0
:       lda     (scr),y
        and     kk,y
        ora     dd,y
        sta     (scr),y
        iny
        cpy     #4
        bcc     :-
        inc     _hg_y
        jsr     _frame_tick
        dec     _hg_n
        bne     @line
@done:  rts

;---------------------------------------------------------------------------
; erase_box: the arena's tiles back under byte columns er_c.. (er_n: 2 or 3,
; all inside the arena, bytes 8-33), lines er_y0..er_y1 (inside 12-187), as
; tile_want has them in phase tm_phase: what a sprite drew comes off. A
; tile row at a time: the bytes' sources found once, then their lines.
;---------------------------------------------------------------------------
; tsrc: the tile sources of byte columns er_c.. (er_n of them, 1-3) on line
; er_y0, as tile_want has them in phase tm_phase: pp (a pointer a column;
; past er_n they point at tile art too, never at an I/O address), ly (2 x
; the line within the tile) and cnt (the lines left of the tile row, no more
; than left). With RAMRD off.
tsrc:   lda     _er_y0
        sec
        sbc     #12
        tax
        and     #15
        asl     a
        sta     ly
        lsr     a
        eor     #$FF
        sec
        adc     #16             ; 16 - the line
        cmp     left
        bcc     :+
        lda     left
:       sta     cnt
        sta     cnt0
        txa
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        tax
        lda     row13,x
        sta     t1              ; the row's first tile
        lda     _er_c           ; the first column's tile
        lsr     a
        jsr     tbase
        sta     pp
        stx     pp+1
        lda     _er_c           ; the last's
        clc
        adc     _er_n
        sbc     #0              ; (C clear: - 1)
        lsr     a
        jsr     tbase
        sta     pp+4
        stx     pp+5
        lda     _er_c
        lsr     a
        bcs     @odd
        lda     pp              ; even: A+0, A+1, B+0
        sta     pp+2
        lda     pp+1
        sta     pp+3
        inc     pp+2            ; (frames are 32-byte aligned: no carry)
        rts
@odd:   inc     pp              ; odd: A+1, B+0, B+1
        lda     pp+4
        sta     pp+2
        lda     pp+5
        sta     pp+3
        inc     pp+4
        rts

; A = group -> X:A = ax_tiles + 32 x the frame of tile t1 + group - 4
tbase:  sec
        sbc     #4
        clc
        adc     t1
        tay
        lda     _tile_want,y
        tay
        lda     _tile_n,y       ; first + (phase and (n - 1))
        sec
        sbc     #1
        and     _tm_phase
        clc
        adc     _tile_first,y
        sta     t2
        lsr     a
        lsr     a
        lsr     a
        tax
        lda     t2
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        clc
        adc     #<_ax_tiles
        bcc     :+
        inx
:       pha
        txa
        clc
        adc     #>_ax_tiles
        tax
        pla
        rts

; scr = line X, byte column er_c, on the page drawn on
.macro  ERSCR
        lda     _rowlo,x
        clc
        adc     _er_c
        sta     scr
        lda     _rowhi,x
        ora     _hg_page
        sta     scr+1
.endmacro

; A (byte 2), dd+1, dd to the screen: er_n of them. Without branching: the
; writes past er_n land on a byte written again after (w2, w1: 2 and 1 for 3
; bytes, 1 and 1 for 2, 0 and 0 for 1).
.macro  PUT3
        ldy     w2
        sta     (scr),y
        ldy     w1
        lda     dd+1
        sta     (scr),y
        ldy     #0
        lda     dd
        sta     (scr),y
.endmacro

; w1, w2 for er_n; and the lines to go (left)
wsetup: ldx     _er_n
        dex
        stx     w2
        cpx     #2
        bcc     :+
        ldx     #1
:       stx     w1
        lda     _er_y1
        sec
        sbc     _er_y0
        sta     left
        inc     left
        rts

; a tile row done (cnt lines): the next, or the end (C set)
rowend: lda     left
        sec
        sbc     cnt0
        sta     left
        beq     :+
        stx     _er_y0
        clc
        rts
:       sec
        rts

; erase_box: the arena's tiles back under byte columns er_c.. (er_n: 1-3, all
; inside the arena, bytes 8-33), lines er_y0..er_y1 (inside 12-187): what a
; sprite drew comes off. A tile row at a time: the sources, then the lines.
_erase_box:
        jsr     wsetup
erow:   jsr     tsrc
        sta     RAMRDON
        ldx     _er_y0
eline:  ERSCR
        ldy     ly
        lda     (pp),y
        sta     dd
        lda     (pp+2),y
        sta     dd+1
        lda     (pp+4),y
        PUT3
        inx
        inc     ly
        inc     ly
        dec     cnt
        bne     eline
        sta     RAMRDOFF
        jsr     rowend
        bcs     :+
        jsr     _frame_tick     ; the next tile row
        jmp     erow
:       jmp     _frame_tick

; comp: a sprite (3 bytes a line at aux bdp, keep bytes at aux bkp) drawn
; over the tiles beneath it rather than the screen: byte columns er_c.. (er_n
; of them), lines er_y0..er_y1. No erasing first and no reading the screen,
; so gfx.c uses it for a sprite nothing else touches.
comp:   lda     #0
        sta     sly
        jsr     wsetup
crow:   jsr     tsrc
        sta     RAMRDON
        ldx     _er_y0
cline:  ERSCR
        ldy     ly
        lda     (pp),y
        sta     dd
        lda     (pp+2),y
        sta     dd+1
        lda     (pp+4),y
        sta     dd+2
        ldy     sly
        lda     (bkp),y
        and     dd
        ora     (bdp),y
        sta     dd
        iny
        lda     (bkp),y
        and     dd+1
        ora     (bdp),y
        sta     dd+1
        iny
        lda     (bkp),y
        and     dd+2
        ora     (bdp),y
        iny
        sty     sly
        PUT3
        inx
        inc     ly
        inc     ly
        dec     cnt
        bne     cline
        sta     RAMRDOFF
        jsr     rowend
        bcs     :+
        jsr     _frame_tick
        jmp     crow
:       jmp     _frame_tick

; a sprite set up (col0, hg_nb, bdp, bkp) at line hg_y: comp it
compbox:
        lda     col0
        sta     _er_c
        lda     _hg_nb
        sta     _er_n
        lda     _hg_y
        sta     _er_y0
        clc
        adc     #15
        sta     _er_y1
        jmp     comp

row13:  .byte   0, 13, 26, 39, 52, 65, 78, 91, 104, 117, 130

; by a bomber's pixel x: its first byte column (2g, or 2g+1 for shifts 4-6),
; bit 7 set when it touches 3 bytes (any shift but 0) -- gfxa.s's plan
        .rodata
_px_box:
        .repeat 140, PX
        .byte   2 * (PX / 7) + ((PX .mod 7) >= 4) + ((PX .mod 7) <> 0) * $80
        .endrepeat
        .segment "LC"

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

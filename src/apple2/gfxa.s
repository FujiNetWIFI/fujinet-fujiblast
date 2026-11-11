;
; gfxa.s -- gfx.c's inner loops: a text row's line packed into fragments,
; and each page's queue of arena tiles to draw.
;

        .export         _pack_text, _tile_mark, _tile_drain, _tile_part
        .export         _tp_l0, _tp_n
        .export         _ptx_cells, _ptx_line, _ptx_fg, _ptx_hi
        .export         _tm_page, _tm_phase, _tile_want, _tile_shown0, _tile_shown1
        .export         _tqn0, _tqn1
        .import         _fragbuf, _fgbuf, _font3, _tile_first, _tile_n
        .import         _dh_tile, _ax_tiles, _frame_tick
        .importzp       _dh_g, _dh_y, _dh_n, _dh_src, src2, shp

zp_a    = src2                  ; the blitter's, free between its calls
zp_b    = shp

TILES   = 143
ARENA_W = 13
REDRAW  = $FF

        .bss
_ptx_cells: .res 2              ; pack_text: the row's 35 cells
_ptx_line:  .res 1              ; its line, 0-7
_ptx_fg:    .res 1              ; the colours: text, highlighted
_ptx_hi:    .res 1
nb:     .res 7
ci:     .res 1
tmp:    .res 1
tmpy:   .res 1
_tm_page:   .res 1              ; the page the tile calls work on
_tm_phase:  .res 1              ; the animation's phase, 0-3
_tp_l0:     .res 1              ; tile_part: the first line, and how many
_tp_n:      .res 1
_tile_want: .res TILES
_tile_shown0: .res TILES        ; REDRAW: queued
_tile_shown1: .res TILES
tq0:    .res TILES
tq1:    .res TILES
_tqn0:  .res 1
_tqn1:  .res 1

        .segment "LC"           ; beside the blitter (src/apple2/fujiblst.cfg)

; ---------------------------------------------------------------------------
; pack_text: line ptx_line of 35 cells (char-32, bit 7 highlighted) into
; fragbuf/fgbuf's 20 groups. 7 cells of 4 pixels make 4 groups.
_pack_text:
        lda     _ptx_cells
        sta     zp_a
        lda     _ptx_cells+1
        sta     zp_a+1
        lda     _ptx_line       ; zp_b = font3 + line * 64
        lsr     a
        lsr     a
        sta     tmp
        lda     _ptx_line
        and     #3
        clc
        ror     a
        ror     a
        ror     a
        clc
        adc     #<_font3
        sta     zp_b
        lda     tmp
        adc     #>_font3
        sta     zp_b+1
        ldx     #0
        stx     ci
@blk:   ldy     ci
        jsr     glyph
        sta     nb
        iny
        jsr     glyph
        sta     nb+1
        iny
        jsr     glyph
        sta     nb+2
        iny
        jsr     glyph
        sta     nb+3
        iny
        jsr     glyph
        sta     nb+4
        iny
        jsr     glyph
        sta     nb+5
        iny
        jsr     glyph
        sta     nb+6
        ldy     ci              ; a group's colour: its first cell's
        jsr     colour
        sta     _fgbuf,x
        iny
        iny
        jsr     colour
        sta     _fgbuf+1,x
        iny
        iny
        jsr     colour
        sta     _fgbuf+2,x
        iny
        iny
        jsr     colour
        sta     _fgbuf+3,x
        lda     nb+1            ; n0 | n1 << 4
        asl     a
        asl     a
        asl     a
        asl     a
        ora     nb
        and     #$7F
        sta     _fragbuf,x
        lda     nb+3            ; n2 << 1 | n3 << 5
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        sta     tmp
        lda     nb+2
        asl     a
        ora     tmp
        and     #$7F
        sta     _fragbuf+1,x
        lda     nb+5            ; n3 >> 2 | n4 << 2 | (n5 & 1) << 6
        and     #1
        clc
        ror     a
        ror     a
        ror     a
        sta     tmp
        lda     nb+4
        asl     a
        asl     a
        ora     tmp
        sta     tmp
        lda     nb+3
        lsr     a
        lsr     a
        ora     tmp
        sta     _fragbuf+2,x
        lda     nb+6            ; n5 >> 1 | n6 << 3
        asl     a
        asl     a
        asl     a
        sta     tmp
        lda     nb+5
        lsr     a
        ora     tmp
        sta     _fragbuf+3,x
        inx
        inx
        inx
        inx
        lda     ci
        clc
        adc     #7
        sta     ci
        cpx     #20
        bcs     :+
        jmp     @blk
:       rts

; A = the font's bits for cell Y (Y kept)
glyph:  lda     (zp_a),y
        sty     tmpy
        and     #$3F
        tay
        lda     (zp_b),y
        ldy     tmpy
        rts

; A = cell Y's colour
colour: lda     (zp_a),y
        bmi     :+
        lda     _ptx_fg
        rts
:       lda     _ptx_hi
        rts

; ---------------------------------------------------------------------------
; tile_mark(A): tile A to be drawn on page tm_page (once, until drawn).
_tile_mark:
        tax
        lda     _tm_page
        bne     @p1
        lda     _tile_shown0,x
        cmp     #REDRAW
        beq     @done
        lda     #REDRAW
        sta     _tile_shown0,x
        ldy     _tqn0
        txa
        sta     tq0,y
        inc     _tqn0
@done:  rts
@p1:    lda     _tile_shown1,x
        cmp     #REDRAW
        beq     @done
        lda     #REDRAW
        sta     _tile_shown1,x
        ldy     _tqn1
        txa
        sta     tq1,y
        inc     _tqn1
        rts

; tile_drain: draw page tm_page's queued tiles, in phase tm_phase.
_tile_drain:
@next:  lda     _tm_page
        bne     @p1
        ldy     _tqn0
        beq     @done
        dey
        sty     _tqn0
        lda     tq0,y
        tax
        lda     _tile_want,x
        sta     _tile_shown0,x
        jmp     @draw
@p1:    ldy     _tqn1
        beq     @done
        dey
        sty     _tqn1
        lda     tq1,y
        tax
        lda     _tile_want,x
        sta     _tile_shown1,x
@draw:  lda     #0
        sta     _tp_l0
        lda     #16
        sta     _tp_n
        jsr     tdraw
        jmp     @next
@done:  rts

; tile_part(A): lines tp_l0 .. tp_l0 + tp_n - 1 of tile A as it is now (what
; a sprite covered).
_tile_part:
        tax
        ; fall through

; tile X's lines tp_l0.. (tp_n of them), at its place
tdraw:  txa
        ldy     #0
:       cmp     #ARENA_W
        bcc     :+
        sbc     #ARENA_W
        iny
        bne     :-
:       clc
        adc     #4              ; the arena's first group
        sta     _dh_g
        tya                     ; line 12 + 16 r + l0
        asl     a
        asl     a
        asl     a
        asl     a
        adc     #12
        adc     _tp_l0
        sta     _dh_y
        lda     _tp_n
        sta     _dh_n
        ldy     _tile_want,x    ; the frame: first + (phase & (n - 1))
        lda     _tile_n,y
        sec
        sbc     #1
        and     _tm_phase
        clc
        adc     _tile_first,y
        sta     tmp             ; ax_tiles + frame * 64 + l0 * 4
        lsr     a
        lsr     a
        sta     tmpy
        lda     tmp
        and     #3
        clc
        ror     a
        ror     a
        ror     a
        sta     tmp
        lda     _tp_l0
        asl     a
        asl     a
        clc
        adc     tmp
        clc
        adc     #<_ax_tiles
        sta     _dh_src
        lda     tmpy
        adc     #>_ax_tiles
        sta     _dh_src+1
        jmp     _dh_tile

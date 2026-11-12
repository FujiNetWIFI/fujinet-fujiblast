;
; gfxa.s -- gfx.c's inner loops: a text row's line packed into fragments,
; and each page's queue of arena tiles to draw.
;

        .export         _pack_text, _tile_mark, _tile_drain, _tile_part
        .export         _tp_l0, _tp_n
        .export         _ptx_cells, _ptx_line, _ptx_fg, _ptx_hi
        .export         _tm_page, _tm_phase, _tile_want, _tile_shown0, _tile_shown1
        .export         _tqn0, _tqn1, _spr_arena_m
        .import         _sp_on, _sp_x, _sp_y, _sp_fr, _sp_seat, _sp_big
        .import         _spr_ax, _spr_ay, _spr_af, _spr_as
        .import         _fragbuf, _fgbuf, _ax_font3, _ax_font5, _tile_first, _tile_n
        .export         _pack_big, _big_col, _plan, _erase_diff, _ebox
        .export         _keep_scan, _sprites_m, _spph
        .import         _sp_fr, _dr_on, _dr_x, _dr_s, _dr_f, _dr_g, _dr_ng, _fr_win0
        .import         _hg_mover, _hg_mover_c, _hg_bomber, _hg_bomber_c, _hg_big, _frame_tick
        .importzp       _hg_x, _hg_bg, _hg_c0, _hg_nb
        .import         _keep, _sp_on, _sp_x, _sp_y, _sp_seat, _sp_big, _arena
        .import         _dr_c, _dr_n, _dr_y, _nw_c, _nw_n, _nw_y, _dr_h, _solo
        .import         _px_box, _erase_box
        .importzp       _er_c, _er_n, _er_y0, _er_y1
        .import         _hg_tile, _ax_tiles, _frame_tick
        .importzp       _hg_g, _hg_g1, _hg_y, _hg_n, _hg_src, src2, shp

RAMRDOFF := $C002
RAMRDON  := $C003

zp_a    = src2                  ; the blitter's, free between its calls
zp_b    = shp

TILES   = 143
ARENA_W = 13
REDRAW  = $FF

        .bss
_big_col:   .res 4              ; pack_big: a big row's 4 colours
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
        lda     _ptx_line       ; zp_b = ax_font3 + line * 64 (aux)
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
        adc     #<_ax_font3
        sta     zp_b
        lda     tmp
        adc     #>_ax_font3
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
        sta     RAMRDON         ; (the font is in aux)
        lda     (zp_b),y
        sta     RAMRDOFF
        ldy     tmpy
        rts

; pack_big: line ptx_line of a big row's cells (ptx_cells: char-32, bits 6-7
; the colour) into fragbuf/fgbuf, groups hg_g..hg_g1: the 5x7 font a pixel
; in, in big_col[colour].
_pack_big:
        lda     _ptx_cells
        sta     zp_a
        lda     _ptx_cells+1
        sta     zp_a+1
        lda     _ptx_line       ; zp_b = ax_font5 + line * 64 (aux)
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
        adc     #<_ax_font5
        sta     zp_b
        lda     tmp
        adc     #>_ax_font5
        sta     zp_b+1
        ldx     _hg_g
@g:     txa
        tay
        lda     (zp_a),y
        pha
        and     #$3F
        tay
        sta     RAMRDON
        lda     (zp_b),y
        sta     RAMRDOFF
        asl     a
        sta     _fragbuf,x
        pla
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        tay
        lda     _big_col,y
        sta     _fgbuf,x
        cpx     _hg_g1
        inx
        bcc     @g
        rts

; A = cell Y's colour
colour: lda     (zp_a),y
        bmi     :+
        lda     _ptx_fg
        rts
:       lda     _ptx_hi
        rts

; ---------------------------------------------------------------------------
; The rest touches no aux RAM, so it lives in main memory (the card is full).
        .code

; ---------------------------------------------------------------------------
; spr_arena_m(A): sprite A at arena x spr_ax, y spr_ay (16 units a tile), in
; frame spr_af and seat spr_as's colours (gfx.h's spr_arena). x * 7 / 16 to
; pixels: (x - x / 8) / 2.
_spr_arena_m:
        tay
        jsr     _frame_tick     ; (keeps Y)
        lda     _spr_ax
        lsr     a
        lsr     a
        lsr     a
        sta     tmp
        lda     _spr_ax
        sec
        sbc     tmp
        lsr     a
        clc
        adc     #28             ; the arena's first pixel (gfx.c AR_X)
        sta     _sp_x,y
        lda     _spr_ay
        clc
        adc     #12             ; and line (AR_Y)
        sta     _sp_y,y
        lda     _spr_af
        sta     _sp_fr,y
        lda     _spr_as
        and     #3
        sta     _sp_seat,y
        lda     #0
        sta     _sp_big,y
        lda     #1
        sta     _sp_on,y
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
        sta     _hg_g
        tya                     ; line 12 + 16 r + l0
        asl     a
        asl     a
        asl     a
        asl     a
        adc     #12
        adc     _tp_l0
        sta     _hg_y
        lda     _tp_n
        sta     _hg_n
        ldy     _tile_want,x    ; the frame: first + (phase & (n - 1))
        lda     _tile_n,y
        sec
        sbc     #1
        and     _tm_phase
        clc
        adc     _tile_first,y
        sta     tmp             ; ax_tiles + frame * 32 + l0 * 2
        lsr     a
        lsr     a
        lsr     a
        sta     tmpy
        lda     tmp
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        sta     tmp
        lda     _tp_l0
        asl     a
        clc
        adc     tmp
        clc
        adc     #<_ax_tiles
        sta     _hg_src
        lda     tmpy
        adc     #>_ax_tiles
        sta     _hg_src+1
        jmp     _hg_tile

; ---------------------------------------------------------------------------
; gfx.c's render, the sprites' part (main memory: no room in the card)
        .code

; plan(A = page offset 0 or 8): where each sprite to be drawn goes (nw_c,
; nw_n bytes, nw_y lines) and which are solo: in the arena and touching no
; other sprite's box (a kept one's as drawn, or one being drawn's new one).
; The boxes first (bx*), then the pairs.
_plan:
        sta     pso
        ldx     #7
@k:     lda     #0
        sta     _solo,x
        sta     bxon,x
        lda     _sp_on,x
        bne     :+
        jmp     @next
:       lda     _keep,x
        beq     @drawn
        txa                     ; kept: its box as drawn on this page
        clc
        adc     pso
        tay
        lda     _dr_c,y
        sta     bxc0,x
        clc
        adc     _dr_n,y
        sta     bxc1,x
        lda     _dr_y,y
        sta     bxy0,x
        clc
        adc     _dr_h,y
        sta     bxy1,x
        jmp     @on
@drawn: lda     _sp_y,x
        sta     _nw_y,x
        sta     bxy0,x
        clc
        adc     #16
        sta     bxy1,x
        cpx     #4
        bcc     @bomber
        lda     _sp_seat,x      ; a mover: group (in sp_seat), 3 bytes at offset 3
        asl     a
        sta     _nw_c,x
        sta     bxc0,x
        lda     _sp_big,x
        cmp     #1
        lda     #2
        adc     #0
        sta     _nw_n,x
        jmp     @solo
@bomber:
        lda     _sp_big,x
        beq     :+
        lda     #0              ; big: never solo (never in the arena); all across
        sta     _nw_c,x
        sta     bxc0,x
        lda     #40
        sta     _nw_n,x
        sta     bxc1,x
        lda     bxy0,x
        clc
        adc     #32
        sta     bxy1,x
        jmp     @on
:       ldy     _sp_x,x
        cpy     #133
        bcc     :+
        ldy     #132
:       lda     _px_box,y
        and     #$7F
        sta     _nw_c,x
        sta     bxc0,x
        lda     _px_box,y
        asl     a               ; C: 3 bytes
        lda     #2
        adc     #0
        sta     _nw_n,x
@solo:  lda     _nw_c,x
        clc
        adc     _nw_n,x
        sta     bxc1,x
        lda     _arena
        sta     _solo,x
@on:    lda     #1
        sta     bxon,x
@next:  dex
        bmi     :+
        jmp     @k
:       ldx     #7              ; then the pairs: a solo one touching another isn't
@k2:    lda     _solo,x
        beq     @n2
        stx     pk
        ldy     #7
@m:     cpy     pk
        beq     @nm
        lda     bxon,y
        beq     @nm
        lda     bxc0,y
        cmp     bxc1,x
        bcs     @nm
        lda     bxc0,x
        cmp     bxc1,y
        bcs     @nm
        lda     bxy0,y
        cmp     bxy1,x
        bcs     @nm
        lda     bxy0,x
        cmp     bxy1,y
        bcs     @nm
        lda     #0
        sta     _solo,x
        beq     @n2
@nm:    dey
        bpl     @m
@n2:    dex
        bpl     @k2
        rts

; keep_scan(A = page offset): keep[k] for each sprite drawn on this page as
; it is now (keepers' first part); A: any kept. spph: the movers' phase.
_keep_scan:
        sta     pso
        lda     #0
        sta     pany
        ldx     #7
@k:     lda     #0
        sta     _keep,x
        lda     _sp_on,x
        beq     @n
        txa
        clc
        adc     pso
        tay
        lda     _dr_on,y
        beq     @n
        lda     _dr_x,y
        cmp     _sp_x,x
        bne     @n
        lda     _dr_y,y
        cmp     _sp_y,x
        bne     @n
        lda     _dr_s,y
        cmp     _sp_seat,x
        bne     @n
        cpx     #4
        bcs     @mv
        lda     _sp_big,x       ; a bomber: frame | big << 7
        lsr     a
        lda     _sp_fr,x
        bcc     @f
        ora     #$80
        bne     @f
@mv:    lda     _sp_fr,x        ; a mover: frame * 4 + phase
        asl     a
        asl     a
        ora     _spph
@f:     cmp     _dr_f,y
        bne     @n
        lda     #1
        sta     _keep,x
        sta     pany
@n:     dex
        bpl     @k
        lda     pany
        ldx     #0
        rts

; sprites_m(A = page offset): draw on this page every sprite not kept, the
; bombs first (gfx.c's sprites), and note what each drew (dr_*). spph: the
; movers' phase.
_sprites_m:
        sta     pso
        lda     #0
        sta     si
@i:     jsr     _frame_tick
        ldx     si
        lda     sorder,x
        tax                     ; X: the slot
        lda     _keep,x
        bne     @next
        stx     sk
        txa
        clc
        adc     pso
        tay                     ; Y: its record on this page
        sty     sj
        lda     _sp_on,x
        sta     _dr_on,y
        beq     @next
        lda     _sp_x,x
        sta     _dr_x,y
        lda     _sp_y,x
        sta     _dr_y,y
        sta     _hg_y
        lda     _sp_seat,x
        sta     _dr_s,y
        cpx     #4
        bcc     @bomber
        lda     #2              ; a mover: group in sp_seat, offset in sp_big
        sta     _dr_ng,y
        lda     #16
        sta     _dr_h,y
        lda     _sp_fr,x
        asl     a
        asl     a
        ora     _spph
        sta     _dr_f,y
        lda     _sp_seat,x
        sta     _dr_g,y
        sta     _hg_g
        lda     _sp_fr,x        ; image (dud, offset, phase)
        asl     a
        ora     _sp_big,x
        asl     a
        asl     a
        ora     _spph
        sta     _hg_g1
        lda     _solo,x
        beq     :+
        jsr     _hg_mover_c
        jmp     @rec
:       jsr     _hg_mover
@rec:   ldy     sj
        lda     _hg_c0
        sta     _dr_c,y
        lda     _hg_nb
        sta     _dr_n,y
@next:  inc     si
        lda     si
        cmp     #8
        bcs     :+
        jmp     @i
:       rts
@bomber:
        lda     _sp_seat,x
        sta     _hg_bg
        lda     _sp_big,x
        beq     @small
        lda     #3
        sta     _dr_ng,y
        lda     #32
        sta     _dr_h,y
        lda     _sp_fr,x
        ora     #$80
        sta     _dr_f,y
        lda     _sp_fr,x        ; the winner, twice the size: whole groups
        sec
        sbc     _fr_win0
        cmp     #2
        bcs     @drawsm         ; (other big frames: drawn small)
        sta     _hg_g1
        lda     _sp_x,x
        ldy     #0
:       cmp     #7
        bcc     :+
        sbc     #7
        iny
        bne     :-
:       sty     _hg_g
        tya
        ldy     sj
        sta     _dr_g,y
        jsr     _hg_big
        jmp     @rec
@small: lda     #2
        sta     _dr_ng,y
        lda     #16
        sta     _dr_h,y
        lda     _sp_fr,x
        sta     _dr_f,y
@drawsm:
        lda     _sp_x,x
        cmp     #133
        bcc     :+
        lda     #132
:       sta     _hg_x
        lda     _sp_fr,x
        sta     _hg_g1
        lda     _solo,x
        beq     :+
        jsr     _hg_bomber_c
        jmp     :++
:       jsr     _hg_bomber
:       ldy     sj
        lda     _hg_g
        sta     _dr_g,y
        jmp     @rec

sorder: .byte   4, 5, 6, 7, 0, 1, 2, 3  ; the bombs under the bombers

; erase_diff(A = slot k, solo): its old place on this page off, but for what
; its new box covers anyway -- the strips above, below, left and right.
_erase_diff:
        tay
        clc
        adc     pso
        tax                     ; X: the old (k + page), Y: the new
        lda     _dr_c,x
        sta     oc0
        clc
        adc     _dr_n,x
        sta     oc1
        lda     _dr_y,x
        sta     oy0
        clc
        adc     #16
        sta     oy1
        lda     _nw_c,y
        sta     bc0
        clc
        adc     _nw_n,y
        sta     bc1
        lda     _nw_y,y
        sta     by0
        clc
        adc     #16
        sta     by1
        lda     oc0             ; apart: all of the old one
        cmp     bc1
        bcs     @all
        lda     bc0
        cmp     oc1
        bcs     @all
        lda     oy0
        cmp     by1
        bcs     @all
        lda     by0
        cmp     oy1
        bcc     @parts
@all:   lda     oc0
        ldx     oc1
        ldy     oy0
        jsr     ebox_cols
        lda     oy1
        jmp     ebox_to
@parts: lda     oy0             ; above the new box
        cmp     by0
        bcs     :+
        lda     oc0
        ldx     oc1
        ldy     oy0
        jsr     ebox_cols
        lda     by0
        jsr     ebox_to
:       lda     by1             ; below
        cmp     oy1
        bcs     :+
        lda     oc0
        ldx     oc1
        ldy     by1
        jsr     ebox_cols
        lda     oy1
        jsr     ebox_to
:       lda     oy0             ; the lines both share: max(oy0, by0) ..
        cmp     by0
        bcs     :+
        lda     by0
:       sta     ry0
        lda     oy1             ; .. min(oy1, by1) - 1
        cmp     by1
        bcc     :+
        lda     by1
:       sta     ry1
        lda     oc0             ; left of the new box
        cmp     bc0
        bcs     :+
        ldx     bc0
        ldy     ry0
        jsr     ebox_cols
        lda     ry1
        jsr     ebox_to
:       lda     bc1             ; right of it
        cmp     oc1
        bcs     :+
        ldx     oc1
        ldy     ry0
        jsr     ebox_cols
        lda     ry1
        jmp     ebox_to
:       rts

; er_c = A, er_n = X - A, er_y0 = Y
ebox_cols:
        sta     _er_c
        txa
        sec
        sbc     _er_c
        sta     _er_n
        sty     _er_y0
        rts
; er_y1 = A - 1, then ebox
ebox_to:
        sec
        sbc     #1
        sta     _er_y1
        ; fall through

; ebox: erase_box clipped to the arena (bytes 8-33, lines 12-187)
_ebox:  lda     _er_n
        beq     @no
        lda     _er_y1
        cmp     #12
        bcc     @no
        lda     _er_y0
        cmp     #188
        bcs     @no
        lda     _er_c
        cmp     #34
        bcs     @no
        clc
        adc     _er_n
        cmp     #9
        bcc     @no
        lda     _er_c           ; the left
        cmp     #8
        bcs     :+
        lda     #8
        sec
        sbc     _er_c
        sta     pm
        lda     _er_n
        sec
        sbc     pm
        sta     _er_n
        lda     #8
        sta     _er_c
:       lda     _er_c           ; the right
        clc
        adc     _er_n
        cmp     #35
        bcc     :+
        lda     #34
        sec
        sbc     _er_c
        sta     _er_n
:       lda     _er_y0          ; the top, the bottom
        cmp     #12
        bcs     :+
        lda     #12
        sta     _er_y0
:       lda     _er_y1
        cmp     #188
        bcc     :+
        lda     #187
        sta     _er_y1
:       jmp     _erase_box
@no:    rts

        .bss
pso:    .res 1                  ; the page's offset (plan, erase_diff)
_spph:  .res 1                  ; the movers' animation phase (keep_scan, sprites_m)
pany:   .res 1
si:     .res 1
sk:     .res 1
sj:     .res 1
bxon:   .res 8                  ; plan: each sprite's box, if on
bxc0:   .res 8
bxc1:   .res 8
bxy0:   .res 8
bxy1:   .res 8
pk:     .res 1
pm:     .res 1
bc0:    .res 1                  ; a box: byte columns bc0 to bc1 - 1, lines by0 to by1 - 1
bc1:    .res 1
by0:    .res 1
by1:    .res 1
oc0:    .res 1                  ; another
oc1:    .res 1
oy0:    .res 1
oy1:    .res 1
ry0:    .res 1
ry1:    .res 1

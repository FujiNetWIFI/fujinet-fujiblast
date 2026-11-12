;
; compose.s -- a zone's display list, composed into its back slot (gfx.c
; does the rest of the screen; nmi.s shows the slot a frame later).
;
; A zone is a row of the neutral screen (SCREEN, 40 bytes: the chars MARIA
; reads) of one kind (gfx.c's row_kind): TEXT / HUD (320A, a byte a char),
; BIG (160A, two bytes a char), GFX / ARENA (160B cells). Its objects:
;   a run of chars (a 5-byte header: the chars' address, the mode, the
;   palette and width, where it goes), then the bombers' two layers.
; A run goes from a char to the last one before two blanks (blank = 0:
; nothing to draw), splits at a change of palette and at 32 chars (a
; header's width), and is cut short when the line's DMA (426 cycles: 16 to
; start, 10 a header, 3 a byte read (+3 a pointer), 6 to end) wouldn't
; hold it. The bombers' objects are paid for first: a bomber is never what
; goes.
;
        .include "a78.inc"
        .export _compose_dirty
        .export _rk, _rp, _zdirty, _hud_pal, _o_on, _o_x, _o_y, _o_fr, _o_seat
        .import _zflip, _zfront, _slot_lo, _slot_hi, _dsp_ready

        .segment "ZEROPAGE"
zz:     .res 1          ; the zone
rowp:   .res 2          ; its screen row
dlp:    .res 2          ; its display list slot
di:     .res 1          ; the list's end
cyc:    .res 2          ; the line's DMA so far
lim:    .res 2          ; and what the runs may use
kind:   .res 1
b1:     .res 1          ; a run's header mode byte
unit:   .res 1          ; hpos = char * 2^unit
per:    .res 1          ; DMA cycles a char
nch:    .res 1          ; chars in the row
palm:   .res 1          ; 0: one palette (0), 1: pbuf
rs:     .res 1          ; a run: start, end, palette, length
re:     .res 1
rpl:    .res 1
rl:     .res 1
cost:   .res 2
tmp:    .res 1
sp_t:   .res 1          ; the zone's top line
sp_n:   .res 1
en:     .res 1          ; chars in the header being made

        .bss
_rk:     .res ZONES
_rp:     .res ZONES
_zdirty: .res ZONES
_hud_pal: .res 40
pbuf:    .res 40
_o_on:   .res 4
_o_x:    .res 4
_o_y:    .res 4
_o_fr:   .res 4
_o_seat: .res 4

        .segment "OVL1RO"       ; (read by the main thread only)
; the screen's rows
.macro  rowtab part
        .repeat ZONES, i
        .byte   part ($6800 + i * 40)
        .endrepeat
.endmacro
row_lo: rowtab <
row_hi: rowtab >
; per-char cost tables, by chars (0-32): 6 n and 9 n
.macro  multab k, part
        .repeat 33, i
        .byte   part (k * i)
        .endrepeat
.endmacro
m6lo:   multab 6, <
m6hi:   multab 6, >
m9lo:   multab 9, <
m9hi:   multab 9, >
; BIG text: the palette of colour c (0-3): menus (yellow P3, white P0), a seat's
; (P4+seat, c3 = yellow P3)
bigtab: .byte   3, 0, 0, 3
        .byte   4, 4, 4, 3
        .byte   5, 5, 5, 3
        .byte   6, 6, 6, 3
        .byte   7, 7, 7, 3

        .code

; the dirty zones composed; the frame NMI will show them
.proc   _compose_dirty
        ldx     #0
        stx     tmp
@z:     lda     _zdirty,x
        beq     @n
        lda     #0
        sta     _zdirty,x
        stx     zz
        jsr     compose
        ldx     zz
        inc     tmp
@n:     inx
        cpx     #ZONES
        bcc     @z
        lda     tmp
        beq     @r
        lda     #1
        sta     _dsp_ready
@r:     rts
.endproc

; zone zz
.proc   compose
        ldx     zz
        lda     row_lo,x
        sta     rowp
        lda     row_hi,x
        sta     rowp+1
        lda     _zfront,x
        eor     #1
        sta     tmp
        txa
        asl     a
        ora     tmp
        tax
        lda     _slot_lo,x
        sta     dlp
        lda     _slot_hi,x
        sta     dlp+1
        lda     #0
        sta     di
        ; the zone's top line, and the bombers on it: 32 cycles each
        lda     zz
        asl     a
        asl     a
        asl     a
        sta     sp_t
        lda     #0
        sta     sp_n
        ldx     #3
@c:     lda     _o_on,x
        beq     @cn
        jsr     visible         ; carry: seen
        bcc     @cn
        inc     sp_n
@cn:    dex
        bpl     @c
        ; lim = 420 - 32 * n
        lda     sp_n
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a               ; 32 n (n <= 4: 128; no carry out of a byte but for n = 8)
        sta     tmp
        lda     #<420
        sec
        sbc     tmp
        sta     lim
        lda     #>420
        sbc     #0
        sta     lim+1
        lda     #<16
        sta     cyc
        lda     #0
        sta     cyc+1
        ; the runs by the row's kind
        ldx     zz
        lda     _rk,x
        sta     kind
        cmp     #4
        beq     @hud
        cmp     #1
        beq     @big
        ; TEXT (and anything else with a byte a char): 320A / the cells
        lda     #0
        sta     palm
        lda     #40
        sta     nch
        lda     kind
        beq     @text
        lda     #$E0            ; GFX, ARENA: 160B, indirect
        sta     b1
        lda     #2
        sta     unit
        lda     #9
        sta     per
        jmp     @go
@text:  lda     #$60            ; 320A, indirect
        sta     b1
        lda     #2
        sta     unit
        lda     #6
        sta     per
        jmp     @go
@hud:   lda     #1
        sta     palm
        lda     #40
        sta     nch
        lda     #$60
        sta     b1
        lda     #2
        sta     unit
        lda     #6
        sta     per
        ldy     #39
@hp:    lda     _hud_pal,y
        cmp     #4
        bcs     @hw
        adc     #4              ; (carry clear)
        bne     @hs
@hw:    lda     #0
@hs:    sta     pbuf,y
        dey
        bpl     @hp
        jmp     @go
@big:   lda     #1
        sta     palm
        lda     #20
        sta     nch
        lda     #$60            ; 160A, indirect
        sta     b1
        lda     #3
        sta     unit
        lda     #9
        sta     per
        ; the row's palette set: menus 0, seat n 1 + n
        ldx     zz
        lda     _rp,x
        sec
        sbc     #4
        cmp     #4
        bcc     @sm
        lda     #$FF
@sm:    clc
        adc     #1              ; 0 (menu) or 1-4
        asl     a
        asl     a
        sta     tmp             ; * 4
        ldy     #19
@bp:    sty     rs              ; (the char's index; the colours are the row's second half)
        tya
        clc
        adc     #20
        tay
        lda     (rowp),y
        and     #3
        clc
        adc     tmp
        tax
        lda     bigtab,x
        ldy     rs
        sta     pbuf,y
        dey
        bpl     @bp
@go:    jsr     runs
        ; the bombers: the body, then the visor and boots
        ldx     #0
@b:     lda     _o_on,x
        beq     @bn
        stx     rs
        jsr     visible
        bcc     @bx
        ; A = hi byte (0x8F - d)
        sta     rl
        lda     _o_fr,x
        asl     a
        asl     a               ; frame * 4: the body's column
        sta     re
        lda     _o_seat,x
        clc
        adc     #4
        jsr     shl5            ; palette 4 + seat << 5
        ora     #30
        sta     rpl             ; byte 3 of the body's header
        lda     re
        ldy     rl
        jsr     put5b           ; lo = column, hi, byte 3 = rpl
        lda     #(3 << 5) | 30  ; the visor and boots: palette 3, the next object
        sta     rpl
        lda     re
        clc
        adc     #2
        ldy     rl
        jsr     put5b
@bx:    ldx     rs
@bn:    inx
        cpx     #4
        bcc     @b
        ; the end of the list
        ldy     di
        lda     #0
        sta     (dlp),y
        iny
        sta     (dlp),y
        ldx     zz
        lda     #1
        sta     _zflip,x
        rts
.endproc

; A << 5
shl5:   asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        rts

; a bomber's header: lo = A, byte 1 $40 (160A direct), hi = Y, byte 3 = rpl,
; hpos = _o_x[rs]. A zone's list holds 12 of them at most.
put5b:  sta     tmp
        lda     di
        cmp     #56
        bcs     @r
        sty     rl
        ldy     di
        lda     tmp
        sta     (dlp),y
        iny
        lda     #$40
        sta     (dlp),y
        iny
        lda     rl
        sta     (dlp),y
        iny
        lda     rpl
        sta     (dlp),y
        iny
        stx     tmp
        ldx     rs
        lda     _o_x,x
        sta     (dlp),y
        ldx     tmp
        iny
        sty     di
@r:     rts

; is bomber X on zone zz (its top line sp_t)? carry set, A = 0x8F - d where
; d = top - y, when -8 < d < 16; X kept
visible:
        lda     sp_t
        sec
        sbc     _o_y,x          ; d, as a byte
        bcs     @pos
        cmp     #249            ; d < 0: -8 < d
        bcc     @no
        bcs     @ok
@pos:   cmp     #16
        bcs     @no
@ok:    sta     tmp
        lda     #$8F
        sec
        sbc     tmp
        sec
        rts
@no:    clc
        rts

; ---- the runs of the zone's chars ----------------------------------------------
; palette of char Y: pbuf[Y] (palm), else 0
.proc   runs
        lda     #0
        sta     rs              ; i
@top:   lda     rs
        cmp     nch
        bcs     @done
        tay
        lda     (rowp),y
        bne     @start
        inc     rs
        bne     @top
@start: sty     re              ; e = s = i
        lda     palm
        beq     @p0
        lda     pbuf,y
        .byte   $2C
@p0:    lda     #0
        sta     rpl
        ; extend: j = i + 1 ...
        iny
@j:     cpy     nch
        bcs     @emit
        lda     (rowp),y
        beq     @blank
        lda     palm
        beq     @same
        lda     pbuf,y
        cmp     rpl
        bne     @emit           ; another palette: a run of its own
@same:  sty     re
        iny
        bne     @j
@blank: tya
        sec
        sbc     re
        cmp     #2              ; two blanks since the last char: the run ends
        bcs     @emit
        iny
        bne     @j
@emit:  lda     re
        sec
        sbc     rs
        clc
        adc     #1
        sta     rl              ; the run's length
        jsr     emit
        bcs     @done           ; no room: no more
        lda     re
        clc
        adc     #1
        sta     rs
        jmp     @top
@done:  rts
.endproc

; the run rs..re (length rl, palette rpl) as headers of up to 32 chars; carry
; set: out of room (or out of budget)
.proc   emit
@more:  lda     rl
        bne     @go
        clc
        rts
@go:
        cmp     #33
        bcc     @n
        lda     #32
@n:     sta     en
@fit:   ldx     en
        beq     @full
        ; cost = 10 + per * n
        lda     per
        cmp     #6
        beq     @six
        lda     m9lo,x
        ldy     m9hi,x
        jmp     @c
@six:   lda     m6lo,x
        ldy     m6hi,x
@c:     clc
        adc     #10
        sta     cost
        tya
        adc     #0
        sta     cost+1
        ; fits if cyc + cost <= lim: lim - (cyc + cost) >= 0
        lda     cyc
        clc
        adc     cost
        sta     tmp
        lda     cyc+1
        adc     cost+1
        tay
        lda     lim
        sec
        sbc     tmp
        lda     lim+1
        sty     tmp
        sbc     tmp
        bcs     @take
        dec     en              ; a char fewer
        jmp     @fit
@full:  sec
        rts
@take:  lda     di
        cmp     #56
        bcs     @full
        ldy     di
        lda     rowp
        clc
        adc     rs
        sta     (dlp),y
        iny
        lda     b1
        sta     (dlp),y
        iny
        lda     rowp+1
        adc     #0
        sta     (dlp),y
        iny
        lda     rpl
        jsr     shl5
        sta     tmp
        lda     #0
        sec
        sbc     en
        and     #31
        ora     tmp
        sta     (dlp),y
        iny
        lda     rs
        ldx     unit
@sh:    dex
        bmi     @hd
        asl     a
        jmp     @sh
@hd:    sta     (dlp),y
        iny
        sty     di
        lda     cyc
        clc
        adc     cost
        sta     cyc
        lda     cyc+1
        adc     cost+1
        sta     cyc+1
        lda     rs
        clc
        adc     en
        sta     rs
        lda     rl
        sec
        sbc     en
        sta     rl
        jmp     @more
@ok:    clc
        rts
.endproc

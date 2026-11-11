;
; gfxa.s -- the display's interrupts and the players (gfx.c builds the
; screens).
;
; The display list (gfx.c) is 16 blank lines and 25 rows of 8 scanlines,
; each row text (mode 2), big text (mode 6) or multicolour (mode 4). Each
; row has its charset and colours in the rw_* tables. A row whose next row
; differs has a DLI that loads the next row's (dli_list says which); the
; VBI loads row 0's. An arena row's charset is ANIM: the arena set of this
; frame -- the VBI turns the four sets over every 4 jiffies, which animates
; every bomb, flame and powerup at no cost per tile.
;
;
; The OS's second VBI stage is skipped (CRITIC set): it copies shadows we
; write ourselves, and when it ran late it rewrote DLISTL mid-screen, which
; restarts the display list there (a row too many, the arena moved down).
; input.c reads the stick and trigger from the hardware for the same
; reason.
;
; The VBI must stay short: whatever the OS still does after it (AltirraOS
; copies the colour shadows even with CRITIC set) lands where the beam is,
; and past the HUD's DLI that colours the arena with the HUD's. So the
; players are drawn by the main loop (spr_flush, from frame_wait) into the
; PMG area not shown, and the VBI only shows it: PMBASE, the positions,
; colours and sizes. Each player is one bomber, its visor that player's
; missile (fifth player: PF3) seen through a hole in the bomber.
;
; IRQs go back on first in the VBI, so the serial port's needn't wait it
; out -- unless the VBI came in over an IRQ handler (the stacked P has I
; set): that IRQ isn't acknowledged yet, and CLI would take it again,
; nested, until the stack wraps (M.U.L.E.).
;
        .export _dsp_init, _dsp_dma, _anim, _dlist, _dli_list
        .export _rw_chb, _rw_bk, _rw_c0, _rw_c1, _rw_c2, _rw_c3
        .export _spr_x, _spr_y, _spr_fr, _spr_big, _spr_col, _spr_flush
        .import _snd_vbi, _spr_frames, _spr_visor, _spr_visor_x
        .constructor clrlow, 30

        .include "hw.inc"

        .segment "ZEROPAGE"
sp_dst:  .res 2
sp_mis:  .res 2
sp_src:  .res 2
sp_vis:  .res 2
sp_n:    .res 1
sp_h:    .res 1
sp_i:    .res 1
sp_v:    .res 1
sp_bits: .res 1
sp_buf:  .res 1
sp_k:    .res 1

        .segment "DLIST"
_dlist:  .res $60

        .segment "PMGFREE"
_rw_chb: .res ROWS
_rw_bk:  .res ROWS
_rw_c0:  .res ROWS
_rw_c1:  .res ROWS
_rw_c2:  .res ROWS
_rw_c3:  .res ROWS
_dli_list: .res ROWS
dli_idx: .res 1
anim_t:  .res 1
anim_page: .res 1
nx_chb:  .res 1
nx_bk:   .res 1
nx_c0:   .res 1
nx_c1:   .res 1
nx_c2:   .res 1
nx_c3:   .res 1

        .segment "BSS2"
_dsp_dma: .res 1        ; SDMCTL: 0 while a screen is built
_anim:   .res 1
_spr_x:  .res 4         ; HPOS
_spr_y:  .res 4         ; PMG line of the top (0: hidden)
_spr_fr: .res 4
_spr_big: .res 4        ; 1: twice the size
_spr_col: .res 4
drw_y:   .res 8         ; as drawn, in buffer b at b * 4 + n
drw_fr:  .res 8
drw_big: .res 8
front:   .res 1         ; the PMG area shown: 0 PMG ($B800), 1 PMG2 ($B000)
flip:    .res 1         ; 1: the other is drawn, show it at the VBI
sh_hp:   .res 4         ; what goes with it
sh_hm:   .res 4
sh_col:  .res 4
sh_sz:   .res 4
sh_sizem: .res 1

        .code

; Low memory ($0480-$09FF: LOWBSS, BSS2) holds variables cc65 doesn't
; clear; picoboot used part of it while loading.
clrlow: lda     #0
        tax
@a:     sta     $0480,x
        sta     $0580,x
        sta     $0680,x
        sta     $0780,x
        sta     $0880,x
        inx
        bne     @a
        ldx     #$7F
@b:     sta     $0980,x
        dex
        bpl     @b
        rts

; void dsp_init(void)
_dsp_init:
        lda     #>CHARSETS
        sta     anim_page
        lda     #0
        sta     NMIEN
        lda     #<dli
        sta     VDSLST
        lda     #>dli
        sta     VDSLST+1
        lda     #<_dlist
        sta     SDLSTL
        lda     #>_dlist
        sta     SDLSTL+1
        lda     #6              ; immediate VBI
        ldy     #<vbi
        ldx     #>vbi
        jsr     SETVBV
        lda     #$C0
        sta     NMIEN
        rts

; A row's DLI stores the next row's registers, ready since the last DLI
; (or the VBI): little runs before the WSYNC, or the stores would miss the
; next row's first line. Then it readies the row after.
dli:    pha
        txa
        pha
        lda     nx_c2           ; a text row's background, the border and
        ldx     nx_bk           ; the text's brightness first: they show at
        sta     WSYNC           ; the first line's left edge
        sta     COLPF0+2
        stx     COLBK
        lda     nx_c1
        sta     COLPF0+1
        lda     nx_chb
        sta     CHBASE
        lda     nx_c0
        sta     COLPF0
        lda     nx_c3
        sta     COLPF0+3
        tya
        pha
        jsr     prep
        pla
        tay
        pla
        tax
        pla
        rti

; the next DLI's row, ready in nx_*
prep:   ldx     dli_idx
        inc     dli_idx
        ldy     _dli_list,x
        lda     _rw_chb,y
        cmp     #ANIM
        bne     @c
        lda     anim_page
@c:     sta     nx_chb
        lda     _rw_bk,y
        sta     nx_bk
        lda     _rw_c0,y
        sta     nx_c0
        lda     _rw_c1,y
        sta     nx_c1
        lda     _rw_c2,y
        sta     nx_c2
        lda     _rw_c3,y
        sta     nx_c3
        rts

vbi:    tsx                     ; the stack: Y X A P PCL PCH
        lda     $0104,x
        and     #$04
        bne     @irq
        cli
@irq:   lda     #0
        sta     ATRACT
        sta     dli_idx
        inc     anim_t
        lda     anim_t
        and     #3
        bne     @row0
        lda     _anim
        clc
        adc     #1
        and     #3
        sta     _anim
        asl
        asl
        adc     #>CHARSETS
        sta     anim_page
@row0:  lda     _dsp_dma
        sta     SDMCTL
        sta     DMACTL
        lda     #<_dlist
        sta     DLISTL
        lda     #>_dlist
        sta     DLISTL+1
        lda     _rw_chb
        cmp     #ANIM
        bne     @c
        lda     anim_page
@c:     sta     CHBASE
        sta     CHBAS
        lda     _rw_bk
        sta     COLBK
        sta     COLOR4
        lda     _rw_c0
        sta     COLPF0
        sta     COLOR0
        lda     _rw_c1
        sta     COLPF0+1
        sta     COLOR0+1
        lda     _rw_c2
        sta     COLPF0+2
        sta     COLOR0+2
        lda     _rw_c3
        sta     COLPF0+3
        sta     COLOR0+3
        lda     #$11            ; players over the playfield; missiles in PF3
        sta     PRIOR
        sta     GPRIOR
        jsr     prep            ; the first DLI's row
        lda     flip            ; the players drawn by spr_flush: show them
        beq     @nf
        lda     front
        eor     #1
        sta     front
        tax
        lda     pmpage,x
        sta     PMBASE
        ldx     #3
@sh:    lda     sh_hp,x
        sta     HPOSP0,x
        lda     sh_hm,x
        sta     HPOSM0,x
        lda     sh_col,x
        sta     COLPM0,x
        sta     PCOLR0,x
        lda     sh_sz,x
        sta     SIZEP0,x
        dex
        bpl     @sh
        lda     sh_sizem
        sta     SIZEM
        lda     #0
        sta     flip
@nf:    jsr     _snd_vbi
        lda     KEYDEL          ; the keyboard's debounce, stage 2's job
        beq     @kd
        dec     KEYDEL
@kd:    lda     #1              ; no stage 2: run late (after this VBI and the
        sta     CRITIC          ; serial IRQs), its DLISTL copy restarts the
        jmp     SYSVBV          ; display list mid-screen. We set it all here.

; ---- the players -----------------------------------------------------------

        .rodata
pmpage: .byte   >PMG, >PMG2

        .code

; void spr_flush(void): the players as spr_* say, drawn into the PMG area
; not shown, to be shown at the next VBI. (Not while the last isn't yet.)
_spr_flush:
        lda     flip
        beq     @go
        rts
@go:    lda     front
        eor     #1
        sta     sp_buf
        ldx     #3
@loop:  stx     sp_n
        txa
        ldy     sp_buf
        beq     @k
        ora     #4
@k:     sta     sp_k
        tay
        lda     _spr_y,x
        cmp     drw_y,y
        bne     @redraw
        lda     _spr_fr,x
        cmp     drw_fr,y
        bne     @redraw
        lda     _spr_big,x
        cmp     drw_big,y
        beq     @pos
@redraw:
        jsr     erase
        jsr     draw
        ldx     sp_n
@pos:   lda     _spr_y,x
        beq     @off
        lda     _spr_x,x
        sta     sh_hp,x
        ldy     _spr_fr,x
        lda     _spr_visor_x,y
        ldy     _spr_big,x
        beq     @n1
        asl
@n1:    clc
        adc     _spr_x,x
        sta     sh_hm,x
        jmp     @col
@off:   lda     #0
        sta     sh_hp,x
        sta     sh_hm,x
@col:   lda     _spr_col,x
        sta     sh_col,x
        lda     _spr_big,x
        sta     sh_sz,x
        dex
        bpl     @loop
        lda     #0              ; SIZEM: a visor is 4 colour clocks (8 big)
        ldx     #3
@sm:    asl
        asl
        ldy     _spr_big,x
        bne     @q
        ora     #1
        bne     @nx
@q:     ora     #3
@nx:    dex
        bpl     @sm
        sta     sh_sizem
        lda     #1
        sta     flip
        rts

; the pointers for player sp_n at line A in buffer sp_buf
at_line:
        sta     sp_dst
        sta     sp_mis
        ldx     sp_buf
        lda     pmpage,x
        clc
        adc     #3
        sta     sp_mis+1
        adc     #1
        adc     sp_n
        sta     sp_dst+1
        lda     sp_n            ; its missile's bits: 3 << 2n
        asl
        tay
        lda     #3
@b:     dey
        bmi     @d
        asl
        jmp     @b
@d:     sta     sp_bits
        rts

; erase player sp_n as drawn in buffer sp_buf (sp_k)
erase:  ldx     sp_k
        lda     #16
        ldy     drw_big,x
        beq     @h
        lda     #32
@h:     sta     sp_h
        lda     drw_y,x
        beq     @x
        jsr     at_line
        lda     sp_bits
        eor     #$FF
        sta     sp_bits
        ldy     #0
@l:     lda     #0
        sta     (sp_dst),y
        lda     (sp_mis),y
        and     sp_bits
        sta     (sp_mis),y
        iny
        cpy     sp_h
        bne     @l
@x:     rts

; draw player sp_n as spr_* say into buffer sp_buf (sp_k)
draw:   ldx     sp_n
        ldy     sp_k
        lda     _spr_y,x
        sta     drw_y,y
        lda     _spr_fr,x
        sta     drw_fr,y
        lda     _spr_big,x
        sta     drw_big,y
        lda     _spr_y,x
        bne     @go
        rts
@go:    jsr     at_line
        ldx     sp_n
        lda     _spr_fr,x       ; frame * 16
        ldy     #0
        sty     sp_src+1
        asl
        rol     sp_src+1
        asl
        rol     sp_src+1
        asl
        rol     sp_src+1
        asl
        rol     sp_src+1
        tay                     ; the low byte
        clc
        adc     #<_spr_visor
        sta     sp_vis
        lda     sp_src+1
        adc     #>_spr_visor
        sta     sp_vis+1
        tya
        clc
        adc     #<_spr_frames
        sta     sp_src
        lda     sp_src+1
        adc     #>_spr_frames
        sta     sp_src+1
        lda     _spr_big,x
        bne     @big
        ldy     #0
@s:     lda     (sp_src),y
        sta     (sp_dst),y
        lda     (sp_vis),y
        beq     @sn
        lda     (sp_mis),y
        ora     sp_bits
        sta     (sp_mis),y
@sn:    iny
        cpy     #16
        bne     @s
        rts
@big:   ldy     #0
@b:     sty     sp_i
        lda     (sp_vis),y
        sta     sp_v
        lda     (sp_src),y
        pha
        tya
        asl
        tay
        pla
        sta     (sp_dst),y
        iny
        sta     (sp_dst),y
        lda     sp_v
        beq     @bn
        lda     (sp_mis),y
        ora     sp_bits
        sta     (sp_mis),y
        dey
        lda     (sp_mis),y
        ora     sp_bits
        sta     (sp_mis),y
@bn:    ldy     sp_i
        iny
        cpy     #16
        bne     @b
        rts

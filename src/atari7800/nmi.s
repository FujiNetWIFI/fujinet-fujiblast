;
; nmi.s -- the display's interrupts: a DLI where a zone needs MARIA set
; otherwise than the zone above (CHARBASE, CTRL, BACKGRND: the tables
; gfx.c's tables_build makes), and the frame's DLI on the blank below the
; screen, which keeps the clock, plays the sound, turns the arena's
; character set over and shows what gfx.c composed.
;
; The DLI for zone Z comes right after MARIA has fetched zone Z-1's last
; line; MAME applies the read mode and the colours a line at a time, as the
; line's DMA starts. So (M.U.L.E.'s probe, tools/a78sim/probe in the mule
; repo):
;   at once   CHARBASE, CTRL (the read mode already zone Z's unless Z is
;             320A under a 160 zone: a 320A -> 160 switch lands a line
;             early, on a text row's last line, which the font leaves
;             blank), BACKGRND
;   160 -> 320A: WSYNC, WSYNC (raster C's start), then the read mode: a line
;             late if the zone above was heavy, on the text's first line
; MARIA's own WSYNC ($24); nothing here touches the TIA.
;
; The clock is frame_count in jiffies of 1/60 s however fast the console
; runs: NTSC (59.958 Hz) counts one more every 1421 frames, PAL (49.920)
; six for five frames and one more every 523.
;
        .include "a78.inc"
        .export _nmi, _irq, _frame_count, _mt_start, _dsp_ready
        .export _lv_cb, _lv_ca, _lv_cb2, _lv_bg, _lv_f, _dll_off, _mt_ntsc, _mt_top, _anim
        .export _zflip, _zfront, _mt_dll, _slot_lo, _slot_hi
        .export _dlslots_first, _mt_load0
        .import _snd_vbi
        .import __DLSLOTS_RUN__, __DLSLOTS2_RUN__

        .segment "ZEROPAGE"
dcb:    .res 1          ; the next DLI's CHARBASE, CTRL, BACKGRND, flags, late CTRL
dca:    .res 1
dbg:    .res 1
df:     .res 1
dcb2:   .res 1
dord:   .res 1          ; and its place in the tables
anim8:  .res 1          ; the arena's set * 8, added to the animated CHARBASE
tmp:    .res 1

        .segment "CONSBSS"      ; (console RAM: read in the DLI)
_lv_cb:  .res NDLI
_lv_ca:  .res NDLI
_lv_cb2: .res NDLI
_lv_bg:  .res NDLI
_lv_f:   .res NDLI      ; 7 late (160 -> 320A), 6 the frame's, 0 animated
_dll_off: .res ZONES    ; zone z's DLL entry: _mt_dll + dll_off[z]
_mt_dll: .res 3 * (ZONES + 8)
_zflip:  .res ZONES     ; composed into the back slot: show it next frame
_zfront: .res ZONES     ; the slot shown (0, 1)

        .segment "DLSLOTS"
_dlslots: .res 33 * 64
        .segment "DLSLOTS2"
_dlslots2: .res 17 * 64

        .bss
.ifdef NMILOG
nlog_i: .res 1
nlog:   .res 128
        .export nlog, nlog_i
.endif
_frame_count: .res 2
_dsp_ready: .res 1      ; gfx.c: the zones are composed, flip them
_mt_ntsc: .res 1
_mt_top:  .res 1        ; the DLL's blank entries above the zones
_anim:    .res 1
anim_t:   .res 1
c5:       .res 1
cfix:     .res 2

        .code

; 64-byte display-list slots: zone z's slot s is number z*2+s
_dlslots_first = __DLSLOTS_RUN__

.macro  slottab part
        .repeat ZONES * 2, i
        .if i < 33
        .byte   part (__DLSLOTS_RUN__ + i * 64)
        .else
        .byte   part (__DLSLOTS2_RUN__ + (i - 33) * 64)
        .endif
        .endrepeat
.endmacro
_slot_lo:
        slottab <
_slot_hi:
        slottab >

; void mt_start(void): the DLL's offsets and entries are gfx.c's; this
; arms the first DLI, the clock and DMA
_mt_start:
.ifdef NMILOG
        lda     #$FF
        sta     $0296           ; TIM64T
.endif
        lda     #0
        sta     _frame_count
        sta     _frame_count+1
        sta     c5
        sta     anim_t
        sta     _anim
        sta     anim8
        jsr     fixreset
        jsr     load0
@vb:    bit     MSTAT           ; in a blank: DMA on, CTRL as zone 0's DLI will
        bpl     @vb
        lda     dca
        sta     CTRL
        rts

fixreset:
        lda     _mt_ntsc
        beq     @pal
        lda     #<1421
        sta     cfix
        lda     #>1421
        sta     cfix+1
        rts
@pal:   lda     #<523
        sta     cfix
        lda     #>523
        sta     cfix+1
        rts

_mt_load0 = load0
; the first DLI's (zone 0's) values
load0:  ldx     #0
        stx     dord
; X = the ordinal: its values into dcb.. (CHARBASE moves with the arena set
; when flagged)
load:   lda     _lv_f,x
        sta     df
        lda     _lv_cb,x
        sta     dcb
        lda     df
        and     #1
        beq     @s
        lda     dcb
        clc
        adc     anim8
        sta     dcb
@s:     lda     _lv_ca,x
        sta     dca
        lda     _lv_cb2,x
        sta     dcb2
        lda     _lv_bg,x
        sta     dbg
        rts

; ---- the DLI --------------------------------------------------------------------
_nmi:   pha
.ifdef NMILOG
        txa
        pha
        ldx     nlog_i
        cpx     #63
        bcc     @lg
        ldx     #0
@lg:
        lda     $0284           ; INTIM
        sta     nlog,x
        lda     dord
        sta     nlog+64,x
        inx
        stx     nlog_i
        pla
        tax
.endif
        lda     dcb
        sta     CHARBASE
        lda     dca
        sta     CTRL
        lda     dbg
        sta     BACKGRND
        txa
        pha
        ldx     dord
        bit     df
        bmi     late
        bvs     frame
next:   inx
        stx     dord
        jsr     load
        pla
        tax
        pla
_irq:   rti
late:   lda     dcb2
        sta     WSYNC
        sta     WSYNC
        sta     CTRL
        jmp     next

        ; ---- the frame ----
        ; zone 0's values first: should this run on into the next frame's
        ; DLIs, they nest in it with the tables in order (a frame's
        ; glitch at worst, never every zone after off by one)
frame:  tya
        pha
        jsr     load0
        jsr     clock
        jsr     _snd_vbi
        lda     _dsp_ready
        beq     @done
        ldy     #ZONES - 1      ; the zones just composed
@f:     lda     _zflip,y
        beq     @nf
        lda     #0
        sta     _zflip,y
        lda     _zfront,y
        eor     #1
        sta     _zfront,y
        sta     tmp
        tya
        asl     a
        ora     tmp
        tax                     ; z * 2 + the slot
        lda     _slot_hi,x
        pha
        lda     _slot_lo,x
        ldx     _dll_off,y
        sta     _mt_dll+2,x
        pla
        sta     _mt_dll+1,x
@nf:    dey
        bpl     @f
        lda     #0
        sta     _dsp_ready
@done:  inc     anim_t          ; the arena's sets turn over every 4 jiffies
        lda     anim_t
        and     #3
        bne     @na
        lda     _anim
        clc
        adc     #1
        and     #3
        sta     _anim
        asl     a
        asl     a
        asl     a
        sta     anim8
        jsr     load0           ; (the new CHARBASE)
@na:    pla
        tay
        pla
        tax
        pla
        rti

; frame_count: 60 a second
clock:  inc     _frame_count
        bne     @c1
        inc     _frame_count+1
@c1:    lda     _mt_ntsc
        bne     @fix
        inc     c5              ; PAL: a sixth jiffy every five frames
        lda     c5
        cmp     #5
        bcc     @fix
        lda     #0
        sta     c5
        jsr     @one
@fix:   lda     cfix
        bne     @dl
        dec     cfix+1
@dl:    dec     cfix
        lda     cfix
        ora     cfix+1
        bne     @r
        jsr     fixreset
@one:   inc     _frame_count
        bne     @r
        inc     _frame_count+1
@r:     rts

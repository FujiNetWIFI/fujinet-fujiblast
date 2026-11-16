;
; snd.s -- POKEY's voices 1 and 2, played by the immediate VBI (gfxa.s).
; POKEY's voices 3 and 4 clock the serial port for netstream (nstream.s,
; AUDCTL $28), so this writes AUDF1, AUDC1, AUDF2 and AUDC2 and nothing
; else: never AUDCTL, STIMER (it resets the serial clock's dividers too),
; SKCTL or IRQEN. Keep it short: the VBI holds off the serial IRQ.
;
; Three players ("slots", X = 0, 2, 4):
;   0  voice 1: a jingle, or the music's ch1 (the drums and the bass)
;   2  voice 2: the effects
;   4  the music's ch2 (the robots' chatter), on voice 2 while no effect is
;
; A sound is steps of three bytes: AUDF, AUDC, frames. Frames 0 ends it;
; $FE calls the steps at AUDF/AUDC (one level), $FD returns, $FF jumps there
; (the music, gen/atari/music.s). sound.c sets a slot going (snd_ptr, high
; byte last); snd_hold set keeps the VBI off slots 0 and 4 while sound.c
; starts both (the music's two channels start on the same frame).
;
; On PAL every fifth VBI steps slots 0 and 4 twice: the music's tempo.
;
        .export _snd_vbi, _snd_ptr, _snd_left, _snd_mute, _snd_hold
        .export _snd_loopon, _snd_pal

        .include "hw.inc"

        .segment "ZEROPAGE"
_snd_ptr:  .res 6       ; slot 0 at +0, 2 at +2, 4 at +4 (0 high: none)

        .segment "BSS2"
_snd_left: .res 6       ; frames left of the step
_snd_mute: .res 1       ; bit 0: all off; bit 1: no music (the loop)
_snd_hold: .res 1
_snd_loopon: .res 1     ; 2 while slot 0 plays the loop, else 0
_snd_pal:  .res 1       ; 1 on PAL
pal_n:     .res 1
cur_f:     .res 6       ; what the slot plays this frame
cur_c:     .res 6       ; (0: quiet)
ret:       .res 6       ; where $FD goes back to

        .code

_snd_vbi:
        lda     _snd_hold
        bne     @fx
        ldx     #0
        jsr     step
        ldx     #4
        jsr     step
        lda     _snd_pal
        beq     @fx
        dec     pal_n
        bpl     @fx
        lda     #4
        sta     pal_n
        ldx     #0
        jsr     step
        ldx     #4
        jsr     step
@fx:    ldx     #2
        jsr     step

        ldy     #0              ; voice 1: slot 0
        lda     _snd_mute
        lsr     a
        bcs     @q1             ; all off
        lda     _snd_mute
        and     _snd_loopon
        bne     @q1             ; no music, and slot 0 is playing it
        lda     cur_f
        sta     AUDF1
        ldy     cur_c
@q1:    sty     AUDC1

        ldy     #0              ; voice 2: an effect, else the chatter
        lda     _snd_mute
        lsr     a
        bcs     @q2
        ldx     #2
        lda     _snd_ptr+3
        bne     @v2
        lda     _snd_mute
        and     #2
        bne     @q2
        ldx     #4              ; (quiet when the loop isn't on)
@v2:    lda     cur_f,x
        sta     AUDF1+2
        ldy     cur_c,x
@q2:    sty     AUDC1+2
        rts

; slot X a frame on: its cur_f, cur_c this frame
step:   lda     _snd_left,x
        beq     @next
        dec     _snd_left,x
        rts
@next:  lda     _snd_ptr+1,x
        beq     @quiet
@fetch: lda     (_snd_ptr,x)
        sta     cur_f,x
        inc     _snd_ptr,x
        bne     :+
        inc     _snd_ptr+1,x
:       lda     (_snd_ptr,x)
        sta     cur_c,x
        inc     _snd_ptr,x
        bne     :+
        inc     _snd_ptr+1,x
:       lda     (_snd_ptr,x)
        beq     @end
        cmp     #$FD
        bcs     @code
        sbc     #0              ; (carry clear: frames - 1 left after this one)
        sta     _snd_left,x
        inc     _snd_ptr,x
        bne     :+
        inc     _snd_ptr+1,x
:       rts
@end:   sta     _snd_ptr+1,x
@quiet: lda     #0
        sta     cur_c,x
        rts

@code:  beq     @ret            ; $FD
        cmp     #$FF
        beq     @jump
        lda     _snd_ptr,x      ; $FE: back to the step after this
        clc
        adc     #1
        sta     ret,x
        lda     _snd_ptr+1,x
        adc     #0
        sta     ret+1,x
@jump:  lda     cur_f,x
        sta     _snd_ptr,x
        lda     cur_c,x
        sta     _snd_ptr+1,x
        jmp     @fetch
@ret:   lda     ret,x
        sta     _snd_ptr,x
        lda     ret+1,x
        sta     _snd_ptr+1,x
        jmp     @fetch

;
; snd.s -- two voices, played by the immediate VBI (gfxa.s): voice 1 the
; jingles, voice 2 the effects. POKEY's voices 3 and 4 clock the serial
; port for netstream (nstream.s), so they are never touched.
;
; A sound is steps of three bytes: AUDF, AUDC, frames; frames 0 ends it.
; sound.c sets a voice going (snd_ptr, high byte last).
;
        .export _snd_vbi, _snd_ptr, _snd_left, _snd_mute

AUDF1   = $D200
AUDC1   = $D201

        .segment "ZEROPAGE"
_snd_ptr:  .res 4       ; voice 1 at +0, voice 2 at +2 (0 high: none)

        .segment "BSS2"
_snd_left: .res 4       ; frames left of the step
_snd_mute: .res 1
cur_f:     .res 4
cur_c:     .res 4

        .code

_snd_vbi:
        ldx     #0
        jsr     voice
        ldx     #2
voice:  lda     _snd_left,x
        bne     @play
        lda     _snd_ptr+1,x
        beq     @quiet
        lda     (_snd_ptr,x)
        sta     cur_f,x
        jsr     next
        lda     (_snd_ptr,x)
        sta     cur_c,x
        jsr     next
        lda     (_snd_ptr,x)
        beq     @end
        sta     _snd_left,x
        jsr     next
@play:  dec     _snd_left,x
        lda     _snd_mute
        bne     @quiet
        lda     cur_f,x
        sta     AUDF1,x
        lda     cur_c,x
        sta     AUDC1,x
        rts
@end:   lda     #0
        sta     _snd_ptr+1,x
@quiet: lda     #0
        sta     AUDC1,x
        rts

next:   inc     _snd_ptr,x
        bne     @x
        inc     _snd_ptr+1,x
@x:     rts

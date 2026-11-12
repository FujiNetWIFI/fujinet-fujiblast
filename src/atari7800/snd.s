;
; snd.s -- the Atari 7800 copy of src/atari/snd.s: two voices, played from
; the frame's NMI (nmi.s): voice 1 the jingles, voice 2 the effects, on the
; FujiNet cartridge's POKEY ($0450; the 64 kHz clock). Nothing shares the
; voices here (no netstream), so 3 and 4 are simply unused. Never a store
; to $00-$1F.
;
; A sound is steps of three bytes: AUDF, AUDC, frames; frames 0 ends it.
; sound.c sets a voice going (snd_ptr, high byte last).
;
        .export _snd_vbi, _snd_ptr, _snd_left, _snd_mute

AUDF1   = $0450
AUDC1   = $0451

        .segment "ZEROPAGE"
_snd_ptr:  .res 4       ; voice 1 at +0, voice 2 at +2 (0 high: none)

        .bss
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

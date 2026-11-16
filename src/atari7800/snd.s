; snd.s -- the Atari 7800 copy of src/atari/snd.s: POKEY's voices on the
; FujiNet cartridge ($0450; the 64 kHz clock), played from the frame's NMI
; (nmi.s, through bank.c's snd_frame: this code and the music live in bank 1,
; segment SOUND). Nothing here shares the voices (no netstream), so the
; music's ch2 has voice 3 to itself. Never a store to $00-$1F.
;
; Three players ("slots", X = 0, 2, 4), each on its own voice (AUDF1+X):
;   0  voice 1: a jingle, or the music's ch1 (the drums and the bass)
;   2  voice 2: the effects
;   4  voice 3: the music's ch2 (the robots' chatter)
;
; A sound is steps of three bytes: AUDF, AUDC, frames. Frames 0 ends it;
; $FE calls the steps at AUDF/AUDC (one level), $FD returns, $FF jumps there
; (the music, gen/atari7800/music.s). sound.c sets a slot going (snd_ptr, high
; byte last); snd_hold set keeps the NMI off slots 0 and 4 while sound.c
; starts both (the music's two channels start on the same frame).
;
; On PAL every fifth frame steps slots 0 and 4 twice: the music's tempo.
;
        .export _snd_vbi, _snd_ptr, _snd_left, _snd_mute, _snd_hold
        .export _snd_loopon
        .import _mt_ntsc

AUDF1   = $0450
AUDC1   = $0451

        .segment "ZEROPAGE"
_snd_ptr:  .res 6       ; slot 0 at +0, 2 at +2, 4 at +4 (0 high: none)

        .bss
_snd_left: .res 6       ; frames left of the step
_snd_mute: .res 1       ; bit 0: all off; bit 1: no music (the loop)
_snd_hold: .res 1
_snd_loopon: .res 1     ; 2 while slot 0 plays the loop, else 0
pal_n:     .res 1
cur_f:     .res 6       ; what the slot plays this frame
cur_c:     .res 6       ; (0: quiet)
ret:       .res 6       ; where $FD goes back to

        .segment "SOUND"

_snd_vbi:
        lda     _snd_hold
        bne     @fx
        ldx     #0
        jsr     step
        ldx     #4
        jsr     step
        lda     _mt_ntsc
        bne     @fx
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

        ldy     #0              ; voice 2: the effects
        lda     _snd_mute
        lsr     a
        bcs     @q2
        lda     cur_f+2
        sta     AUDF1+2
        ldy     cur_c+2
@q2:    sty     AUDC1+2

        ldy     #0              ; voice 3: the chatter
        lda     _snd_mute
        and     #3
        bne     @q3             ; all off, or no music
        lda     cur_f+4
        sta     AUDF1+4
        ldy     cur_c+4
@q3:    sty     AUDC1+4
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

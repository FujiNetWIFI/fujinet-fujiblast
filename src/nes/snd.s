; snd.s -- the round's music (gen/nes/music.s from music/play.txt) on the NES's
; APU, stepped a jiffy at a time by the NMI (crt0.s), not by the game loop:
; the loop makes up passes when drawing is slow, and the drums would drift.
;
; ch1 (slot 0): the triangle (bass, kick) or, for a snare or hat, the noise
; channel. ch2 (slot 2): pulse 2. The effects (sound.c) have pulse 1 and
; the noise channel too: snd_fxnoise is set while one holds the noise, and
; the drums keep out of it. The jingles use pulse 2, but only while the loop
; is off.
;
; Steps (A, B, n) as in tools/mkmus_nes.py: n jiffies; n = $FE call (one
; level), $FD return, $FF jump, 0 end.

        .export _mus_nmi
        .exportzp _snd_ptr
        .export _snd_left, _snd_mute, _snd_hold, _snd_loopon, _snd_fxnoise
        .export _snd_hi2, _snd_p2on

PULSE2  = $4004
TRI     = $4008
NOISEV  = $400C

        .segment "ZEROPAGE"
_snd_ptr:  .res 4       ; slot 0 at +0, slot 2 at +2 (0 high: none)

        .segment "BSS"
_snd_left: .res 4       ; jiffies left of the step
_snd_mute: .res 1       ; bit 0: all off; bit 1: no music (the loop)
_snd_hold: .res 1       ; the NMI leaves the slots alone (they are being started)
_snd_loopon: .res 1     ; 2 while the loop plays
_snd_fxnoise: .res 1    ; an effect (or jingle) has the noise channel
_snd_hi2:  .res 1       ; the high bits last written to pulse 2's $4007 (a write restarts the wave)
_snd_p2on: .res 1       ; the loop has pulse 2
cur_a:     .res 4       ; the step a slot plays
cur_b:     .res 4
ret:       .res 4       ; where $FD goes back to
o1:        .res 1       ; what to sound this frame (0: nothing)
o2:        .res 1
drumon:    .res 1       ; a drum has the noise channel

        .segment "CODE"

_mus_nmi:
        lda     _snd_hold
        beq     @run
        rts
@run:
        ldx     #0
        jsr     step
        ldx     #2
        jsr     step

        lda     _snd_mute
        beq     @live
        lda     #0              ; muted: the steps go on, nothing sounds
        sta     o1
        sta     o2
        beq     @go
@live:  lda     cur_b
        sta     o1
        lda     cur_b+2
        sta     o2

@go:    lda     o1              ; ---- noise: a drum
        bpl     @nodrum
        lda     _snd_fxnoise
        bne     @fxown
        lda     o1
        and     #$0F
        ora     #$30
        sta     NOISEV
        lda     cur_a
        sta     NOISEV+2
        lda     #$08
        sta     NOISEV+3
        lda     #1
        sta     drumon
        bne     @tri
@fxown: lda     #0
        sta     drumon
        beq     @tri
@nodrum:
        lda     drumon
        beq     @tri
        lda     #0
        sta     drumon
        lda     _snd_fxnoise
        bne     @tri
        lda     #$30
        sta     NOISEV
@tri:   lda     o1              ; ---- triangle
        and     #$40
        beq     @triq
        lda     #$FF
        sta     TRI
        lda     cur_a
        sta     TRI+2
        lda     o1
        and     #7
        sta     TRI+3
        bpl     @p2             ; (always)
@triq:  lda     #$80
        sta     TRI
@p2:    lda     o2              ; ---- pulse 2
        and     #$0F
        beq     @p2q
        ora     #$B0            ; 50%, halt, constant volume
        sta     PULSE2
        lda     cur_a+2
        sta     PULSE2+2
        lda     o2
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        and     #7
        cmp     _snd_hi2
        beq     @same
        sta     _snd_hi2
        ora     #$08
        sta     PULSE2+3
@same:  lda     #1
        sta     _snd_p2on
@done:  rts
@p2q:   lda     _snd_p2on
        beq     @done
        lda     #0
        sta     _snd_p2on
        lda     #$30
        sta     PULSE2
        rts

; step: a jiffy of slot X (0 or 2)
step:   lda     _snd_left,x
        beq     @next
        dec     _snd_left,x
        rts
@next:  lda     _snd_ptr+1,x
        beq     @quiet
@fetch: lda     (_snd_ptr,x)
        sta     cur_a,x
        inc     _snd_ptr,x
        bne     :+
        inc     _snd_ptr+1,x
:       lda     (_snd_ptr,x)
        sta     cur_b,x
        inc     _snd_ptr,x
        bne     :+
        inc     _snd_ptr+1,x
:       lda     (_snd_ptr,x)
        beq     @end
        cmp     #$FD
        bcs     @code
        sbc     #0              ; (carry clear: n - 1 left after this one)
        sta     _snd_left,x
        inc     _snd_ptr,x
        bne     :+
        inc     _snd_ptr+1,x
:       rts
@end:   sta     _snd_ptr+1,x
@quiet: lda     #0
        sta     cur_b,x
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
@jump:  lda     cur_a,x
        sta     _snd_ptr,x
        lda     cur_b,x
        sta     _snd_ptr+1,x
        jmp     @fetch
@ret:   lda     ret,x
        sta     _snd_ptr,x
        lda     ret+1,x
        sta     _snd_ptr+1,x
        jmp     @fetch

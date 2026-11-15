*
* sound3a.asm -- the CoCo 3's sound: the GIME timer's IRQ, once a half wave
* of the tone (src/coco3/sound3.c sets the timer to the half wave's length),
* flips the 6-bit DAC between the volume and 0. Its IRQ comes by the same
* vector as the field sync's (the game's irq_isr, plat.c): the vector's
* target is one of these three entries, which take the timer's and pass the
* rest on.
*
*   irq3    a tone: _ph toggles $00/$FF, the DAC is _lev & _ph
*   irq3h   a tone too low for the 12-bit timer: every second IRQ
*   irq3n   noise: an LFSR a step, the DAC _lev or 0 by its low bit
*
        section code

        import  _ph,_hf,_lev,_lf
        import  _irq_isr
        export  _irq3,_irq3h,_irq3n

_irq3
        lda     $FF92           ; the GIME's IRQ sources, acknowledged
        bita    #$20            ; the timer?
        beq     @other
        com     _ph
        lda     _lev
        anda    _ph
        sta     $FF20
        rti
@other  jmp     _irq_isr        ; no: the field sync's (the keyboard)

_irq3h
        lda     $FF92
        bita    #$20
        beq     @other
        com     _hf
        bmi     @skip           ; (every second one)
        com     _ph
        lda     _lev
        anda    _ph
        sta     $FF20
@skip   rti
@other  jmp     _irq_isr

_irq3n
        lda     $FF92
        bita    #$20
        beq     @other
        lda     _lf             ; the LFSR a step
        lsra
        bcc     @nf
        eora    #$B8
@nf     sta     _lf
        anda    #1
        beq     @o
        lda     _lev
@o      sta     $FF20
        rti
@other  jmp     _irq_isr

        endsect

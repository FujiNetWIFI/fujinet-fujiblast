*
* sound3a.asm -- the CoCo 3's sound: the GIME timer's IRQ, ~4.4 kHz, mixes
* two voices onto the 6-bit DAC (src/coco3/sound3.c sets them up). Its IRQ
* comes by the same vector as the field sync's (the game's irq_isr, plat.c):
* irq3 is the vector's target, takes the timer's and passes the rest on.
*
*   voice 1  the effects: a square wave (phase v1p stepped by v1s, high for
*            the top half) or noise (an LFSR, clocked when the phase wraps),
*            at volume v1v (0-31)
*   voice 2  the jingles: a square wave, v2p, v2s, v2v
*
        section code

        import  _v1p,_v1s,_v1v,_v1n,_lf,_v2p,_v2s,_v2v
        import  _irq_isr
        export  _irq3

_irq3
        lda     $FF92           ; the GIME's IRQ sources, acknowledged
        anda    #$20            ; the timer?
        bne     @tick
        jmp     _irq_isr        ; no: the field sync's (the keyboard)
@tick   ldd     _v1p
        addd    _v1s
        std     _v1p
        bcc     @s1
        tst     _v1n
        beq     @s1
        lda     _lf             ; noise: the LFSR a step
        lsra
        bcc     @nf
        eora    #$B8
@nf     sta     _lf
@s1     tst     _v1n
        bne     @noise
        ldb     _v1v
        tst     _v1p            ; the top bit of the phase
        bmi     @o1
        clrb
        bra     @o1
@noise  clrb
        lda     _lf
        anda    #1
        beq     @o1
        ldb     _v1v
@o1     pshs    b
        ldd     _v2p
        addd    _v2s
        std     _v2p
        bmi     @hi2            ; (std left the phase's top bit in N)
        clrb
        bra     @o2
@hi2    ldb     _v2v
@o2     addb    ,s+
        aslb
        aslb
        stb     $FF20
        rti

        endsect

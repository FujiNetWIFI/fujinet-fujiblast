;
; mock.s -- the Mockingboard: found, and played a jiffy at a time.
;
; One AY-3-8910 (the first, at $Cn00): channel A the jingles, channel B the
; effects. The sounds are the Atari's (src/apple2/sndtab.s, in aux RAM):
; steps of AUDF, AUDC, jiffies (0 jiffies ends it). AUDF is POKEY's
; divider, so the AY's period (1.023 MHz / 16 / f) is 2 (AUDF + 1); AUDC's
; low nibble is the volume, $8x noise (its period AUDF / 8), else a tone.
; In the language card: it reads the steps with RAMRD on.
;
        .export         _mb_detect, _mb_start, _mb_tick, _mb_quiet
        .exportzp       _mb_slot
        .importzp       src2, shp
        .import         popa

RAMRDOFF := $C002
RAMRDON  := $C003

        .zeropage
_mb_slot: .res 1                ; $Cn: the board's page, 0 none

mbp     = shp                   ; (the blitter's: free between its calls)
sp      = src2

        .bss
v_lo:   .res 2                  ; each voice's step pointer (0 high: quiet)
v_hi:   .res 2
v_left: .res 2                  ; jiffies left of the step
mix:    .res 1                  ; R7 as written
tmp:    .res 1

        .segment "LC"

; uint8_t mb_detect(void): the slot's page ($C1-$C7) or 0. A 6522's timer
; 1 counts down every cycle: two reads 109 cycles apart differ by about that
; (emulators' timers move in steps).
_mb_detect:
        lda     #$C7
@slot:  sta     mbp+1
        lda     #0
        sta     mbp
        ldy     #4              ; start timer 1 (emulators' 6522s don't count
        lda     #$FF            ; until it is; a real one always does; other
        sta     (mbp),y         ; cards' $Cnxx is ROM, which takes no writes)
        iny
        sta     (mbp),y
        dey
        lda     (mbp),y         ; (5 cycles: read on the last)
        sta     tmp             ; 3
        ldx     #20             ; 2
:       dex                     ; 20 x 5 - 1 = 99
        bne     :-
        lda     (mbp),y         ; 5: 109 cycles after
        eor     #$FF            ; tmp - A, the cycles it counted
        sec
        adc     tmp
        sec
        sbc     #100            ; 100..120: a 6522 counting
        cmp     #21
        bcs     @no
        lda     mbp+1           ; found: set the 6522 and quiet the AY
        sta     _mb_slot
        ldy     #3
        lda     #$FF            ; DDRA: the AY's bus
        sta     (mbp),y
        dey
        lda     #$07            ; DDRB: BC1, BDIR, RESET
        sta     (mbp),y
        ldy     #0
        tya
        sta     (mbp),y         ; reset
        lda     #$04
        sta     (mbp),y
        jsr     _mb_quiet
        lda     _mb_slot
        ldx     #0
        rts
@no:    lda     mbp+1
        sec
        sbc     #1
        cmp     #$C0
        bne     @slot
        lda     #0
        sta     _mb_slot
        tax
        rts

; AY register X = A
ayw:    pha
        lda     _mb_slot
        sta     mbp+1
        lda     #0
        sta     mbp
        txa
        ldy     #1
        sta     (mbp),y         ; ORA: the register
        dey
        lda     #$07            ; latch the address
        sta     (mbp),y
        lda     #$04
        sta     (mbp),y
        pla
        iny
        sta     (mbp),y         ; ORA: the value
        dey
        lda     #$06            ; write
        sta     (mbp),y
        lda     #$04
        sta     (mbp),y
        rts

; void mb_quiet(void): both voices off, the AY silent
_mb_quiet:
        lda     #0
        sta     v_hi
        sta     v_hi+1
        ldx     #8
        jsr     ayw             ; volumes 0
        lda     #0
        ldx     #9
        jsr     ayw
        lda     #0
        ldx     #10
        jsr     ayw
        lda     #$3F            ; tones and noise off
        sta     mix
        ldx     #7
        jmp     ayw

; void __fastcall__ mb_start(uint8_t voice, const void *steps): steps an
; aux address (sndtab.s); voice 0 the jingles, 1 the effects
_mb_start:
        sta     sp
        stx     sp+1
        jsr     popa
        tay
        lda     sp
        sta     v_lo,y
        lda     #0
        sta     v_left,y
        lda     sp+1
        sta     v_hi,y          ; (high last: mb_tick reads it)
        rts

; void mb_tick(void): a jiffy of both voices
_mb_tick:
        lda     _mb_slot
        beq     @x
        ldx     #0
        jsr     voice
        ldx     #1
        jsr     voice
@x:     rts

voice:  lda     v_left,x
        beq     @step
        dec     v_left,x
        rts
@step:  lda     v_hi,x
        bne     :+
        jmp     @off
:       sta     sp+1
        lda     v_lo,x
        sta     sp
        stx     tmp
        sta     RAMRDON         ; the steps are in aux RAM
        ldy     #2
        lda     (sp),y          ; jiffies
        bne     :+
        jmp     @end
:       sta     v_left,x
        dey
        lda     (sp),y          ; AUDC
        sta     ac
        dey
        lda     (sp),y          ; AUDF
        sta     RAMRDOFF
        sta     af
        lda     sp              ; on to the next step
        clc
        adc     #3
        sta     v_lo,x
        bcc     :+
        inc     v_hi,x
:       dec     v_left,x
        lda     ac
        and     #$F0
        cmp     #$80
        beq     @noise
        lda     #0              ; a tone: period 2 (f + 1)
        sta     hi
        lda     af
        clc
        adc     #1
        rol     hi              ; (f = 255: 256)
        asl     a
        rol     hi
        ldx     tmp
        jsr     period
        ldx     tmp
        lda     tbit,x          ; tone on, noise off
        eor     #$FF
        and     mix
        ora     nbit,x
        jmp     @mix
@noise: lda     af              ; noise: period f / 8
        lsr     a
        lsr     a
        lsr     a
        ldx     #6
        jsr     ayw
        ldx     tmp
        lda     nbit,x          ; noise on, tone off
        eor     #$FF
        and     mix
        ora     tbit,x
@mix:   sta     mix
        ldx     #7
        jsr     ayw
        ldx     tmp
        lda     ac
        and     #$0F
        pha
        txa
        clc
        adc     #8
        tax
        pla
        jmp     ayw             ; the volume
@end:   sta     RAMRDOFF
        sta     v_hi,x
@off:   txa                     ; quiet
        clc
        adc     #8
        tax
        lda     #0
        jmp     ayw

; the period: low A, high in hi, voice in X (registers 2X, 2X+1)
period: pha
        txa
        asl     a
        tax
        pla
        stx     rg
        jsr     ayw
        lda     hi
        ldx     rg
        inx
        jmp     ayw

tbit:   .byte   $01, $02        ; R7: tone off, by channel
nbit:   .byte   $08, $10        ; noise off

        .bss
ac:     .res 1
af:     .res 1
hi:     .res 1
rg:     .res 1

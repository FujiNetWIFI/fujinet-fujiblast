;
; lowlevel.s -- the Apple II's hardware outside the blitter: the blank's
; setup, the display modes, the joystick, the speaker, ProDOS, AUXMOVE.
; (From M.U.L.E.'s src/apple2/lowlevel.s.)
;

        .export         _vbl_init, _hgr_on, _hgr_off, _joy_read, _snd_tone
        .export         _mli, _aux_put, _aux_get
        .exportzp       _joy_x, _joy_y
        .importzp       _vbl_mode
        .import         popa, popax

PTRIG     := $C070
PADDL0    := $C064
PADDL1    := $C065
SPKR      := $C030
ENVBL     := $C05B
IOUDISON  := $C07E
IOUDISOFF := $C07F

        .zeropage
_joy_x: .res 1
_joy_y: .res 1

        .segment "LOWCODE"

; void vbl_init(uint8_t mode): 0 //e, 1 IIgs, 2 //c. The //c's VBL flag is
; enabled with the CPU's IRQs masked (the game never takes interrupts), and
; IOUDIS left on (the //c's VBL flag is read through it).
.proc   _vbl_init
        sta     _vbl_mode
        sei
        cmp     #2
        bne     :+
        sta     IOUDISOFF
        bit     ENVBL
        sta     IOUDISON
        bit     PTRIG
:       rts
.endproc

; High-res, page 1, full screen; 80STORE off (PAGE2 picks the page shown),
; 40 columns and AN3 on, so no double hi-res.
.proc   _hgr_on
        sta     $C000           ; 80STORE off
        sta     $C00C           ; 40 columns
        sta     $C05F           ; AN3 on: not double hi-res
        sta     $C050           ; graphics
        sta     $C052           ; full screen
        sta     $C057           ; hi-res
        sta     $C054           ; page 1
        rts
.endproc

.proc   _hgr_off
        sta     $C051
        sta     $C054
        rts
.endproc

; void joy_read(void): both paddle timers in one loop (37 cycles a count,
; about 77 at full travel at 1 MHz), into joy_x and joy_y. Timers still
; running (the //c's frame_tick reads PTRIG every blank) are waited out
; first, or the reading would come short.
.proc   _joy_read
        ldy     #0
:       lda     PADDL0
        ora     PADDL1
        bpl     :+
        dey
        bne     :-
:       lda     #0
        sta     _joy_x
        sta     _joy_y
        tax
        lda     PTRIG
loop:   lda     PADDL0
        bpl     :+
        inc     _joy_x
:       lda     PADDL1
        bpl     :+
        inc     _joy_y
:       inx
        beq     done
        lda     PADDL0
        ora     PADDL1
        bmi     loop
done:   rts
.endproc

; void snd_tone(uint8_t delay, uint8_t n): n clicks of the speaker, delay x
; 14 cycles apart (a square wave of 1020000 / (28 * delay) Hz).
.proc   _snd_tone
        sta     count
        jsr     popa
        sta     delay
        lda     count
        beq     done
loop:   bit     SPKR
        ldx     delay
:       pha
        pla
        nop
        dex
        bne     :-
        dec     count
        bne     loop
done:   rts
        .bss
count:  .res 1
delay:  .res 1
        .code
.endproc

; uint8_t __fastcall__ mli(uint8_t cmd, void *parms): a ProDOS MLI call
; (written into the code, which is RAM); its error, 0 for none.
.proc   _mli
        sta     parms
        stx     parms+1
        jsr     popa
        sta     cmd
        jsr     $BF00
cmd:    .byte   0
parms:  .addr   0
        ldx     #0
        rts
.endproc

; void aux_put(void *dst, const void *src, uint16_t n): main (src) to
; auxiliary RAM (dst), through the firmware's AUXMOVE (80STORE must be off);
; aux_get the other way (overlay.c).
; plat_boot calls it as a constructor, so nothing here may live in BSS.
A1L     := $3C
A2L     := $3E
A4L     := $42
AUXMOVE := $C311

.proc   _aux_get                ; the same, aux (src) to main (dst)
        clc
        bcc     auxmove
.endproc

.proc   _aux_put
        sec
        ; fall through
.endproc

.proc   auxmove
        php                     ; the direction
        sta     n
        stx     n+1
        jsr     popax           ; src
        sta     A1L
        stx     A1L+1
        jsr     popax           ; dst
        sta     A4L
        stx     A4L+1
        lda     n
        ora     n+1
        beq     none
        lda     A1L             ; A2 = src + n - 1
        clc
        adc     n
        sta     A2L
        lda     A1L+1
        adc     n+1
        sta     A2L+1
        lda     A2L
        bne     :+
        dec     A2L+1
:       dec     A2L
        plp
        jmp     AUXMOVE
none:   plp
        rts
        .data                   ; (not BSS: plat_boot runs before the
n:      .word   0               ; language card's image, in BSS, is copied)
        .code
.endproc

; plat.c's plat_boot runs as a constructor: before main, in the ONCE
; segment (BSS takes the room after).
        .import         _plat_boot
        .segment "ONCE"
boot:   jmp     _plat_boot
        .constructor    boot

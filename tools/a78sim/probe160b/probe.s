;
; probe.s -- the 160B probe: how much arena line can MARIA draw? Static
; screen, no DLI: 24 zones of 8 lines, each a different load (gen.py) of
; the objects a FujiBlast arena row has: a run of 160B indirect cells
; (CW=1), a left and a right panel, and 0-4 bombers (160B direct, 4 bytes
; wide). check.py reads the snapshot and says which objects MARIA drew.
;
        .setcpu "6502"
DLL      = $8900

BACKGRND = $20
MSTAT    = $28
DPPH     = $2C
DPPL     = $30
CHARBASE = $34
OFFSET   = $38
CTRL     = $3C

        .segment "CODE"
reset:  sei
        cld
        ldx     #$FF
        txs
        lda     #$7F
        sta     CTRL
        lda     #0
        sta     OFFSET
        sta     BACKGRND
        ; palettes: P0-P3 group 0, P4-P7 group 1 (colour 1 each distinct)
        ldx     #0
@pal:   lda     pals,x
        sta     $21,x
        inx
        cpx     #$1F
        bne     @pal
        lda     #0
        sta     OFFSET
        lda     #$90            ; CHARBASE: the char pages at $9000
        sta     CHARBASE
        lda     #>DLL
        sta     DPPH
        lda     #<DLL
        sta     DPPL
@vb:    bit     MSTAT
        bpl     @vb
        lda     #$50            ; DMA on, CW=1, 160 modes
        sta     CTRL
idle:   jmp     idle

; $21-$3F: P0..P7 C1-C3, the gaps are other registers (WSYNC, MSTAT, DPPH,
; DPPL, CHARBASE, OFFSET, CTRL): written, then set properly above/below
pals:   .byte   $34,$34,$34, 0, $84,$84,$84, 0, $C4,$C4,$C4, 0, $1C,$1C,$1C, 0
        .byte   $5A,$5A,$5A, 0, $2A,$2A,$2A, 0, $9E,$9E,$9E, 0, $46,$46,$46

nmi:    rti
irq:    rti

        .segment "CLAIM"
        .byte   "FUJI", 1, 0, 1, 0, 0, 0

        .segment "VECTORS"
        .byte   $FF, $87
        .word   nmi, reset, irq

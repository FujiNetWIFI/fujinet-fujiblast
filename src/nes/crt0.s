;
; crt0.s -- FujiBlast on the NES: the iNES header, start-up, the frame's
; interrupts and the PPU queue (src/nes/neshw.h). From M.U.L.E.'s
; src/nes/crt0.s, without its scanline-driven pattern table switching: the
; screen is a plain name table of 8x8 cells (one pattern table, 256 cells)
; and the arena's tile animation is name table writes.
;
; The NMI counts the frames, drains the PPU queue (a budget's worth), sends
; the sprites (OAM page $0200, unless the game is writing it: oam_lock) and
; puts the display back for the next frame.
;
; The status bar (split_on): the backdrop colour is bd_top for the first
; lines of the frame and bd_bot from line SPLIT_LINE+2 down. The NMI writes
; bd_top; the MMC3's scanline interrupt (a counter clocked by the sprites'
; pattern fetches) lands in the black band's last rows (solid black cells,
; so nothing shows of what it does). It turns rendering off at dot ~150 of
; its line (the dot M.U.L.E. found safe: switched off earlier, a 2C02 and
; MesenCE copy OAM row 0 over another row at the pre-render line), writes
; the backdrop in the next hblank, parks the video address on a palette
; entry that is black (with rendering off the PPU shows the colour the
; address points at), and a line later, in the hblank, sets the address for
; the line after (the picture goes on from there) and turns rendering on.
;
; Game code never touches the PPU while it shows a picture: drawing goes
; into the queue (ppuq) and the NMI drains it. While a whole screen is
; drawn, ppu_direct is set: the NMI turns the display off and leaves the
; PPU alone, and the game drains the queue itself as it fills.
;
; MMC3's bank select register is shared: the game sets R6 for the art
; (prg_r6), the interrupt writes nothing to it but the IRQ enable ($E000/
; $E001, which are not the select register).
;
        .export         __STARTUP__ : absolute = 1
        .export         _exit
        .export         _ppu_drain, _prg_r6
        .exportzp       _qhead, _qtail, _ppu_direct, _mmc3_sel, _ppu_off
        .exportzp       _oam_lock, _bd_top, _bd_bot, _split_on, _oam_show
        .export         _frame_count, _ppuq, _oam
        .import         initlib, donelib, callmain, zerobss, copydata
        .include        "zeropage.inc"

PPUCTRL   = $2000
PPUMASK   = $2001
PPUSTATUS = $2002
OAMADDR   = $2003
PPUSCROLL = $2005
PPUADDR   = $2006
PPUDATA   = $2007
OAMDMA    = $4014

CTRL_ON   = $A0                 ; NMI, 8 x 16 sprites, background at $0000
MASK_ALL  = $1E                 ; background and sprites, no clipping

IRQ_LINE  = 28                  ; the interrupt lands in this line
; the video address of line 32, where rendering resumes: row 4, fine Y 0
; ($2006's high byte can't set the fine Y's top bit: a row's first line)
SPLIT_V_HI = $00
SPLIT_V_LO = $80
OFF_DELAY = 9                   ; loops before rendering goes off (dot ~150)
GAP_DELAY = 2                   ; then the wait to the hblank
LINE_DELAY = 67                 ; and two lines on, the hblank before line 32

NMI_BUDGET = 88                 ; a byte copied is one unit (15 cycles), a command 8
CLK_EVERY  = 608                ; 60.0988 * 607 / 608 = 60.000 Hz

; ---------------------------------------------------------------------------
; The header: NES 2.0, mapper 4, 128K PRG, 16K CHR-RAM, 8K PRG-RAM.
.segment "HEADER"
        .byte   "NES", $1A
        .byte   8               ; PRG: 8 x 16K
        .byte   0               ; CHR-ROM: none (RAM)
        .byte   $41             ; mapper 4 (low), vertical mirroring
        .byte   $08             ; NES 2.0
        .byte   0, 0            ; mapper high, submapper; ROM size high
        .byte   $07             ; PRG-RAM 64 << 7 = 8K
        .byte   $08             ; CHR-RAM 64 << 8 = 16K
        .byte   0, 0, 0, 0

; ---------------------------------------------------------------------------
.segment "ZEROPAGE"
_qhead:      .res 1             ; the queue: where the game writes next
_qtail:      .res 1             ; where the drain reads next
_ppu_direct: .res 1
_mmc3_sel:   .res 1             ; the game's bank select (R6)
_ppu_off:    .res 1             ; the NMI has turned the display off
_oam_lock:   .res 1             ; the game is writing OAM: no DMA
_oam_show:   .res 1             ; the sprites' page the NMI sends (always OAM: tools/nessim/mesensim.py reads it)
_bd_top:     .res 1             ; the backdrop colour at the top of the frame
_bd_bot:     .res 1             ; and below the split
_split_on:   .res 1             ; 1: the interrupt changes it at SPLIT_LINE
budget:      .res 1
op:          .res 1
tmp:         .res 1
cnt:         .res 1
clk_n:       .res 2             ; frames to the next one not counted

.segment "BSS"
_frame_count: .res 2            ; (cleared by zerobss)

.segment "PPUQ"
_ppuq:       .res 256

.segment "OAM"
_oam:        .res 256

; ---------------------------------------------------------------------------
.segment "STARTUP"

start:
        sei
        cld
        ldx     #$FF
        txs
        inx
        stx     PPUCTRL         ; NMI off
        stx     PPUMASK
        stx     $4010           ; no DMC interrupt
        lda     #$40
        sta     $4017           ; no APU frame interrupt
        stx     $E000           ; no MMC3 interrupt
        bit     PPUSTATUS
@v1:    bit     PPUSTATUS
        bpl     @v1

        ; MMC3: PRG mode 0 ($8000 R6, $A000 R7, $C000 fixed), CHR mode 0
        ldy     #0
@mm:    sty     $8000
        lda     mmc3_init,y
        sta     $8001
        iny
        cpy     #8
        bne     @mm
        lda     #6
        sta     _mmc3_sel
        sta     $8000
        lda     #$80
        sta     $A001           ; WRAM on, writable
        lda     #0
        sta     $A000           ; vertical mirroring

        ; RAM: all of it zero (the stack's page too: nothing is on it yet)
        lda     #0
        tax
@clr:   sta     $00,x
        sta     $0100,x
        sta     $0200,x
        sta     $0300,x
        sta     $0400,x
        sta     $0500,x
        sta     $0600,x
        sta     $0700,x
        inx
        bne     @clr
        lda     #6
        sta     _mmc3_sel

        ; sprites all off the screen
        lda     #$F8
@ofs:   sta     _oam,x
        inx
        bne     @ofs

@v2:    bit     PPUSTATUS
        bpl     @v2

        lda     #>_oam
        sta     _oam_show
        jsr     zerobss
        jsr     copydata
        lda     #<$0800         ; the C stack: $0500-$07FF
        ldx     #>$0800
        sta     c_sp
        stx     c_sp+1
        lda     #1
        sta     _ppu_direct     ; the display stays off until gfx_init
        lda     #CTRL_ON
        sta     PPUCTRL         ; the frame clock
        cli                     ; (the scanline interrupt)
        jsr     initlib
        jsr     callmain
_exit:  jsr     donelib
        jmp     start

mmc3_init:
        .byte   0, 2, 12, 13, 14, 15    ; CHR: 0-3K the background's table; 12-15K the sprites'
        .byte   0, 7                    ; PRG: the art's bank; R7's bank

; ---------------------------------------------------------------------------
.segment "CODE"

; prg_r6(bank): the window at $8000 gets PRG bank A.
_prg_r6:
        ldx     #6
        stx     _mmc3_sel
        stx     $8000
        sta     $8001
        rts

; The display for the next frame: the backdrop for the top, scroll 0, and
; the interrupt for the split.
show:
        bit     PPUSTATUS
        lda     #$3F
        sta     PPUADDR
        lda     #0
        sta     PPUADDR
        lda     _bd_top
        sta     PPUDATA
        lda     #CTRL_ON
        sta     PPUCTRL
        lda     #0
        sta     PPUSCROLL
        sta     PPUSCROLL
        sta     PPUADDR
        sta     PPUADDR
        lda     #MASK_ALL
        sta     PPUMASK
        sta     $E000           ; (any value: the interrupt off, acknowledged)
        lda     _split_on
        beq     @r
        lda     #IRQ_LINE
        sta     $C000           ; the latch
        sta     $C001           ; reload at the next clock
        sta     $E001           ; on
@r:     rts

nmi:
        pha
        txa
        pha
        tya
        pha
        lda     clk_n           ; (one frame in CLK_EVERY isn't counted:
        bne     @cl             ; the server's clock is exactly 60 Hz)
        lda     clk_n+1
        bne     @ch
        lda     #<(CLK_EVERY - 1)
        sta     clk_n
        lda     #>(CLK_EVERY - 1)
        sta     clk_n+1
        jmp     @c
@ch:    dec     clk_n+1
@cl:    dec     clk_n
        inc     _frame_count
        bne     @c
        inc     _frame_count+1
@c:     lda     _ppu_direct
        bne     @direct
        lda     #NMI_BUDGET
        jsr     _ppu_drain
        lda     _oam_lock
        bne     @nodma
        lda     #0
        sta     OAMADDR
        lda     #>_oam
        sta     OAMDMA
@nodma: jsr     show
        jmp     @out
@direct:
        lda     #0
        sta     PPUMASK
        sta     $E000
        lda     #1
        sta     _ppu_off
@out:   pla
        tay
        pla
        tax
        pla
        rti

; The split (see the top). 7 cycles from the interrupt's line (dot ~260 of
; the one before) to the first instruction.
irq:
        pha
        txa
        pha
        tya
        pha
        sta     $E000           ; acknowledged, off
        ldy     #OFF_DELAY
@d1:    dey
        bne     @d1
        lda     #0
        sta     PPUMASK         ; rendering off, dot ~150 of the line
        ldy     #GAP_DELAY
@d2:    dey
        bne     @d2
        ; the hblank: the backdrop, then the address on palette entry 3
        bit     PPUSTATUS
        lda     #$3F
        ldx     #0
        ldy     _bd_bot
        sta     PPUADDR
        stx     PPUADDR
        sty     PPUDATA         ; (the address is $3F01 now: white, but it is the hblank)
        sta     PPUADDR
        lda     #3
        sta     PPUADDR         ; $3F03: black while rendering is off
        ldy     #LINE_DELAY
@d3:    dey
        bne     @d3
        ; the next line's hblank: the address for the line after, rendering on
        lda     #SPLIT_V_HI
        ldx     #SPLIT_V_LO
        ldy     #MASK_ALL
        sta     PPUADDR
        stx     PPUADDR
        sty     PPUMASK
        pla
        tay
        pla
        tax
        pla
        rti

; ---------------------------------------------------------------------------
; ppu_drain(budget): run queued commands while the budget lasts (255: all
; of them). Only with the PPU free: from the NMI, or the game while
; ppu_direct is set. A command is: n (1-127; bit 7: a fill), the address
; (high, low), then n bytes (a fill: one byte, written n times).
_ppu_drain:
        sta     budget
@next:  ldx     _qtail
        cpx     _qhead
        bne     @have
        rts
@have:  lda     _ppuq,x
        sta     op
        and     #$7F
        sta     cnt
        ldy     budget
        cpy     #255
        beq     @go
        lda     op
        bmi     @fcost
        lda     cnt             ; a copy: n + 8
        clc
        adc     #8
        bne     @chk
@fcost: lda     cnt             ; a fill: n/2 + 8
        lsr     a
        clc
        adc     #8
@chk:   cmp     budget
        beq     @take
        bcc     @take
        rts                     ; not this frame
@take:  sta     tmp
        lda     budget
        sec
        sbc     tmp
        sta     budget
@go:    inx
        bit     PPUSTATUS
        lda     _ppuq,x
        sta     PPUADDR
        sta     tmp             ; (a palette's: the address goes off it below)
        inx
        lda     _ppuq,x
        sta     PPUADDR
        inx
        ldy     cnt
        lda     op
        bmi     @fill
@cl:    lda     _ppuq,x
        sta     PPUDATA
        inx
        dey
        bne     @cl
        beq     @done
@fill:  lda     _ppuq,x
        inx
@fl:    sta     PPUDATA
        dey
        bne     @fl
@done:  stx     _qtail
        lda     tmp
        cmp     #$3F
        bne     @next
        ldy     #0              ; (with the display off, the colour the
        sty     PPUADDR         ; address points at is what the screen shows)
        sty     PPUADDR
        beq     @next

; ---------------------------------------------------------------------------
.segment "VECTORS"
        .word   nmi
        .word   start
        .word   irq

.segment "CLAIM"
        .byte   "FUJI"

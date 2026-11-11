;
; loader.s -- FUJIBLST.SYSTEM (built by the Makefile, put on the disk by
; apple2/disk-post): ProDOS runs it at $2000; it loads FUJIBLST
; and jumps to it. (In place of cc65's loader.system, whose I/O buffer at
; $BB00 would cap the program there.)
;
; FUJIBLST is the memory from $0803 up (src/apple2/fujiblst.cfg), with the
; DHGR pages' 16K of zeros in it: the loader reads $0803-$1FFF, skips the
; zeros (SET_MARK) and reads $6000 up to the end, with its I/O buffer in
; the skipped pages. The program may run to $BE00 (the C stack is above).
;
        .setcpu "6502"

MLI     := $BF00
OPEN    = $C8
READ    = $CA
SETMARK = $CE
CLOSE   = $CC
QUIT    = $65
LOW     = $0803
HIGH    = $6000
TOP     = $BE00
IOBUF   = $2400                 ; (this loader is at $2000-$20FF)

        .segment "LOADER"

start:  jsr     MLI             ; open FUJIBLST (the prefix is the boot volume)
        .byte   OPEN
        .addr   p_open
        bne     fail
        lda     p_open+5
        sta     p_read1+1
        sta     p_mark+1
        sta     p_read2+1
        sta     p_close+1
        jsr     MLI             ; $0803-$1FFF
        .byte   READ
        .addr   p_read1
        bne     fail
        jsr     MLI             ; past the pages' zeros
        .byte   SETMARK
        .addr   p_mark
        bne     fail
        jsr     MLI             ; $6000 up (the file ends where it ends)
        .byte   READ
        .addr   p_read2
        cmp     #$4C            ; (end of file: fine)
        beq     :+
        cmp     #0
        bne     fail
:       jsr     MLI
        .byte   CLOSE
        .addr   p_close
        jmp     LOW

fail:   ldx     #0              ; say so, wait for a key, back to ProDOS
:       lda     msg,x
        beq     :+
        ora     #$80
        sta     $0400,x
        inx
        bne     :-
:       lda     $C000
        bpl     :-
        sta     $C010
        jsr     MLI
        .byte   QUIT
        .addr   p_quit
        brk

msg:    .byte   "CAN'T LOAD FUJIBLST", 0
name:   .byte   8, "FUJIBLST"
p_open: .byte   3
        .addr   name, IOBUF
        .byte   0
p_read1: .byte  4, 0
        .addr   LOW, $2000 - LOW
        .word   0
p_mark: .byte   2, 0
        .faraddr HIGH - LOW
p_read2: .byte  4, 0
        .addr   HIGH, TOP - HIGH
        .word   0
p_close: .byte  1, 0
p_quit: .byte   4, 0
        .word   0
        .byte   0
        .word   0

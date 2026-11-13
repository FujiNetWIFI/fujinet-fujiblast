; ---------------------------------------------------------------------------
; engine.inc -- the display engine. Every pixel goes through PIX, and a pixel
; is written to the screen only when what it shows changes.
;
; The console writes VRAM one pixel at a time: the value to port 1, the
; column to port 4, the row to port 5, a $60 then $50 strobe to port 0, and
; no port write for SETTLE clocks after the strobe (the BIOS waits 136,
; M.U.L.E. 94; build.sh SETTLE=). A written pixel costs at least SETTLE +
; the port work, about 160 clocks, so at most ~180 pixels a frame (29,568
; clocks). The only lever left is writing fewer pixels, so:
;
;   - PIX holds two bytes a pixel, BG (the tiles, no sprites) then SH (what
;     the screen shows), both port-1 codes (CV0-CV3): a changed pixel goes
;     to OUTS 1 as it is, and one DC walks both;
;   - the kernels compare and write: K1T (a tile: want = its art, stored
;     into BG too) and K1C (a composite: want = (BG AND m) + a, the pairs
;     from a slot's image: m $FF a 0 clear, m 0 a CODE opaque);
;   - both read their stream through DC1, and are unrolled 8 pixels wide
;     with a write stub a pixel; the next pixel's compare runs inside the
;     settle, and PADT/PADC NOPs make up what SETTLE still wants;
;   - the console's row (port 5) goes out once a row, with the tone bits
;     from SNDB (no INS 5), and the column is ~x - k in each stub.
;
; A window is WX, WY, WW (1-8), WH (1-64) with its stream at WSRC (hi, lo)
; and WSTEP bytes skipped after each row. The kernels are leaves (PI) and
; spend every register r0-r11, Q, DC0 and DC1.
;
; Clocks (MAME f8.cpp): K1T 58 a pixel unchanged, K1C 70; written ~182/194;
; a row ~120 more (tools/chf/cyc.py counts them from the listing).
; ---------------------------------------------------------------------------

; NOPs after each write's strobe: the work before the next port write is
; 108 clocks in K1T (the stub's 48 + a pixel's 60) and 118 in K1C, and at
; least 136 before a row's OUTS 5.
PADT    EQU ((SETTLE-108+3)/4)*(SETTLE>108)
PADC    EQU ((SETTLE-118+3)/4)*(SETTLE>118)

; the stub of pixel k: write it, then on to the next pixel
WSTUB   MACRO k, nxt, pad
        OUTS 1                  ; the value
        LR 4,A
        LR A,3                  ; ~(x of pixel 0)
        IF (k)<>0
        AI -(k)
        ENDIF
        OUTS 4                  ; ~(x + k)
        LR A,0
        OUTS 0                  ; $60
        LR A,1
        OUTS 0                  ; $50: the strobe
        LI 0FFH                 ; (settling from here)
        ADC                     ; DC0 back on SH
        LR A,4
        ST                      ; SH = what it shows now
        IF (pad)>0
        REPT pad
        NOP
        ENDM
        ENDIF
        BR nxt
        ENDM

; the setup both kernels share: the window's registers, then the first row
; out and into the unrolled row at pixel 8 - w (Q from the kernel's table)
KSETUP  MACRO tab
        DCI SNDB
        LM
        LR 2,A                  ; the tone bits
        DCI WX
        LM
        LR 5,A                  ; x
        LM
        LR 6,A                  ; y
        LM
        LR 7,A                  ; w
        LM
        LR 9,A                  ; h: rows left
        LM
        LR 8,A                  ; the stream's step
        LM
        LR 10,A                 ; the stream
        LM
        LR 11,A
        LR DC,H
        XDC                     ; DC1 = the stream
        LR A,6                  ; r2 = tone | ~y
        COM
        NI 03FH
        AS 2
        LR 2,A
        LR A,5                  ; r3 = ~(x + w - 8): pixel k's column is ~r3 + k
        AS 7
        AI -8
        COM
        LR 3,A
        LR A,7                  ; Q = the entry of pixel 8 - w
        COM
        AI 9                    ; 8 - w
        SL 1
        DCI tab
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR A,6                  ; H = DC0 = PIX row y, column x
        AI PIX>>8
        LR 10,A
        LR A,5
        SL 1
        LR 11,A
        LR DC,H
        LI 060H
        LR 0,A
        LI 050H
        LR 1,A
        LR A,2
        OUTS 5                  ; the first row
        LR P0,Q
        ENDM

; ---------------------------------------------------------------------------
; KTILE (K1T) -- the window from a stream of codes, a byte a pixel: BG = the
;   code, and the screen where SH differs.  Leaf.  Clobbers r0-r11, Q, DCs.
; ---------------------------------------------------------------------------
KTILE:  KSETUP KTTAB
KTW0:   WSTUB 0, KTP1, PADT
KTW1:   WSTUB 1, KTP2, PADT
KTW2:   WSTUB 2, KTP3, PADT
KTW3:   WSTUB 3, KTP4, PADT
KTPIX   MACRO stub
        XDC
        LM                      ; the code
        XDC
        ST                      ; BG = it
        CM                      ; against SH
        BNZ stub
        ENDM
KTP0:   KTPIX KTW0
KTP1:   KTPIX KTW1
KTP2:   KTPIX KTW2
KTP3:   KTPIX KTW3
KTP4:   KTPIX KTW4
KTP5:   KTPIX KTW5
KTP6:   KTPIX KTW6
KTP7:   KTPIX KTW7
KTEND:  DS 9
        BNZ KTNXT
        POP
KTNXT:  XDC                     ; the stream's next row
        LR A,8
        ADC
        XDC
        LR A,10                 ; PIX's next row
        INC
        LR 10,A
        LR DC,H
        DS 2                    ; ~(y + 1)
        LR A,2
        OUTS 5
        LR P0,Q
KTW4:   WSTUB 4, KTP5, PADT
KTW5:   WSTUB 5, KTP6, PADT
KTW6:   WSTUB 6, KTP7, PADT
KTW7:   WSTUB 7, KTEND, PADT
KTTAB:  DW KTP0, KTP1, KTP2, KTP3, KTP4, KTP5, KTP6, KTP7

; ---------------------------------------------------------------------------
; KCOMP (K1C) -- the window composed: want = (BG AND m) + a, the (m, a)
;   pairs from the stream (2 bytes a pixel); the screen where SH differs.
;   BG is left alone.  Leaf.  Clobbers r0-r11, Q, DCs.
; ---------------------------------------------------------------------------
KCOMP:  KSETUP KCTAB
KCW0:   WSTUB 0, KCP1, PADC
KCW1:   WSTUB 1, KCP2, PADC
KCW2:   WSTUB 2, KCP3, PADC
KCW3:   WSTUB 3, KCP4, PADC
KCPIX   MACRO stub
        LM                      ; BG
        XDC
        NM                      ; AND m
        AM                      ; + a
        XDC
        CM                      ; against SH
        BNZ stub
        ENDM
KCP0:   KCPIX KCW0
KCP1:   KCPIX KCW1
KCP2:   KCPIX KCW2
KCP3:   KCPIX KCW3
KCP4:   KCPIX KCW4
KCP5:   KCPIX KCW5
KCP6:   KCPIX KCW6
KCP7:   KCPIX KCW7
KCEND:  DS 9
        BNZ KCNXT
        POP
KCNXT:  XDC
        LR A,8
        ADC
        XDC
        LR A,10
        INC
        LR 10,A
        LR DC,H
        DS 2
        LR A,2
        OUTS 5
        LR P0,Q
KCW4:   WSTUB 4, KCP5, PADC
KCW5:   WSTUB 5, KCP6, PADC
KCW6:   WSTUB 6, KCP7, PADC
KCW7:   WSTUB 7, KCEND, PADC
KCTAB:  DW KCP0, KCP1, KCP2, KCP3, KCP4, KCP5, KCP6, KCP7

; ---------------------------------------------------------------------------
; EINIT -- the screen cleared (the BIOS: value 0, palette 0 on every row),
;   PIX to match, the slots empty, their images' clear borders.
; ---------------------------------------------------------------------------
EINIT:  ENTER
        CLR
        LR 3,A
        PI BCLRSCR
        DCI PIX                 ; 16K of CV0: 64 x 256
        LI 64
        LR 1,A
        CLR
        LR 0,A
EIN1:   LI CV0
        ST
        DS 0
        BNZ EIN1
        DS 1
        BNZ EIN1
        DCI IMGS                ; the images: all clear (FF, 00) pairs
        LI IMGSLEN/2/256
        LR 1,A
        CLR
        LR 0,A
EIN2:   LI 0FFH
        ST
        CLR
        ST
        DS 0
        BNZ EIN2
        DS 1
        BNZ EIN2
        DCI TIMG                ; a clear row, and the merge buffer
        LI 100
        LR 0,A
EIN3:   LI 0FFH
        ST
        CLR
        ST
        DS 0
        BNZ EIN3
        PI SLCLR
        LEAVE

; SLCLR -- every slot hidden, nothing drawn, no image cached.  Leaf.
;   Clobbers A, r0, DC0.
SLCLR:  DCI SLOTS
        LI 8*16
        LR 0,A
SLC1:   CLR
        ST
        DS 0
        BNZ SLC1
        DCI CTAG
        LI 8*4
        LR 0,A
SLC2:   LI 0FFH
        ST
        DS 0
        BNZ SLC2
        POP

; ---------------------------------------------------------------------------
; EFILL -- the rectangle WX, WY, WW (any), WH in code r0.  BG too.
; ---------------------------------------------------------------------------
EFILL:  ENTER
        LR A,0                  ; a row of 8 of the code
        DCI ET
        LIS 8
        LR 1,A
EFL1:   LR A,0
        ST
        DS 1
        BNZ EFL1
        DCI WW
        LM
        STB ET+8                ; the width left
        LDB WX
        STB ET+9                ; this strip's x
EFL2:   LDB ET+8                ; a strip of up to 8
        CI 8
        BC EFL3
        LIS 8
EFL3:   STB WW
        COM
        INC
        STB WSTEP               ; the 8 again on each row: back -w
        LI ET>>8
        STB WSRC
        LI ET&0FFH
        STB WSRC+1
        LDB ET+9
        STB WX
        PI KTILE
        LDB WW
        LR 0,A
        LDB ET+9
        AS 0
        STB ET+9
        LDB ET+8
        COM
        INC
        AS 0
        COM
        INC
        STB ET+8                ; left - this strip
        NI 0FFH
        BNZ EFL2
        LEAVE

; ---------------------------------------------------------------------------
; EPAL -- rows r0..r1 (inclusive) in palette r2.  Only what changes goes out:
;   the two columns go through K1T like any pixel.
; ---------------------------------------------------------------------------
EPAL:   ENTER
        LR A,1                  ; (never past row 63: PIX ends there)
        CI 63
        BC EPL0
        LI 63
        LR 1,A
EPL0:   LR A,0
        CI 63
        BC EPL1
        LEAVE
EPL1:   LR A,0
        STB WY
        LR A,0                  ; h = r1 - r0 + 1
        COM
        AS 1
        INC
        INC
        STB WH
        LI PALCOL
        STB WX
        LIS 2
        STB WW
        LI -2
        STB WSTEP
        LR A,2
        SL 1
        LR 2,A
        LI PALCODE&0FFH
        AS 2
        STB WSRC+1
        LI PALCODE>>8
        LNK
        STB WSRC
        PI KTILE
        LEAVE
; columns 125 and 126 of each palette: bit 1 of each is the palette's bit
PALCODE: DB CV0, CV0, CV2, CV0, CV0, CV2, CV2, CV2

; ---------------------------------------------------------------------------
; Text: the family's 3x5 font (gen font.inc, $20-$5F) in a 4x6 cell, drawn
; opaque in TXINK (TXINK2 on odd rows: stripes) on value 0.
;
; ETXT -- the string at DC0 (NUL ends it) from TXX, TXY.  TXX moves on.
; ETCH -- the character r0 there.
; ---------------------------------------------------------------------------
ETXT:   ENTER
        LR Q,DC
ETX1:   LR A,QU                 ; (the kernel spends Q: the pointer lives in RAM)
        STB TXPTR
        LR A,QL
        STB TXPTR+1
        LR DC,Q
        LM
        LR 0,A
        NI 0FFH
        BZ ETXX
        LR Q,DC
        LR A,QU
        STB TXPTR
        LR A,QL
        STB TXPTR+1
        PI ETCH
        DCI TXPTR
        LM
        LR QU,A
        LM
        LR QL,A
        BR ETX1
ETXX:   LEAVE

ETCH:   ENTER
        LR A,0                  ; lower case folds to upper; past $5F a blank
        CI 060H-1
        BC ETC0
        AI -32
        CI 05FH
        BC ETC0
        LI ' '
ETC0:   AI -32                  ; the glyph's index
        LR 5,A
        LDB TXINK
        LR 6,A
        LDB TXINK2
        LR 7,A
        LDB TXY                 ; r8: rows from an odd one
        LR 8,A
        DCI CELL
        XDC                     ; DC1 = the cell
        DCI FNTR0
        LR A,5
        ADC                     ; DC0 = the glyph's row 0
        LIS 5
        LR 9,A
ETC1:   LM                      ; bits 2-0: the row, left first
        LR 4,A
        LR A,8                  ; this row's ink
        NI 1
        LR A,6
        BZ ETC2
        LR A,7
ETC2:   LR 3,A
        LIS 3
        LR 2,A
ETC3:   LR A,4                  ; bit 2 is the next pixel
        SL 1
        LR 4,A
        NI 8
        LR A,3
        BNZ ETC4
        LI CV0
ETC4:   XDC
        ST
        XDC
        DS 2
        BNZ ETC3
        LI CV0                  ; the gap column
        XDC
        ST
        XDC
        LI 63                   ; the next table: 64 on, LM moved one
        ADC
        LR A,8
        INC
        LR 8,A
        DS 9
        BNZ ETC1
        DCI CELL+20             ; the gap row
        LIS 4
        LR 9,A
ETC5:   LI CV0
        ST
        DS 9
        BNZ ETC5
        LDB TXX
        STB WX
        LR 0,A
        LDB TXY
        STB WY
        LIS 4
        STB WW
        AS 0
        STB TXX                 ; the next cell
        LIS 6
        STB WH
        CLR
        STB WSTEP
        LI CELL>>8
        STB WSRC
        LI CELL&0FFH
        STB WSRC+1
        PI KTILE
        LEAVE

; ---------------------------------------------------------------------------
; DMARK -- tile r0 to be drawn again: DIRTY, and queued (DQ) unless it was
;   already.  A full queue: DQOVF, and ADIRTY scans DIRTY instead.  Leaf.
;   Clobbers A, r1, DC0.
; ---------------------------------------------------------------------------
DMARK:  LR A,0
        DCI DIRTY
        ADCU
        LM
        NI 0FFH
        BNZ DMKX
        LR A,0
        DCI DIRTY
        ADCU
        LIS 1
        ST
        LDB DQT
        LR 1,A
        INC
        NI 63
        DCI DQH
        CM                      ; full when tail + 1 = head
        BZ DMKO
        DCI DQT
        ST
        LR A,1
        DCI DQ
        ADC
        LR A,0
        ST
DMKX:   POP
DMKO:   LIS 1
        STB DQOVF
        POP

; ---------------------------------------------------------------------------
; Tiles. ETILE -- tile r0 (0-142) shows art frame r1: BG = the frame's art;
; the screen too, unless a slot's rect (drawn or wanted) covers the tile:
; then only BG, and that slot is redrawn over it (SL_FORCE), so no pixel is
; written twice.
; ---------------------------------------------------------------------------
ETILE:  ENTER
        LR A,0
        LR 11,A
        DCI SHOWN               ; what it shows now
        ADCU
        LR A,1
        ST
        DCI TTX                 ; its pixel x, y
        LR A,11
        ADCU
        LM
        STB WX
        DCI TTY
        LR A,11
        ADCU
        LM
        STB WY
        LR A,1                  ; the art: TART + 30 f
        SL 1
        DCI TFOFF
        ADC
        LM
        LR 2,A
        LM
        LR 3,A                  ; (STB's DCI would move DC0 between the two)
        LR A,2
        STB WSRC
        LR A,3
        STB WSRC+1
        LIS TW
        STB WW
        LIS TH
        STB WH
        PI SLHIT                ; r0: a slot over it?
        LR A,0
        NI 0FFH
        BNZ ETBG
        CLR
        STB WSTEP
        PI KTILE
        LEAVE
ETBG:   PI EBGONLY
        LEAVE

; EBGONLY -- the window's BG from its stream, a byte a pixel (6 x 5, the
;   screen untouched).  Leaf.  Clobbers A, r0-r3, H, DCs.
EBGONLY: DCI WSRC
        LM
        LR 10,A
        LM
        LR 11,A
        LR DC,H
        XDC
        LDB WY
        AI PIX>>8
        LR 10,A
        LDB WX
        SL 1
        LR 11,A
        LIS TH
        LR 2,A
EBG1:   LR DC,H
        LIS TW
        LR 3,A
EBG2:   XDC
        LM
        XDC
        ST
        LM                      ; (past SH)
        DS 3
        BNZ EBG2
        LR A,10
        INC
        LR 10,A
        DS 2
        BNZ EBG1
        POP

; SLHIT -- r0 = 1 if a slot's drawn or wanted rect meets the window (WX,
;   WY, WW, WH), and those slots marked SL_FORCE.  Clobbers A, r0-r7, DC0.
SLHIT:  ENTER
        CLR
        LR 0,A
        LIS 8
        LR 1,A
        DCI SLOTS
SLH1:   LR Q,DC                 ; this slot
        LM                      ; WX, WY, WF, WV
        LR 2,A
        LM
        LR 3,A
        LM
        LM
        NI 0FFH
        BZ SLH2
        LR H,DC
        PI MEETW
        BNZ SLH3
        LR DC,H
SLH2:   LM                      ; DX, DY, DF, DV
        LR 2,A
        LM
        LR 3,A
        LM
        LM
        NI 0FFH
        BZ SLH4
        PI MEETW
        BZ SLH4
SLH3:   LR DC,Q                 ; it meets: redraw it over the new BG
        LI SL_FORCE
        ADC
        LIS 1
        ST
        LR 0,A
        LR A,1                  ; its CHGM bit: slot 8 - r1
        COM
        INC
        AI 8
        DCI SLBIT
        ADC
        LM
        LR 2,A
        DCI CHGM
        LM
        NS 2
        BNZ SLH4
        DCI CHGM
        LM
        AS 2
        DCI CHGM
        ST
SLH4:   LR DC,Q
        LI 16
        ADC
        DS 1
        BNZ SLH1
        LEAVE

; MEETW -- NZ if the 6x5 rect at r2, r3 meets the window WX, WY, WW, WH;
;   else Z.  Leaf.  Clobbers A, r4-r7, DC0.
MEETW:  DCI WX
        LM
        LR 4,A
        LM
        LR 5,A
        LM
        LR 6,A
        LM
        LR 7,A
        LR A,4                  ; a = r2 - WX: it meets when -6 < a < WW
        COM
        INC
        AS 2
        LR 4,A
        AI 5
        BM MWN
        LR A,4
        COM                     ; WW - 1 - a
        AS 6
        BM MWN
        LR A,5                  ; b = r3 - WY: -5 < b < WH
        COM
        INC
        AS 3
        LR 5,A
        AI 4
        BM MWN
        LR A,5
        COM
        AS 7
        BM MWN
        LIS 1
        NI 0FFH
        POP
MWN:    CLR                     ; (CLR sets no flags)
        NI 0FFH
        POP

; ---------------------------------------------------------------------------
; Text screens. ECLS -- the picture (cols 4-105, rows 4-61) to value 0, every
;   row in palette r0 (0: black, white ink).  Only what changes is written.
; ECENT -- the string at DC0 centred on the picture's width at row r1, in
;   ink r2 (r3 on odd rows).
; ---------------------------------------------------------------------------
ECLS:   ENTER
        LR A,0
        STB GT+28
        LIS 4
        STB WX
        STB WY
        LI 102
        STB WW
        LI 58
        STB WH
        LI CV0
        LR 0,A
        PI EFILL
        CLR                     ; every row's palette (the hidden ones too)
        LR 0,A
        LI 63
        LR 1,A
        LDB GT+28
        LR 2,A
        PI EPAL
        LEAVE

ECENT:  ENTER
        LR Q,DC
        LR A,QU
        STB TXPTR
        LR A,QL
        STB TXPTR+1
        LR A,1
        STB TXY
        LR A,2
        STB TXINK
        LR A,3
        STB TXINK2
        LR DC,Q                 ; its length (STB moved DC0)
        CLR
        LR 1,A
ECN1:   LM
        NI 0FFH
        BZ ECN2
        LR A,1
        INC
        LR 1,A
        BR ECN1
ECN2:   LR A,1                  ; x = 4 + (102 - 4 len) / 2 = 55 - 2 len
        SL 1
        COM
        INC
        AI 55
        STB TXX
        DCI TXPTR
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        PI ETXT
        LEAVE

; ---------------------------------------------------------------------------
; EBIG -- the character r0 at TXX, TXY, twice the size (each font pixel 2x2:
;   a 8x12 cell), opaque, in TXINK (TXINK2 on odd font rows).  TXX moves on
;   8.  EBTXT -- the string at DC0 so.
; ---------------------------------------------------------------------------
EBTXT:  ENTER
        LR Q,DC
EBT1:   LR A,QU
        STB TXPTR
        LR A,QL
        STB TXPTR+1
        LR DC,Q
        LM
        LR 0,A
        NI 0FFH
        BZ EBTX
        LR Q,DC
        LR A,QU
        STB TXPTR
        LR A,QL
        STB TXPTR+1
        PI EBIG
        DCI TXPTR
        LM
        LR QU,A
        LM
        LR QL,A
        BR EBT1
EBTX:   LEAVE

EBIG:   ENTER
        LR A,0                  ; the glyph's index (as ETCH)
        CI 060H-1
        BC EBI0
        AI -32
        CI 05FH
        BC EBI0
        LI ' '
EBI0:   AI -32
        LR 5,A
        LDB TXINK
        LR 6,A
        LDB TXINK2
        LR 7,A
        CLR
        LR 8,A                  ; font row
        DCI BIGCELL
        XDC
        DCI FNTR0
        LR A,5
        ADC
        LIS 5
        LR 9,A
EBI1:   LM                      ; the row's 3 bits
        LR 4,A
        LR A,8
        NI 1
        LR A,6
        BZ EBI2
        LR A,7
EBI2:   LR 3,A
        LIS 3
        LR 2,A
EBI3:   LR A,4
        SL 1
        LR 4,A
        NI 8
        LR A,3
        BNZ EBI4
        LI CV0
EBI4:   XDC                     ; two pixels
        ST
        ST
        XDC
        DS 2
        BNZ EBI3
        LI CV0                  ; the gap: two
        XDC
        ST
        ST
        XDC
        LI 63
        ADC
        LR A,8
        INC
        LR 8,A
        DS 9
        BNZ EBI1
        LDB TXX                 ; the window: 8 x 12, each cell row twice (step -8)
        STB WX
        LR 0,A
        AI 8
        STB TXX
        LDB TXY
        STB WY
        LIS 8
        STB WW
        LIS 2
        STB WH
        LI -8
        STB WSTEP
        LI BIGCELL>>8
        STB WSRC
        LI BIGCELL&0FFH
        STB WSRC+1
        LIS 5
        STB GT+29
EBI5:   PI KTILE                ; a font row: two screen rows
        LDB WY
        AI 2
        STB WY
        DCI WSRC+1
        LM
        AI 8
        DCI WSRC+1
        ST
        LDB WSRC
        LNK
        STB WSRC
        DECB GT+29
        BNZ EBI5
        LI CV0                  ; the gap: two rows of value 0
        STB BIGCELL+40
        LIS 8
        STB WW
        LI BIGCELL>>8           ; (BIGCELL+40..47, again)
        STB WSRC
        LI (BIGCELL+40)&0FFH
        STB WSRC+1
        DCI BIGCELL+40
        LIS 8
        LR 0,A
EBI6:   LI CV0
        ST
        DS 0
        BNZ EBI6
        PI KTILE
        LEAVE

; SLSEATS -- the slots afresh: hidden, no image; the bombers' inks (blue,
;   red, green, red/blue stripes), the movers bombs.
SLSEATS: ENTER
        PI SLCLR
        CLR
        LR 0,A
SLS0:   LR A,0
        SL 4
        DCI SLOTS+SL_INK
        ADC
        XDC
        LR A,0
        SL 1
        DCI SEATINK
        ADC
        LM
        XDC
        ST
        XDC
        LM
        XDC
        ST
        LR A,0
        INC
        LR 0,A
        CI 4
        BNZ SLS0
        DCI SLOTS+4*16+SL_KIND
        LIS 4
        LR 0,A
SLS9:   LIS 1
        ST
        LI 15
        ADC
        DS 0
        BNZ SLS9
        LIS 4
        STB NSLOT
        LEAVE
; the seats' inks (even rows, odd rows of the picture), and their bits
; (fixed: every page reads them)
SEATINK: DB CV1, CV1, CV2, CV2, CV3, CV3, CV2, CV1
SEATBIT: DB 1, 2, 4, 8

; ---------------------------------------------------------------------------
; start.inc -- power-up, the pages, the clock (M.U.L.E.'s clients/channelf
; start.inc, cut to what FujiBlast needs).
; ---------------------------------------------------------------------------

; ---------------------------------------------------------------------------
; START -- the BIOS jumps here ($0802) at power-up and on RESET. It has
; zeroed the scratchpad and set r59 = 40; RAM is the cart's and outlives a
; RESET, so everything is set here.
; ---------------------------------------------------------------------------
START:  DI
        LISU 7                  ; r59 = KSTK0: the K-stack empty
        LISL 3
        LI KSTK0
        LR S,A
        DCI VARS                ; the globals and the tables after them: 0
        LI (RAMEND-VARS+255)>>8
        LR 1,A
        CLR
        LR 0,A
STZL:   CLR
        ST
        DS 0
        BNZ STZL
        DS 1
        BNZ STZL
        LIS PG_ART              ; the art and tables into RAM
        LR 9,A
        PI SELPG
        DCI PGWIN
        XDC
        DCI DATA
        XDC
        LI (DATALEN+255)>>8
        LR 1,A
        CLR
        LR 0,A
STCPL:  LM
        XDC
        ST
        XDC
        DS 0
        BNZ STCPL
        DS 1
        BNZ STCPL
        LIS PG_BOOT
        LR 9,A
        PI SELPG
        PI SNDINIT
        PI EINIT
        PI FNCHK
        BZ STOK1
        JMP NOCART
STOK1:  LDB FNPVER
        CI 1
        BNC STOK2               ; protocol 2 or later
        JMP OLDCART
STOK2:  JMP MAIN

; No FujiNet cart answering, or one too old to page: say so and stop.
NOCART: DCI S_NOCART
        BR STOP
OLDCART: DCI S_OLDCART
STOP:   LR Q,DC
        LIS 0                   ; white on black (palette 0)
        LI CV1
        STB TXINK
        STB TXINK2
        LI 12
        STB TXX
        LI 28
        STB TXY
        LR DC,Q
        PI ETXT
HALT:   BR HALT
S_NOCART: DB "NO FUJINET FOUND",0
S_OLDCART: DB "CART FIRMWARE TOO OLD",0

; FNCHK -- Z set if the cart's 'F','N' magic is there.  Leaf; clobbers A, DC0.
FNCHK:  DCI FNMAG0
        LM
        CI 'F'
        BNZ FNCHKX
        LM
        CI 'N'
FNCHKX: POP

; ---------------------------------------------------------------------------
; SELPG -- map page r9 at $4800.  Leaf; keeps DC0.  Clobbers A, DC1.
; With PGPUSH, the only stores to FNBANK (tools/chf/checkcalls.py).
; ---------------------------------------------------------------------------
SELPG:  XDC
        LR A,9
        DCI CURPG
        ST
        DCI FNBANK
        ADC
        ST                      ; any value: the page is mapped from here on
        XDC
        POP

; PGPUSH -- remember the page mapped now, then map page r9.  Leaf.
;   Clobbers A, r8, DC0.
PGPUSH: DCI CURPG
        LM
        LR 8,A                  ; the page now
        LM                      ; (CURPG, PGSP, PGSTK follow each other)
        ADC                     ; DC0 = PGSTK + depth
        LR A,8
        ST
        DCI PGSP
        LM
        INC
        DCI PGSP
        ST
        LR A,9
        DCI CURPG
        ST
        DCI FNBANK
        ADC
        ST
        POP

; PGPOP -- map the page PGPUSH remembered.  Leaf.  Clobbers A, r8, r9, DC0, DC1.
PGPOP:  DCI PGSP
        LM
        AI 0FFH
        LR 8,A
        DCI PGSP
        ST                      ; DC0 = PGSTK
        LR A,8
        ADC
        LM
        LR 9,A
        JMP SELPG               ; (its POP returns for us)

; FAR -- call ent, which is in page pg, from anywhere (ENTER first), and come
; back to the page mapped before.  Results in r0-r6; A, r7-r9 and the DCs
; do not survive.
FAR     MACRO pg, ent
        LIS pg
        LR 9,A
        PI PGPUSH
        PI ent
        PI PGPOP
        ENDM

; GOPAGE -- page pg's entry ent (E_*), from fixed code (ENTER first): the
; page stays mapped.  A comes back as the entry left it.
GOPAGE  MACRO pg, ent
        LIS pg
        LR 9,A
        PI SELPG
        PI PGWIN+(ent)
        ENDM

STUB:   POP

; ---------------------------------------------------------------------------
; TICKRD -- r0:r1 = the cart's tick (60 Hz).  The bytes change together but
;   are read one at a time: HI, LO, HI, again if HI moved.  Leaf.
;   Clobbers A, DC0.
; ---------------------------------------------------------------------------
TICKRD: DCI FNTICK+1
        LM
        LR 1,A
        DCI FNTICK
        LM
        LR 0,A
        LM
        XS 1
        BNZ TICKRD
        POP

; FRWAIT -- wait for the tick to move on from FRAMES; FRAMES = the tick.
;   Leaf.  Clobbers A, r0, r1, DC0.
FRWAIT: DCI FNTICK+1
        LM
        LR 1,A
        DCI FNTICK
        LM
        LR 0,A
        LM
        XS 1
        BNZ FRWAIT
        DCI FRAMES
        LM
        XS 0
        BNZ FRWNEW
        LM
        XS 1
        BZ FRWAIT
FRWNEW: DCI FRAMES
        LR A,0
        ST
        LR A,1
        ST
        POP

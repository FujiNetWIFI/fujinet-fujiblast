; ---------------------------------------------------------------------------
; slots.inc -- the sprites: 4 bombers and 4 movers (kicked or thrown bombs),
; each a 6x5 picture over the tiles, drawn through K1C so that each pixel of
; a change is visited once and written only if it changes.
;
; A slot is 16 bytes at SLOTS + 16s: what is wanted (W*: x, y, frame,
; visible) and what is drawn (D*), the body's ink on even and odd rows of
; the picture (stripes when they differ), its kind (0 a bomber: the frame is
; a sprite frame; 1 a mover: a tile frame), FORCE (a tile changed under it),
; and its image: where the picture of WF is, as K1C pairs, 10 x 9 with two
; clear pixels round it so the union of a 1-2 pixel move is one window read
; straight from it.
;
; Images are a cache: a bomber keeps 3 (its walk's frames; CTAG holds which
; frames they are), a mover 1. A window another wanted slot also meets is
; composed in MIMG first (MERGE: slot order, later on top).
;
; Whoever changes a slot's W* or FORCE sets its CHGM bit (SLSET does): a
; pass looks at no other slot.
; ---------------------------------------------------------------------------

SL_WX   EQU 0
SL_WY   EQU 1
SL_WF   EQU 2
SL_WV   EQU 3
SL_DX   EQU 4
SL_DY   EQU 5
SL_DF   EQU 6
SL_DV   EQU 7
SL_INK  EQU 8
SL_INK2 EQU 9
SL_KIND EQU 10
SL_FORCE EQU 11
SL_IMG  EQU 12                  ; 2: the image of WF (hi, lo)
SL_CNXT EQU 14                  ; the cache entry a miss takes next
IMROW   EQU 20                  ; an image row: 10 pairs
IMSZ    EQU 192
IMGSLEN EQU 4*3*IMSZ+4*IMSZ     ; (the images' RAM, IMGS on)

; CLAMP2 -- (inline) A (a move) clamped to -2..2; C set if it was in -8..8
;   (else C clear, A spoiled).
CLAMP2  MACRO
        AI 8
        CI 16
        BNC m_out               ; (C clear: too far)
        AI -6                   ; d + 2
        BP m_pos
        CLR
        BR m_in
m_pos:  CI 4
        BC m_in
        LIS 4
m_in:   AI -2
        CI 0FFH                 ; C set (A kept)
m_out:
        ENDM

; ---------------------------------------------------------------------------
; ESLOTS -- the slots CHGM marks brought up to date on the screen. When no two
;   wanted slots are near each other (ENEAR), no window can need a merge: each
;   slot then makes its image only if its frame changed, and ESLOT takes its
;   fast path. Else the images first, all of them (MERGE reads them).
; ---------------------------------------------------------------------------
ESLOTS: LDB CHGM                ; (nothing changed: out before ENTER)
        NI 0FFH
        BNZ ESS0
        POP
ESS0:   ENTER
        PI ENEAR
        LDB NEAR
        NI 0FFH
        BZ ESS3
        LDB CHGM                ; near: every changed slot's image first
        STB ESM
        CLR
        STB ESN
ESS1:   LDB ESM
        NI 1
        BZ ESS2
        PI EIMG
ESS2:   LDB ESM
        SR 1
        STB ESM
        BZ ESS3
        INCB ESN
        BR ESS1
ESS3:   LDB CHGM
        STB ESM
        CLR
        STB ESN
        STB CHGM
ESS4:   LDB ESM
        NI 1
        BZ ESS5
        LDB NEAR
        NI 0FFH
        BNZ ESS4A
        PI EIMGQ                ; (its image, if its frame is new)
ESS4A:  PI ESLOT
ESS5:   LDB ESM
        SR 1
        STB ESM
        BZ ESS6
        INCB ESN
        BR ESS4
ESS6:   LEAVE

; EIMGQ -- EIMG for slot ESN only if it is wanted with a frame it was not
;   drawn with (or was not drawn).  Leaf (EIMG's POP returns for it).
EIMGQ:  LDB ESN
        SL 4
        AI SL_WF
        DCI SLOTS
        ADC
        LM
        LR 0,A                  ; WF
        LM                      ; WV
        NI 0FFH
        BZ EIQX
        LM                      ; DX, DY
        LM
        LM                      ; DF
        XS 0
        BNZ EIQ1
        LM                      ; DV
        NI 0FFH
        BNZ EIQX
EIQ1:   JMP EIMG
EIQX:   POP

; ENEAR -- NEAR = 1 if two wanted slots (of the first NSLOT) are within 10
;   pixels across and 9 down of each other: a window of one may then meet
;   the other.  The wanted ones' x, y gathered first (EV), then each pair.
;   Leaf.  Clobbers A, r0-r9, Q, DCs.
ENEAR:  CLR
        STB NEAR
        LR 8,A                  ; how many gathered
        LDB NSLOT
        LR 9,A
        DCI EV
        XDC
        DCI SLOTS
ENG1:   LM
        LR 2,A
        LM
        LR 3,A
        LM
        LM                      ; WV
        NI 0FFH
        BZ ENG2
        XDC
        LR A,2
        ST
        LR A,3
        ST
        XDC
        LR A,8
        INC
        LR 8,A
ENG2:   LI 12
        ADC
        DS 9
        BNZ ENG1
        LR A,8                  ; pairs: a against those after it
        CI 1
        BC ENRX
        DCI EV
ENR1:   LM
        LR 2,A                  ; xa
        LM
        LR 3,A                  ; ya
        LR Q,DC                 ; (b: from here; the next a too)
        DS 8
        BZ ENRX
        LR A,8
        LR 9,A
ENR2:   LM
        COM
        INC
        AS 2                    ; xa - xb
        AI 9                    ; within: -10 < d < 10
        CI 18
        BNC ENR3
        LM
        COM
        INC
        AS 3
        AI 8
        CI 16
        BNC ENR4
        LIS 1
        STB NEAR
        POP
ENR3:   LM                      ; (yb skipped)
ENR4:   DS 9
        BNZ ENR2
        LR DC,Q
        BR ENR1
ENRX:   POP

; SLSET -- slot r0's W* = r1 (x), r2 (y), r3 (frame), r4 (visible); its
;   CHGM bit set if that changed anything.  Leaf.  Clobbers A, r5, DC0.
SLSET:  LR A,0
        SL 4
        DCI SLOTS
        ADC
        LR A,1
        CM
        BNZ SLS1
        LR A,2
        CM
        BNZ SLS1
        LR A,3
        CM
        BNZ SLS1
        LR A,4
        CM
        BNZ SLS1
        POP
SLS1:   LR A,0
        SL 4
        DCI SLOTS
        ADC
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        LR A,4
        ST
        ; (on into SLMARK)

; SLMARK -- slot r0's CHGM bit set.  Leaf.  Clobbers A, r5, DC0.
SLMARK: LR A,0
        DCI SLBIT
        ADC
        LM
        LR 5,A
        DCI CHGM                ; (no OR with a register: add the bit if clear)
        LM
        NS 5
        BNZ SLM9
        DCI CHGM
        LM
        AS 5
        DCI CHGM
        ST
SLM9:   POP
SLBIT:  DB 1, 2, 4, 8, 10H, 20H, 40H, 80H

; IMSET -- (inline) slot ESN (H)'s SL_IMG = its image entry r8; DC0 = it.
;   Clobbers A, r0, Q.
IMSET   MACRO
        LDB ESN                 ; IMGADR + 6s + 2k
        LR 0,A
        SL 1
        AS 0
        SL 1
        LR 0,A
        LR A,8
        SL 1
        AS 0
        DCI IMGADR
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,H
        LI SL_IMG
        ADC
        LR A,QU
        ST
        LR A,QL
        ST
        LR DC,Q
        ENDM

; ---------------------------------------------------------------------------
; EIMG -- slot ESN's SL_IMG = the image of its frame WF (if it is wanted on
;   the screen): from the cache, or made in the entry the cache gives up.
;   Bomber art is a byte a pixel ($FF clear, 1 body, else a code), the
;   body in INK on the picture's even rows, INK2 on its odd ones; a mover's
;   is a tile's (CV0 clear).  Leaf.  Clobbers A, r0-r9, H, Q, DCs.
; ---------------------------------------------------------------------------
EIMG:   LDB ESN
        LR 9,A
        SL 4
        DCI SLOTS
        ADC
        LR H,DC                 ; the slot
        LM
        LM
        LM
        LR 4,A                  ; WF
        LM                      ; WV
        NI 0FFH
        BNZ EIM0
        POP
EIM0:   LR DC,H
        LI SL_INK
        ADC
        LM
        LR 6,A                  ; ink
        LM
        LR 7,A                  ; ink on odd rows
        LM
        LR 1,A                  ; kind
        LR A,9                  ; the cache: CTAG + 4s
        SL 1
        SL 1
        DCI CTAG
        ADC
        LR Q,DC
        CLR
        LR 8,A                  ; entry k
        LR A,1
        NI 0FFH
        LIS 3
        BZ EIMH
        LIS 1                   ; (a mover has one)
EIMH:   LR 2,A
EIMH1:  LR A,4                  ; a hit?
        CM
        JZ EIMHIT
        LR A,8
        INC
        LR 8,A
        DS 2
        BNZ EIMH1
        LR A,1                  ; a miss: the entry SL_CNXT (a mover: 0)
        NI 0FFH
        CLR
        BNZ EIMM1
        LR DC,H
        LI SL_CNXT
        ADC
        LM
        LR 8,A
        INC
        CI 3
        BNZ EIMM0
        CLR
EIMM0:  LR 0,A                  ; (LI below is A too)
        LR DC,H
        LI SL_CNXT
        ADC
        LR A,0
        ST
        LR A,8
EIMM1:  LR 8,A
        DCI NMISS               ; (the rigs count the misses)
        LM
        INC
        DCI NMISS
        ST
        LR DC,Q                 ; CTAG[k] = WF
        LR A,8
        ADC
        LR A,4
        ST
        IMSET                   ; SL_IMG = the entry, DC0 on it
        LI 2*IMROW+4            ; its pixel (2, 2)
        ADC
        XDC                     ; (DC1, while DC0 finds the art)
        LR A,4                  ; the art
        SL 1
        LR 0,A
        LR A,1
        NI 0FFH
        BZ EIM2
        DCI TFOFF
        BR EIM3
EIM2:   DCI SFOFF
EIM3:   LR A,0
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        XDC                     ; DC0 the image, DC1 the art
        LIS 5
        LR 5,A                  ; rows
        CLR
        LR 9,A                  ; the picture's row (its parity: the ink)
EIMR:   LR A,9
        NI 1
        LR A,6
        BZ EIM4
        LR A,7
EIM4:   LR 3,A                  ; this row's ink
        LIS 6
        LR 2,A
EIMP:   XDC
        LM                      ; the art's pixel
        XDC
        CI 0FFH
        BZ EIMC
        LR 0,A
        LR A,1
        NI 0FFH
        LR A,0
        BZ EIM5
        CI CV0                  ; a mover: the tile's floor is clear
        BZ EIMC
        BR EIMO
EIM5:   CI 1                    ; a bomber: 1 is the body
        BNZ EIMO
        LR A,3
EIMO:   LR 0,A
        CLR
        ST                      ; m = 0: opaque
        LR A,0
        ST                      ; a = the code
        BR EIMN
EIMC:   LI 0FFH
        ST
        CLR
        ST
EIMN:   DS 2
        BNZ EIMP
        LIS IMROW-12
        ADC
        LR A,9
        INC
        LR 9,A
        DS 5
        BNZ EIMR
        POP
EIMHIT: IMSET                   ; the cache has it: entry r8
        POP

; ---------------------------------------------------------------------------
; ESLOT -- slot ESN brought up to date on the screen: a move of up to 2
;   pixels each way is one window (the union of before and after); else the
;   old rect, then the new.
; ---------------------------------------------------------------------------
ESLOT:  ENTER
        LDB ESN
        SL 4
        DCI SLOTS
        ADC
        LR H,DC
        LM
        LR 0,A                  ; WX
        LM
        LR 1,A                  ; WY
        LM
        LR 2,A                  ; WF
        LM
        LR 3,A                  ; WV
        LM
        LR 4,A                  ; DX
        LM
        LR 5,A                  ; DY
        LM
        LR 6,A                  ; DF
        LM
        LR 7,A                  ; DV
        LIS SL_FORCE-8
        ADC
        LM
        NI 0FFH
        BNZ ESL2
        LR A,0                  ; W* = D*: nothing to do
        XS 4
        BNZ ESL2
        LR A,1
        XS 5
        BNZ ESL2
        LR A,2
        XS 6
        BNZ ESL2
        LR A,3
        XS 7
        BNZ ESL2
        LEAVE
ESL2:   LDB NEAR                ; the fast path: alone (no merge), shown before
        NI 0FFH                 ;  and now, moved 8 or less each way: it draws
        BNZ ESL2G               ;  2 pixels of the move at most, and comes
        LR A,3                  ;  back next pass for the rest (a slow pass
        NI 0FFH                 ;  must not make the next one slower)
        BZ ESL2G
        LR A,7
        NI 0FFH
        BZ ESL2G
        LR A,4                  ; dx = WX - DX
        COM
        INC
        AS 0
        CLAMP2
        BNC ESL2G
        LR 8,A
        LR A,5                  ; dy
        COM
        INC
        AS 1
        CLAMP2
        BNC ESL2G
        LR 9,A
        LR A,4                  ; where it is drawn: D + the clamped step
        AS 8
        LR 6,A
        LR A,5
        AS 9
        LR 7,A
        LR A,6
        XS 0
        BNZ ESL2P
        LR A,7
        XS 1
        BZ ESL2F
ESL2P:  LR A,5                  ; short of W: again next pass
        STB ES+3
        LDB ESN
        LR 0,A
        PI SLMARK
        LDB ES+3
        LR 5,A
ESL2F:  LR A,6
        LR 0,A
        LR A,7
        LR 1,A
        JMP ESLF
ESL2G:  LR A,4                  ; the old rect kept (D* is overwritten now)
        STB ES
        LR A,5
        STB ES+1
        LR A,7
        STB ES+2
        LR DC,H                 ; drawn = wanted, FORCE off: EWIN reads W*
        LIS SL_DX
        ADC
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        LIS SL_FORCE-SL_DV-1
        ADC
        CLR
        ST
        LR A,3                  ; both on the screen, and near: one window
        NI 0FFH
        BZ ESL5
        LR A,7
        NI 0FFH
        BZ ESL5
        LR A,4                  ; dx = WX - DX
        COM
        INC
        AS 0
        LR 8,A
        AI 2                    ; |dx| <= 2
        CI 4
        BNC ESL5
        LR A,5
        COM
        INC
        AS 1
        LR 9,A
        AI 2
        CI 4
        BNC ESL5
        LR A,8                  ; x from the smaller, w = 6 + |dx|
        NI 0FFH
        LR A,4
        BP ESL3
        LR A,0
        STB WX
        LR A,8
        COM
        INC
        BR ESL3A
ESL3:   STB WX
        LR A,8
ESL3A:  AI TW
        STB WW
        LR A,9
        NI 0FFH
        LR A,5
        BP ESL4
        LR A,1
        STB WY
        LR A,9
        COM
        INC
        BR ESL4A
ESL4:   STB WY
        LR A,9
ESL4A:  AI TH
        STB WH
        LDB ESN
        STB ESP
        PI EWIN
        LEAVE
ESL5:   LDB ES+2                ; apart: the old rect...
        NI 0FFH
        BZ ESL6
        LDB ES
        STB WX
        LDB ES+1
        STB WY
        LIS TW
        STB WW
        LIS TH
        STB WH
        LI 0FFH
        STB ESP
        PI EWIN
ESL6:   LDB ESN                 ; ... then the new
        SL 4
        DCI SLOTS
        ADC
        LM
        LR 0,A
        LM
        LR 1,A
        LM
        LM
        NI 0FFH
        BZ ESL7
        LR A,0
        STB WX
        LR A,1
        STB WY
        LIS TW
        STB WW
        LIS TH
        STB WH
        LDB ESN
        STB ESP
        PI EWIN
ESL7:   LEAVE

; ESLF -- ESLOT's fast path: r0/r1 where it is drawn now, r2/r3 WF/WV,
;   r4/r5 DX/DY, r8/r9 the step, H the slot.  The union window straight from
;   the slot's image, through K1C.
ESLF:   LR DC,H                 ; drawn = wanted
        LIS SL_DX
        ADC
        LR A,0
        ST
        LR A,1
        ST
        LR A,2
        ST
        LR A,3
        ST
        LR A,8                  ; x0, w; the image's column 2 - (WX - x0)
        NI 0FFH
        LR A,4
        BP ESF1                 ; dx >= 0: x0 = DX
        LR A,0
ESF1:   LR 6,A                  ; x0
        LR A,8
        NI 0FFH
        BP ESF2
        COM
        INC
ESF2:   AI TW
        LR 7,A                  ; w
        LR A,9
        NI 0FFH
        LR A,5
        BP ESF3
        LR A,1
ESF3:   LR 10,A                 ; (y0 in r10; H is not needed now)
        LR A,9
        NI 0FFH
        BP ESF4
        COM
        INC
ESF4:   AI TH
        LR 11,A                 ; h
        LR A,6                  ; the image offset: (y0 - WY + 2) 20 + (x0 - WX + 2) 2
        COM
        INC
        AS 0
        COM
        INC                     ; x0 - WX (0, -1 or -2)
        AI 2
        SL 1
        LR 4,A
        LR A,10
        COM
        INC
        AS 1
        COM
        INC
        AI 2                    ; y0 - WY + 2: 0-2
        SL 1
        SL 1
        LR 5,A                  ; x 4
        SL 1
        SL 1
        AS 5                    ; x 20
        AS 4
        LR 4,A
        LDB ESN                 ; + SL_IMG
        SL 4
        AI SL_IMG
        DCI SLOTS
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LR A,4
        ADC
        LR Q,DC
        DCI WX                  ; the window: WX WY WW WH WSTEP WSRC
        LR A,6
        ST
        LR A,10
        ST
        LR A,7
        ST
        LR A,11
        ST
        LR A,7
        SL 1
        COM
        INC
        AI IMROW
        ST
        LR A,QU
        ST
        LR A,QL
        ST
        PI KCOMP
        LEAVE

; ---------------------------------------------------------------------------
; EWIN -- the window WX, WY, WW, WH composed and drawn: slot ESP's image (FF:
;   none, the background) unless another wanted slot meets the window; then
;   all of them, merged.
; ---------------------------------------------------------------------------
EWIN:   ENTER
        LDB WX                  ; a slot at x meets it when WX - 6 < x < WX + WW:
        COM                     ;   x - (WX - 5) < WW + 5 (unsigned)
        INC
        AI 5
        LR 4,A
        LDB WW
        AI 5
        LR 5,A
        LDB WY                  ; and WY - 5 < y < WY + WH
        COM
        INC
        AI 4
        LR 6,A
        LDB WH
        AI 4
        LR 7,A
        LDB ESP
        LR 8,A
        CLR
        LR 0,A                  ; t
        LDB NSLOT               ; (8 while a mover is shown, else the bombers)
        LR 1,A
        DCI SLOTS
EWN1:   LR Q,DC
        LM
        AS 4
        LR 2,A                  ; x - (WX - 5)
        LM
        AS 6
        LR 3,A
        LM
        LM                      ; WV
        NI 0FFH
        BZ EWN2
        LR A,0
        XS 8
        BZ EWN2                 ; (its own)
        LR A,2                  ; COM; AS r: carry when r > A
        COM
        AS 5
        BNC EWN2
        LR A,3
        COM
        AS 7
        BC EWN8                 ; it meets: merged
EWN2:   LR DC,Q
        LI 16
        ADC
        LR A,0
        INC
        LR 0,A
        DS 1
        BNZ EWN1
        LR A,8                  ; alone
        CI 0FFH
        BNZ EWN4
        LI TIMG>>8              ; none at all: the background (the clear row,
        STB WSRC                ;  again each row)
        LI TIMG&0FFH
        STB WSRC+1
        LDB WW
        SL 1
        COM
        INC
        STB WSTEP
        BR EWN9
EWN8:   DCI NMRG
        LM
        INC
        DCI NMRG
        ST
        PI MERGE                ; others: merged in MIMG, a row of WW pairs
        LI MIMG>>8
        STB WSRC
        LI MIMG&0FFH
        STB WSRC+1
        CLR
        STB WSTEP
        BR EWN9
EWN4:   PI ESRC                 ; slot ESP's image, from the window's corner
        LDB WW
        SL 1
        COM
        INC
        AI IMROW
        STB WSTEP
EWN9:   DCI NWIN                ; (the rigs count them)
        LM
        INC
        DCI NWIN
        ST
        PI KCOMP
        LEAVE

; ESRC -- WSRC = slot ESP's image at the window's top-left: pixel
;   (WX - SX + 2, WY - SY + 2).  Leaf.  Clobbers A, r0-r3, Q, DC0.
ESRC:   LDB ESP
        SL 4
        DCI SLOTS
        ADC
        LR Q,DC
        LM
        LR 2,A                  ; SX
        LM
        LR 3,A                  ; SY
        LDB WY                  ; rows down, x 20
        COM
        INC
        AS 3
        COM
        INC                     ; WY - SY
        AI 2
        LR 1,A
        CLR
ESR1:   DS 1                    ; r1 times
        BM ESR2
        AI IMROW
        BR ESR1
ESR2:   LR 0,A
        LDB WX
        COM
        INC
        AS 2
        COM
        INC                     ; WX - SX
        AI 2
        SL 1
        AS 0
        LR 0,A                  ; the offset (< 180)
        LR DC,Q                 ; the slot's image
        LI SL_IMG
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LR A,0
        ADCU
        LR Q,DC
        LR A,QU
        STB WSRC
        LR A,QL
        STB WSRC+1
        POP

; ---------------------------------------------------------------------------
; MERGE -- MIMG = the window as K1C pairs (WW a row): clear, then each wanted
;   slot that meets it, in slot order (later on top), its opaque pixels.
; ---------------------------------------------------------------------------
MERGE:  ENTER
        LDB WW                  ; clear: WW x WH pairs
        LR 0,A
        LDB WH
        LR 1,A
        CLR
MRG0:   AS 0
        DS 1
        BNZ MRG0
        LR 1,A
        DCI MIMG
MRG1:   LI 0FFH
        ST
        CLR
        ST
        DS 1
        BNZ MRG1
        CLR
        STB EM+1                ; t
MRG2:   LDB EM+1
        SL 4
        DCI SLOTS
        ADC
        LM
        LR 2,A
        LM
        LR 3,A
        LM
        LM
        NI 0FFH
        BZ MRG3
        PI MEETW
        BZ MRG3
        LR A,2
        STB EM+2                ; its x, y
        LR A,3
        STB EM+3
        PI OVLAY
MRG3:   INCB EM+1
        CI 8
        BNZ MRG2
        LEAVE

; OVLAY -- slot EM+1 (at EM+2, EM+3) laid over MIMG where the two meet.
;   Leaf.  Clobbers A, r0-r9, Q, DCs.
OVLAY:  DCI WX
        LM
        LR 4,A                  ; WX
        LM
        LR 5,A                  ; WY
        LM
        LR 6,A                  ; WW
        LM
        LR 7,A                  ; WH
        LDB EM+2
        LR 2,A
        LDB EM+3
        LR 3,A
        ; x0 = max(WX, SX), x1 = min(WX + WW, SX + 6)
        LR A,4
        COM
        INC
        AS 2                    ; SX - WX
        LR A,4
        BM OVL1
        LR A,2
OVL1:   LR 8,A                  ; x0
        LR A,4
        AS 6
        LR 0,A                  ; WX + WW
        LR A,2
        AI TW
        LR 1,A                  ; SX + 6
        COM
        INC
        AS 0                    ; (WX + WW) - (SX + 6)
        LR A,0
        BP OVL2
        BR OVL3
OVL2:   LR A,1
OVL3:   COM
        INC
        AS 8
        COM
        INC
        LR 0,A                  ; cols = x1 - x0
        ; y0 = max(WY, SY), y1 = min(WY + WH, SY + 5)
        LR A,5
        COM
        INC
        AS 3
        LR A,5
        BM OVL4
        LR A,3
OVL4:   LR 9,A                  ; y0
        LR A,5
        AS 7
        LR 1,A                  ; WY + WH
        LR A,3
        AI TH
        LR 6,A
        COM
        INC
        AS 1
        LR A,1
        BP OVL5
        BR OVL6
OVL5:   LR A,6
OVL6:   COM
        INC
        AS 9
        COM
        INC
        LR 1,A                  ; rows = y1 - y0
        ; src = its image + (y0 - SY + 2) * 20 + (x0 - SX + 2) * 2
        LR A,3
        COM
        INC
        AS 9
        AI 2
        LR 6,A
        CLR
OVL7:   DS 6
        BM OVL8
        AI IMROW
        BR OVL7
OVL8:   LR 6,A
        LR A,2
        COM
        INC
        AS 8
        AI 2
        SL 1
        AS 6
        LR 6,A                  ; the offset
        LDB EM+1
        SL 4
        AI SL_IMG
        DCI SLOTS
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LR A,6
        ADCU
        XDC                     ; DC1 = src
        ; dst = MIMG + (y0 - WY) * 2WW + (x0 - WX) * 2
        LDB WW
        SL 1
        LR 6,A                  ; 2 WW: the dst's row
        LR A,5
        COM
        INC
        AS 9
        LR 7,A
        CLR
OVL9:   DS 7
        BM OVLA
        AS 6
        BR OVL9
OVLA:   LR 7,A
        LR A,4
        COM
        INC
        AS 8
        SL 1
        AS 7
        DCI MIMG
        ADC                     ; DC0 = dst
        ; rows x cols
        LR A,0
        SL 1
        COM
        INC
        LR 3,A                  ; -2 cols
        AI IMROW
        LR 4,A                  ; the src's step
        LR A,3
        AS 6
        LR 5,A                  ; the dst's step
OVLR:   LR A,0
        LR 2,A
OVLP:   XDC
        LM                      ; m
        NI 0FFH
        BNZ OVLS
        LM                      ; a: opaque, laid on
        XDC
        LR 7,A
        CLR
        ST
        LR A,7
        ST
        BR OVLN
OVLS:   LM                      ; clear: both skipped
        XDC
        LIS 2
        ADC
OVLN:   DS 2
        BNZ OVLP
        XDC
        LR A,4
        ADC
        XDC
        LR A,5
        ADC
        DS 1
        BNZ OVLR
        POP

; the images: slot s's entry k (a bomber 3, a mover 1), at IMGADR + 6s + 2k
IMGADR: DB (IMGS+0*576+0*IMSZ)>>8, (IMGS+0*576+0*IMSZ)&0FFH
        DB (IMGS+0*576+1*IMSZ)>>8, (IMGS+0*576+1*IMSZ)&0FFH
        DB (IMGS+0*576+2*IMSZ)>>8, (IMGS+0*576+2*IMSZ)&0FFH
        DB (IMGS+1*576+0*IMSZ)>>8, (IMGS+1*576+0*IMSZ)&0FFH
        DB (IMGS+1*576+1*IMSZ)>>8, (IMGS+1*576+1*IMSZ)&0FFH
        DB (IMGS+1*576+2*IMSZ)>>8, (IMGS+1*576+2*IMSZ)&0FFH
        DB (IMGS+2*576+0*IMSZ)>>8, (IMGS+2*576+0*IMSZ)&0FFH
        DB (IMGS+2*576+1*IMSZ)>>8, (IMGS+2*576+1*IMSZ)&0FFH
        DB (IMGS+2*576+2*IMSZ)>>8, (IMGS+2*576+2*IMSZ)&0FFH
        DB (IMGS+3*576+0*IMSZ)>>8, (IMGS+3*576+0*IMSZ)&0FFH
        DB (IMGS+3*576+1*IMSZ)>>8, (IMGS+3*576+1*IMSZ)&0FFH
        DB (IMGS+3*576+2*IMSZ)>>8, (IMGS+3*576+2*IMSZ)&0FFH
        DB (IMGS+2304+0*IMSZ)>>8, (IMGS+2304+0*IMSZ)&0FFH, 0, 0, 0, 0
        DB (IMGS+2304+1*IMSZ)>>8, (IMGS+2304+1*IMSZ)&0FFH, 0, 0, 0, 0
        DB (IMGS+2304+2*IMSZ)>>8, (IMGS+2304+2*IMSZ)&0FFH, 0, 0, 0, 0
        DB (IMGS+2304+3*IMSZ)>>8, (IMGS+2304+3*IMSZ)&0FFH, 0, 0, 0, 0

; fixa.inc -- fixed A, $0000-$05FF: the same in every page's assembly and
; always mapped (2650 page 0, with the screen, console RAM and RAM-L).
;
; The start-up, the vblank's work, the input, the display primitives and
; the main loop (src/main.c): it runs the link's step and the screen the
; state's phase wants, one page each. Only fixed code may switch pages
; (PGCALL), since the next fetch from the window comes from the new page.
;
; Register conventions: R0 data, R1-R3 arguments; a routine says what it
; clobbers. PSL is PSLBASE (logical compares, no carry-in) everywhere but
; inside 16-bit arithmetic.

        ORG     $0000
        BCTA,UN START
        DB      $17             ; RETC,UN: the interrupt guard at $0003

; ---- the zero-page vectors: ZBSR *Zx is a 2-byte call from either page ----
        ORG     $0004
ZDPOS:  DWBE    DPOS
ZDPUTC: DWBE    DPUTC
ZDTXT:  DWBE    DTXT
ZDNUM:  DWBE    DNUM
ZSNDQ:  DWBE    SNDQUE
ZDFILL: DWBE    DFILL
ZDCLR:  DWBE    DCLR
ZFRAME: DWBE    FRAME
ZCENTR: DWBE    CENTRE
ZUDCSET: DWBE   UDCSET
ZDGLY:  DWBE    DGLYPH
ZSPRCOL: DWBE   SPRCOL
ZACTQ:  DWBE    ACTQ
ZTDRAW: DWBE    TDRAW
ZTCELL: DWBE    TCELL
ZSCRCLR: DWBE   SCRCLR

        ORG     $0040
; ---- pointer words: page-0 code's way into page 1 ------------------------
PBANK:  DWBE    FNBANK          ; *PBANK,Rn selects page Rn
PRSEL:  DWBE    FNRSEL
PRDAT:  DWBE    FNRDAT
PRDATA: DWBE    FNRDATA         ; the reply
PTXD:   DWBE    FNDATA          ; *PTXD,Rn appends Rn to the TX stream
PREPLY: DWBE    FNREPLY
PERR:   DWBE    FNERR
PRXLL:  DWBE    FNRXLL
PMAGF:  DWBE    FNMAGF
PMAGN:  DWBE    FNMAGN
PPVER:  DWBE    FNPVER
PSNDI:  DWBE    SNDIDX          ; tables in fixed B (fixed A is the tight one)
PSNDN:  DWBE    SNDNP           ; the notes: pitch, frames, VOLUME
PSNDN1: DWBE    SNDNF
PSNDN2: DWBE    SNDNV
PKT0:   DWBE    KT0
PKT1:   DWBE    KT1
PKT2:   DWBE    KT2
PKTP:   DWBE    KTP
PDEFSP: DWBE    DEFSPEC
PDEFURL: DWBE   DEFURL

; ---- start-up ----------------------------------------------------------------
; RESET restarts here but never resets the cart: the page, cart RAM and the
; socket are as they were. So everything is set up afresh (the socket is
; closed before it is opened again).
START:  LODI,R0 $20             ; interrupts inhibited (none are wired anyway)
        LPSU
        LODI,R0 PSLBASE
        LPSL
        LODI,R1 0
        STRA,R1 CURPG
        LODA,R0 *PBANK,R1       ; page 0 into the window
        EORZ    R0
        STRA,R0 PITCH
        STRA,R0 VOLUME
        LODI,R3 $F0-$D0         ; zone A cleared
ST2:    STRA,R0 $18D0-1,R3
        BDRR,R3 ST2
        LODI,R3 0               ; zone B (256)
ST3:    STRA,R0 $1A00,R3,-
        BRNR,R3 ST3
        LODI,R0 $FF
        STRA,R0 SNDP
        STRA,R0 SNDQ
        STRA,R0 KRAW
        STRA,R0 MYSEAT
        STRA,R0 VPOS            ; rows from the top of the screen
        BSTA,UN SCRINIT
        LODA,R0 *PMAGF          ; the cart: 'F','N', protocol 2 or later
        COMI,R0 $46
        BCFA,EQ STBAD
        LODA,R0 *PMAGN
        COMI,R0 $4E
        BCFA,EQ STBAD
        LODA,R0 *PPVER
        COMI,R0 2
        BCTA,LT STBAD
        BCTA,UN MAININIT


; SCRINIT: the screen: 13 rows of 8x16 cells, text, the look's ground and
; inks; cleared; the sprites off and 16 lines (each picture row twice), no
; animated glyphs. SCRCLR: the same but the ground and inks kept. Clobbers R0-R3.
SCRINIT: LODI,R0 LOOKBG
        STRA,R0 BGCB
SCRCLR: EORZ    R0
        STRA,R0 DCOLOR
        STRA,R0 GFXMOD
        LODI,R3 8
SCI1:   STRA,R0 SPRSH-1,R3      ; sprites parked (Y 0: off)
        BDRR,R3 SCI1
        STRA,R0 SPRC01
        STRA,R0 SC01SH
        STRA,R0 SPRC23
        STRA,R0 SC23SH
        LODI,R0 $FE             ; no arena glyphs to animate
        STRA,R0 ANWANT
        STRA,R0 ANSHOW
        LODI,R0 $FF             ; no bomber pictures wanted
        LODI,R3 4
SCI2:   STRA,R0 FRWANT-1,R3
        STRA,R0 FRSHOW-1,R3
        BDRR,R3 SCI2
        LODA,R0 BGCB
        STRA,R0 BGCOL
        LODI,R1 0
        LODI,R3 13
        BCTA,UN DCLR            ; tail call

; ---- the vblank ----------------------------------------------------------------
; VBSYNC: wait for the next vertical blank, then what must happen in it:
; the sprites (from their shadow), the disc (the 2637 digitises it only
; then), the user glyphs whose pictures changed (so no row is drawn half
; old, half new: the bombers' frames, the arena's animation), the sound.
; A pass that ran late, into a blank (VSEEN: the main loop saw the display
; since the last), has it now instead of waiting a frame more. Clobbers
; R0-R3.
VBSYNC: TPSU    $80
        BCFR,EQ VBS1            ; the display: wait for the blank
        LODA,R0 VSEEN           ; a blank: a new one?
        BCFR,EQ VBS0
VBSW:   TPSU    $80             ; the same one: its end, then the next
        BCTR,EQ VBSW
VBS1:   TPSU    $80
        BCFR,EQ VBS1
VBS0:   EORZ    R0
        STRA,R0 VSEEN
        LODI,R3 8               ; the sprites
VBS2:   LODA,R0 SPRSH-1,R3
        STRA,R0 SPRPOS-1,R3
        BDRR,R3 VBS2
; the disc: one axis a frame, each kept, the mux set for the other (not
; if the blank is over: outside it the pot reads $FF)
        TPSU    $80
        BCFR,EQ VBS4A
        LODA,R0 POT
        LODA,R1 AXSEL
        BCFR,EQ VBS3
        STRA,R0 AXV
        LODA,R0 BGCB
        IORI,R0 MUXH
        STRA,R0 BGCOL
        LODI,R0 1
        BCTR,UN VBS4
VBS3:   STRA,R0 AXH
        LODA,R0 BGCB
        STRA,R0 BGCOL
        EORZ    R0
VBS4:   STRA,R0 AXSEL
VBS4A:; the user glyphs: four rewrites at most, from the art page (borrowed)
        LODI,R1 PG_ART
        LODA,R0 *PBANK,R1
        LODI,R0 4
        STRA,R0 VBUD
        LODI,R1 3               ; the bombers' pictures, seat 3 down
VBF1:   LODA,R0 FRWANT,R1
        COMA,R0 FRSHOW,R1
        BCTR,EQ VBF3
        STRA,R0 FRSHOW,R1
        STRA,R1 VSEAT
        RRL,R0                  ; its frame: A_FRAMES + 8 f
        RRL,R0
        RRL,R0
        ADDI,R0 A_FRAMES&$FF
        STRA,R0 VSRCL
        LODI,R0 A_FRAMES>>8
        STRA,R0 VSRCH
        LODZ    R1              ; into glyph seat: its end, 8 s + 8
        RRL,R0
        RRL,R0
        RRL,R0
        ADDI,R0 8
        STRZ    R2
        BSTA,UN VBCOPY
        LODA,R1 VSEAT
        LODA,R0 VBUD
        SUBI,R0 1
        STRA,R0 VBUD
        BCTA,EQ VBF9
VBF3:   SUBI,R1 1
        BCFR,LT VBF1
        LODA,R0 ANWANT          ; the arena's: bomb, the arms across, down
        COMA,R0 ANSHOW
        BCTR,EQ VBF9
        STRA,R0 ANSHOW
        LODA,R1 VBUD
        COMI,R1 3
        BCTR,LT VBF8            ; (no room: next time)
        RRL,R0
        RRL,R0
        RRL,R0
        ADDI,R0 A_BOMB0&$FF
        STRA,R0 VSRCL
        LODI,R0 A_BOMB0>>8
        STRA,R0 VSRCH
        LODI,R2 8*(GBOMB-GUDC)+8
VBF4:   STRA,R2 VSEAT
        BSTA,UN VBCOPY
        LODA,R0 VSRCL           ; the next glyph's frames: 16 on
        ADDI,R0 16
        STRA,R0 VSRCL
        LODA,R0 VSEAT
        ADDI,R0 8
        STRZ    R2
        COMI,R0 8*(GFLAMEV-GUDC)+16
        BCFR,EQ VBF4
        BCTR,UN VBF9
VBF8:   LODI,R0 $FD             ; (wanted again)
        STRA,R0 ANSHOW
VBF9:   LODA,R3 CURPG           ; the window given back
        LODA,R0 *PBANK,R3
; the sound: a queued one starts, a note counts down, the next one, the end
        LODA,R1 SNDQ
        COMI,R1 $FF
        BCTR,EQ VBN1
        LODI,R0 $FF
        STRA,R0 SNDQ
        LODA,R0 *PSNDI,R1       ; its first note
        STRA,R0 SNDP
        EORZ    R0
        STRA,R0 SNDCNT
        COMI,R1 MUS_TITLE
        BCTR,LT VBN0
        LODI,R0 1
VBN0:   STRA,R0 SNDJ
VBN1:   LODA,R1 SNDP
        COMI,R1 $FF
        BCTR,EQ VBN9
        LODA,R0 SNDCNT
        BCFR,EQ VBN5            ; the note goes on
        LODA,R0 *PSNDN1,R1      ; the next: its frames (0: the end)
        BCFR,EQ VBN3
        LODI,R0 $FF
        STRA,R0 SNDP
        EORZ    R0
        STRA,R0 VOLUME
        STRA,R0 PITCH
        RETC,UN
VBN3:   STRA,R0 SNDCNT
        LODA,R0 *PSNDN,R1
        STRA,R0 PITCH
        LODA,R0 *PSNDN2,R1
        STRA,R0 VOLUME
        ADDI,R1 1
        STRA,R1 SNDP
VBN5:   LODA,R0 SNDCNT
        SUBI,R0 1
        STRA,R0 SNDCNT
VBN9:   RETC,UN
; VBCOPY: 8 bytes from VSRCH:VSRCL into the user glyphs, ending before
; UDC + R2. Leaf. Clobbers R0, R2, R3.
VBCOPY: LODI,R3 8
VBC1:   LODA,R0 *VSRCH,R3,-
        STRA,R0 UDC,R2,-
        BRNR,R3 VBC1
        RETC,UN

; SNDQUE: start sound or jingle R0 at the next vblank (none while muted;
; it cuts whatever plays). Leaf. Clobbers R1.
SNDQUE: STRZ    R1
        LODA,R0 FLAGS
        TMI,R0  FL_MUTE
        RETC,EQ
        STRA,R1 SNDQ
        RETC,UN

; FRAME: the next frame: the vblank, the clock, the controls; Option turns
; the sound off and on. Calls two down (VBSYNC's). Clobbers R0-R3.
FRAME:  BSTA,UN VBSYNC
        LODA,R0 $18FF           ; (the row the beam is on: D in the blank)
        STRA,R0 VBEND
        BSTA,UN TICKRD
        LODA,R0 INDIR
        STRA,R0 PRVDIR
        BSTR,UN KEYDEC
        LODA,R0 EVKEY
        COMI,R0 K_OPT
        RETC,LT
        RETC,GT
        LODA,R0 FLAGS
        EORI,R0 FL_MUTE
        STRA,R0 FLAGS
        LODI,R0 $FF             ; (and whatever plays stops)
        STRA,R0 SNDP
        EORZ    R0
        STRA,R0 VOLUME
        RETC,UN

; ---- input ----------------------------------------------------------------------
; KEYDEC: this frame's controls from the disc (as last digitised), the
; keypad and the panel. INDIR: directions held (diagonals too). INBTN:
; FIRE held, EVBTN: it went down. EVKEY: a key that went down (K_*,
; digits; FIRE is keypad 2's wire, so 2 is never a digit). Leaf.
; Clobbers R0-R2.
KEYDEC: LODI,R2 0
        LODA,R0 AXV
        COMI,R0 JOYLO
        BCFR,LT KD1
        IORI,R2 D_UP
KD1:    COMI,R0 JOYHI
        BCFR,GT KD2
        IORI,R2 D_DOWN
KD2:    LODA,R0 AXH
        COMI,R0 JOYLO
        BCFR,LT KD3
        IORI,R2 D_LEFT
KD3:    COMI,R0 JOYHI
        BCFR,GT KD4
        IORI,R2 D_RIGHT
KD4:    STRA,R2 INDIR
        LODA,R0 KEYPAD+1        ; FIRE
        ANDI,R0 $08
        BCTR,EQ KD5
        LODA,R0 INBTN           ; down: an event if it was up
        EORI,R0 1
        STRA,R0 EVBTN
        LODI,R0 1
        STRA,R0 INBTN
        BCTR,UN KD6
KD5:    STRA,R0 EVBTN
        STRA,R0 INBTN
; a key: the first found, edge-detected
KD6:    LODI,R1 K_NONE
        LODA,R0 KEYPAD          ; b3 1, b2 4, b1 7, b0 Clear
        ANDI,R0 $0F
        BCTR,EQ KD10
        STRZ    R2
        LODA,R0 *PKT0,R2
        BCTR,UN KD12
KD10:   LODA,R0 KEYPAD+1        ; b2 5, b1 8, b0 0 (b3 is FIRE)
        ANDI,R0 $07
        BCTR,EQ KD11
        STRZ    R2
        LODA,R0 *PKT1,R2
        BCTR,UN KD12
KD11:   LODA,R0 KEYPAD+2        ; b3 3, b2 6, b1 9, b0 Enter
        ANDI,R0 $0F
        BCTR,EQ KD13
        STRZ    R2
        LODA,R0 *PKT2,R2
        BCTR,UN KD12
KD13:   LODA,R0 PANEL           ; b0 Start, b1 Option, b2 Select
        ANDI,R0 $07
        BCTR,EQ KD14
        STRZ    R2
        LODA,R0 *PKTP,R2
KD12:   STRZ    R1
KD14:   LODZ    R1
        COMA,R0 KRAW
        STRA,R1 KRAW
        BCFR,EQ KD15
        LODI,R1 K_NONE          ; held: no event
KD15:   STRA,R1 EVKEY
        RETC,UN

; ---- the display ------------------------------------------------------------------
; DPOS: point the display at row R1 (0-12); R2 is left alone (the cell
; before the first one written: see DAT). Leaf. Clobbers R0.
DPOS:   LODZ    R1
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        STRA,R0 DROWL
        LODI,R0 SCR>>8
        STRA,R0 DROWH
        RETC,UN

; DPUTC: glyph R0 (0-63) in DCOLOR at the next cell (R2 pre-incremented).
; DGLYPH: R0 already carries its colour bits. An exact $C0 (a blank in
; the fourth ink) would start block graphics: it is written as $00, which
; looks the same. Leaf. Clobbers R0, advances R2.
DPUTC:  IORA,R0 DCOLOR
DGLYPH: COMI,R0 GMOSAIC
        BCFR,EQ DPC1
        EORZ    R0
DPC1:   STRA,R0 *DROWH,R2,+
        RETC,UN

; DTXT: the glyph string at R0:R1 (glyph codes, $FF ends it; DB strings
; are glyphs, see GLYPHS). DTXTS: the one at DSTRH:DSTRL. Calls one
; down. Clobbers R0, R3; advances R2.
DTXT:   STRA,R0 DSTRH
        STRA,R1 DSTRL
DTXTS:  LODI,R3 $FF
DTX1:   LODA,R0 *DSTRH,R3,+
        COMI,R0 $FF
        RETC,EQ
        BSTR,UN DPUTC
        BCTR,UN DTX1

; CENTRE: the glyph string at R0:R1 centred on row R3 (16 cells), in
; DCOLOR. Calls two down. Clobbers R0-R3.
CENTRE: STRA,R0 DSTRH
        STRA,R1 DSTRL
        LODI,R2 $FF             ; its length
CEN1:   LODA,R0 *DSTRH,R2,+
        COMI,R0 $FF
        BCFR,EQ CEN1
        LODI,R0 16              ; the cell before (16 - n) / 2
        SUBZ    R2
        RRR,R0
        ANDI,R0 $7F
        SUBI,R0 1
        STRZ    R2
        LODZ    R3
        STRZ    R1
        BSTA,UN DPOS
        BCTR,UN DTXTS           ; tail call

; DFILL: R3 cells of R0 (glyph and colour bits) at the next cells.
; Leaf. Clobbers R0, R3; advances R2.
DFILL:  STRA,R0 *DROWH,R2,+
        BDRR,R3 DFILL
        RETC,UN

; DCLR: R3 rows of spaces from row R1. Calls one down. Clobbers R0-R3.
DCLR:   BSTA,UN DPOS
        LODI,R2 $FF
        LODI,R0 0
DCL1:   STRA,R0 *DROWH,R2,+
        COMI,R2 15
        BCFR,EQ DCL1
        ADDI,R1 1
        BDRR,R3 DCLR
        RETC,UN

; DNUM: R0 (0-255) in decimal, right-aligned in R1 cells (1-3), in DCOLOR,
; at the next cells (a leading place blank). Calls one down. Clobbers
; R0, R1, R3; advances R2.
DNUM:   STRA,R0 NUMV
        LODI,R3 0               ; the places shown so far
        COMI,R1 3
        BCFR,EQ DNU2
        LODI,R1 100
        BSTR,UN DNPL
        LODI,R1 2
DNU2:   COMI,R1 2
        BCFR,EQ DNU3
        LODI,R1 10
        BSTR,UN DNPL
DNU3:   LODA,R0 NUMV            ; the units: always
        IORI,R0 GDIGIT
        BCTA,UN DPUTC           ; tail call
; one place: R1 its power; a leading zero is a space (R3 counts digits shown)
DNPL:   LODI,R0 0
        STRA,R0 NUMD
DNP1:   LODA,R0 NUMV
        COMZ    R1
        BCTR,LT DNP2
        SUBZ    R1
        STRA,R0 NUMV
        LODA,R0 NUMD
        ADDI,R0 1
        STRA,R0 NUMD
        BCTR,UN DNP1
DNP2:   LODA,R0 NUMD
        IORZ    R3
        BCTR,EQ DNP3            ; a leading zero
        LODI,R3 1
        LODA,R0 NUMD
        IORI,R0 GDIGIT
        BCTA,UN DPUTC
DNP3:   EORZ    R0
        BCTA,UN DPUTC

; UDCSET: user glyph R2 (0-7) = the 8 bytes at R0:R1 in the art page
; (A_*), now: a screen's own, set before it shows (the art page borrowed).
; Calls one down. Clobbers R0-R3.
UDCSET: STRA,R0 VSRCH
        STRA,R1 VSRCL
        LODI,R1 PG_ART
        LODA,R0 *PBANK,R1
        LODZ    R2
        RRL,R0
        RRL,R0
        RRL,R0
        ADDI,R0 8
        STRZ    R2
        BSTA,UN VBCOPY
        LODA,R3 CURPG
        LODA,R0 *PBANK,R3
        RETC,UN

; ---- the arena's tiles -----------------------------------------------------------
; the cell each tile code shows as (src/proto.h T_*; look.inc's inks)
TCELLS: DB      TC_FLOOR,TC_PILLAR,TC_BLOCK,TC_BURN,TC_BOMB,TC_DUD
        DB      TC_FLAMEC,TC_FLAMEH,TC_FLAMEV,TC_FLAMEH,TC_FLAMEH,TC_FLAMEV,TC_FLAMEV
        DB      TC_FIRE,TC_BOMBUP,TC_SPEED,TC_KICK,TC_GLOVE

; TDRAW: tile R1 (0-142) as the map has it (our pending bomb, PEND, a bomb)
; into its cell. TCELL: cell R0 into tile R1's. Leaf. Clobbers R0-R2.
TDRAW:  LODA,R0 ST+OFF_MAP,R1
        COMA,R1 PEND
        BCFR,EQ TDR1
        LODI,R0 T_BOMB
TDR1:   STRZ    R2
        LODA,R0 TCELLS,R2
TCELL:  STRA,R0 TDV
        LODI,R2 AROW            ; the row: tile / 13
        LODZ    R1
TDR2:   COMI,R0 ARENA_W
        BCTR,LT TDR3
        SUBI,R0 ARENA_W
        ADDI,R2 1
        BCTR,UN TDR2
TDR3:   ADDI,R0 ACOL            ; R2:R0 = row, column
        STRZ    R1
        LODZ    R2
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        ADDZ    R1
        STRZ    R2
        LODA,R0 TDV
        STRA,R0 SCR,R2
        RETC,UN

; ACTQ: an action to send: R1 its kind (ACT_*), R2 R3 its arguments; the
; next seq (never 0) in R0. One waits at a time: a second replaces it.
; Leaf.
ACTQ:   STRA,R1 AK
        STRA,R2 A1
        STRA,R3 A2
        LODA,R0 TXF
        IORI,R0 TX_ACT
        STRA,R0 TXF
        LODA,R0 ASEQ
        ADDI,R0 1
        BCFR,EQ ACQ1
        LODI,R0 1
ACQ1:   STRA,R0 ASEQ
        RETC,UN

; ---- pages ---------------------------------------------------------------------------
; PGCALL: entry R3 of page R1, which stays in the window. Calls the page,
; costing one return address, not two.
PGCALL: STRA,R1 CURPG
        LODA,R0 *PBANK,R1
        BXA     WIN,R3          ; (the entry's return is our caller's)
; PGFAR: the same, from a page: the page that was there comes back. One
; level only (PGSAVE).
PGFAR:  LODA,R0 CURPG
        STRA,R0 PGSAVE
        BSTR,UN PGCALL
        LODA,R1 PGSAVE
        STRA,R1 CURPG
        LODA,R0 *PBANK,R1
        RETC,UN

        INCLUDE "src/main.inc"

        IF      $ > $0600
        ERROR   "fixed A runs into RAM-L"
        ENDIF

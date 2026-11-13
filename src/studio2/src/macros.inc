; macros.inc -- FujiBlast's macros, after fujinet.inc and s2macro.inc
; (M.U.L.E.'s clients/studio2 macros.inc, and the state's own).
;
; Registers (fujiblst.asm has the whole list): RA.1 is $08 everywhere
; outside the runtime, so RA reaches the variables on page $08 in three
; instructions (VAR/GETV/PUTV/SETV). RB.1 is HI(ST): RB reads the state in
; the cart's RAM the same way (GETS). RC is the leaves' PC (LCALL).

; Keep `size` bytes on one page, and out of $x000-$x3FF. Every routine with
; a short branch starts with one; tools/s2/fitcheck.py holds the sizes to
; the routines (tools/s2/fitfix.py sets them). Never fall into an RFIT:
; the gap is zeros, and $00 is IDL.
RFIT    MACRO   size
        IF      ((($) & 0FFH) + (size)) > 100H
        ORG     (($) + 0FFH) & 0FF00H
        ENDIF
        IF      (($) & 0C00H) == 0
        ORG     (($) & 0F000H) + 0400H
        ENDIF
        ENDM

; Keep a data block of `size` bytes out of $x000-$x3FF (no page rule).
DFIT    MACRO   size
        IF      (($) & 0C00H) == 0
        ORG     (($) & 0F000H) + 0400H
        ENDIF
        IF      ($) + (size) > (($) & 0F000H) + 1000H
        ORG     (($) & 0F000H) + 1400H
        ENDIF
        ENDM

; A table that one LDI of its low byte must index: all on one page.
PFIT    MACRO   size
        IF      ((($) & 0FFH) + (size)) > 100H
        ORG     (($) + 0FFH) & 0FF00H
        ENDIF
        IF      (($) & 0C00H) == 0
        ORG     (($) & 0F000H) + 0400H
        ENDIF
        ENDM

; ---- console RAM --------------------------------------------------------
VAR     MACRO   v               ; RA -> v (page $08)
        LDI     LO(v)
        PLO     RA
        ENDM
GETV    MACRO   v               ; D = v
        VAR     v
        LDN     RA
        ENDM
PUTV    MACRO   v               ; v = D (RD.1 is free between calls)
        PHI     RD
        VAR     v
        GHI     RD
        STR     RA
        ENDM
SETV    MACRO   v, val          ; v = val
        VAR     v
        LDI     val
        STR     RA
        ENDM
VARX    MACRO   base            ; RA -> base[s], s = R9.0 (page $08)
        GLO     R9
        ADI     LO(base)
        PLO     RA
        ENDM
INCV    MACRO   v               ; v += 1; D = the new v
        VAR     v
        LDN     RA
        ADI     1
        STR     RA
        ENDM

; ---- the state, in the cart's RAM -----------------------------------------
; GETS off: D = ST[off] (RB.1 is HI(ST): RB.0 left at off).
GETS    MACRO   off
        LDI     LO(ST+(off))
        PLO     RB
        LDN     RB
        ENDM

; D = the byte at a (any cart page: the reply, the status). R8.
SLD     MACRO   a
        LDI     HI(a)
        PHI     R8
        LDI     LO(a)
        PLO     R8
        LDN     R8
        ENDM

; ---- poking the cart's RAM --------------------------------------------------
; POKEAT a: the poke pointer at a (a constant), the stream open; then PKD
; stores D there and moves on (RF.1 on REGDATA meanwhile: PKEND parks it).
; A REGSEL anywhere closes the stream. 27 instructions, then 2 a byte.
POKEAT  MACRO   a
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_PTRLO
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        LDI     LO(a)
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_PTRHI
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        LDI     HI(a)
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_POKE
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        ENDM
; POKEST: the poke pointer at ST's page, its low byte D (ST + D when ST is
; page-aligned), the stream open: 22 instructions.
POKEST  MACRO
        PHI     RD
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_PTRLO
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        GHI     RD
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_PTRHI
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        LDI     HI(ST)
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_POKE
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        ENDM
; POKESTP page: the poke pointer at page + R9.0, the stream open. Clobbers
; D (keeps R9).
POKESTP MACRO   page
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_PTRLO
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        GLO     R9
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_PTRHI
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        LDI     HI(page)
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGSEL)
        PHI     RF
        LDI     FN_REG_POKE
        PLO     RF
        LDN     RF
        LDI     HI(FN_REGDATA)
        PHI     RF
        ENDM
PKD     MACRO                   ; store D, the pointer on
        PLO     RF
        LDN     RF
        ENDM
PKEND   MACRO
        LDI     FN_PARK
        PHI     RF
        ENDM

; ---- hotspots ------------------------------------------------------------
; A text-engine parameter list: RF.1 on TX_PARAM, then P/PD three
; instructions a byte, then GOP runs the operation and parks RF.
PARS    MACRO
        LDI     TX_PARAM
        PHI     RF
        ENDM
P       MACRO   v
        LDI     v
        PLO     RF
        LDN     RF
        ENDM
PD      MACRO                   ; the parameter in D
        PLO     RF
        LDN     RF
        ENDM
GOP     MACRO   code
        LDI     TX_OP
        PHI     RF
        LDI     code
        PLO     RF
        LDN     RF
        LDI     FN_PARK
        PHI     RF
        ENDM
; A register write: REGSEL n, REGDATA v.
REG     MACRO   n, v
        LDI     n
        HOT     HI(FN_REGSEL)
        LDI     v
        HOT     HI(FN_REGDATA)
        ENDM

; HOLD1 / HOLD0: the cart holds the picture as it is while a pass draws, so
; it shows whole (CLS lets it go too).
HOLD1   MACRO
        PARS
        P       1
        GOP     TOP_HOLD
        ENDM
HOLD0   MACRO
        PARS
        P       0
        GOP     TOP_HOLD
        ENDM

; ---- calls ----------------------------------------------------------------
; LCALL leaf: a leaf on its own page, run with P = C; it returns with
; SEP R3. 6 instructions (SCRT's FCALL/FRET: 33). A leaf never FCALLs,
; FJMPs or LCALLs, never touches RD.0 (the beep), keeps RA.1 and RB.1, and
; returns its result in D/DF or R8/R9 (tools/s2/checkleaf.py).
LCALL   MACRO   adr
        LDI     HI(adr)
        PHI     RC
        LDI     LO(adr)
        PLO     RC
        SEP     RC
        ENDM

; The routine at R9 (icall), as if FCALLed.
ICALL   MACRO
        SEP     R4
        DW      icall
        ENDM

; ---- drawing ---------------------------------------------------------------
OPN     MACRO   code            ; a text-engine operation, no parameters
        LDI     code
        HOT     TX_OP
        ENDM

; ---- sound -----------------------------------------------------------------
SND     MACRO   id              ; a cue (sound.inc)
        LDI     id
        FCALL   snd_play
        ENDM

; ---- segments ---------------------------------------------------------------
SEGEND  MACRO   lim             ; a segment must stop short of lim
        IF      $ > lim
        ERROR   "segment overflows its window"
        ENDIF
        ENDM

; ---------------------------------------------------------------------------
; fujiblst.asm -- FujiBlast for the RCA Studio II, over FujiNet.
;
; A hand-written CDP1802 port of the FujiBlast client (src/*.c): the link,
; the walker and the screens transcribed from the Channel F port
; (src/channelf), the console's runtime from M.U.L.E.'s clients/studio2.
; The cart draws (fujinet-firmware pico/studio2, protocol 2): the tiles are
; stamped into its background, the bombers and the bombs in flight are its
; sprites; the 1802 runs the game and the link and sends small operations.
;
; MEMORY. The runtime at $0400-$07FB ("FUJI" at $07FC); the code at
; $0C00-$CFFF, never in $x000-$x3FF (RFIT/DFIT); the cart's RAM at
; $D400-$D7FF, written by POKE (equ.inc). Console RAM ($0800-$09FF): the
; ledger (src/ram.inc) and the stack.
;
; REGISTERS. R0 DMA, R1 the ISR, R2 the stack (X = 2), R3 the PC, R4-R6
; SCRT (FCALL/FRET), R7 FJMP, R8-R9 scratch, RA.1 = $08 (GETV/PUTV), RB.1 =
; HI(ST) (GETS), RC the leaves' PC (LCALL), RD.0 the beep (ISR) and RD.1
; SCRT's, RE the frame count (ISR), RF the hotspots (macros only).
;
; RULES (tools/s2): no 3-cycle opcode ($C0-$CF) and no IDL (checkrom), a
; short branch stays on its page (RFIT, fitcheck), RF only read with LDN,
; nothing stored through RB (checkrom), a value in D not lost to a pointer
; load (checkldr).
; ---------------------------------------------------------------------------
        CPU     1802
        INCLUDE "fujinet.inc"
        INCLUDE "s2macro.inc"
        INCLUDE "macros.inc"
        INCLUDE "flags.inc"
        INCLUDE "proto.inc"
        INCLUDE "equ.inc"
        INCLUDE "ram.inc"
        INCLUDE "art.inc"

; ---- the runtime: $0400-$07FB ----------------------------------------------------
        ORG     0400H
        INCLUDE "s2call.inc"
        INCLUDE "fujilib.inc"
RT_SEGEND:
        SEGEND  FN_CLAIM
        S2CLAIM

; ---- the code: $0C00-$0FFF, then $x400-$xFFF up to $B3FF ---------------------------
        ORG     0C00H
        INCLUDE "main.inc"
        INCLUDE "input.inc"
        INCLUDE "sound.inc"
        INCLUDE "ui.inc"
        INCLUDE "gfx.inc"
        INCLUDE "net.inc"
        INCLUDE "p_boot.inc"
        INCLUDE "p_menu.inc"
        INCLUDE "p_room.inc"
        INCLUDE "p_play.inc"
        INCLUDE "p_arena.inc"
        INCLUDE "p_test.inc"
DATA_SEGEND:                            ; (data from here: no page rules)
        INCLUDE "config.inc"
        INCLUDE "artdata.inc"
        DFIT    STATE_SIZE
        INCLUDE "canned.inc"
FIX_SEGEND:
        SEGEND  0B400H

; ---- the bombers' frames: $B400-$B87F (page s = seat s) ------------------------------
        ORG     0B400H
        INCLUDE "artfr.inc"
FR_SEGEND:
        SEGEND  0C000H

; ---- the cart's RAM: image pages the client writes ----------------------------------
        ORG     ST
        REPT    CRAMEND-ST
        DB      0
        ENDM

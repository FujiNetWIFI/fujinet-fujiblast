; fujiblst.asm -- FujiBlast for the Bally Astrocade, on the FujiNet cart.
;
; A banked cartridge (firmware protocol 2, APPBANK): the image is the 8K
; window and 4K pages; one read at FNBKSEL+page maps a page into
; 2000H-2FFFH. 3000H-3AFFH never moves: the resident core (start-up, the
; raster and frame interrupts, paging, the frame, input, text, the mailbox,
; the main loop) lives there, the mailbox above it. Every page opens with
; the 55H header whose start vector is RSTENT, so a console RESET with any
; page mapped comes back to a cold start; then its entries, a JP each
; (E_*). Code in a page never maps another page itself: it goes through
; FARCALL or PGCALL (resident), which give its own back.
;
;   2000H-2FFFH  page 0 (boot) or any other: one assembly, every page
;                PHASEd to 2000H (tools/astro/checkcalls.py stops a jump
;                from one page into another)
;   3000H-3AFFH  resident
;   3B00H-3FFFH  the mailbox (fujilib.inc); "FUJI" at 3CFCH (build.sh)
;   4000H-4D1FH  the picture, 84 lines (equ.inc)
;   4D20H-4FBFH  the state, the variables, the vector, the stack (ram.inc)
;
; It ports the C client (src/*.c) by way of the Arcadia's (src/arcadia):
; the link, the decoder, the walker and the screens are transcribed from
; it, routine for routine and name for name; the hardware layer is
; M.U.L.E.'s Astrocade client's (fujinet-multiplayer-mule clients/astrocade)
; and the ISS tracker's raster (fujinet-iss-tracker astrocade/raster.inc).

        INCLUDE "srcdir/src/hvglib.inc"
        INCLUDE "srcdir/src/equ.inc"
        INCLUDE "proto.inc"
        INCLUDE "flags.inc"
        INCLUDE "artequ.inc"
        INCLUDE "srcdir/src/look.inc"
        INCLUDE "srcdir/src/pages.inc"
        INCLUDE "srcdir/src/ram.inc"
        INCLUDE "srcdir/src/macros.inc"

; =======================================================================
; page 0: boot
; =======================================================================
        ORG     FIRSTC
MB_P0:
        PAGEHDR
        INCLUDE "srcdir/src/p_boot.inc"
MB_P0END:

; =======================================================================
; resident: 3000H-3AFFH
; =======================================================================
        ORG     3000H
MB_R:
RSTENT: LD      A,(FNBKSEL+0)   ; RESET with any page mapped: page 0, cold
        JP      PRGSTR
RNAME:  DB      "FUJIBLAST",0

MB_R_CORE:
        INCLUDE "srcdir/src/core.inc"
MB_R_MAIN:
        INCLUDE "srcdir/src/main.inc"
MB_R_MAIL:
        INCLUDE "srcdir/src/mail.inc"
MB_R_TEXT:
        INCLUDE "srcdir/src/text.inc"
MB_R_FONT:
        INCLUDE "srcdir/src/font46.inc"
MB_R_FUJI:
        INCLUDE "srcdir/src/fujilib.inc"
MB_R_DATA:
        INCLUDE "srcdir/src/bands.inc"
        INCLUDE "config.inc"
MB_REND:

; =======================================================================
; the pages
; =======================================================================
        ORG     2000H+PG_NET*1000H
        PHASE   2000H
MB_P2:
        PAGEHDR
        INCLUDE "srcdir/src/p_net.inc"
MB_P2END:
        DEPHASE

        ORG     2000H+PG_ARENA*1000H
        PHASE   2000H
MB_P3:
        PAGEHDR
        INCLUDE "srcdir/src/p_arena.inc"
MB_P3END:
        DEPHASE

        ORG     2000H+PG_PLAY*1000H
        PHASE   2000H
MB_P4:
        PAGEHDR
        INCLUDE "srcdir/src/p_play.inc"
MB_P4END:
        DEPHASE

        ORG     2000H+PG_DRAW*1000H
        PHASE   2000H
MB_P5:
        PAGEHDR
        INCLUDE "srcdir/src/p_draw.inc"
MB_P5_ART:
        INCLUDE "art.inc"
MB_P5END:
        DEPHASE

        ORG     2000H+PG_MENU*1000H
        PHASE   2000H
MB_P6:
        PAGEHDR
        INCLUDE "srcdir/src/p_menu.inc"
MB_P6END:
        DEPHASE

        ORG     2000H+PG_ROOM*1000H
        PHASE   2000H
MB_P7:
        PAGEHDR
        INCLUDE "srcdir/src/p_room.inc"
MB_P7END:
        DEPHASE

        ORG     2000H+PG_SND*1000H
        PHASE   2000H
MB_P8:
        PAGEHDR
        INCLUDE "srcdir/src/p_snd.inc"
MB_P8_TAB:
        INCLUDE "snd.inc"
MB_P8END:
        DEPHASE

        ORG     2000H+PG_TEST*1000H
        PHASE   2000H
MB_P9:
        PAGEHDR
        INCLUDE "srcdir/src/p_test.inc"
MB_P9_CANNED:
        INCLUDE "canned.inc"
MB_P9END:
        DEPHASE

        ORG     2000H+PG_ICON*1000H
        PHASE   2000H
MB_P10:
        PAGEHDR
        INCLUDE "srcdir/src/p_icon.inc"
MB_P10_ART:
        INCLUDE "icon.inc"
MB_P10END:
        DEPHASE

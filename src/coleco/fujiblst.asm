; fujiblst.asm -- FujiBlast for the ColecoVision, on the FujiNet cart.
;
; A hand-written Z80 port (zmac 1.3): the Adam port's screen, art and sound
; (src/adam, itself the C client on the TMS9918A), the Astrocade port's logic
; and link (src/astrocade), and M.U.L.E.'s ColecoVision client's cart layer
; (fujinet-multiplayer-mule clients/coleco). A stock console: 1K of RAM. The
; FujiNet cartridge serves a banked image (APPBANK) with its mailbox at F800H:
;
;   8000H-BFFFH  fixed: the header, the frame interrupt, the screen, the link
;   C000H-DFFFH  the page: one read of FNBKSEL+n maps image page n
;   E000H-F7FFH  fixed (the art)
;   F800H-FFFFH  the mailbox
;
; Page 0 is image 4000H (the window's own C000H); page n >= 1 follows the
; first 32K. A console RESET leaves the page where it was: START selects
; page 0. Code in a page never maps another page itself: it goes through
; FARCALL or PGCALL (fixed). zmac wants its ORGs in order, so the pages
; n >= 1 are assembled first, at 0000H.. (each PHASEd to C000H), then the
; window at 8000H; build.sh and tools/coleco/mkimage.py put them in order.

        INCLUDE "os7.inc"
        INCLUDE "equ.inc"
        INCLUDE "proto.inc"
        INCLUDE "flags.inc"
        INCLUDE "artequ.inc"
        INCLUDE "pages.inc"
        INCLUDE "ram.inc"
        INCLUDE "macros.inc"

; =======================================================================
; the pages 1.. : two 4K pages to an image page (pages.inc)
; =======================================================================
        ORG     0000H
        PHASE   0C000H
MB_P1:  PAGEHDR 1
        INCLUDE "p_net.inc"
MB_P1END:
        DEPHASE

        ORG     1000H
        PHASE   0D000H
MB_P2:  PAGEHDR 1
        INCLUDE "p_arena.inc"
MB_P2END:
        DEPHASE

        ORG     2000H
        PHASE   0C000H
MB_P3:  PAGEHDR 2
        INCLUDE "p_play.inc"
MB_P3END:
        DEPHASE

        ORG     3000H
        PHASE   0D000H
MB_P4:  PAGEHDR 2
        INCLUDE "p_menu.inc"
MB_P4END:
        DEPHASE

        ORG     4000H
        PHASE   0C000H
MB_P5:  PAGEHDR 3
        INCLUDE "p_room.inc"
MB_P5END:
        DEPHASE

        ORG     5000H
        PHASE   0D000H
MB_P6:  PAGEHDR 3
        INCLUDE "p_test.inc"
MB_P6END:
        DEPHASE

; =======================================================================
; the window: fixed, 8000H-BFFFH
; =======================================================================
        ORG     8000H
MB_F:
; The cartridge header. 55H AAH skips the BIOS's title screen: straight to
; START.
        DB      55H,0AAH
        DW      SPRORD          ; OS7's sprite tables and work buffer: never
        DW      SPRORD          ;   used (its object routines aren't)
        DW      SPRORD
        DW      CTLDAT          ; POLLER's controller area
        DW      START
        JP      RSTRET          ; RST 08H .. 38H
        JP      RSTRET
        JP      RSTRET
        JP      RSTRET
        JP      RSTRET
        JP      RSTRET
        JP      RSTRET
        JP      NMI             ; 8021H: the frame
        DB      "FUJIBLAST/FUJINET/2026",0
SPRORD  EQU     SNDAREA         ; (harmless: OS7 never writes there for us)
RSTRET: RET

MB_F_CORE:
        INCLUDE "core.inc"
MB_F_VDP:
        INCLUDE "vdp.inc"
MB_F_GFX:
        INCLUDE "gfx.inc"
MB_F_MAIL:
        INCLUDE "mail.inc"
MB_F_SOUND:
        INCLUDE "sound.inc"
MB_F_FUJI:
        INCLUDE "fujilib.inc"
MB_F_ARENA:
        INCLUDE "arena.inc"
MB_F_SPR:
        INCLUDE "sprites.inc"
MB_F_DATA:
        INCLUDE "config.inc"
MB_F_LOGO:
        INCLUDE "artl.inc"
MB_F_MAIN:
        INCLUDE "main.inc"
MB_FEND:

; =======================================================================
; page 0: boot (C000H at power-on)
; =======================================================================
        ORG     0C000H
MB_P0:  PAGEHDR 0
        INCLUDE "p_boot.inc"
MB_P0_ART:
        INCLUDE "art0.inc"
MB_P0END:

; =======================================================================
; the window: fixed, E000H-F7FFH
; =======================================================================
        ORG     0E000H
MB_G:
MB_G_ART:
        INCLUDE "art.inc"
MB_GEND:

; the mailbox: F800H-FFFFH, clear but for the claim mkimage stamps
        ORG     0FFFFH
        DB      0

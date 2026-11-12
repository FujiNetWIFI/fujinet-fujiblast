; fujiblst.asm -- FujiBlast for the Emerson Arcadia 2001, on the FujiNet cart.
;
; Assembled once per page (build.sh writes fb_pN.asm: PAGE EQU N, then
; this): every assembly holds the fixed halves, identically (the build
; checks), and its own page in the window at $0800-$0FFF. So a page can
; call fixed code but never another page -- another page's labels don't
; exist in its assembly. The image (protocol 2, APPBANK) is page 0's 8K,
; the mailbox empty and claimed, then pages 1.. (tools/arc/mkblast.py).
;
;   $0000-$05FF  fixed A   src/fixa.inc   start, vblank, input, display, main loop
;   $0600-$07FF  RAM-L     cart RAM: the state, the table list (src/ram.inc)
;   $0800-$0FFF  window    a 2K page: entries at $0800 + 3k, its id at $0FFF
;   $2000-$28FF  fixed B   src/fixb.inc   the mailbox, cart RAM writes, tables
;   $2900-$2AFF  RAM-H     cart RAM: free
;   $2B00-$2FFF  mailbox
;
; It ports the C client (src/*.c): comments name the routine each piece
; comes from. The link, the decoder and the walker follow the Odyssey 2's
; (src/o2), the hardware layer M.U.L.E.'s Arcadia client.

        CPU     2650
        INCLUDE "src/macros.inc"
        GLYPHS
        INCLUDE "flags.inc"
        INCLUDE "proto.inc"
        INCLUDE "artequ.inc"
        INCLUDE "src/equ.inc"
        INCLUDE "src/look.inc"
        INCLUDE "src/ram.inc"
        INCLUDE "src/pages.inc"

        INCLUDE "src/fixa.inc"

        ORG     WIN
        IF      PAGE == PG_BOOT
        INCLUDE "src/p_boot.inc"
        ELSEIF  PAGE == PG_MENU
        INCLUDE "src/p_menu.inc"
        ELSEIF  PAGE == PG_NET
        INCLUDE "src/p_net.inc"
        ELSEIF  PAGE == PG_ARENA
        INCLUDE "src/p_arena.inc"
        ELSEIF  PAGE == PG_PLAY
        INCLUDE "src/p_play.inc"
        ELSEIF  PAGE == PG_ROOM
        INCLUDE "src/p_room.inc"
        ELSEIF  PAGE == PG_ART
        INCLUDE "src/p_art.inc"
        ELSEIF  PAGE == PG_TEST
        INCLUDE "src/p_test.inc"
        ELSE
        INCLUDE "src/p_stub.inc"
        ENDIF
        IF      $ > $0FFF
        ERROR   "this page is full"
        ENDIF
        ORG     $0FFF
        DB      PAGE            ; the page's id

        ORG     $2000
        INCLUDE "src/fixb.inc"
        IF      $ > $2900
        ERROR   "fixed B runs into RAM-H"
        ENDIF

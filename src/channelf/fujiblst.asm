; ---------------------------------------------------------------------------
; fujiblst.asm -- FujiBlast for the Fairchild Channel F, over FujiNet.
;
; A hand-written F8 port of the FujiBlast client (src/*.c): the link, the
; walker and the screens transcribed from the Astrocade port
; (src/astrocade), the console's idioms from M.U.L.E.'s clients/channelf.
; The display (src/engine.inc) writes a pixel only when it changes.
;
; THE CART. fujinet-firmware pico/channelf at protocol 2: the fixed 16K at
; $0800-$47FF, and one 8K page at a time at $4800-$67FF, chosen by a store
; to $FD80+n (APPBANK); a 60 Hz tick at $FC0E; 30K of RAM at $8000. The
; image is the 16K and six pages: the fixed block is assembled at 0
; PHASEd to $0800, page n at $4000 + n * $2000 PHASEd to $4800, so p2bin's
; 0-$FFFF is the image. Only SELPG stores to $FD80
; (tools/chf/checkcalls.py). Code in a page never calls another page.
; ---------------------------------------------------------------------------
        CPU F3850

        INCLUDE "srcdir/src/equ.inc"
        INCLUDE "srcdir/src/macros.inc"
        INCLUDE "srcdir/src/ram.inc"
        INCLUDE "flags.inc"
        INCLUDE "proto.inc"

; ---- pages --------------------------------------------------------------
PG_BOOT  EQU 0                  ; the start, the appkeys, the socket, messages
PG_MENU  EQU 1                  ; the title, the name, the rooms
PG_ARENA EQU 2                  ; the arena: drawn, played
PG_ROOM  EQU 3                  ; the waiting room, the trophies
PG_ART   EQU 4                  ; the art and tables: copied to RAM at boot
PG_TEST  EQU 5                  ; the rigs' offline scenes
NPAGES   EQU 6

; PAGEHDR -- a page's first bytes: its id twice (the second complemented),
; then its entries (E_*).
PAGEHDR MACRO id, e1, e2, e3, e4
        DB id, (~(id))&0FFH
        JMP e1
        JMP e2
        JMP e3
        JMP e4
        ENDM
E_ENTER EQU 2
E_STATE EQU 5
E_TICK  EQU 8
E_EXTRA EQU 11

; the boot page's messages (MSGID; b7 on E_FAIL: the room forgotten too)
E_SAVENM EQU E_EXTRA            ; the boot page's: key 0 = MYNAME
E_CONN   EQU E_EXTRA+3          ;   "CONNECTING", the socket: r0 = 1 open
E_JOIN   EQU E_EXTRA+6          ;   the room kept, "JOINING", the socket
E_CLOSE  EQU E_EXTRA+9          ;   the socket closed
E_CLRRM  EQU E_EXTRA+12         ;   key 0x10 emptied, no table
E_MSG    EQU E_EXTRA+15         ;   message MSGID
E_FAIL   EQU E_EXTRA+18         ;   the socket closed, message MSGID

M_JOIN   EQU 0
M_NOANS  EQU 1
M_LOST   EQU 2
M_NOROOM EQU 3
M_VERSION EQU 4
M_CONN   EQU 5
M_QUIET  EQU 6

; =======================================================================
; the fixed 16K: $0800-$47FB
; =======================================================================
        ORG 0
        PHASE 0800H
MB_FIX:
        DB 055H                 ; the BIOS's cart signature
        DB 000H                 ; ($0801 is skipped)
        JMP START               ; $0802

        INCLUDE "srcdir/src/start.inc"
MB_MAIN:
        INCLUDE "srcdir/src/main.inc"
MB_MAIL:
        INCLUDE "srcdir/src/mail.inc"
MB_NET:
        INCLUDE "srcdir/src/net.inc"
MB_ENGINE:
        INCLUDE "srcdir/src/engine.inc"
MB_SLOTS:
        INCLUDE "srcdir/src/slots.inc"
MB_INPUT:
        INCLUDE "srcdir/src/input.inc"
MB_SOUND:
        INCLUDE "srcdir/src/sound.inc"
MB_FONT:
        INCLUDE "font.inc"
MB_XYPX:
        INCLUDE "xypx.inc"
MB_FIXEND:
        IF $ > 047FCH
        ERROR "the fixed block runs into the claim"
        ENDIF
        DEPHASE

; the claim: the cart keeps the mailbox alive for an image that has it
        ORG 03FFCH
        DB "FUJI"

; =======================================================================
; the pages: $4800-$67FF
; =======================================================================
PAGE    MACRO n
        ORG 04000H+(n)*02000H
        PHASE PGWIN
        ENDM

        PAGE PG_BOOT
MB_P0:  PAGEHDR PG_BOOT, BENTER, STUB, STUB, SAVENM
        JMP CONN                ; E_CONN
        JMP JOIN                ; E_JOIN
        JMP SCLOSE              ; E_CLOSE
        JMP CLRROOM             ; E_CLRRM
        JMP MSG                 ; E_MSG
        JMP FAIL                ; E_FAIL
        INCLUDE "srcdir/src/p_boot.inc"
MB_P0END:
        DEPHASE

        PAGE PG_MENU
MB_P1:  PAGEHDR PG_MENU, MENTER, TLSTART, TLTICK, STUB
        INCLUDE "srcdir/src/p_menu.inc"
MB_P1END:
        DEPHASE

        PAGE PG_ARENA
MB_P2:  PAGEHDR PG_ARENA, AENTER, ASTATE, ATICK, STUB
        INCLUDE "srcdir/src/p_arena.inc"
        INCLUDE "srcdir/src/p_play.inc"
MB_P2END:
        DEPHASE

        PAGE PG_ROOM
MB_P3:  PAGEHDR PG_ROOM, RENTER, RSTATE, RTICK, STUB
        INCLUDE "srcdir/src/p_room.inc"
MB_P3END:
        DEPHASE

; The art page is assembled where START copies it: DATA, in RAM.
        ORG 04000H+PG_ART*02000H
        PHASE DATA
MB_P4:  DB PG_ART, (~PG_ART)&0FFH
        INCLUDE "art.inc"
        INCLUDE "snd.inc"
MB_P4END:
DATALEN EQU $-DATA
        IF DATALEN > PGSIZE
        ERROR "the art page is over 8K"
        ENDIF
        DEPHASE

        PAGE PG_TEST
MB_P5:  PAGEHDR PG_TEST, TENTER, STUB, TTICK, STUB
        INCLUDE "srcdir/src/p_test.inc"
        INCLUDE "canned.inc"
MB_P5END:
        DEPHASE

        ORG 0FFFFH
        DB 0FFH                 ; (p2bin: the image is all 64K)
        END

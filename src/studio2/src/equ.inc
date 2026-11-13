; equ.inc -- FujiBlast's own numbers on the Studio II. The protocol's are in
; build/studio2/proto.inc (tools/o2/mkproto.py), the pictures' in art.inc
; (tools/s2/mkart_s2.py); the cart's in rt/fujinet.inc.

; ---- the cart's RAM: image pages the client writes by POKE ($D400-$DBFF) ----
ST      EQU     0D400H          ; the state (247), patched in place
TBLS    EQU     0D500H          ; the table list: 8 x 24
CR_URL  EQU     0D600H          ; 66: the room's URL (key 0x10), "" none
TRACE   EQU     0D700H          ; the rigs' traces (DEBUG)
CRAMEND EQU     0D800H          ; the cart RAM pages the image carries

; ---- the network --------------------------------------------------------------
NETDEV  EQU     71H             ; N1:
NCOPEN  EQU     4FH
NCCLOSE EQU     43H
NCREAD  EQU     52H
NCSTAT  EQU     53H
NCWRITE EQU     57H
NMRW    EQU     0CH             ; OPEN mode: read and write
FCACK   EQU     06H
FCCKEY  EQU     0DBH            ; CLOSE_APPKEY
FCOKEY  EQU     0DCH            ; OPEN_APPKEY
FCRKEY  EQU     0DDH            ; READ_APPKEY
FCWKEY  EQU     0DEH            ; WRITE_APPKEY
AK_CREATOR EQU  1               ; the lobby's keys: creator 1, app 1
AK_APP  EQU     1
AK_NAME EQU     0               ; our name
AK_ROOM EQU     10H             ; tcp://host:port/?table=id

; ---- the keys ---------------------------------------------------------------
; KHELD / KNEW: a bit a key that matters (both pads merged)
KB_UP   EQU     01H             ; 2 (1, 3 too)
KB_DOWN EQU     02H             ; 8 (7, 9 too)
KB_LEFT EQU     04H             ; 4 (1, 7 too)
KB_RIGHT EQU    08H             ; 6 (3, 9 too)
KB_FIRE EQU     10H             ; 5
KB_MENU EQU     20H             ; 0
KB_DIRS EQU     0FH

; ---- the screen -----------------------------------------------------------------
ARENA_Y EQU     10              ; the arena's top line
TILE_W  EQU     4               ; pixels
TILE_H  EQU     10              ; lines
PANEL_X EQU     53              ; the power-ups' column
BOMBER_H EQU    15              ; a bomber's frame: a blank row, then 14
MASK_H  EQU     16              ; its mask: the halo row, 14, the row under
SPR_UP  EQU     5               ; a bomber's frame starts this far above its tile

NONE    EQU     0FFH

; ---- what the link's patches did, for the arena (MFLAGS) -----------------------
MF_MAP  EQU     01H
MF_BOOM EQU     02H
MF_FIZZ EQU     04H
MF_MOVERS EQU   08H

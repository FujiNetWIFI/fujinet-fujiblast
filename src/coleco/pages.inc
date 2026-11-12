; pages.inc -- which page holds what (fujiblst.asm lays them out). The
; image's pages are 8K at C000H (equ.inc): page 0 is the window's own (at
; power-on), pages 1.. follow. The Astrocade's pages were 4K and so are
; these: two share an image page, one at C000H, one (bit 7 of its number)
; at D000H; each opens with its image page's number, then its entries, a
; JP each (E_*, equ.inc; PGCALL adds 1000H for the upper).
PG_BOOT  EQU    0               ; start-up checks, appkeys, the socket, messages (8K)
PG_NET   EQU    1               ; the link: a step a frame
PG_ARENA EQU    81H             ; the arena: entered, a new state drawn
PG_PLAY  EQU    2               ; the arena: a frame (walking, bombs, the bombers)
PG_MENU  EQU    82H             ; the title, the name, the rooms
PG_ROOM  EQU    3               ; the waiting room, the trophies
PG_TEST  EQU    83H             ; the rigs' offline scenes
NPAGES   EQU    4

; the boot page's entries
E_SAVENM EQU    E_EXTRA         ; key 0 = MYNAME
E_CONN   EQU    E_EXTRA+3       ; "CONNECTING", the socket: A = 1 open
E_JOIN   EQU    E_EXTRA+6       ; the room kept, "JOINING", the socket: A = 1
E_CLOSE  EQU    E_EXTRA+9       ; the socket closed
E_CLRRM  EQU    E_EXTRA+12      ; key 0x10 emptied, no table
E_MSG    EQU    E_EXTRA+15      ; message MSGID
E_FAIL   EQU    E_EXTRA+18      ; the socket closed, message MSGID (b7: the room forgotten)
; the link's
E_HELLO  EQU    E_STATE         ; at a table: counters, the state zeroed, HELLO
E_INIT   EQU    E_TICK          ; a fresh link's counters

; the boot page's messages (MSGID)
M_JOIN   EQU    0
M_NOANS  EQU    1
M_LOST   EQU    2
M_NOROOM EQU    3
M_VERSION EQU   4
M_CONN   EQU    5
M_QUIET  EQU    6               ; (none)

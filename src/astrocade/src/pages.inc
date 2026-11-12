; pages.inc -- which page holds what (fujiblst.asm lays them out).
; Page 1 is the resident half (3000H-3AFFH), never selected.
PG_BOOT  EQU    0               ; start-up checks, appkeys, the socket, messages
PG_NET   EQU    2               ; the link: a step a frame
PG_ARENA EQU    3               ; the arena: entered, a new state drawn
PG_PLAY  EQU    4               ; the arena: a frame (walking, bombs, the bombers)
PG_DRAW  EQU    5               ; the compositor: tiles and bombers, their art
PG_MENU  EQU    6               ; the title, the name, the rooms
PG_ROOM  EQU    7               ; the waiting room, the trophies
PG_SND   EQU    8               ; the sound: its tables and its tick
PG_TEST  EQU    9               ; the rigs' offline scenes
PG_ICON  EQU    10              ; the bombers and tiles outside the arena (icons)
NPAGES   EQU    11

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
E_INIT   EQU    E_TICK          ; a fresh link's counters (the table list)

; the boot page's messages (MSGID)
M_JOIN   EQU    0
M_NOANS  EQU    1
M_LOST   EQU    2
M_NOROOM EQU    3
M_VERSION EQU   4
M_CONN   EQU    5
M_QUIET  EQU    6               ; (none)

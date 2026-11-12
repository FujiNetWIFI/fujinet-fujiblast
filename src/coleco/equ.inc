; equ.inc -- the screen's layout, the pages' entries, FujiBlast's own numbers.
; The hardware's are in os7.inc and fujilib.inc, the protocol's in proto.inc
; (tools/o2/mkproto.py from src/proto.h), the art's in artequ.inc.
;
; Names follow the Astrocade's (src/astrocade/src/equ.inc) wherever the port
; transcribes its logic, so the two read side by side. The screen is the
; Adam's (src/adam/gfx.c): the TMS9918A's Graphics II as a character map,
; 32 x 24 cells.

; ---- the screen (src/adam/gfx.c) -------------------------------------------------
TXT_SHIFT EQU   2               ; a neutral text column c is cell c-2
BIG_COL EQU     6               ; a BIG row's first cell
ARENA_COL EQU   5               ; the arena's first cell column
CELLS_W EQU     32
; TMS9918A colours the screen uses (os7.inc T_*): the seats', the menus'
SC_BLACK EQU    1
SC_BLUE EQU     4
SC_WHITE EQU    15
; text colour bytes (fg*16 + bg) on the menus' blue
TC_WHITE EQU    0F4H
TC_YELLOW EQU   0B4H
TC_CYAN EQU     74H
TC_GREEN EQU    34H
TC_RED  EQU     94H
TC_GREY EQU     0E4H

; ---- the pages -------------------------------------------------------------------
; Page n >= 1 is mapped at C000H by a read of FNBKSEL+n (fujilib.inc); page 0
; is the window's own. A page's first byte is its number, then its entries, a
; JP each (E_*): the screens' pages are drawn afresh, a new state, a frame.
PGENT   EQU     0C001H
E_ENTER EQU     PGENT           ; the screens' pages: drawn afresh
E_STATE EQU     PGENT+3         ;   a new state
E_TICK  EQU     PGENT+6         ;   a frame
E_EXTRA EQU     PGENT+9

; ---- input -------------------------------------------------------------------------
D_UP    EQU     01H             ; INDIR bits: the handle's own (port 10H b0-3)
D_DOWN  EQU     02H
D_LEFT  EQU     04H
D_RIGHT EQU     08H
K_NONE  EQU     0FFH            ; EVKEY: a key that went down this frame
K_ENT   EQU     0AH             ; '#' (0-9: the digits)
K_CLR   EQU     0BH             ; '*': leave the table
K_MUTE  EQU     0CH             ; '9' outside a name (FRAME takes it: never seen by the game)
K_BACK  EQU     0DH             ; '*' in a name: rub out

; ---- sounds (src/platform-specific/sound.h), then the jingles ----------------------
SND_DROP   EQU  0
SND_BOOM   EQU  1
SND_PICKUP EQU  2
SND_KICK   EQU  3
SND_THROW  EQU  4
SND_FIZZLE EQU  5
SND_DIE    EQU  6
SND_BEEP   EQU  7
SND_GO     EQU  8
SND_SELECT EQU  9
MUS_TITLE  EQU  10
MUS_ROUND  EQU  11
MUS_WIN    EQU  12
MUS_GAME   EQU  13

; ---- the link (p_net.inc) ------------------------------------------------------------
NETDEV  EQU     71H             ; N1:
NCOPEN  EQU     4FH             ; 'O'
NCCLOSE EQU     43H             ; 'C'
NCREAD  EQU     52H             ; 'R'
NCWRITE EQU     57H             ; 'W'
NCSTAT  EQU     53H             ; 'S'
NMRDWR  EQU     0CH             ; OPEN: read and write
FCCKEY  EQU     0DBH            ; CLOSE_APPKEY
FCOKEY  EQU     0DCH            ; OPEN_APPKEY
FCRKEY  EQU     0DDH            ; READ_APPKEY
FCWKEY  EQU     0DEH            ; WRITE_APPKEY
NK_READ  EQU    1               ; NKIND: the transaction out
NK_STATUS EQU   2
NK_WRITE EQU    3
NK_OTHER EQU    4               ; (a blocking one's, from the boot page)
NL_LINK  EQU    1               ; NLINK: the socket is open
NL_JOINED EQU   2               ;   HELLO sent: at a table
NL_HEARD EQU    4               ;   a commit came since
EV_STATE EQU    1               ; NEVT: what this frame's step brought
EV_ERROR EQU    2
EV_LOST  EQU    4
EV_NOANS EQU    8               ;   no state in JOIN_WAIT frames
TX_HELLO EQU    01H             ; TXF: what the next WRITE carries
TX_ACT   EQU    02H
TX_MOVE  EQU    04H
TX_CREDIT EQU   08H
TX_PING  EQU    10H
TX_RESYNC EQU   20H
TX_TABLES EQU   40H
TX_BYE   EQU    80H
MF_MAP   EQU    01H             ; MFLAGS: the patches since the arena looked
MF_BOOM  EQU    02H
MF_FIZZ  EQU    04H
MF_MOVERS EQU   08H
FL_ROOM  EQU    01H             ; FLAGS: key 0x10 holds our server and table
FL_TEST  EQU    02H             ;   an offline test scene: the link is quiet
FL_MUTE  EQU    04H

; ---- the screens at a table (src/main.c) ---------------------------------------------
S_NONE  EQU     0
S_WAIT  EQU     1
S_ARENA EQU     2
S_ROUND EQU     3
S_GAME  EQU     4

NONE    EQU     0FFH
MAX_TABLES EQU  8
MOVE_EVERY EQU  4               ; jiffies between our moves, at least
JOIN_WAIT EQU   600             ; frames for the first state before giving up

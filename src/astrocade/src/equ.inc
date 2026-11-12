; equ.inc -- the screen's layout, the pages' entries, FujiBlast's own numbers.
; The hardware's are in hvglib.inc and fujilib.inc, the protocol's in
; proto.inc (tools/o2/mkproto.py from src/proto.h), the art's in artequ.inc.
;
; Names follow the Arcadia's (src/arcadia/src/equ.inc) wherever the port
; transcribes its logic, so the two read side by side.

; ---- the screen ------------------------------------------------------------------
; 160x84 at 2 bits a pixel, 40 bytes a line, from 4000H; past line 84 the
; border's colour (HORCB b6-7: colour 0, the frame), and RAM that is ours.
LINES   EQU     84
SCR     EQU     NORMEM          ; 4000H
MAGOFF  EQU     4000H           ; a magic write's address = screen address - 4000H
TXH     EQU     7               ; a text row: 6 lines of 4x6 glyph, 1 between
HUDLN   EQU     0               ; the HUD: lines 0-6
ATOP    EQU     7               ; the arena's first line
TILEH   EQU     7               ; a tile: 2 bytes (8 pixels) x 7 lines
ACOLB   EQU     1               ; the arena's first byte: tile (c, r) is bytes
                                ;  ACOLB+2c, +1 at lines ATOP+7r ..+6
AWALLR  EQU     27              ; the right wall's byte (the left's is byte 0)
PANB    EQU     28              ; the stats panel: bytes 28-39
APIX0   EQU     4*ACOLB         ; the arena's left edge in pixels

; ---- the raster: a band entry (resident tables, BANDTB) ---------------------------
; HORCB, colours R3, R2, R1, L1, then the next interrupt's scanline. The
; frame interrupt (VBLLN, below the picture and its footer) loads entry 0
; for the top of the next frame. Colours 0 (left and right: the frame, the
; border, the text's ground) and L2, L3 stay as a screen set them. Under
; the picture the border shows HORCB's b6-7 colour: a band there with
; HORCB = 40H+n paints the footer in its L1 (no RAM: past VERBL).
FOOT    EQU     40H             ; HORCB: the border is colour 1
VERBLV  EQU     LINES*2         ; 168: the picture's end (VERBL, in scanlines)
VBLLN   EQU     204             ; the frame interrupt's scanline: under the
                                ;  footer (lines 84-101: bands of border colour)
BANDOFS EQU     1               ; a band's interrupt fires on the scanline above it
BPAD    EQU     5               ; NOPs before its first write (ISS's measure; the
                                ;  spike sweeps it again for this handler)
BLINE   MACRO   ln              ; the INLIN of a band starting at picture line ln
        DB      2*(ln)-BANDOFS
        ENDM

; ---- colours: hue<<3 | luma, from MAME's astrocade_palette (tools/astro/mkbands.py)
; The arena's four (left palette): 0 dark (the frame, walls, shades, visors),
; 1 floor, 2 warm, 3 bright. look.inc holds the spike's looks.

; ---- the pages -------------------------------------------------------------------
; Every page opens with the 55H header (the BIOS's menu link, the name,
; RSTENT), then its entries, a JP each.
PGENT   EQU     2007H
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
K_ENT   EQU     0AH             ; '=' (0-9: the digits)
K_CLR   EQU     0BH             ; 'C'
K_MUTE  EQU     0CH             ; 'MR' (FRAME takes it: never seen by the game)
K_BACK  EQU     0DH             ; 'CE'

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

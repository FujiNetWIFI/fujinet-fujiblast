; equ.inc -- the Arcadia 2001, the FujiNet cart's mailbox, and FujiBlast's
; own numbers. The protocol's are in proto.inc (tools/o2/mkproto.py from
; src/proto.h); the art's frame numbers in artequ.inc.

; ---- the 2637 UVI and the console's RAM -------------------------------------
SCR     EQU     $1800           ; 13 rows of 16 cells (13-row mode)
SPRPOS  EQU     $18F0           ; sprite n: Y at +2n (256 - line), X at +2n+1
VPOS    EQU     $18FC           ; rows start at line 255 - value ($FF: the top)
PITCH   EQU     $18FD           ; b7 multicolour (never set here), b0-6 pitch
VOLUME  EQU     $18FE           ; b0-2 volume, b3 tone, b4 noise, b5-7 shift
KEYPAD  EQU     $1900           ; P1 keypad columns $1900-$1902 (b3..b0, active high)
PANEL   EQU     $1908           ; b0 Start, b1 Option, b2 Select
UDC     EQU     $1980           ; 8 user glyphs $38-$3F, 8 bytes each; $38-$3B are
                                ; also sprites 0-3's pictures
GFXMOD  EQU     $19F8           ; b7 rows start in block graphics, b6 26 lines
BGCOL   EQU     $19F9           ; b7 8x8 cells, b6 pot mux, b3 ink set, b0-2 ground
SPRC23  EQU     $19FA           ; sprites 2/3: colour b3-5 / b0-2, b7/b6 1 = 8 lines
SPRC01  EQU     $19FB           ; sprites 0/1, the same
POT     EQU     $19FF           ; P1 disc: mux clear vertical, set horizontal

MUXH    EQU     $40             ; BGCOL: the pot reads the horizontal axis
INK0    EQU     $00             ; a cell's ink (its top two bits)
INK1    EQU     $40
INK2    EQU     $80
INK3    EQU     $C0
GMOSAIC EQU     $C0             ; this exact cell starts block graphics to the row's end
GTEXT   EQU     $40             ; and this one ends it
; the eight colours (sprites, the ground)
CWHITE  EQU     0
CYELLOW EQU     1
CCYAN   EQU     2
CGREEN  EQU     3
CMAGEN  EQU     4
CRED    EQU     5
CBLUE   EQU     6
CBLACK  EQU     7

; The ROM's glyphs worth naming.
GSPACE  EQU     $00
GSLASH  EQU     $01
GSOLID  EQU     $03
GTOPBAR EQU     $04
GRTBAR  EQU     $05
GBOTBAR EQU     $06
GLFBAR  EQU     $07
GDIGIT  EQU     $10             ; '0'
GALPHA  EQU     $1A             ; 'A'
GDOT    EQU     $34
GUDC    EQU     $38             ; user glyph 0 (sprite 0's picture)
; the arena's four user glyphs (the text screens load their own there)
GBRICK  EQU     $3C
GBOMB   EQU     $3D
GFLAMEH EQU     $3E
GFLAMEV EQU     $3F
; the text screens'
GCURSOR EQU     $3C
GDASH   EQU     $3D
GCOLON  EQU     $3E
GTROPHY EQU     $3F

; ---- the screen's layout -----------------------------------------------------
AROW    EQU     1               ; the arena's first row
ACOL    EQU     1               ; and column: tile (c, r) is cell (ACOL+c, AROW+r)
SCOL    EQU     15              ; our stats' column
SPRX0   EQU     43+8*ACOL       ; a sprite's X register at the arena's left edge
SPRY0   EQU     256-16*AROW     ; and its Y register at the top (lines go up as it goes down)

; ---- the cart's mailbox (fujinet-firmware pico/arcadia fuji_mailbox.h) ------
FNRDATA EQU     $2B00           ; the reply (slice 0)
FNACKSQ EQU     $2C00
FNERR   EQU     $2C02
FNREPLY EQU     $2C03
FNRXLL  EQU     $2C04
FNRXLH  EQU     $2C05
FNMAGF  EQU     $2C09
FNMAGN  EQU     $2C0A
FNPVER  EQU     $2C0B           ; 2: APPBANK, cart RAM, the tick
FNTICKL EQU     $2C0E           ; 60 Hz, read HI, LO, HI
FNTICKH EQU     $2C0F
FNRSEL  EQU     $2D00
FNBANK  EQU     $2D80           ; read +n: the window shows page n
FNRDAT  EQU     $2E00
FNDATA  EQU     $2F00
FRDEVIC EQU     $00
FRCMD   EQU     $01
FRNPARM EQU     $02
FRDATRS EQU     $05
FRSEQ   EQU     $10
FRRAMP0 EQU     $14             ; cart RAM pointer = $000 + value (RAM-L)
FRRAMP1 EQU     $15             ; $100 + value
FRRAMP2 EQU     $16             ; $200 + value (RAM-H)
FRRAMP3 EQU     $17             ; $300 + value
FRRAMPK EQU     $18             ; each REGDATA stores and steps the pointer
FCACK   EQU     $06
FNDFUJI EQU     $70
NETDEV  EQU     $71             ; N1:
NCOPEN  EQU     $4F             ; 'O'
NCCLOSE EQU     $43             ; 'C'
NCREAD  EQU     $52             ; 'R'
NCWRITE EQU     $57             ; 'W'
NCSTAT  EQU     $53             ; 'S'
NMRDWR  EQU     $0C             ; OPEN: read and write
FCCKEY  EQU     $DB             ; CLOSE_APPKEY
FCOKEY  EQU     $DC             ; OPEN_APPKEY
FCRKEY  EQU     $DD             ; READ_APPKEY
FCWKEY  EQU     $DE             ; WRITE_APPKEY

; ---- the window ---------------------------------------------------------------
WIN     EQU     $0800           ; every page: entries at $0800 + 3k, its id at $0FFF
E_ENTER EQU     0               ; the screens' pages: drawn afresh
E_STATE EQU     3               ;   a new state
E_TICK  EQU     6               ;   a frame
E_EXTRA EQU     9

; ---- input -----------------------------------------------------------------------
D_UP    EQU     $01             ; INDIR bits
D_DOWN  EQU     $02
D_LEFT  EQU     $04
D_RIGHT EQU     $08
JOYLO   EQU     $40             ; a pot under this: up / left
JOYHI   EQU     $C0             ; over this: down / right
K_NONE  EQU     $FF             ; EVKEY: a key that went down this frame
K_ENT   EQU     $0A             ; (0-9: the digits; FIRE is '2' and reads as EVBTN)
K_CLR   EQU     $0B
K_START EQU     $0C
K_OPT   EQU     $0D
K_SEL   EQU     $0E

; ---- sounds (src/platform-specific/sound.h), then the jingles ---------------------
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

; ---- the link (p_net.inc) --------------------------------------------------------
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
TX_HELLO EQU    $01             ; TXF: what the next WRITE carries
TX_ACT   EQU    $02
TX_MOVE  EQU    $04
TX_CREDIT EQU   $08
TX_PING  EQU    $10
TX_RESYNC EQU   $20
TX_TABLES EQU   $40
TX_BYE   EQU    $80
MF_MAP   EQU    $01             ; MFLAGS: the patches since the arena looked
MF_BOOM  EQU    $02
MF_FIZZ  EQU    $04
MF_MOVERS EQU   $08
FL_ROOM  EQU    $01             ; FLAGS: key 0x10 holds our server and table
FL_TEST  EQU    $02             ;   an offline test scene: the link is quiet
FL_MUTE  EQU    $04

; ---- the screens at a table (src/main.c) -----------------------------------------
S_NONE  EQU     0
S_WAIT  EQU     1
S_ARENA EQU     2
S_ROUND EQU     3
S_GAME  EQU     4

NONE    EQU     $FF
MAX_TABLES EQU  8
MOVE_EVERY EQU  4               ; jiffies between our moves, at least
JOIN_WAIT EQU   600             ; frames for the first state before giving up

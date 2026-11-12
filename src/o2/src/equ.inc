; equ.inc -- constants: the console, the cartridge, the screens, FujiBlast.
; The game's own (tiles, the state's layout, the frames) are gen/o2/proto.inc,
; made from src/proto.h.

; ---- P1 (all active low but COPY and LUM) ----
; Main code runs with external RAM and the cart on the bus; the interrupts
; switch to the VDC and put P1 back. COPY low always, or the cart's write
; strobe is gated off. The bank bits stay high: pages are picked through the
; mailbox (CODEBANK), not by P10/P11.
P1_MAIN equ 0AFh                ; XRAM + cart selected, VDC parked
P1_VDC  equ 0B7h                ; VDC selected
P1_JOY  equ 0BBh                ; keyboard scanner (joystick) selected

; ---- the mailbox (fuji_mailbox.h, protocol 2) ----
FN_W_REGSEL     equ 0E0h
FN_W_REGDATA    equ 0E1h
FN_W_DATA       equ 0E2h
FN_W_SEQ        equ 0E3h
FN_REG_DEVICE   equ 000h
FN_REG_CMD      equ 001h
FN_REG_NPARAM   equ 002h
FN_REG_DATA_RST equ 005h
FN_REG_CODEBANK equ 008h
FN_REG_POKE_LO  equ 00Ah
FN_REG_POKE_HI  equ 00Bh
FN_REG_POKE     equ 00Ch
FN_REG_RXWIN    equ 00Dh
FN_R_ACKSEQ     equ 0F20h
FN_R_STATUS     equ 0F21h
FN_R_ERR        equ 0F22h
FN_R_REPLY_CMD  equ 0F23h
FN_R_RXLEN_LO   equ 0F24h
FN_R_RXLEN_HI   equ 0F25h
FN_R_MAGIC0     equ 0F29h
FN_R_MAGIC1     equ 0F2Ah
FN_R_PROTO_VER  equ 0F2Bh
FN_R_DATA       equ 0F30h       ; the read window: 208 bytes from RXWIN*16
FN_PROTO_VER    equ 2
O2_APP_HDR      equ 0F30h
O2_APP_VERSION  equ 1

; ---- the Fuji device (fujiCmd.h) and the network device ----
FUJI_DEVICEID   equ 070h
FC_ACK          equ 006h
FC_NAK          equ 015h
FC_AKOPEN       equ 0DCh        ; OPEN_APPKEY (6: creator, app, key, mode, 0)
FC_AKREAD       equ 0DDh        ; READ_APPKEY: u16 length, then the key
FC_AKWRITE      equ 0DEh        ; WRITE_APPKEY (the key's bytes)
NET_DEVICEID    equ 071h
NC_OPEN         equ 04Fh        ; OPEN (mode, translation; "N:url",0)
NC_CLOSE        equ 043h
NC_READ         equ 052h        ; READ (u16 length)
NC_WRITE        equ 057h        ; WRITE (u16 length; the bytes)
NC_STATUS       equ 053h        ; STATUS: u16 waiting, connected, nDevStatus
NM_RW           equ 00Ch

; ---- the VDC's character generator (o2doc appendix C) ----
G_SPACE equ 12
G_BLOCK equ 47
G_BALL  equ 49
G_MAN_R equ 50
G_TIMES equ 41
CH_BLOCK equ 0Fh                ; text codes $00-$1F: glyph code + 32
CH_BALL  equ 11h
CH_MAN   equ 12h

; Character colours, as MAME's i8244 draws them (control byte bits 1-3).
; A character can't be black: 0 is the darkest, a dark grey.
C_DGREY  equ 0
C_RED    equ 1
C_GREEN  equ 2
C_TAN    equ 3
C_BLUE   equ 4
C_VIOLET equ 5
C_CYAN   equ 6
C_WHITE  equ 7

; Sprite attributes: bits 5-3 the colour (same order), bit 2 double size.
S_COL   equ 8                   ; x colour

; The colour register ($A3): bits 5-3 the background (black, blue, green,
; cyan, red, violet, yellow, white), bits 2-0 the grid, bit 6 the grid
; bright (g7000.h col_bck_*, col_grd_*, col_grd_lum).

; The control register ($A0).
CTL_OFF   equ 0
CTL_FG    equ vdc_ctrl_fore                               ; objects only
CTL_ARENA equ vdc_ctrl_fill|vdc_ctrl_fore|vdc_ctrl_grid   ; objects + cells

SPR_OFF equ 0F8h                ; a sprite's Y below the screen

; ---- the arena screen (ov_arena.inc) ----
; The grid in fill mode is the arena's walls: a column byte's bit for the
; grid row the beam is in fills that 16 x 24 cell, and the bands rewrite all
; ten columns at every tile row, so a tile is 16 x 16: one position unit a
; pixel across and a line down. Ten of the thirteen columns show (the grid's
; X 13-173); VOFF is the arena column in view column 0.
Y0      equ 40                  ; tile row r's first line: Y0 + 16r
TILE_H  equ 16
VIEW_W  equ 10
OFF_MAX equ 3                   ; ARENA_W - VIEW_W
X_VIEW  equ 8                   ; a character/sprite X register at view
                                ; column 0's left edge (it draws 5 right)
X_MID   equ X_VIEW+4            ; ... centred in it (8 pixels wide)
Y_HUD   equ 22                  ; the HUD's line of characters
    ifndef GLEAD
GLEAD   equ 3                   ; lines a row band starts before its row
    endif
    ifndef GPREN
GPREN   equ 3                   ; a band's delay before its grid-off: 2+2n
    endif                       ; cycles, so it lands in the blank (NTSC)
    ifndef GNOPN
GNOPN   equ 5                   ; ... before its grid-on, 4 lines on
    endif
    ifndef GPRENP
GPRENP  equ 5                   ; ... the same on PAL (the boot pokes them):
    endif                       ; a PAL line is longer in cycles
    ifndef GNOPNP
GNOPNP  equ 1
    endif
    ifndef GLEADP
GLEADP  equ 2
    endif
PALPASS equ 565                 ; pal_check: passes a frame from which a
                                ; console is PAL (NTSC ~443, PAL ~686)
    ifndef LOOK
LOOK    equ 0                   ; 0: a block is a whole cell; 1: blocks the
    endif                       ; top of the cell, pillars the whole
Y_CHAR  equ 2                   ; a tile's character: its line in the tile
    ifndef ARENA_COL
ARENA_COL equ col_bck_green|col_grd_lum|col_grd_yellow  ; floor, walls (user: yellow)
    endif

; ---- the text screens (ov_text.inc) ----
; T0: 16 characters at the top, the four quads interleaved (A columns 0, 2,
; 4, 6 at X_TEXT, B 1, 3, 5, 7 at +8, C and D the next eight at +64, +72);
; then NTROWS rows of NTCOLS singles, TPITCH lines apart.
Y_T0    equ 18                  ; MAME hides characters above Y=14 (NTSC)
X_TEXT  equ 16
NTROWS  equ 7
NTCOLS  equ 12
TROW    equ 2*NTCOLS            ; a row's table: NTCOLS x (pointer, control)
Y_TROW0 equ 40
TPITCH  equ 30                  ; 14 lines of characters, 16 for the band
TLEAD   equ 1                   ; lines a row band starts before its gap
X_TROW  equ 36                  ; a row's characters, centred
    ifndef TEXT_COL
TEXT_COL equ col_bck_blue       ; the text screens: black text background
    endif

; SPRTAB: the sprites' (Y, X, attribute) x 4, at the same address in both
; overlays (main code pokes it; the swap leaves it alone).
SPRTAB  equ 7F4h
; The swap (ov.inc) copies an overlay's $600-$6F7 and $700-$7F3.
OV_N6   equ 0F8h
OV_N7   equ 0F4h

; ---- the stick (input.inc): JOY / JOYEV bits ----
J_UP    equ 01h
J_DOWN  equ 02h
J_LEFT  equ 04h
J_RIGHT equ 08h
J_FIRE  equ 10h

; ---- the keyboard (input.inc): BIOS readkey codes, row x 8 + column ----
K_SPACE equ 0Ch
K_M     equ 26h
K_CLR   equ 2Eh
K_ENT   equ 2Fh
K_NONE  equ 0FFh

; ---- sounds (snd.inc): beep ids; from SND_SOFT on they never cut another ----
; (The 8244 has one voice: an effect cuts a jingle.) Ids from SND_JINGLE on
; are jingles: p9 plays their notes one after another.
SND_SELECT equ 0
SND_DROP   equ 1
SND_BOOM   equ 2
SND_PICKUP equ 3
SND_KICK   equ 4
SND_THROW  equ 5
SND_FIZZLE equ 6
SND_DIE    equ 7
SND_BEEP   equ 8
SND_GO     equ 9
SND_SOFT   equ 10
N_LO       equ 10               ; the jingles' notes
N_MID      equ 11
N_HI       equ 12
N_TOP      equ 13
N_END      equ 14
SND_JINGLE equ 20h
SND_TITLE  equ 20h
SND_ROUND  equ 21h
SND_WIN    equ 22h
SND_GAME   equ 23h

; ---- the game (src/game.h's) ----
MAX_TABLES equ 8
NONE    equ 0FFh

; ---- pages ----
PG_BOOT  equ 0
PG_ALIB  equ 1                  ; the arena's drawing, by farcall
PG_PLAY  equ 2                  ; the arena's main loop
PG_OV    equ 3                  ; the overlays' swap, by farcall
PG_TLIB  equ 4                  ; the text screens' drawing, by farcall
PG_NET   equ 5                  ; the link, by farcall
PG_NCTL  equ 6                  ; the appkeys and the socket, by farcall
PG_MAIN  equ 7                  ; at a table: the loop, the room, trophies
PG_MENU  equ 8                  ; the title, the name, the rooms
PG_IO    equ 9                  ; a frame's input and sound, by farcall
IO_FRAME equ 0
PG_WALK  equ 10                 ; our walking (move.c), by farcall
W_WALK   equ 0
W_CENTRE equ 1
W_SEATS  equ 2
PG_TEST  equ 11                 ; the offline test scenes
E_ENTER  equ 0                  ; PG_PLAY's: the arena
E_JOIN   equ 0                  ; PG_MAIN's
E_BACK   equ 1
E_LOST   equ 2
E_ERR    equ 3
E_LEAVE  equ 4
E_TITLE  equ 0                  ; PG_MENU's
E_ROOMS  equ 1
S_NONE   equ 0                  ; SCREEN
S_WAIT   equ 1
S_ARENA  equ 2
S_ROUND  equ 3
S_GAME   equ 4
A_ROWS    equ 0                 ; PG_ALIB's
A_ROW     equ 1
A_PIECES  equ 2
A_HUD     equ 3
A_BOMBERS equ 4
A_MID     equ 5
A_SHAPES  equ 6
E_RUN    equ 0
O_ARENA  equ 0                  ; PG_OV's entries
O_TEXT   equ 1
N_STEP   equ 0                  ; PG_NET's
N_HELLO  equ 1
N_ACT    equ 2
N_TABLES equ 3
N_LEAVE  equ 4
N_INIT   equ 5
K_LOAD   equ 0                  ; PG_NCTL's
K_SAVENAME equ 1
K_SAVEROOM equ 2
K_CLEARROOM equ 3
C_OPEN   equ 4
C_CLOSE  equ 5
L_CLEAR  equ 0                  ; PG_TLIB's
L_ROW    equ 1
L_T0     equ 2

; INFRAME: a frame's input and sound (p9), farcalled where it's written --
; no wrapper's level on the stack (BIOS readkey is under it). Clobbers A,
; r0-r7, F0.
INFRAME macro
    mov r7,#PG_IO
    mov a,#IO_FRAME
    FCALL farcall
    endm

; SCRNEW: a blank text screen (p4's L_CLEAR), farcalled in place.
SCRNEW  macro
    mov r7,#PG_TLIB
    mov a,#L_CLEAR
    FCALL farcall
    endm

; FARC: farcall entry en of page pg (r2-r5 pass through).
FARC    macro pg,en
    mov r7,#pg
    mov a,#en
    FCALL farcall
    endm

; GOTO: one way to another page's entry (page.inc far_go).
GOTO    macro pg,en
    mov r7,#en
    mov a,#pg
    jmp far_go
    endm

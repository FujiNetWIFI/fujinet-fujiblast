; ram.inc -- where everything lives.
;
; Internal RAM (64 bytes):
;   $00-$07  register bank 0: the main program
;   $08-$17  the stack, 8 levels: the interrupts take up to 2
;   $18-$1F  register bank 1: the interrupts' own (r5: the next band, r4 a
;            text row's Y, r3 a band's argument, r6 P1, r7 A)
;   $20-$3C  below
;   $3D-$3F  the BIOS's (its keyboard and IRQ routines)

RAWBUF  equ 020h                ; 23: a frame being sent, before COBS (p5's
                                ; send: only within a net step)
RAWMAX  equ 21                  ; (HELLO's 21: the longest)
LASTF   equ 035h                ; p2: FRAMES at the last pass (the jiffies owed)
TESTHK  equ 036h                ; the test hook: a rig pokes 0A0h + a scene
STATSUM equ TESTHK              ; p2: our powers summed (the boot's done with
                                ; the hook): one going up is a pickup
ODIRTY  equ 037h                ; the object groups the vblank writes: OD_*
FRAMES  equ 039h                ; vblanks, for the main loop's clock
QUIET   equ 03Ah                ; frames whose last band has run
MODE    equ 03Bh                ; MODE_*
SNDCNT  equ 03Ch                ; the sound: frames left (beep.inc)

    if RAWBUF+RAWMAX > LASTF
    error "RAWBUF has grown into LASTF"
    endif

OD_SING equ 01h                 ; singles 0-11 (the arena's pieces; the text
                                ; rows' X)
OD_QA   equ 02h                 ; quads 0-1 (the arena's; T0's first half)
OD_QH   equ 04h                 ; quads 2-3 (the HUD; T0's second half)
OD_ALL  equ 07h

MODE_PAL   equ 080h             ; a PAL console (the boot measures it)

; External RAM (128 bytes, $00-$7F, MOVX with P1_MAIN):
LINEBUF equ 000h                ; 16: a text line being built (p4.inc)
TXCOL   equ 010h                ; its colour
TXCOL2  equ 011h                ; ... characters with bit 7 set
OLDPG   equ 012h                ; farcall's caller's page
CURPG   equ 013h                ; the page mapped at $800
TMP0    equ 014h                ; a loop's index across calls that keep nothing
TMP1    equ 015h
TMP2    equ 016h
; the input (input.inc)
KLAST   equ 017h                ; BIOS readkey's memory (its r7)
KEY     equ 018h                ; the key pressed this frame (K_NONE: none)
JOY     equ 019h                ; the stick now: J_* bits
JOYP    equ 01Ah                ; last frame's
JOYEV   equ 01Bh                ; pressed since last frame
REP     equ 01Ch                ; auto-repeat countdown
; the transaction (mail.inc)
NSEQ    equ 01Dh                ; the sequence number posted
LTMP    equ 01Eh                ; a library's own scratch

; ---- the link (p5.inc) ----
NLINK   equ 01Fh                ; NL_*
NKIND   equ 020h                ; the transaction out: NK_* (0 none)
NWAIT   equ 021h                ; frames it has been out
DPOS    equ 022h                ; the reply's next byte to decode
DLEFT   equ 023h                ; ... bytes left
DCODE   equ 024h                ; COBS: data bytes left in this block
DFLAG   equ 025h                ; DF_*
D1      equ 026h                ; the last two bytes decoded (the sums, at
D2      equ 027h                ; the frame's end)
DN      equ 028h                ; ... how many of them there are (0-2)
FIDX    equ 029h                ; the frame's bytes processed (0: its type next)
FTYPE   equ 02Ah                ; its type
S1      equ 02Bh                ; Fletcher-16 over what was processed
S2      equ 02Ch
QST     equ 02Dh                ; a patch: 0 its offset next, 1 its length,
QOFF    equ 02Eh                ; 2 its bytes (QLEN left, the next at QOFF),
QLEN    equ 02Fh                ; 3 a bad one: the rest ignored
TAKEN   equ 030h                ; encoded bytes taken, mod 256
CREDITED equ 031h               ; ... at the last credit
QUIETL  equ 032h                ; frames since a frame came (16 bits)
QUIETH  equ 033h
QPINGL  equ 034h                ; ... mod PING_EVERY (16 bits)
QPINGH  equ 035h
QASK    equ 036h                ; ... mod ASK_AGAIN
RHOLD   equ 037h                ; frames to the next RESYNC allowed; bit 7: owed
TXF     equ 038h                ; frames to send: TX_*
ASEQ    equ 039h                ; the action to send: its seq, kind, arguments
AK      equ 03Ah
A1      equ 03Bh
A2      equ 03Ch
NEVT    equ 03Dh                ; what the step saw: EV_*
NERR    equ 03Eh                ; an E frame's code
JNOTE   equ 03Fh                ; the jingle playing: 0 none, SND_JINGLE+k asked
                                ; for, 40h+ its next note's place (p9)
TBLN    equ 040h                ; the tables the server has (0FFh: no answer)
TBLHAVE equ 041h                ; the records in (a bit each)
NSTAT   equ 042h                ; frames to the next STATUS
MAPDL   equ 043h                ; map rows patched since the arena looked (bit
MAPDH   equ 044h                ; r: row r)
MFLAGS  equ 045h                ; MF_*

NL_LINK   equ 01h               ; the socket is open
NL_JOINED equ 02h               ; HELLO sent: commits count
NL_HEARD  equ 04h               ; a commit came since HELLO
NK_READ   equ 1
NK_WRITE  equ 2
NK_STATUS equ 3
DF_BLK    equ 01h               ; a block began in this frame
DF_255    equ 02h               ; ... its code was 255 (no zero after it)
DF_SKIP   equ 04h               ; a bad frame: up to its end ignored
TX_HELLO  equ 01h
TX_ACT    equ 02h
TX_MOVE   equ 04h
TX_CREDIT equ 08h
TX_PING   equ 10h
TX_RESYNC equ 20h
TX_TABLES equ 40h
TX_BYE    equ 80h
EV_STATE  equ 01h
EV_ERROR  equ 02h
EV_LOST   equ 04h
MF_MAP    equ 01h               ; the map was patched
MF_BOOM   equ 02h               ; a tile became a blast's centre
MF_FIZZ   equ 04h               ; a bomb became a dud
MF_MOVERS equ 08h               ; the movers were patched
MF_PIECES equ 10h               ; p2: the pieces to redraw (our bomb)

; ---- the game ----
SRVTBL  equ 046h                ; 8: the table's id (NUL-padded); MYNAME
MYNAME  equ 04Eh                ; right after it: HELLO sends both as they are
MYSEAT  equ 056h                ; NONE: watching
MVX     equ 057h                ; our own walker (move.c)
MVY     equ 058h
MVDIR   equ 059h
MVACC   equ 05Ah
MVBUMP  equ 05Bh
PEND    equ 05Ch                ; our bomb shown before the server has it
PENDL   equ 05Dh                ; ... frames it may wait
MYWARP  equ 05Eh
SENTX   equ 05Fh                ; the last move sent
SENTY   equ 060h
SENTD   equ 061h
SENTT   equ 062h                ; frames since
BUMPT   equ 063h                ; frames since the last bump
DISPX   equ 064h                ; 4: the bombers as drawn
DISPY   equ 068h                ; 4
DIET    equ 06Ch                ; 4: frames since one was knocked out
SPF     equ 070h                ; 4: the frame shown (FR_*), 0FFh hidden
SHAPED  equ 074h                ; 4: the frame in each sprite's shape
VOFF    equ 078h                ; the arena column in view column 0 (0-3)
BLINK   equ 079h                ; 0 or 0FFh: flames and burning blocks lit
LITL    equ 07Ah                ; the rows with lit tiles (flames, burning
LITH    equ 07Bh                ; blocks): the blink redraws only these
SCREEN  equ 07Ch                ; the screen up: SC_*
HUDT    equ 07Dh                ; the HUD as drawn: the timer
HUDA    equ 07Eh                ; ... the alive mask
FLAGS   equ 07Fh                ; FL_*

FL_MUTE equ 01h                 ; FLAGS: the sound off
FL_ROOM equ 02h                 ; key 0x10 holds our server (p6.inc)
FL_TEST equ 04h                 ; an offline test scene: the link is quiet

; scratch that only one page's routine uses at a time
NSING   equ TMP2                ; p1's ar_pieces: singles in use
ROWT    equ TMP1                ; p1: a row being built
JOINT   equ TMP1                ; p7's join wait (16 bits)
JOINTH  equ TMP2
OWED    equ 038h                ; (internal RAM) jiffies owed (PAL: 6 a 5 frames)
SHOWNQ  equ TBLN                ; p2: the round (OFF_SEQ) drawn -- the table
WASA    equ TBLHAVE             ; list's bytes, idle at a table; and the alive
                                ; mask last seen
MOVED   equ TMP0                ; p2: we moved this jiffy (TMP0: the text
                                ; screens', idle in the arena)
FTILE   equ LTMP                ; p2's fire: the tile we stand on
SEL     equ MYSEAT              ; p8's rooms: the one picked (MYSEAT is set at
                                ; the table)
RCOUNT  equ SENTT               ; p8: frames counted (SENTT: the arena's)
; the walker's scratch (walk.inc), in internal RAM: RAWBUF's, idle outside a
; net step
WLV     equ RAWBUF+0            ; our speed level
WSX     equ RAWBUF+1            ; the position before trying a turn
WSY     equ RAWBUF+2
WSA     equ RAWBUF+3
WB      equ RAWBUF+4            ; the other way, if the turn is walled
WN      equ RAWBUF+5            ; units left this jiffy
WMOVED  equ RAWBUF+6            ; moved yet
JCNT    equ RAWBUF+7            ; p2: jiffies left this frame

; the arena's drawing reads the bombers' positions (p1.inc)
SPX     equ DISPX
SPY     equ DISPY

; ---------------------------------------------------------------------------
; ram.inc -- the cart's 30K of RAM ($8000-$F7FF), directly read and written.
;
; It survives a console RESET (the cart has no reset line), so START sets
; every byte it relies on. tools/chf/checksize.py checks that nothing runs
; past RAMTOP.
; ---------------------------------------------------------------------------

; PIX: the screen, two bytes a pixel, row y at PIX + 256y: byte 2x the
; background (BG: the tiles, no sprites), byte 2x+1 what VRAM shows (SH).
; Both are port-1 codes (CV0-CV3). One DC walks both (engine.inc).
PIX     EQU 08000H              ; to $BFFF
DATA    EQU 0C000H              ; the ART page's copy (art, tables): to $C9FF
IMGS    EQU 0CA00H              ; the slots' images (slots.inc IMGADR): to $D5FF
TIMG    EQU 0D600H              ; 20 bytes: a row of clear pixels (FF, 00)
MIMG    EQU 0D620H              ; 180: a merged image (slots overlapping)
ST      EQU 0D700H              ; the state (247), patched in place
TBLS    EQU 0D800H              ; the table list: 8 x 24
SLOTS   EQU 0D900H              ; 8 x 16: the sprites (slots.inc)
CTAG    EQU 0D980H              ; 8 x 4: the frames each slot's images hold
SHOWN   EQU 0DA00H              ; 143: the tile frame on the screen (FF: none)
BORN    EQU 0DA90H              ; 143: the tick (low) a tile became what it is
DIRTY   EQU 0DB20H              ; 143: nonzero = redraw the tile
ROWPAL  EQU 0DBB0H              ; 64: each row's palette, wanted
DQ      EQU 0DBF0H              ; 64: the dirty tiles' queue (DMARK, ADIRTY)
VARS    EQU 0DC40H              ; 1K: the globals below
RAMEND  EQU 0E040H              ; first byte nothing uses
RAMTOP  EQU 0F000H              ; never past here (DC0 runs into the mailbox)

; --- globals ----------------------------------------------------------------
; core: the clock and the pages
FRAMES  EQU VARS+000H           ; 2: the cart's tick when the loop last woke
CURPG   EQU VARS+004H           ; page mapped at $4800
PGSP    EQU VARS+005H           ; page stack depth
PGSTK   EQU VARS+006H           ; 4: pages under a FAR call
TESTHK  EQU VARS+00AH           ; 4: the rigs' test hook (p_boot)
DBGAT   EQU VARS+00EH           ; where the loop is (the harness peeks it)
JIFFY   EQU VARS+010H           ; 2: the game's clock (jiffies run)
SNDB    EQU VARS+012H           ; the tone bits now out on port 5 (7-6)
DT      EQU VARS+013H           ; jiffies this pass runs (FRAME; at most 8)

; the engine's arguments and scratch (engine.inc)
WX      EQU VARS+020H           ; a window: x, y, w, h
WY      EQU VARS+021H
WW      EQU VARS+022H
WH      EQU VARS+023H
WSTEP   EQU VARS+024H           ; the stream's bytes skipped after a row
WSRC    EQU VARS+025H           ; 2: the stream (big-endian: hi, lo)
ET      EQU VARS+030H           ; 16: engine scratch (never across a call)
CELL    EQU VARS+040H           ; 24: a text cell, 4 x 6 codes
TXINK   EQU VARS+058H           ; text: the ink code
TXINK2  EQU VARS+059H           ;   on odd rows (stripes)
TXX     EQU VARS+05AH           ;   the next cell's x
TXY     EQU VARS+05BH           ;   its row
NDRAWN  EQU VARS+05CH           ; 2: pixels written (DEBUG builds)
ANROW   EQU VARS+05EH           ; the tile row the animation looks at next
ES      EQU VARS+060H           ; 16: a slot's copy (slots.inc)
ESN     EQU VARS+070H           ; the slot being drawn
ESM     EQU VARS+07EH           ; ESLOTS: the CHGM bits still to do
ESP     EQU VARS+071H           ; the window's own slot (FF: none)
EM      EQU VARS+072H           ; 12: the merge's scratch
HUDC    EQU VARS+080H           ; 10: what the panel shows (p_arena AHUD)
MYSEAT  EQU VARS+08AH           ; our seat (FF: watching)
TXPTR   EQU VARS+08CH           ; 2: ETXT's string
PASSES  EQU VARS+08EH           ; 2: the main loop's passes (the rigs read it)
CHGM    EQU VARS+090H           ; slots changed since drawn (bit s)
DQH     EQU VARS+091H           ; DQ's head (next out) and tail (next in)
DQT     EQU VARS+092H
DQOVF   EQU VARS+093H           ; DQ overflowed: scan DIRTY instead
NMISS   EQU VARS+094H           ; image cache misses (the rigs read it)
NWIN    EQU VARS+095H           ; windows drawn (the rigs read it)
NMRG    EQU VARS+096H           ; of them merged
NSLOT   EQU VARS+097H           ; slots EWIN looks at: 4, or 8 with movers
NEAR    EQU VARS+098H           ; two wanted slots near each other (ENEAR)
BOOMT   EQU VARS+099H           ; the last blast's centre tile (MAPMARK)
EV      EQU VARS+0A0H           ; 16: ENEAR's wanted slots' x, y
BIGCELL EQU VARS+380H           ; 48: EBIG's cell, 8 x 6 (rows doubled by the window)

; input and sound (input.inc, sound.inc)
INPDIR  EQU VARS+0C0H           ; held directions (IN_*)
INPBTN  EQU VARS+0C1H           ; button held
EVBTN   EQU VARS+0C2H           ; taken: the button was pressed
EVKEY   EQU VARS+0C3H           ; taken: a key
INLBTN  EQU VARS+0C4H           ; latched until taken
INLKEY  EQU VARS+0C5H
INPREV  EQU VARS+0C6H           ; last scan: controller
INPREV0 EQU VARS+0C7H           ; last scan: console buttons
INPHK   EQU VARS+0C8H           ; test hook: nonzero = use the next three
INPHKD  EQU VARS+0C9H
INPHKB  EQU VARS+0CAH
INPHKK  EQU VARS+0CBH
SNDID   EQU VARS+0D0H           ; playing (255 none)
SNDT    EQU VARS+0D1H           ; frames left in this note
SNDN    EQU VARS+0D2H           ; note number
SNDMUTE EQU VARS+0D3H
SNDTONE EQU VARS+0D4H           ; tone bits held (port 5 bits 7-6)
SNDAGE  EQU VARS+0D5H           ; frames into the note
SNDMODE EQU VARS+0D6H           ; its retrigger rule
SNDPTR  EQU VARS+0D7H           ; 2: the note list

; the link (net.inc): NINIT zeroes NZERO up to NZEND
NSEQ    EQU VARS+200H           ; the SEQ ACKSEQ echoes when it is done
NKIND   EQU VARS+201H           ; NK_*: the transaction out (0 none)
NWAIT   EQU VARS+202H           ; passes it has been out
NLINK   EQU VARS+203H           ; NL_*
NZERO   EQU VARS+204H
NEVT    EQU VARS+204H           ; EV_*: what this pass's step brought
NERR    EQU VARS+205H           ; an E frame's code
NBW     EQU VARS+206H           ; 2: bytes a STATUS said are waiting, not yet read
TXF     EQU VARS+208H           ; TX_*: frames waiting to be sent
ASEQ    EQU VARS+209H           ; the action: seq, kind, args (its frame's payload)
AK      EQU VARS+20AH
A1      EQU VARS+20BH
A2      EQU VARS+20CH
SENTX   EQU VARS+20DH           ; our move: x, y, dir|moving, the warp it answers
SENTY   EQU VARS+20EH
SENTD   EQU VARS+20FH
MYWARP  EQU VARS+210H
TAKEN   EQU VARS+211H           ; bytes taken from the link (mod 256)
CREDITED EQU VARS+212H          ; and told the server
QUIET   EQU VARS+213H           ; 2: jiffies since a good frame
QPING   EQU VARS+215H           ; 2: the same, to the next PING
QASK    EQU VARS+217H           ; and to the next HELLO
RHOLD   EQU VARS+218H           ; b0-6 passes to the next RESYNC, b7 one owed
DLEFT   EQU VARS+219H           ; the READ's bytes not yet decoded
DRPOS   EQU VARS+21AH           ; the reply's offset of the next one
DCODE   EQU VARS+21BH           ; (DCODE, FBN, DFLAG in this order: DECODE)
FBN     EQU VARS+21CH           ; FBUF's bytes
DFLAG   EQU VARS+21DH
MFLAGS  EQU VARS+21EH           ; MF_*: what the patches did
TBLN    EQU VARS+21FH           ; T frames: the count (FF: none yet)
TBLHAVE EQU VARS+220H           ;  and the records in (a bit each)
NZEND   EQU VARS+221H
NBAD    EQU VARS+221H           ; frames that failed their sums (the rigs)
JOINT   EQU VARS+222H           ; 2: jiffies waiting for the first state
DRP2    EQU VARS+224H           ; 3: DECODE's place across FREND
RAWN    EQU VARS+227H           ; the frame being built: its length
BK      EQU VARS+228H           ;  its TXF bit's number
FLAGS   EQU VARS+229H           ; FL_*
MSGID   EQU VARS+22AH           ; the boot page's message
FAILRM  EQU VARS+22BH           ;  and E_FAIL's: the room forgotten too
SHOWS   EQU VARS+22CH           ; S_*: the screen at the table
SHSEQ   EQU VARS+22DH           ; the arena's round seq when it was entered
RCOUNT  EQU VARS+22EH           ; a count for a loop around calls
T2      EQU VARS+22FH
RAWBUF  EQU VARS+230H           ; 24: a frame being built
MYNAME  EQU VARS+250H           ; 9: ASCII, NUL-padded
SRVTBL  EQU VARS+260H           ; 9: the table's id, NUL-padded
FBUF    EQU VARS+280H           ; FBMAX: a frame being decoded (to VARS+304H)

; scratch for the game logic (any routine; never across a call)
GT      EQU VARS+100H           ; 32
; the test scenes' own (p_test)
TV      EQU VARS+140H           ; 64

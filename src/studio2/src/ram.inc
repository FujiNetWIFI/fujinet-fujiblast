; ram.inc -- console RAM ($0800-$09FF), the ledger. $0800-$080F are the
; runtime's (rt/s2macro.inc); page $08 is reached through RA (VAR/GETV/
; PUTV); page $09 holds the frame buffers and, from the top down, the stack.
; The state itself is in the cart's RAM (equ.inc ST), read through RB.

; ---- core ------------------------------------------------------------------
TESTHK  EQU     V_APP+00H       ; 4: the harness's hook (3C C3, 5A ..., A5)
JIFFY   EQU     V_APP+04H       ; 2: the game's clock (jiffies run), lo hi
LASTRE  EQU     V_APP+06H       ; RE.0 when the last pass began
DT      EQU     V_APP+07H       ; jiffies this pass runs (at most 8)
PASSES  EQU     V_APP+08H       ; 2: the main loop's passes (the rigs read it)
FLAGS   EQU     V_APP+0AH       ; FL_*
DBGAT   EQU     V_APP+0BH       ; where the loop is (the harness peeks it)
; the keys (input.inc)
KHELD   EQU     V_APP+0CH       ; KB_*: held now
KNEW    EQU     V_APP+0DH       ; KB_*: pressed since a screen took them
KPREV   EQU     V_APP+0EH       ; KB_*: held at the scan before
KDIG    EQU     V_APP+0FH       ; the lowest digit held (0-9, NONE)
KREPT   EQU     V_APP+10H       ; JIFFY lo when a held direction repeats
KTMP    EQU     V_APP+11H       ; the key number OUT 2 latches
; sound (sound.inc)
SNDPH   EQU     V_APP+12H       ; the cue's next (on, off) pair; 0 none
SNDPL   EQU     V_APP+13H
SNDT    EQU     V_APP+14H       ; JIFFY lo when that pair starts
SNDID   EQU     V_APP+15H       ; the cue playing
KEV     EQU     V_APP+16H       ; a digit pressed or repeating (NONE: taken)
; scratch for one routine at a time (never across a call)
T0      EQU     V_APP+18H
T1      EQU     V_APP+19H
T2      EQU     V_APP+1AH
T3      EQU     V_APP+1BH
T4      EQU     V_APP+1CH
T5      EQU     V_APP+1DH
T6      EQU     V_APP+1EH
T7      EQU     V_APP+1FH

FL_ROOM EQU     01H             ; FLAGS: key 0x10 holds our server and table
FL_TEST EQU     02H             ;   an offline test scene: the link is quiet
FL_MUTE EQU     04H

; ---- the link (net.inc): $0830-$085F ------------------------------------------
NSEQ    EQU     V_APP+20H       ; the SEQ ACKSEQ echoes when it is done
NKIND   EQU     V_APP+21H       ; NK_*: the transaction out (0 none)
NWAIT   EQU     V_APP+22H       ; passes it has been out
NLINK   EQU     V_APP+23H       ; NL_*
NZERO   EQU     V_APP+24H       ; (ninit zeroes NZERO up to NZEND)
NEVT    EQU     V_APP+24H       ; EV_*: what this pass's step brought
NERR    EQU     V_APP+25H       ; an E frame's code
NBW     EQU     V_APP+26H       ; 2: bytes a STATUS said are waiting, not read
TXF     EQU     V_APP+28H       ; TX_*: frames waiting to be sent
ASEQ    EQU     V_APP+29H       ; the action: seq, kind, args (its payload)
AK      EQU     V_APP+2AH
A1      EQU     V_APP+2BH
A2      EQU     V_APP+2CH
TAKEN   EQU     V_APP+2DH       ; bytes taken from the link (mod 256)
CREDITED EQU    V_APP+2EH       ;  and told the server
QUIET   EQU     V_APP+2FH       ; 2: jiffies since a good frame
QPING   EQU     V_APP+31H       ; 2: the same, to the next PING
QASK    EQU     V_APP+33H       ; and to the next HELLO
RHOLD   EQU     V_APP+34H       ; b0-6 passes to the next RESYNC, b7 one owed
DLEFT   EQU     V_APP+35H       ; the READ's bytes not yet decoded
DRPOS   EQU     V_APP+36H       ; the reply's offset of the next one
DCODE   EQU     V_APP+37H       ; the COBS block's data bytes left
FBN     EQU     V_APP+38H       ; FBUF's bytes
DFLAG   EQU     V_APP+39H       ; DF_*
TBLN    EQU     V_APP+3AH       ; T frames: the count (FF: none yet)
TBLHAVE EQU     V_APP+3BH       ;  and the records in (a bit each)
NZEND   EQU     V_APP+3CH
NBAD    EQU     V_APP+3CH       ; frames that failed their sums (the rigs)
JOINT   EQU     V_APP+3DH       ; 2: jiffies waiting for the first state
RAWN    EQU     V_APP+3FH       ; the frame being built: its length
BK      EQU     V_APP+40H       ;  its TXF bit's number
SGAP    EQU     V_APP+41H       ; jiffies before the next STATUS
SHOWS   EQU     V_APP+42H       ; S_*: the screen at the table
SHSEQ   EQU     V_APP+43H       ; the arena's round seq when it was entered
MSGID   EQU     V_APP+44H       ; the message to show (b7: the room forgotten)
MRC     EQU     V_APP+45H       ; a count for a loop around calls
NDBG    EQU     V_APP+46H       ; mailbox reads while a transaction was out
DPOS    EQU     V_APP+47H       ; FPATCH: the frame's next byte
DRUN    EQU     V_APP+48H       ;  the run's bytes left
DOFF    EQU     V_APP+49H       ;  the state's offset
DREM    EQU     V_APP+4AH       ;  the frame's bytes left
DSV1    EQU     V_APP+4BH       ; decode's place across frend: the reply's
DSV2    EQU     V_APP+4CH       ;  pointer (low), the chunk's bytes left

; ---- our walker and play (p_play.inc): $0860-$087F ------------------------------
MYSEAT  EQU     V_APP+50H       ; our seat (NONE: watching)
WX      EQU     V_APP+51H       ; our walker (move.c mv_x, mv_y, mv_dir, mv_acc)
WY      EQU     V_APP+52H
WDIR    EQU     V_APP+53H
WACC    EQU     V_APP+54H
WBUMP   EQU     V_APP+55H       ; mv_bump
PEND    EQU     V_APP+56H       ; our bomb, before the server has it (NONE)
PENDL   EQU     V_APP+57H       ; jiffies it waits for the server
WMOVED  EQU     V_APP+58H       ; we moved this pass
WSPD    EQU     V_APP+59H       ; our speed level (P_SPEED)
WD      EQU     V_APP+5AH       ; mv_step's way
WN      EQU     V_APP+5BH       ;  its unit steps left
WSX     EQU     V_APP+5CH       ; try_dirs: the walker before the turn (x, y, acc)
WSY     EQU     V_APP+5DH
WSA     EQU     V_APP+5EH
WTB     EQU     V_APP+5FH       ;  the way on, if the turn is walled
SENTX   EQU     V_APP+60H       ; our move as sent: x, y, dir|moving
SENTY   EQU     V_APP+61H
SENTD   EQU     V_APP+62H
SENTW   EQU     V_APP+63H       ; jiffies before a changed move may go (MOVE_EVERY)
SENTH   EQU     V_APP+64H       ;  before it goes anyway (the heartbeat)
BUMPW   EQU     V_APP+65H       ; jiffies before another bump may go
WME     EQU     V_APP+66H       ; we walk this pass
WCNT    EQU     V_APP+67H       ; the pass's jiffies left
WMS     EQU     V_APP+68H       ; mv_step moved

; ---- the arena (p_arena.inc): $0880-$08EF ---------------------------------------
DQH     EQU     V_APP+70H       ; the dirty tiles' queue: next out, next in
DQT     EQU     V_APP+71H
REDRAW  EQU     V_APP+72H       ; a sweep over every tile: the next (NONE: off)
ANIMPH  EQU     V_APP+73H       ; the two-frame tiles' frame now (0/1)
ANIMT   EQU     V_APP+74H       ; jiffies to the next flip
SHAKE   EQU     V_APP+75H       ; jiffies of the blast's shake left
FLASH   EQU     V_APP+76H       ; jiffies of the inverted picture left
SDX     EQU     V_APP+78H       ; 4: each seat drawn at (units)
SDY     EQU     V_APP+7CH       ; 4
SPX     EQU     V_APP+80H       ; 4: its sprite's x (pixels), NONE hidden
SPY     EQU     V_APP+84H       ; 4: its sprite's y (lines)
SFW     EQU     V_APP+88H       ; 4: the frame it wants (FR_*), NONE hidden
SFS     EQU     V_APP+8CH       ; 4: the frame its image holds
SRK     EQU     V_APP+90H       ; 4: its sprite slot (0-3), by y
DIET    EQU     V_APP+94H       ; 4: jiffies into its fall (0 none)
MVK     EQU     V_APP+98H       ; 4: each mover's image shown (NONE hidden)
MVPX    EQU     V_APP+9CH       ; 4: its x
MVPY    EQU     V_APP+0A0H      ; 4: its y
HTIM    EQU     V_APP+0A4H      ; the HUD as drawn: the clock,
HALV    EQU     V_APP+0A5H      ;  who is alive,
HWIN    EQU     V_APP+0A6H      ;  4: the wins,
HSTAT   EQU     V_APP+0AAH      ;  5: our power-ups
HSEAT   EQU     V_APP+0AFH      ;  and the seat marked ours
WASALV  EQU     V_APP+0B0H      ; the alive mask at the last state
LASTPH  EQU     V_APP+0B1H      ; the phase at the last state
WALKF   EQU     V_APP+0B2H      ; the jiffy clock's walk phase base
GI      EQU     V_APP+0B3H      ; a loop's count (across calls)
GS      EQU     V_APP+0B4H      ; the seat being done (across calls)
UPC     EQU     V_APP+0B5H      ; frame uploads left this pass
SDIRTY  EQU     V_APP+0B6H      ; seats whose sprite needs sending (a bit each)
ORDER   EQU     V_APP+0B8H      ; 4: the seat in each sprite slot (by y)
SKEY    EQU     V_APP+0BCH      ; 4: its slot key: its sprite's y, 255 hidden
VIEWS   EQU     V_APP+0C0H      ; an effect is on the VIEW (0: plain)
MFLAGS  EQU     V_APP+0C1H      ; MF_*: what the patches did since the arena looked
MYWARP  EQU     V_APP+0C2H      ; the warp our moves answer
TXM     EQU     V_APP+0C8H      ; txt_at's mode (TM_SET, | TM_SHORT)
NUMB    EQU     V_APP+0C9H      ; 4: txt_num's digits, NUL-ended
MENUSEL EQU     V_APP+0CDH      ; a menu's choice
MENUN   EQU     V_APP+0CEH      ;  and how many
MYNAME  EQU     V_APP+0D0H      ; 9: ASCII, NUL-padded
SRVTBL  EQU     V_APP+0DAH      ; 9: the table's id, NUL-padded
RFINAL  EQU     V_APP+0E4H      ; the trophies: the game's end (not a round's)
RSEAT   EQU     V_APP+0E5H      ;  the winner (OFF_RESULT)
RTIM    EQU     V_APP+0E6H      ;  the clock shown
RFRZ    EQU     V_APP+0E7H      ;  jiffies the arena stays, inverted
SSYT    EQU     V_APP+0E8H      ; seats_show's lines: 0 the room's, 4 the trophies'
SLD     EQU     V_APP+0E9H      ; 4: each seat's P_DIR when last drawn (NONE: redo)
LSEATED EQU     V_APP+0EDH      ; OFF_SEATED when findseat last ran
TRP     EQU     V_APP+0EEH      ; DEBUG: the walk trace's next entry (TRACE + TRP)
; the test scenes' own (p_test.inc)
TBW     EQU     V_APP+0C4H      ; the bench's walk (0-160)
TBB     EQU     V_APP+0C5H      ;  its way (0 out, 1 back)
TBT     EQU     V_APP+0C6H      ;  jiffies to the blasts
TBON    EQU     V_APP+0C7H      ;  the blast is on

PHEND   EQU     V_APP+0F0H
        IF      HI(PHEND-1) <> HI(RAMVARS)
        ERROR   "RAM ledger runs off page $08"
        ENDIF

; ---- page $09 ------------------------------------------------------------------
FBUF    EQU     0900H           ; 132: a frame being decoded
RAWBUF  EQU     0984H           ; 24: a frame being built
DQ      EQU     099CH           ; 32: the dirty tiles' queue (a ring)
ANIMB   EQU     09BCH           ; 18: the two-frame tiles on screen, a bit each
STKLOW  EQU     09CEH           ; the stack grows down from $09FF to here

; fbdefs.inc -- what every bank of the FujiBlast 2600 client agrees on.
; EQUATES ONLY: included before the ORG, next to vcs.inc and fujinet.inc.
;
; THE ZERO PAGE IS THE BINDING LIMIT: 118 bytes ($80-$F5; the stack is
; $F6-$FF) for everything. So:
;   * PERSISTENT variables are allocated in order from $80 by VAR below.
;   * The POOL is what is left at the top. Anything that lives only inside one
;     hop (a compose unit, a link step, the HELLO frame) or only inside one
;     kernel pass (the arena's row/seam cells, player 1's job list) is an
;     offset into it, and they alias each other freely: a hop never overlaps a
;     kernel pass, and BUILDJOBS (vblank) sets the kernel's cells last.
;   * The last bytes of the pool are the text kernel's and the menus' own.
;
; The state (st[247]) is not kept: the MAP lives in the cartridge's planes
; (MAPROW), the few header/player fields the screens need are persistent
; cells here, written by the decoder through the SDEST table; the HUD's timer
; and trophies are blitted to the text rows as they arrive.

; ---------------- the banks (32K: fifteen 2K banks + the fixed half) ----
BKBOOT  EQU     0               ; cold start, appkeys, the socket
BKARENA EQU     1               ; the arena kernel and its frame
BKCOMP  EQU     2               ; compose rows into the planes (art tables)
BKNET   EQU     3               ; the link engine and the decoder
BKPLAY  EQU     4               ; the walker, input, actions
BKSCR   EQU     5               ; the text screens
BKDEC   EQU     6               ; the frame decoder
NBANKS  EQU     15

MGOTO   EQU     FNTAIL          ; the trampoline, first thing in the tail

; A bank switch is a jump; where the bank entered carries on is data in MENT.
ENCOLD  EQU     0               ; power-on: boot
ENFRAME EQU     1               ; a display bank: carry on into the next frame
ENWORK  EQU     2               ; a work bank: do this frame's hop
ENENTER EQU     3               ; a display bank: a new screen, set it up
ENVB    EQU     4               ; the arena: back from the vblank's compose hop
ENARENA EQU     5               ; the compose bank: set the arena's planes up
; MRET = (bank << 4) | entry code: where a work bank hands back to

; ---------------- persistent zero page ----------------
ZPC     SET     $80
VAR     MACRO   name,size
name    EQU     ZPC
ZPC     SET     ZPC+size
        ENDM

; the transport and the system
        VAR     FNSEQ,1         ; the sequence a transaction waits for
        VAR     MTMP,1          ; scratch: any routine may clobber it
        VAR     MTMP2,1
        VAR     MENT,1          ; EN*: what the bank entered should do
        VAR     MRET,1          ; the entry code a display bank is re-entered with
        VAR     MFRAME,1        ; free-running frame counter, 16 bits
        VAR     MFRAMEH,1
        VAR     INCUR,1         ; input levels, active high: IN_*
        VAR     INPREV,1
; the screens
        VAR     MSCR,1          ; the screen showing (SC_*)
; the arena
        VAR     P0X,1           ; player 0's colour clock
        VAR     P0COL,1
        VAR     HTXT,1          ; the text rows' ink
; compose
        VAR     DIRTYL,1        ; dirty map rows, bit r of 11
        VAR     DIRTYH,1
        VAR     CROW,1          ; the row being composed, $FF = none
        VAR     CREG,1          ; its next unit: $FF the prelude, else 0-5
        VAR     CSET,1          ; art set offset: 0 quiet row, 126 busy row
; the game state the screens and the arena need
        VAR     BX,4            ; world x (units of 1/16 tile, 0-192)
        VAR     BY,4            ; world y (0-160)
        VAR     BDIR,4          ; dir | moving, per seat
        VAR     MFLG,1          ; MY seat's flags (kick 1, glove 2), bombs allowed,
        VAR     MBOMB,1         ;   speed level and bombs out (in this order)
        VAR     MSPD,1
        VAR     MOUT,1
        VAR     MACC,1          ; the walker's sub-unit accumulator
        VAR     SRVX,1          ; the server's x,y for MY seat (a warp snaps to it)
        VAR     SRVY,1
        VAR     MYSEAT,1        ; $FF until the names say
        VAR     PHASE,1
        VAR     ROUNDS,1
        VAR     HOST,1
        VAR     RESULT,1
        VAR     SEATED,1        ; seat masks
        VAR     ALIVE,1
        VAR     MISM,4          ; per seat: bit i set = name byte i differs from mine
; the link
        VAR     LREQ,1          ; the transaction in flight (LQ_*), 0 = none
        VAR     LTMO,1          ; frames it has been out
        VAR     LPOLL,1         ; frames until the next STATUS
        VAR     LPING,1         ; keepalive clock (2-frame units)
        VAR     LCRED,1         ; bytes taken from the stream, mod 256
        VAR     LCRSNT,1        ; ...as last told to the server
        VAR     LACT,1          ; the queued action (ACT_*), 0 = none
        VAR     LA1,1
        VAR     LA2,1
        VAR     LWARP,1         ; my warp byte as last seen
        VAR     LFLAGS,1        ; LF_*
; the decoder (a reply is decoded a few bytes a hop)
        VAR     RPOS,1          ; the next byte of the reply window
        VAR     RLEFT,1         ; reply bytes still to decode
        VAR     DST,1           ; DS_*: frame parse state
        VAR     DCNT,1          ; data bytes left in this COBS block
        VAR     DCODE,1         ; the block's code byte (255 = no implied zero)
        VAR     DS1,1           ; Fletcher sums so far
        VAR     DS2,1
        VAR     DH1,1           ; the two-byte delay line: the last two bytes of
        VAR     DH2,1           ;   a frame are its sums, so a byte is acted on
                                ;   only when two newer ones have arrived
        VAR     DOFF,1          ; the patch's current state offset
        VAR     DLEN,1          ; bytes left in the patch run
        VAR     DMROW,1         ; map run: the row and column of the next byte
        VAR     DMCOL,1
        VAR     DMSEG,1         ; ...and the column the buffered segment began at
        VAR     DTYPE,1         ; the frame's type
        VAR     DFILL,1         ; bytes in the delay line (0-2)
; identity (HELLO)
        VAR     NAME,8
        VAR     TABLE,8
PERSIST EQU     ZPC

; ---------------- the pool ----------------
POOL    EQU     ZPC
        IF      POOL > $EB
        ERROR   "persistent zero page overruns the pool"
        ENDIF

; the arena kernel's per-pass cells (set in the vblank; hops may use the
; space before it, never after)
ROW     EQU     POOL+0          ; tile row 0-10, then 11 = done
NSEAM   EQU     POOL+1          ; the row at which player 1 changes bomber
JOBI    EQU     POOL+2
CA      EQU     POOL+3          ; this row's colours: even entries
CB      EQU     POOL+4          ;   odd entries
P0CNT   EQU     POOL+5
P1CNT   EQU     POOL+6
P0PTR   EQU     POOL+7          ; 2 bytes: shape - top entry
P1PTR   EQU     POOL+9
NJOBS   EQU     3
JROW    EQU     POOL+11         ; 4 bytes, $FF ends
JX      EQU     POOL+15         ; 3
JCOL    EQU     POOL+18
JTOP    EQU     POOL+21
POOLEND EQU     POOL+24
; BUILDJOBS' own scratch (cells the kernel sets later, or sets after)
TEOP    EQU     POOL+0          ; 4: but ROW is set after BUILDJOBS, NSEAM at its end
TUSED   EQU     POOL+4
TLAST   EQU     POOL+9
BJROW   EQU     POOL+10
P0TOP   EQU     POOL+23         ; (aliases JTOP[2]: used before the jobs are built)
BVIS    EQU     POOL+22         ; (aliases JTOP[1], same reason)
PH_WAIT EQU     0
PH_COUNT EQU    1
PH_PLAY EQU     2
PH_ROUNDEND EQU 3
PH_GAMEOVER EQU 4
; the text kernel's cells (live through a frame's text rows)
MLINE   EQU     POOL+24
PAD3    EQU     MLINE           ; TXTPOS' 3-cycle pad target: any scratch will do
SAVSP   EQU     POOL+25
SND     EQU     POOL+26         ; the sound effect's step (0 = silent)
        INCLUDE "sndequ.inc"
; the text screens' persistent cells live on cells that are dead outside the
; arena (the compose bank's, and player 0's): the arena re-initialises them
MTABL   EQU     CREG            ; its row-colour table
MTABH   EQU     CSET
MSEL    EQU     P0X             ; a menu's selection
MSHOW   EQU     P0COL           ; the value last drawn (ROUNDS)
SC_WAIT EQU     1
SC_ROUND EQU    2
SC_OVER EQU     3
SC_CONN EQU     4
SC_LOST EQU     5
SC_TITLE EQU    6
SC_NAME EQU     7
SC_ROOMS EQU    8
SC_ARENA EQU    $10

; hop scratch (any work bank, any hop; all of POOL..POOLEND)
; compose
CJ      EQU     POOL+0
CJN     EQU     POOL+1
CTP     EQU     POOL+2          ; 2 bytes: contribution table of the current tile
CMAXC   EQU     POOL+4
CRPTR   EQU     POOL+17         ; 2 bytes: the row's map bytes (in the planes)
CBD     EQU     POOL+5          ; blit destination (2) and count
CBC     EQU     POOL+7
CARTP   EQU     POOL+8          ; 2 bytes: art table for (set, entry)
RACC    EQU     POOL+10         ; 7 bytes: one register's seven entries
; the link (a transaction is set up with FNDEV..FNNPR, FNBEG is called, and only
; then is a frame built in RAW over them)
FNDEV   EQU     POOL+0
FNCMD   EQU     POOL+1
FNNPR   EQU     POOL+2
FNTMO   EQU     POOL+3
FNCNT   EQU     POOL+4
RAW     EQU     POOL+0          ; the frame being built: up to 21 bytes
RAWMAX  EQU     21
KT0     EQU     POOL+21         ; frame builder / decoder scratch
KT1     EQU     POOL+22
KT2     EQU     POOL+23
KT3     EQU     POOL+24

; the walker (play.asm)
W       EQU     POOL+0          ; 2: x, y
WD      EQU     POOL+2
WP      EQU     POOL+3
WS      EQU     POOL+4
WDX     EQU     POOL+5
WOFF    EQU     POOL+6
WPT     EQU     POOL+7
WSR     EQU     POOL+8
WBUMP   EQU     POOL+9
WMOVED  EQU     POOL+10
WSAV    EQU     POOL+11         ; 3: try_dirs' undo
WH      EQU     POOL+14
WV      EQU     POOL+15
WA      EQU     POOL+16
WPTR    EQU     POOL+17         ; 2: a map row

IN_UP   EQU     $01
IN_DOWN EQU     $02
IN_LEFT EQU     $04
IN_RIGHT EQU    $08
IN_FIRE EQU     $10
IN_RESET EQU    $20
IN_SELECT EQU   $40
DIR_UP  EQU     0
DIR_RIGHT EQU   1
DIR_DOWN EQU    2
DIR_LEFT EQU    3
LANE    EQU     6
TUNIT   EQU     16

STKLOW  EQU     $F6             ; the stack is $F6-$FF

; ---------------- the television standard ----------------
; NTSC: 3 + 37 + 192 + 30 = 262 lines. PAL: 3 + 63 + 192 + 54 = 312 (our 192-line
; picture sits in the middle of PAL's 228: the extra lines are blanking).
        IFNDEF  PAL
PAL     EQU     0
        ENDIF
        IF      PAL
VBTIM   EQU     74              ; timer ticks (64 cycles) for the vblank,
OSTIM   EQU     62              ;   the overscan,
        ELSE
VBTIM   EQU     43
OSTIM   EQU     35
        ENDIF
VITIM   EQU     227             ;   and the 192 visible lines

; ---------------- link constants ----------------
LQ_NONE EQU     0
LQ_WRITE EQU    1
LQ_STAT EQU     2
LQ_READ EQU     3
LQ_OPEN EQU     4
LF_CONN EQU     $01             ; the socket is open
LF_HELLO EQU    $02             ; a HELLO must go out
LF_RESYNC EQU   $04             ; an R must go out
LF_GOT  EQU     $08             ; a state has been committed
LF_PLAY EQU     $10             ; the walker is live: send M
LF_LOST EQU     $20             ; the link is dead
LF_LIST EQU     $40             ; a T (list tables) must go out
LF_BYE  EQU     $80             ; a B must go out

; decoder states
DS_TYPE EQU     0               ; the next payload byte is the type
DS_Q_OFF EQU    1               ; Q: the run's offset
DS_Q_LEN EQU    2               ; Q: its length
DS_Q_DATA EQU   3               ; Q: data
DS_SKIP EQU     4               ; ignore the rest of the frame
DS_C_SEQ EQU    5               ; C: the ack byte
DS_E_CODE EQU   6               ; E: the code
DS_T_IDX EQU    7               ; T: index
DS_T_CNT EQU    8               ; T: count
DS_T_REC EQU    9               ; T: the 24-byte record

; ---------------- the planes (arena mode) ----------------
; Plane r holds playfield register r's table: 77 entries (11 rows x 7) from
; byte PFB. The map rows sit in the free bytes after them; text rows 0 (HUD)
; and 20 (status) are the cartridge's own.
PFB     EQU     6
MAPB    EQU     83              ; first byte of a plane's map rows
PFT0    EQU     FNTEXT+0*$80+PFB
PFT1    EQU     FNTEXT+1*$80+PFB
PFT2    EQU     FNTEXT+2*$80+PFB
PFT3    EQU     FNTEXT+3*$80+PFB
PFT4    EQU     FNTEXT+4*$80+PFB
PFT5    EQU     FNTEXT+5*$80+PFB
ROWS    EQU     11
COLS    EQU     13
ENTS    EQU     7               ; entries (2 lines) a tile row

; tile codes (server/internal/proto)
T_FLOOR EQU     0
T_PILLAR EQU    1
T_BLOCK EQU     2
T_BURN  EQU     3
T_BOMB  EQU     4
T_DUD   EQU     5
T_FLAME_C EQU   6
T_FLAME_H EQU   7
T_FLAME_V EQU   8
T_FLAME_L EQU   9
T_FLAME_R EQU   10
T_FLAME_U EQU   11
T_FLAME_D EQU   12
T_FIRE  EQU     13
T_BOMBUP EQU    14
T_SPEED EQU     15
T_KICK  EQU     16
T_GLOVE EQU     17
NTILES  EQU     18

C_FLOOR EQU     $C2             ; the arena's colours (mkspike.py's)
C_STATIC EQU    $2A

; path buffers
PB_NAMES EQU    FP_SEL0         ; the names block, captured for the screens
PB_HOST EQU     FP_SEL1         ; the server host (from appkey 0x10)
PB_TABLE EQU    FP_SEL2         ; (spare)
PB_STREAM EQU   FP_SEL3         ; transient streams: compose rows, map runs

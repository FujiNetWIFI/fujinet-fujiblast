; ram.inc -- the RAM ledger.
;
; The console has 1K, at 7000H-73FFH (mirrored from 6000H; 7800H up must
; never be touched, see fujilib.inc). The BIOS keeps 73B9H up, OS7's sound
; engine's header area 7020H-702AH (unused here: the sound is ours, src/coleco/
; sound.inc, but the header's pointers must name RAM that is not ours).
; tools/coleco/checksize.py fails the build if the variables run into the
; stack.
;
; The state (ST) is src/proto.h's 247 bytes, patched in place by the link;
; the transcription of the Astrocade's ledger (src/astrocade/src/ram.inc).

CTLDAT  EQU     7000H           ; 12: POLLER's controller area (the header's)
SNDAREA EQU     700CH           ; 11: the header's sound data area, never used
VDPBUSY EQU     7017H           ; the game is at the VDP: the NMI waits
VDPPEND EQU     7018H           ; ... and owes a frame
VDPSEEN EQU     7019H           ; status bits seen (bit 6: a fifth sprite)
FRAMES  EQU     701BH           ; 60 Hz, counted in the NMI (2)
SPARE1D EQU     701DH           ; 701DH-701FH: spare
RAMBASE EQU     702BH           ; past the sound engine's 7020H-702AH

STACK   EQU     OS7STK          ; grows down from the BIOS's own
STKLOW  EQU     7339H           ; 128 bytes of stack: the NMI takes ~45 on top
                                ; of the game's (measured, M.U.L.E.)

RAMP    DEFL    RAMBASE
VAR     MACRO   nm, sz
nm      EQU     RAMP
RAMP    DEFL    RAMP+sz
        ENDM

; ---- the state (src/proto.h's layout): ST + OFF_* ----------------------------------
        VAR     ST,STATE_SIZE
; while no state is live (the boot, the room list) it holds the table list
; and the room's URL
TBLS    EQU     ST              ; MAX_TABLES records of TABLE_REC
URLBUF  EQU     ST+MAX_TABLES*TABLE_REC ; key 0x10 as read (the boot page)
URLMAX  EQU     STATE_SIZE-MAX_TABLES*TABLE_REC-1

; ---- time, paging, input ------------------------------------------------------------
        VAR     LASTF,2         ; FRAMES when the loop last came round
        VAR     FRML,1          ; our frame clock (jiffies): the sum of DT
        VAR     FRMH,1
        VAR     DT,1            ; jiffies since the loop last came round (1-8)
        VAR     CURPG,1         ; the page at 2000H
        VAR     FCHL,2          ; FARCALL's scratch
        VAR     FCTGT,2
        VAR     FCA,1
        VAR     INDIR,1         ; directions held (D_*)
        VAR     PRVDIR,1        ; last frame's (a push is an edge)
        VAR     INBTN,1         ; the trigger held
        VAR     EVBTN,1         ; it went down this frame
        VAR     EVKEY,1         ; a key went down this frame (K_*, digits)
        VAR     KRAW,1          ; last frame's key (edges)
        VAR     FLAGS,1         ; FL_*
        VAR     RNDS,2          ; RAND's seed
        VAR     T0,1            ; scratch: never across a call that says so
        VAR     T1,1
        VAR     T2,1
        VAR     T3,1
        VAR     TESTHK,4        ; the rigs' offline scenes (src/test.c's test_hook)
        VAR     INPHK,1         ; a harness drives the controls: this, the
        VAR     INPHKD,1        ;  directions, the button, a key
        VAR     INPHKB,1
        VAR     INPHKK,1
        VAR     INNAME,1        ; a name is being entered (the keypad's meaning)

; ---- the screen (gfx.inc) --------------------------------------------------------------
        VAR     BLANKED,1       ; the display is off: the VDP bursts may be fast
        VAR     SCRBG,1         ; the screen's background colour (cell 0's)
        VAR     NEXTC,1         ; the next character code free (from below)
        VAR     SSZ,1           ; glyphs in a style's set,
        VAR     FIRSTG,1        ;  the first (ASCII - 32)
        VAR     STYN,1          ; styles in use (the sets stack down from 256)
        VAR     STYC,4          ;  each its colour byte, fg*16 + bg

        VAR     TXB,1           ; text's style: its glyph FIRSTG's character code
        VAR     TXSP,1          ;  and a space's
        VAR     CPSRC,2         ; CELLPUT's, STYUP's scratch
        VAR     CPADR,2
        VAR     CPCNT,2
        VAR     CPC,1
        VAR     NUMBUF,4        ; NUMAT's digits and a NUL
        VAR     CHRBUF,2        ; CHRAT's character and a NUL
        VAR     LOGOC,1         ; the logo's first character code
        VAR     TROC,1          ; the trophy's first code (0: not made this screen)

; ---- text ------------------------------------------------------------------------------

; ---- sprites (sprites.inc) -------------------------------------------------------------
        VAR     SPR,24          ; a record a seat: +0 on, +1 the cheering pair,
                                ;  +2 its frame, +3 its colour's index, +4 x,
                                ;  +5 y (VDP pixels)
        VAR     SROT,1          ; who is in front when three share lines

; ---- sound (sound.inc) -----------------------------------------------------------------
        VAR     SNDV0,3         ; the jingle voice: the next note, frames left
        VAR     SNDV1,3         ; the effects' voice

; ---- the boot, the menus, the table ---------------------------------------------------
        VAR     MSGID,1         ; the boot page's message
        VAR     FAILRM,1        ;  and E_FAIL's: the room forgotten too
        VAR     MYNAME,NAME_LEN+1       ; ASCII, NUL-padded
        VAR     SRVTBL,TABLE_LEN+1      ; the table's id, ASCII, NUL-padded
        VAR     MYSEAT,1        ; NONE: watching
        VAR     SHOWN,1         ; S_*: the screen at the table
        VAR     SHSEQ,1         ; the arena's round seq when it was entered
        VAR     JOINT,2         ; frames waiting for the first state
        VAR     RCOUNT,1        ; a count for a loop around calls

; ---- the link (p_net.inc) --------------------------------------------------------------
        VAR     NSEQ,1          ; the SEQ ACKSEQ echoes when it is done
        VAR     NKIND,1         ; NK_*: the transaction out (0 none)
        VAR     NWAIT,1         ; frames it has been out
        VAR     NLINK,1         ; NL_*
        VAR     NEVT,1          ; EV_*: what this frame's step brought
        VAR     NERR,1          ; an E frame's code
        VAR     NBW,2           ; bytes a STATUS said are waiting, not yet read
        VAR     TXF,1           ; TX_*: frames waiting to be sent
        VAR     ASEQ,1          ; the action: seq, kind, args (its frame's payload)
        VAR     AK,1
        VAR     A1,1
        VAR     A2,1
        VAR     SENTX,1         ; our move: x, y, dir|moving (the payload) ...
        VAR     SENTY,1
        VAR     SENTD,1
        VAR     MYWARP,1        ;  and the warp count it answers
        VAR     TAKEN,1         ; bytes taken from the link (mod 256)
        VAR     CREDITED,1      ; and told the server
        VAR     QUIET,2         ; frames since a good frame
        VAR     QPING,2         ; the same, mod PING_EVERY
        VAR     QASK,1          ; and mod ASK_AGAIN
        VAR     RHOLD,1         ; b0-6 frames to the next RESYNC, b7 one owed
        VAR     DLEFT,1         ; the READ's bytes not yet decoded
        VAR     DRPOS,1         ; the reply's offset of the next one
        VAR     DCODE,1         ; the COBS block's data bytes left
        VAR     DFLAG,1         ; DF_*
        VAR     D1,1            ; the two decoded bytes held back (the sums at
        VAR     D2,1            ;  a frame's end)
        VAR     DN,1            ; how many are held (0-2)
        VAR     FIDX,1          ; the frame's byte: 0 its type, then payload + 1
        VAR     FTYPE,1
        VAR     S1,1            ; the running Fletcher sums
        VAR     S2,1
        VAR     QST,1           ; a patch: 0 a run's offset next, 1 its length,
                                ;  2 in its bytes, 3 a bad run
        VAR     QOFF,1          ; the run's offset (C, E: the first byte; T: count)
        VAR     QLEN,1          ; its bytes left (T: the index)
        VAR     MFLAGS,1        ; MF_*
        VAR     MAPD,18         ; map tiles patched, a bit each (tile t: byte t/8,
                                ;  bit t%8): drawn at the next commit
        VAR     TBLN,1          ; T frames: the count (0FFH: none yet)
        VAR     TBLHAVE,1       ;  and the records in (a bit each)

; ---- the arena (p_arena.inc, p_play.inc, arena.inc) ------------------------------------
        VAR     MVX,1           ; our walker (src/move.c): position, direction,
        VAR     MVY,1           ;  speed accumulator, the bomb it bumped
        VAR     MVDIR,1
        VAR     MVACC,1
        VAR     MVBUMP,1
        VAR     MOVED,1         ; it moved this jiffy
        VAR     PEND,1          ; our bomb, shown before the server has it
        VAR     PENDL,1         ;  and its frames left
        VAR     BUMPT,2         ; the frame of our last bump
        VAR     SENTT,2         ; and of our last move sent
        VAR     DISPX,4         ; where each seat is (units)
        VAR     DISPY,4
        VAR     DIET,4          ; a knocked-out seat's frames (0: none)
        VAR     MOVHX,4         ; each mover (a bomb in the air) at its half-tile cell
        VAR     MOVHY,4
        VAR     MOVON,1         ;  which are on (a bit each)
        VAR     MOVD,1          ;  and which are duds (a bit each)
        VAR     WASALV,1        ; the alive mask at the last state
        VAR     LASTPH,1        ; the phase at the last state
        VAR     HUDTMR,1        ; the HUD as drawn: the timer, the alive mask,
        VAR     HUDALV,1        ;  the wins
        VAR     HUDWIN,4
        VAR     STATS,5         ; our stats as drawn
        VAR     ANPH,15         ; each animated tile kind's phase as drawn
        VAR     ANNEXT,1        ;  and the next to look at
; the screens at a table (p_room.inc), the menus (p_menu.inc)
        VAR     RESFIN,1        ; the trophies: the game's end?
        VAR     RESSEAT,1       ;  the winner
        VAR     RESROW,4        ;  each seat's row (0 none)
        VAR     RESTMR,1        ;  the timer shown
        VAR     SEL,1           ; a menu's choice
        VAR     NSEL,1          ;  of how many
; a frame's scratch (p_play)
        VAR     PME,1           ; we walk this frame: a seat, alive, in play
        VAR     MSPEED,1        ; our speed level
        VAR     MSD,1           ; mv_step: the direction, units left, moved
        VAR     MSN,1
        VAR     MSMOV,1
        VAR     UDD,1           ; unit_step: dx or dy, the lane's offset
        VAR     UOFF,1
        VAR     TDA,1           ; try_dirs: the turn, on; the walker saved
        VAR     TDB,1
        VAR     TSX,1
        VAR     TSY,1
        VAR     TSA,1
        VAR     PFT,1           ; the tile under us
        VAR     PKK,1           ; we kick
        VAR     PMD,1           ; our move's dir|moving
        VAR     PJIF,1          ; the pass's jiffies (1-4)
        VAR     TSTC,2          ; the test scenes' clock
        VAR     NBAD,1          ; the rigs': bad frames
        VAR     PDIN,1          ; and our walker's jiffies: the disc, then a count,
        VAR     PDBG,6          ;  the disc, x, y, dir, acc (the parity check's)
; a screen's own, or a step's: the compositor's lines, the frame being
; sent (p_net POSTW), the title's walkers -- never two at once
        VAR     WORK,80

RAMEND  EQU     RAMP

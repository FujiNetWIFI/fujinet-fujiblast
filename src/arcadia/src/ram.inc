; ram.inc -- every byte of RAM the game keeps, and where.
;
; Console RAM, 292 bytes in 13-row mode: zone A $18D0-$18EF, the island
; $18F8-$18FB, zone B $1A00-$1AFF. Page-0 code (fixed A and every page)
; reads and writes it directly, fixed B through pointer words.
;
; Cart RAM, 1K (protocol 2), read like ROM and written only through the
; mailbox (fixb.inc RAMPUT, the link's patch stream): RAM-L $0600-$07FF
; holds the state (ST, src/proto.h's 247 bytes) and the table list; RAM-H
; is free.
;
; Each zone is handed out in order by VAR; AS stops the build when one
; runs over.

VAR     MACRO   name,n
name    EQU     RAMNEXT
RAMNEXT SET     RAMNEXT+(n)
        IF      RAMNEXT > RAMEND
        ERROR   "RAM zone full at name"
        ENDIF
        ENDM

ZONE    MACRO   lo,hi
RAMNEXT SET     lo
RAMEND  SET     hi
        ENDM

; ---- cart RAM ------------------------------------------------------------------
ST      EQU     $0600           ; the state (proto.h's layout): ST + OFF_*
TBLS    EQU     $0700           ; the table list: MAX_TABLES records of TABLE_REC
TBLSEND EQU     TBLS+MAX_TABLES*TABLE_REC
        IF      ST+STATE_SIZE > TBLS
        ERROR   "the state runs into the table list"
        ENDIF

; ---- zone A: time, input, sound, the display's pointers ------------------------
        ZONE    $18D0,$18F0
        VAR     TICKH,1         ; the cart's tick at the last frame
        VAR     TICKL,1
        VAR     FRMH,1          ; our frame clock (jiffies), big-endian
        VAR     FRML,1
        VAR     DT,1            ; jiffies since the loop last came round (1-8)
        VAR     CURPG,1         ; the page in the window
        VAR     PGSAVE,1        ; PGFAR: the page to give back
        VAR     AXSEL,1         ; the axis the pot reads next vblank (0 vertical)
        VAR     AXV,1           ; the disc, both axes as last read
        VAR     AXH,1
        VAR     INDIR,1         ; directions held (D_*)
        VAR     PRVDIR,1        ; last frame's (a push is an edge)
        VAR     INBTN,1         ; FIRE held
        VAR     EVBTN,1         ; FIRE went down this frame
        VAR     EVKEY,1         ; a key went down this frame (K_*, digits)
        VAR     KRAW,1          ; last frame's key (edges)
        VAR     DROWH,1         ; the display row's base (DAT, DPOS)
        VAR     DROWL,1
        VAR     DCOLOR,1        ; ink bits for DPUTC
        VAR     DSTRH,1         ; the string DTXT reads
        VAR     DSTRL,1
        VAR     T0,1            ; scratch: never across a call that says so
        VAR     T1,1
        VAR     T2,1
        VAR     T3,1
        VAR     BGCB,1          ; BGCOL without the pot mux
        VAR     FLAGS,1         ; FL_*
        VAR     SC01SH,1        ; the sprite colour registers (write-only)
        VAR     SC23SH,1
        VAR     NSEQ,1          ; the SEQ ACKSEQ echoes when it is done
        VAR     NKIND,1         ; NK_*: the link's transaction out (0 none)
        VAR     NWAIT,1         ; frames it has been out

; ---- the island ($18F0-$18F7 are the sprite registers) -------------------------
        ZONE    $18F8,$18FC
        VAR     SNDP,1          ; the note playing: its offset in SNDNOTE ($FF none)
        VAR     SNDCNT,1        ; its frames left
        VAR     SNDQ,1          ; a sound queued ($FF none)
        VAR     SNDJ,1          ; 1: the one playing is a jingle (an effect cuts it)

; ---- zone B ---------------------------------------------------------------------
        ZONE    $1A00,$1B00
        VAR     TESTHK,4        ; the rigs' offline scenes (src/test.c's test_hook)
        VAR     SPRSH,8         ; the sprites' Y, X as the vblank writes them
        VAR     FRWANT,4        ; each seat's frame (its sprite's picture)
        VAR     FRSHOW,4        ;  and the one in its user glyph ($FF: none yet)
        VAR     ANWANT,1        ; the arena's animated glyphs: the frame wanted
        VAR     ANSHOW,1        ;  and shown ($FF: the glyphs aren't the arena's)
        VAR     NUMV,1          ; DNUM's value
        VAR     NUMD,1          ;  and a place's digit
        VAR     VBUD,1          ; VBSYNC: glyph rewrites left this vblank
        VAR     VSEAT,1         ;  the seat (the glyph's end) being copied
        VAR     VSRCH,1         ;  and from where (UDCSET's too)
        VAR     VSRCL,1
        VAR     TDV,1           ; TDRAW: the cell
        VAR     VBEND,1         ; the beam's row when the vblank's work ended
        VAR     VSEEN,1         ; the display seen since the last vblank's work
        VAR     NBAD,1          ; the rigs': bad frames, late arena passes
        VAR     NLATE,1
        VAR     NPASH,1         ; and arena passes
        VAR     NPASL,1
        VAR     PDIN,1
        VAR     PDBG,6          ; and our walker's jiffies: a count, the disc,
                                ;  x, y, dir, acc (the parity check's)
        VAR     MSGID,1         ; the boot page's message
        VAR     FAILRM,1        ;  and E_FAIL's: the room forgotten too
        VAR     MYNAME,NAME_LEN+1       ; ASCII, NUL-padded
        VAR     SRVTBL,TABLE_LEN+1      ; the table's id, ASCII, NUL-padded
        VAR     MYSEAT,1        ; NONE: watching
        VAR     SHOWN,1         ; S_*: the screen at the table
        VAR     SHSEQ,1         ; the arena's round seq when it was entered
        VAR     JOINTH,1        ; frames waiting for the first state
        VAR     JOINTL,1
        VAR     RCOUNT,1        ; a count for a loop around calls
; the link (p_net.inc)
        VAR     NLINK,1         ; NL_*
        VAR     NEVT,1          ; EV_*: what this frame's step brought
        VAR     NERR,1          ; an E frame's code
        VAR     NSTAT,1         ; frames to the next STATUS
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
        VAR     QUIETH,1        ; frames since a good frame
        VAR     QUIETL,1
        VAR     QPINGH,1        ; the same, mod PING_EVERY
        VAR     QPINGL,1
        VAR     QASK,1          ; and mod ASK_AGAIN
        VAR     RHOLD,1         ; b0-6 frames to the next RESYNC, b7 one owed
        VAR     DLEFT,1         ; the READ's bytes not yet decoded
        VAR     DRPOS,1         ; the reply's offset of the last one decoded
        VAR     DCNT,1          ; a chunk's bytes left
        VAR     DRPOS0,1        ; DPOS at the chunk's start
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
        VAR     DBYTE,1         ; the byte being decoded
        VAR     DOLD,1          ; a map byte's old tile
        VAR     DTILE,1         ; and its tile number
        VAR     MFLAGS,1        ; MF_*
        VAR     MAPDL,1         ; map rows patched: rows 0-7 ...
        VAR     MAPDH,1         ;  and 8-10
        VAR     TBLN,1          ; T frames: the count ($FF: none yet)
        VAR     TBLHAVE,1       ;  and the records in (a bit each)
        VAR     RAWN,1          ; the frame being sent: its length
        VAR     RAWBUF,24       ;  and its bytes (type, payload, the two sums)
        VAR     BK,1            ;  its TXF bit
        VAR     S1T,1           ;  its sums
        VAR     S2T,1
        VAR     CFROM,1         ;  a COBS run's start and end
        VAR     CI,1
        VAR     DOUT,1          ; the decoded byte the frame takes
; the arena (p_arena.inc, p_play.inc)
        VAR     MVX,1           ; our walker (src/move.c): position, direction,
        VAR     MVY,1           ;  speed accumulator, the bomb it bumped
        VAR     MVDIR,1
        VAR     MVACC,1
        VAR     MVBUMP,1
        VAR     MOVED,1         ; it moved this jiffy
        VAR     PEND,1          ; our bomb, shown before the server has it
        VAR     PENDL,1         ;  and its frames left
        VAR     BUMPTH,1        ; the frame of our last bump
        VAR     BUMPTL,1
        VAR     SENTTH,1        ; and of our last move sent
        VAR     SENTTL,1
        VAR     DISPX,4         ; where each seat is drawn
        VAR     DISPY,4
        VAR     DIET,4          ; a knocked-out seat's frames (0: none)
        VAR     MOVT,4          ; the tile each mover is drawn on ($FF none)
        VAR     WASALV,1        ; the alive mask at the last state
        VAR     LASTPH,1        ; the phase at the last state
        VAR     HUDTMR,1        ; the HUD as drawn: the timer, the alive mask,
        VAR     HUDALV,1        ;  the wins
        VAR     HUDWIN,4
        VAR     STATS,5         ; our stats as drawn
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
        VAR     TWA,1           ; toward
        VAR     TWD,1
        VAR     TFR,1           ; frame_of
        VAR     PFT,1           ; the tile under us
        VAR     PKK,1           ; we kick
        VAR     PMD,1           ; our move's dir|moving
        VAR     PJIF,1          ; the pass's jiffies (1-4)
        VAR     TWMAX,1         ;  and the others' step (3 a jiffy)
        VAR     TSTCH,1         ; the test scenes' clock
        VAR     TSTCL,1
SCRATCH EQU     RAMNEXT         ; the rest: a screen's own (the title's walkers,
        IF      SCRATCH+16 > $1B00 ;  the trophies' line)
        ERROR   "no room left for SCRATCH"
        ENDIF

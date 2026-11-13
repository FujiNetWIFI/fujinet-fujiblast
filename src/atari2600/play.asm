; play.asm -- bank 4: the input scan, the walker (src/move.c, exactly) and the
; fire button. Hopped to every frame in the overscan, before the link.
;
; The walker owns MY position: BX/BY/BDIR[MYSEAT] are mv_x, mv_y, mv_dir (the
; low 2 bits; bit 2 = moving). The decoder stores the server's idea of it in
; SRVX/SRVY, which a changed warp snaps the walker to; outside PH_PLAY the
; server's position is simply copied in.
        CPU     6502
        INCLUDE "vcs.inc"
        INCLUDE "fujinet.inc"
        INCLUDE "fbdefs.inc"
        INCLUDE "../../build/a26/tail.inc"

BUMPMASK EQU    31              ; a BUMP at most every 32 frames

        ORG     $1000
PENTRY: jsr     SOUND
        jsr     INPUT
        lda     INCUR           ; the RESET switch: start over (the socket is
        and     #IN_RESET       ;   closed at boot; the name gets the seat back)
        beq     PE_A
        lda     INPREV
        and     #IN_RESET
        beq     RESTART
PE_A:   jsr     DIRECT
        lda     LFLAGS
        and     #(~LF_PLAY)&$FF
        sta     LFLAGS
        ldx     MYSEAT
        bmi     PEND            ; not seated (yet)
        lda     SEATED
        and     SBITS,x
        beq     PEND
        lda     PHASE
        cmp     #PH_PLAY
        beq     PLAYING
        lda     SRVX            ; waiting / counting down / over: the server's
        sta     BX,x            ;   idea of where I am is the truth
        lda     SRVY
        sta     BY,x
        lda     PHASE
        cmp     #PH_COUNT
        bne     PEND
        lda     LFLAGS          ; (M frames also during the countdown: harmless)
        ora     #LF_PLAY
        sta     LFLAGS
        jmp     PEND
PLAYING:
        lda     LFLAGS
        ora     #LF_PLAY
        sta     LFLAGS
        lda     ALIVE
        and     SBITS,x
        beq     PEND            ; dead: a spectator
        jsr     WALKER
        IF      PAL
        lda     MFRAME          ; the server's jiffies are 60 a second: one
        and     #3              ;   extra step in 5 frames makes 50 Hz keep up
        bne     PL_P
        lda     MFRAME
        and     #4
        bne     PL_P
        jsr     WALKER
PL_P:
        ENDIF
        jsr     FIRE
PEND:   jsr     ROSTER
        lda     PHASE           ; INPREV bit 7: the last frame was in play
        cmp     #PH_PLAY
        bne     PE1
        lda     INPREV
        bmi     PE0
        lda     #SND_GO         ; (the countdown just ended)
        sta     SND
PE0:    lda     INCUR
        ora     #$80
        jmp     PE2
PE1:    lda     INCUR
PE2:    sta     INPREV
        lda     #ENWORK         ; on to the link
        sta     MENT
        lda     #BKNET
        jmp     MGOTO

RESTART: lda    #ENCOLD
        sta     MENT
        lda     #BKBOOT
        jmp     MGOTO

; ---------------------------------------------------------------- sound
; SOUND -- one step of the effect in play (SND is its step index; 0 = silent).
SOUND:  ldx     SND
        beq     SO9
        lda     SNDC,x
        cmp     #$FF
        beq     SO1
        pha
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        sta     AUDC0
        pla
        and     #15
        sta     AUDV0
        lda     SNDF,x
        sta     AUDF0
        inc     SND
SO9:    rts
SO1:    lda     #0
        sta     AUDV0
        sta     SND
        rts

; ---------------------------------------------------------------- director
; DIRECT -- which screen should be showing; on a change, hand the display to
; its bank and ask for the whole state again (names, map).
DIRECT: lda     MSCR
        cmp     #SC_NAME
        bne     DRA
        jmp     WHEEL
DRA:    cmp     #SC_ROOMS
        bne     DRB
        jmp     ROOMS
DRB:    lda     LFLAGS
        and     #LF_LOST
        beq     DR1
        lda     MSCR            ; the link is dead: say so, then start over
        cmp     #SC_LOST
        bne     DRL1
        inc     MSEL
        bne     DRL2
        jmp     RESTART
DRL1:   lda     #0
        sta     MSEL
DRL2:   lda     #SC_LOST
        jmp     DRSET
DR1:    lda     LFLAGS
        and     #LF_GOT
        bne     DR2
        lda     #SC_CONN
        jmp     DRSET
DR2:    lda     PHASE
        bne     DR3
        lda     #SC_WAIT
        jmp     DRSET
DR3:    cmp     #PH_ROUNDEND
        bne     DR4
        lda     #SC_ROUND
        jmp     DRSET
DR4:    cmp     #PH_GAMEOVER
        bne     DR5
        lda     #SC_OVER
        jmp     DRSET
DR5:    lda     #SC_ARENA
DRSET:  cmp     MSCR
        bne     DR6
        rts
DR6:    sta     MSCR
        cmp     #SC_ROUND
        bne     DR6A
        ldx     #SND_WIN
        stx     SND
DR6A:   cmp     #SC_OVER
        bne     DR6B
        ldx     #SND_OVER
        stx     SND
DR6B:
        cmp     #SC_ARENA
        bne     DR7
        lda     #(BKARENA<<4)|ENENTER
        jmp     DR8
DR7:    lda     #(BKSCR<<4)|ENENTER
DR8:    sta     MRET
        lda     LFLAGS          ; the new screen needs the names and the map
        and     #LF_GOT
        beq     DR9
        lda     LFLAGS
        ora     #LF_RESYNC
        sta     LFLAGS
DR9:    rts

; ---------------------------------------------------------------- name wheel
; WHEEL -- up/down turn the letter, fire adds it, left deletes, right (with at
; least two letters) is done. MSEL = the wheel's letter; MSHOW = $FF: redraw.
WHEEL:  ldx     #0              ; NLEN
WH0:    lda     NAME,x
        beq     WH1
        inx
        cpx     #8
        bne     WH0
WH1:    stx     KT0             ; the length
        lda     INCUR
        and     #IN_UP
        beq     WH2
        lda     INPREV
        and     #IN_UP
        bne     WH2
        inc     MSEL
        lda     MSEL
        cmp     #36
        bcc     WHR
        lda     #0
        sta     MSEL
        jmp     WHR
WH2:    lda     INCUR
        and     #IN_DOWN
        beq     WH3
        lda     INPREV
        and     #IN_DOWN
        bne     WH3
        dec     MSEL
        bpl     WHR
        lda     #35
        sta     MSEL
        jmp     WHR
WH3:    lda     INCUR
        and     #IN_FIRE
        beq     WH4
        lda     INPREV
        and     #IN_FIRE
        bne     WH4
        ldx     KT0
        cpx     #8
        bcs     WHR
        ldy     MSEL
        lda     WHEELCH,y
        sta     NAME,x
        jmp     WHR
WH4:    lda     INCUR
        and     #IN_LEFT
        beq     WH5
        lda     INPREV
        and     #IN_LEFT
        bne     WH5
        ldx     KT0
        beq     WHR
        dex
        lda     #0
        sta     NAME,x
        jmp     WHR
WH5:    lda     INCUR
        and     #IN_RIGHT
        beq     WHD
        lda     INPREV
        and     #IN_RIGHT
        bne     WHD
        lda     KT0
        cmp     #2
        bcc     WHD
        jmp     WHDONE
WHR:    lda     #$FF            ; something changed: draw the name again
        sta     MSHOW
WHD:    lda     MSHOW
        cmp     #$FF
        bne     WHX
        lda     #0              ; the 8 cells of row 8, cols 2-9
        sta     KT1
WHL:    ldx     KT1
        cpx     KT0
        bcs     WHL1
        lda     NAME,x          ; typed
        jmp     WHC
WHL1:   bne     WHL2            ; the candidate sits at the length
        ldy     MSEL
        lda     WHEELCH,y
        jmp     WHC
WHL2:   lda     #' '
WHC:    pha
        lda     KT1
        clc
        adc     #8*12+2
        sta     CBD
        lda     #0
        sta     CBD+1
        sta     CBC
        pla
        ldx     #0
        ldy     #FB_TCELL
        jsr     FBLIT
        inc     KT1
        lda     KT1
        cmp     #8
        bne     WHL
        lda     #0
        sta     MSHOW
WHX:    rts

; WHDONE -- the name is set: the room, or the list of them.
WHDONE: ldx     #0              ; MISM: a name byte not yet sent is a zero
        lda     #0
WD1:    ldy     NAME,x
        beq     WD2
        ora     WBIT,x
WD2:    inx
        cpx     #8
        bne     WD1
        ldx     #3
WD3:    sta     MISM,x
        dex
        bpl     WD3
        lda     #0
        sta     LREQ            ; the link may start
        sta     MSEL
        lda     TABLE
        bne     WD4
        lda     #SC_ROOMS
        jmp     DRSET
WD4:    lda     #SC_CONN
        jmp     DRSET

WBIT:   DB      1,2,4,8,16,32,64,128

; ---------------------------------------------------------------- room list
; ROOMS -- up/down move the cursor (the cursor's table id is what the decoder
; keeps in TABLE: ask for the list again), fire joins it. MSHOW = how many.
ROOMS:  lda     INCUR
        and     #IN_UP
        beq     RM1
        lda     INPREV
        and     #IN_UP
        bne     RM1
        lda     MSEL
        beq     RM1
        dec     MSEL
        jmp     RMR
RM1:    lda     INCUR
        and     #IN_DOWN
        beq     RM2
        lda     INPREV
        and     #IN_DOWN
        bne     RM2
        lda     MSHOW
        cmp     #$FF
        beq     RM2
        ldx     MSEL
        inx
        cpx     MSHOW
        bcs     RM2
        stx     MSEL
RMR:    lda     LFLAGS
        ora     #LF_LIST
        sta     LFLAGS
        rts
RM2:    lda     INCUR
        and     #IN_FIRE
        beq     RM9
        lda     INPREV
        and     #IN_FIRE
        bne     RM9
        lda     TABLE
        beq     RM9
        lda     LFLAGS
        and     #LF_CONN
        beq     RM9
        lda     LFLAGS
        ora     #LF_HELLO
        sta     LFLAGS
        lda     #SC_CONN
        jmp     DRSET
RM9:    rts

WHEELCH: DB     "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

; ROSTER -- the waiting room's controls: the host sets the rounds and starts.
ROSTER: lda     MSCR
        cmp     #SC_WAIT
        bne     RO9
        lda     ROUNDS          ; the rounds, drawn when it changes
        cmp     MSHOW
        beq     RO0
        sta     MSHOW
        clc
        adc     #'0'
        pha
        lda     #14*12+7
        sta     CBD
        lda     #0
        sta     CBD+1
        sta     CBC
        pla
        ldx     #0
        ldy     #FB_TCELL
        jsr     FBLIT
RO0:
        ldx     MYSEAT
        bmi     RO9
        cpx     HOST
        bne     RO9
        lda     LACT
        bne     RO9
        lda     INCUR
        and     #IN_FIRE
        beq     RO1
        lda     INPREV
        and     #IN_FIRE
        bne     RO1
        lda     #ACT_START      ; a press starts the game
        sta     LACT
        rts
RO1:    lda     INCUR           ; left/right set the rounds (1-9)
        and     #IN_LEFT
        beq     RO2
        lda     INPREV
        and     #IN_LEFT
        bne     RO2
        lda     ROUNDS
        cmp     #2
        bcc     RO9
        sbc     #1
        jmp     RO3
RO2:    lda     INCUR
        and     #IN_RIGHT
        beq     RO9
        lda     INPREV
        and     #IN_RIGHT
        bne     RO9
        lda     ROUNDS
        cmp     #9
        bcs     RO9
        adc     #1              ; (carry is clear)
RO3:    sta     LA1
        lda     #ACT_ROUNDS
        sta     LACT
RO9:    rts

; ---------------------------------------------------------------- input
INPUT:  lda     SWCHA
        eor     #$FF
        lsr     a
        lsr     a
        lsr     a
        lsr     a               ; bit0 up, 1 down, 2 left, 3 right
        sta     INCUR
        lda     INPT4
        bmi     IN1
        lda     INCUR
        ora     #IN_FIRE
        sta     INCUR
IN1:    lda     SWCHB
        eor     #$FF
        and     #3
        asl     a
        asl     a
        asl     a
        asl     a
        asl     a
        ora     INCUR
        sta     INCUR
        rts

; ---------------------------------------------------------------- walker
; WALKER -- one jiffy: the stick -> try_dirs; the new position into BX/BY/BDIR.
WALKER: ldx     MYSEAT
        lda     BX,x
        sta     W
        lda     BY,x
        sta     W+1
        jsr     TRYDIRS         ; WMOVED
        ldx     MYSEAT
        lda     W
        sta     BX,x
        lda     W+1
        sta     BY,x
        lda     BDIR,x
        and     #3
        sta     BDIR,x
        lda     WMOVED
        beq     WK1
        lda     BDIR,x
        ora     #4
        sta     BDIR,x
WK1:    rts

; TRYDIRS -- the stick to a direction; on a diagonal, the turn first.
TRYDIRS:
        lda     #$FF
        sta     WH
        sta     WV
        lda     INCUR
        and     #IN_UP
        beq     TD1
        lda     #DIR_UP
        sta     WV
        jmp     TD2
TD1:    lda     INCUR
        and     #IN_DOWN
        beq     TD2
        lda     #DIR_DOWN
        sta     WV
TD2:    lda     INCUR
        and     #IN_LEFT
        beq     TD3
        lda     #DIR_LEFT
        sta     WH
        jmp     TD4
TD3:    lda     INCUR
        and     #IN_RIGHT
        beq     TD4
        lda     #DIR_RIGHT
        sta     WH
TD4:    lda     WH
        cmp     #$FF
        bne     TD5
        lda     WV              ; (vertical only, or neither)
        jmp     MVSTEP
TD5:    lda     WV
        cmp     #$FF
        bne     TD6
        lda     WH
        jmp     MVSTEP
; a diagonal: a = the way we are not going, b = the other
TD6:    ldx     MYSEAT
        lda     BDIR,x
        and     #3
        cmp     #DIR_LEFT
        beq     TD7
        cmp     #DIR_RIGHT
        beq     TD7
        lda     WH              ; going up/down: a = h, b = v
        sta     WSAV+2
        lda     WV
        sta     WA
        jmp     TD8
TD7:    lda     WV              ; going sideways: a = v, b = h
        sta     WSAV+2
        lda     WH
        sta     WA
TD8:    lda     W
        sta     WSAV
        lda     W+1
        sta     WSAV+1
        lda     MACC
        pha
        lda     WSAV+2
        jsr     MVSTEP
        lda     WMOVED
        beq     TD11
        pla
        rts
TD11:   lda     WSAV            ; undo, then try b
        sta     W
        lda     WSAV+1
        sta     W+1
        pla
        sta     MACC
        lda     WA
        jmp     MVSTEP

; MVSTEP -- one jiffy toward A ($FF: still) at my speed level. WMOVED, WBUMP.
MVSTEP: ldx     #0
        stx     WMOVED
        ldx     #$FF
        stx     WBUMP
        cmp     #$FF
        bne     MV1
        lda     #0
        sta     MACC
        rts
MV1:    sta     WD
        ldx     MYSEAT
        lda     BDIR,x
        and     #4
        ora     WD
        sta     BDIR,x          ; mv_dir = d
        lda     MSPD
        asl     a
        asl     a
        clc
        adc     #16             ; SPEED_BASE + lv * SPEED_STEP
        clc
        adc     MACC
        pha
        and     #15
        sta     MACC
        pla
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        sta     KN
MV2:    lda     KN
        beq     MV9
        lda     WD
        jsr     UNITSTEP
        bcc     MV3
        lda     #1
        sta     WMOVED
        dec     KN
        jmp     MV2
MV3:    lda     #0
        sta     MACC
MV9:    rts

KN      EQU     POOL+19         ; (the unit loop's count)

; UNITSTEP -- A = d: one unit toward it; carry set if it moved.
UNITSTEP:
        sta     WD
        and     #1
        eor     #1
        sta     WP              ; the axis moved along: 0 = x (left/right), 1 = y
        eor     #1
        sta     WS              ; the other axis
        lda     #1
        ldx     WD
        cpx     #DIR_RIGHT
        beq     US1
        cpx     #DIR_DOWN
        beq     US1
        lda     #$FF
US1:    sta     WDX
        ldx     WS
        lda     W,x
        and     #15
        sta     WOFF            ; how far off the lane
        lda     W,x
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        sta     WSR             ; the other axis' tile
        ldx     WP
        lda     W,x
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        clc
        adc     WDX
        sta     WPT             ; the tile ahead along the axis (may be $FF)
        lda     WOFF
        beq     US_STRAIGHT
        cmp     #LANE+1
        bcs     US2
        lda     WPT             ; within LANE units: slide back onto the lane
        ldy     WSR
        jsr     WALK
        bcc     US2
        ldx     WS
        dec     W,x
        sec
        rts
US2:    lda     WOFF
        cmp     #TUNIT-LANE
        bcc     US_FAIL
        lda     WPT
        ldy     WSR
        iny
        jsr     WALK
        bcc     US_FAIL
        ldx     WS
        inc     W,x
        sec
        rts
US_FAIL: clc
        rts
US_STRAIGHT:
        ldx     WP
        lda     W,x
        and     #15
        bne     US_MOVE         ; between tiles: nothing in the way
        lda     WPT
        ldy     WSR
        jsr     WALK
        bcs     US_MOVE
        jsr     BUMPED          ; a bomb in the way?
        clc
        rts
US_MOVE: ldx     WP
        lda     W,x
        clc
        adc     WDX
        sta     W,x
        sec
        rts

; WALK -- A = the tile coordinate along the axis, Y = the other: carry set if
; walkable (inside the arena and no pillar, block, burning block, bomb or dud).
WALK:   ldx     WP
        bne     WK_V
        sta     WC              ; moving along x: c = A, r = Y
        sty     WR
        jmp     WK2
WK_V:   sta     WR              ; moving along y: r = A, c = Y
        sty     WC
WK2:    lda     WC
        cmp     #COLS
        bcs     WKNO
        lda     WR
        cmp     #ROWS
        bcs     WKNO
        jsr     TILEAT          ; A = the tile code
        cmp     #T_PILLAR
        bcc     WKYES
        cmp     #T_DUD+1
        bcs     WKYES
WKNO:   clc
        rts
WKYES:  sec
        rts

WC      EQU     POOL+20
WR      EQU     POOL+21

; TILEAT -- the map code at (WC, WR).
TILEAT: ldx     WR
        lda     MROWL,x
        sta     WPTR
        lda     MROWH,x
        sta     WPTR+1
        ldy     WC
        lda     (WPTR),y
        rts

; BUMPED -- the tile at (WC, WR) was in the way: a bomb or dud is remembered.
BUMPED: lda     WC
        cmp     #COLS
        bcs     BU9
        lda     WR
        cmp     #ROWS
        bcs     BU9
        jsr     TILEAT
        cmp     #T_BOMB
        beq     BU1
        cmp     #T_DUD
        bne     BU9
BU1:    lda     WR              ; its tile index: r*13 + c
        asl     a
        asl     a
        sta     WBUMP
        asl     a
        clc
        adc     WBUMP
        adc     WR
        clc
        adc     WC
        sta     WBUMP
BU9:    rts

; ---------------------------------------------------------------- fire
; FIRE -- a press drops a bomb on my tile (or throws the one I stand on, with
; the glove); a bump into a bomb kicks it (with the boots) or relights a dud.
FIRE:   lda     WBUMP
        cmp     #$FF
        beq     FR1
        lda     MFRAME
        and     #BUMPMASK
        bne     FR1
        lda     LACT
        bne     FR1
        ldx     MYSEAT
        lda     BDIR,x
        and     #3
        sta     LA2
        lda     WBUMP
        sta     LA1
        lda     #ACT_BUMP
        sta     LACT
FR1:    lda     INCUR
        and     #IN_FIRE
        beq     FR9
        lda     INPREV
        and     #IN_FIRE
        bne     FR9             ; held: not a press
        lda     LACT
        bne     FR9             ; (one action at a time)
        ldx     MYSEAT          ; my tile: the centre of the box
        lda     BX,x
        clc
        adc     #TUNIT/2
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        cmp     #COLS
        bcc     FR2
        lda     #COLS-1
FR2:    sta     WC
        lda     BY,x
        clc
        adc     #TUNIT/2
        lsr     a
        lsr     a
        lsr     a
        lsr     a
        cmp     #ROWS
        bcc     FR3
        lda     #ROWS-1
FR3:    sta     WR
        jsr     TILEAT
        pha
        lda     MFLG
        and     #2              ; the glove: throw the bomb underfoot
        beq     FR4
        pla
        cmp     #T_BOMB
        bne     FR9
        ldx     MYSEAT
        lda     BDIR,x
        and     #3
        sta     LA1
        lda     #0
        sta     LA2
        lda     #ACT_THROW
        sta     LACT
        rts
FR4:    pla
        cmp     #T_FLOOR
        bne     FR9
        lda     MOUT
        cmp     MBOMB
        bcs     FR9             ; all my bombs are out
        lda     WR              ; tile index r*13 + c
        asl     a
        asl     a
        sta     LA1
        asl     a
        clc
        adc     LA1
        adc     WR
        clc
        adc     WC
        sta     LA1
        lda     #0
        sta     LA2
        lda     #ACT_DROP
        sta     LACT
        lda     #SND_DROP
        sta     SND
FR9:    rts

ACT_DROP EQU    1
ACT_THROW EQU   2
ACT_BUMP EQU    3
ACT_ROUNDS EQU  4
ACT_START EQU   5
ACT_LEAVE EQU   6

        INCLUDE "blit.inc"

DSTART:
SBITS:  DB      1,2,4,8
        INCLUDE "../../build/a26/maprows.inc"
        INCLUDE "../../build/a26/snd.inc"
        END

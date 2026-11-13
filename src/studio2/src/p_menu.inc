; p_menu.inc -- the screens before a table (src/menus.c): the title, the
; name (a wheel on the keypad: 2 and 8 turn it, 5 or 6 takes a letter, 4
; rubs one out, '>' then 5 is done), the rooms.

; ---------------------------------------------------------------------------
; title -- the logo, the four bombers walking, PRESS 5.
; ---------------------------------------------------------------------------
TT_Y    EQU     56              ; the bombers' line

        RFIT    238
title:  OPN     TOP_CLS
        HOLD1
        LDI     0                       ; the logo: 8 stamps
        PLO     R9
tl1:    PARS
        GLO     R9
        ADI     IM_LOGO0
        PD
        GLO     R9
        SHL
        SHL
        SHL
        PD
        P       4
        P       TM_SET
        GOP     TOP_STAMP
        INC     R9
        GLO     R9
        XRI     8
        BNZ     tl1
        SETV    TXM, TM_SET
        LDR     R8, s_for
        LDI     32
        PHI     R9
        FCALL   txt_ctr
        LDR     R8, s_press5
        LDI     84
        PHI     R9
        FCALL   txt_ctr
        SETV    TXM, TM_SET|TM_SHORT
        LDR     R8, s_tag
        LDI     104
        PHI     R9
        FCALL   txt_ctr
        LDI     0                       ; the bombers' sprites: slot s, seat s
        PLO     R9
tl2:    PARS
        GLO     R9
        PD
        GLO     R9
        ADI     IM_SEAT
        PD
        P       HIDDEN
        P       TT_Y
        P       TM_SET
        GOP     TOP_SPRITE
        INC     R9
        GLO     R9
        XRI     SEATS
        BNZ     tl2
        HOLD0
        SND     MUS_TITLE
        SETV    KNEW, 0
        LDR     R8, SFS                 ; no frames uploaded yet
        LDI     NONE
        STR     R8
        INC     R8
        STR     R8
        INC     R8
        STR     R8
        INC     R8
        STR     R8
tl3:    FCALL   frame
        FCALL   in_poll
        FCALL   snd_tick
        FCALL   tl_walk
        GETV    KNEW
        ANI     KB_FIRE
        BZ      tl3
        SETV    KNEW, 0
        OPN     TOP_SPRHIDE
        FRET

; tl_walk: each bomber at x = (JIFFY / 4 + 16 s) mod 64 - 4, walking right.
        RFIT    141
tl_walk:
        HOLD1
        LDI     0
        PLO     R9
tw1:    GETV    JIFFY                   ; the frame: right, walking
        SHR
        SHR
        SHR
        ANI     1
        BZ      tw2
        GLO     R9
        ANI     1
        ADI     1
tw2:    ADI     FR_RIGHT0
        PHI     R9
        VARX    SFS
        LDN     RA
        STR     R2
        GHI     R9
        XOR
        BZ      tw3
        GHI     R9
        STR     RA
        LCALL   seat_up
tw3:    PARS
        GLO     R9
        PD
        GETV    JIFFY
        SHR
        SHR
        STR     R2
        GLO     R9
        SHL
        SHL
        SHL
        SHL
        ADD
        ANI     63
        SMI     4
        PD
        P       TT_Y
        GOP     TOP_SPRMOVE
        INC     R9
        GLO     R9
        XRI     SEATS
        BNZ     tw1
        HOLD0
        FRET

s_for:  DB      "FOR FUJINET", 0
s_tag:  DB      "2-4 PLAYERS", 0

; ---------------------------------------------------------------------------
; name_entry -- the wheel: letters, digits, then the end ('>').
; ---------------------------------------------------------------------------
NE_Y    EQU     46

        RFIT    128
name_entry:
        OPN     TOP_CLS
        SETV    TXM, TM_SET
        LDR     R8, s_name
        LDI     16
        PHI     R9
        FCALL   txt_ctr
        SETV    TXM, TM_SET|TM_SHORT
        LDR     R8, s_nh1
        LDI     84
        PHI     R9
        FCALL   txt_ctr
        LDR     R8, s_nh2
        LDI     94
        PHI     R9
        FCALL   txt_ctr
        LDR     R8, MYNAME
        LDI     9
        PLO     R9
ne0:    LDI     0
        STR     R8
        INC     R8
        DEC     R9
        GLO     R9
        BNZ     ne0
        SETV    MENUSEL, 0              ; the wheel
        SETV    MENUN, 0                ; the letters taken
        SETV    KEV, NONE
ne1:    FCALL   ne_draw
ne2:    FCALL   frame
        FCALL   in_poll
        FCALL   snd_tick
        GETV    KEV
        XRI     NONE
        BZ      ne2
        LDN     RA
        PLO     R9
        SETV    KEV, NONE
        GLO     R9
        FCALL   ne_key
        BZ      ne1
        FRET

; ne_key: digit D. D = 0 to go on, else the name is done.
        RFIT    134
ne_key: XRI     2
        BNZ     nk1
        GETV    MENUSEL                 ; 2: back a letter, round
        BNZ     nk0
        LDI     37
nk0:    SMI     1
        BR      nks
nk1:    XRI     2!8
        BNZ     nk2
        GETV    MENUSEL                 ; 8: on a letter, round
        ADI     1
        XRI     37
        BZ      nks
        XRI     37
nks:    PUTV    MENUSEL
        SND     SND_SELECT
        LDI     0
        FRET
nk2:    XRI     8!4
        BNZ     nk3
        GETV    MENUN                   ; 4: rub one out
        BZ      nk9
        SMI     1
        STR     RA
        ADI     LO(MYNAME)
        PLO     RA
        LDI     0
        STR     RA
        BR      nk9
nk3:    XRI     4!5
        BZ      nk4
        XRI     5!6
        BNZ     nk9
nk4:    GETV    MENUSEL                 ; 5 or 6: take it
        XRI     36
        BNZ     nk5
        GETV    MENUN                   ; the end: two letters at least
        SMI     2
        BNF     nk9
        LDI     1
        FRET
nk5:    GETV    MENUN
        XRI     NAME_LEN
        BZ      nk9
        GETV    MENUSEL
        ADI     LO(WHEEL)
        PLO     R8
        LDI     HI(WHEEL)
        PHI     R8
        LDN     R8
        PHI     R8
        GETV    MENUN
        ADI     1
        STR     RA
        ADI     LO(MYNAME)-1
        PLO     RA
        GHI     R8
        STR     RA
        SND     SND_SELECT
nk9:    LDI     0
        FRET

; ne_draw: the name so far, the wheel's letter after it, inverted.
        RFIT    138
ne_draw:
        PARS                            ; the line cleared
        P       0
        P       NE_Y
        P       64
        P       12
        P       TM_CLEAR|TP_SOLID
        GOP     TOP_RECT
        SETV    TXM, TM_SET
        LDR     R8, MYNAME
        LDI     16
        PLO     R9
        LDI     NE_Y
        PHI     R9
        FCALL   txt_at16
        GETV    MENUN                   ; the wheel's letter at 16 + 4 n
        XRI     NAME_LEN
        BZ      nd9
        LDN     RA
        SHL
        SHL
        ADI     16
        PLO     R9
        PARS
        GLO     R9
        PD
        P       NE_Y
        P       TM_SET
        GETV    MENUSEL
        ADI     LO(WHEEL)
        PLO     R8
        LDI     HI(WHEEL)
        PHI     R8
        LDN     R8
        PD
        GOP     TOP_PTEXT
        PARS
        GLO     R9
        PD
        P       NE_Y
        P       4
        P       12
        P       TM_XOR|TP_SOLID
        GOP     TOP_RECT
nd9:    FRET

        PFIT    38
WHEEL:  DB      "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789>"
s_name: DB      "YOUR NAME?", 0
s_nh1:  DB      "2 8 TURN 5 TAKE", 0
s_nh2:  DB      "4 BACK  > 5 DONE", 0

; ---------------------------------------------------------------------------
; rooms -- the server's tables (the socket is open): asked for every 60
;   jiffies until all have come, 600 at most; then a list: 2 and 8 move, 5
;   joins. D = 1: SRVTBL chosen; 0: no answer (or the link went).
; ---------------------------------------------------------------------------
        RFIT    118
rooms:  SETV    JOINT, 0                ; jiffies asking
        SETV    JOINT+1, 0
        SETV    T7, 0                   ; to the next ask
rm1:    FCALL   frame
        FCALL   in_poll
        FCALL   snd_tick
        GETV    DT
        STR     R2
        GETV    T7
        SM
        STR     RA
        BZ      rm2
        BDF     rm3
rm2:    SETV    T7, 60
        GETV    TXF
        ORI     TX_TABLES
        STR     RA
rm3:    FCALL   nstep
        GETV    NEVT
        ANI     EV_LOST
        BNZ     rm0
        GETV    TBLN                    ; all in?
        XRI     NONE
        BZ      rm4
        LDN     RA
        BZ      rm0                     ; (no tables)
        ADI     LO(MASKS)
        PLO     R8
        LDI     HI(MASKS)
        PHI     R8
        LDN     R8
        STR     R2
        GETV    TBLHAVE
        XOR
        BZ      rm5
rm4:    GETV    DT                      ; 600 jiffies at most
        STR     R2
        GETV    JOINT
        ADD
        STR     RA
        INC     RA
        LDN     RA
        ADCI    0
        STR     RA
        SMI     HI(600)
        BNF     rm1
rm0:    LDI     0
        FRET
rm5:    FJMP    rm_list

        PFIT    9
MASKS:  DB      0, 1, 3, 7, 0FH, 1FH, 3FH, 7FH, 0FFH

; rm_list: the list, and the choice.
        RFIT    199
rm_list:
        OPN     TOP_CLS
        SETV    TXM, TM_SET
        LDR     R8, s_pick
        LDI     0
        PHI     R9
        FCALL   txt_ctr
        SETV    TXM, TM_SET|TM_SHORT
        LDR     R8, s_pickh
        LDI     112
        PHI     R9
        FCALL   txt_ctr
        SETV    TXM, TM_SET
        SETV    GI, 0                   ; each: its name, seated/max
rl1:    GETV    GI
        SHL
        SHL
        SHL
        PLO     R8
        SHL
        STR     R2
        GLO     R8
        ADD
        ADI     LO(TBLS)+8
        PLO     R8
        LDI     HI(TBLS)
        PHI     R8                      ; its name (12 at most: 10 shown)
        GETV    GI
        FCALL   rl_y
        PHI     R9
        LDI     4
        PLO     R9
        LDI     10
        PLO     RC
        FCALL   txt_at
        GETV    GI                      ; seated, '/', max
        SHL
        SHL
        SHL
        PLO     R8
        SHL
        STR     R2
        GLO     R8
        ADD
        ADI     LO(TBLS)+20
        PLO     R8
        LDI     HI(TBLS)
        PHI     R8
        LDA     R8
        ADI     '0'
        PUTV    NUMB
        LDI     '/'
        PUTV    NUMB+1
        LDN     R8
        ADI     '0'
        PUTV    NUMB+2
        SETV    NUMB+3, 0
        GETV    GI
        FCALL   rl_y
        PHI     R9
        LDI     52
        PLO     R9
        LDR     R8, NUMB
        FCALL   txt_at16
        INCV    GI
        STR     R2
        GETV    TBLN
        XOR
        BNZ     rl1
        SETV    MENUSEL, 0
        SETV    KEV, NONE
        SETV    KNEW, 0
        FJMP    rl2

; rm_list, the choice: 2 and 8 move, 5 joins.
        RFIT    116
rl2:    FCALL   rl_cursor
rl3:    FCALL   frame
        FCALL   in_poll
        FCALL   snd_tick
        FCALL   nstep                   ; (the link kept alive)
        GETV    NEVT
        ANI     EV_LOST
        BNZ     rl9
        GETV    KNEW
        ANI     KB_FIRE
        BNZ     rl8
        GETV    KEV
        XRI     NONE
        BZ      rl3
        LDN     RA
        PLO     R9
        SETV    KEV, NONE
        FCALL   rl_cursor               ; (off)
        GLO     R9
        XRI     2
        BNZ     rl4
        GETV    MENUSEL
        BZ      rl2
        SMI     1
        STR     RA
        SND     SND_SELECT
        BR      rl2
rl4:    GLO     R9
        XRI     8
        BNZ     rl2
        GETV    MENUSEL
        ADI     1
        STR     R2
        GETV    TBLN
        SD
        BZ      rl2
        GETV    MENUSEL
        ADI     1
        STR     RA
        SND     SND_SELECT
        BR      rl2
rl8:    SETV    KNEW, 0                 ; 5: that one
        FJMP    rl_take
rl9:    LDI     0
        FRET

; rl_take: SRVTBL = the chosen table's id. D = 1.
        RFIT    37
rl_take:
        GETV    MENUSEL
        SHL
        SHL
        SHL
        PLO     R8
        SHL
        STR     R2
        GLO     R8
        ADD
        ADI     LO(TBLS)
        PLO     R8
        LDI     HI(TBLS)
        PHI     R8
        LDI     LO(SRVTBL)
        PLO     RA
        LDI     TABLE_LEN
        PLO     R9
rlt1:   LDA     R8
        STR     RA
        INC     RA
        DEC     R9
        GLO     R9
        BNZ     rlt1
        LDI     0
        STR     RA
        LDI     1
        FRET

; rl_y: D = the line of row D (14 + 12 row: 8 rows to 110).
        RFIT    8
rl_y:   SHL
        SHL
        STR     R2
        SHL
        ADD
        ADI     14
        FRET

; rl_cursor: the '>' at the choice flipped (XOR: on, then off).
        RFIT    57
rl_cursor:
        GETV    MENUSEL
        FCALL   rl_y
        PHI     R9
        PARS
        P       0
        GHI     R9
        PD
        P       TM_XOR
        P       '>'
        GOP     TOP_PTEXT
        FRET

s_pick: DB      "PICK A ROOM", 0
s_pickh: DB     "2 8 PICK 5 JOIN", 0

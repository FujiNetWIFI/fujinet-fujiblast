; input.inc -- the two keypads, merged (either pad plays).
;
; OUT 2 latches a key number (0-9, the low nibble of M(R(X))) into the
; console's CD4515; EF3 then reads that key on pad A, EF4 on pad B. OUT is a
; memory READ of M(R(X)), so R(X) points at console RAM (KTMP), never at the
; cart. Each key is read on its own, so a held way and 5 are both seen.
;
;   2 4 6 8     the ways (1 3 7 9 the diagonals)
;   5           FIRE: drop a bomb (a throw with the glove); pick, start
;   0           MENU: leave the table (at the table), back (in menus)
;
; KHELD the KB_* bits held now; KNEW the ones pressed since a screen took
; them (it clears them); KDIG the lowest digit held (NONE: none); KEV a digit
; just pressed or repeating (menus, the name), NONE when taken.

KEY_DELAY       EQU     24      ; jiffies before a held key repeats
KEY_RATE        EQU     6       ; jiffies between repeats

; in_poll: every pass: the ten keys, unrolled (a key not held costs 9
; instructions). R8.0 the bits held, RC.0 the lowest digit held. Clobbers
; R8, R9, RC.
        RFIT    247
in_poll:
        LDR     R9, KTMP
        LDI     0
        PLO     R8
        LDI     NONE
        PLO     RC
        SEX     R9
        LDI     0                       ; key 0
        STR     R9
        OUT     2
        DEC     R9
        B3      ikh0
        B4      ikh0
        BR      ikn0
ikh0:  GLO     RC
        XRI     NONE
        BNZ     ikb0
        LDI     0
        PLO     RC
ikb0:  GLO     R8
        ORI     KB_MENU
        PLO     R8
ikn0:
        LDI     1                       ; key 1
        STR     R9
        OUT     2
        DEC     R9
        B3      ikh1
        B4      ikh1
        BR      ikn1
ikh1:  GLO     RC
        XRI     NONE
        BNZ     ikb1
        LDI     1
        PLO     RC
ikb1:  GLO     R8
        ORI     KB_UP|KB_LEFT
        PLO     R8
ikn1:
        LDI     2                       ; key 2
        STR     R9
        OUT     2
        DEC     R9
        B3      ikh2
        B4      ikh2
        BR      ikn2
ikh2:  GLO     RC
        XRI     NONE
        BNZ     ikb2
        LDI     2
        PLO     RC
ikb2:  GLO     R8
        ORI     KB_UP
        PLO     R8
ikn2:
        LDI     3                       ; key 3
        STR     R9
        OUT     2
        DEC     R9
        B3      ikh3
        B4      ikh3
        BR      ikn3
ikh3:  GLO     RC
        XRI     NONE
        BNZ     ikb3
        LDI     3
        PLO     RC
ikb3:  GLO     R8
        ORI     KB_UP|KB_RIGHT
        PLO     R8
ikn3:
        LDI     4                       ; key 4
        STR     R9
        OUT     2
        DEC     R9
        B3      ikh4
        B4      ikh4
        BR      ikn4
ikh4:  GLO     RC
        XRI     NONE
        BNZ     ikb4
        LDI     4
        PLO     RC
ikb4:  GLO     R8
        ORI     KB_LEFT
        PLO     R8
ikn4:
        LDI     5                       ; key 5
        STR     R9
        OUT     2
        DEC     R9
        B3      ikh5
        B4      ikh5
        BR      ikn5
ikh5:  GLO     RC
        XRI     NONE
        BNZ     ikb5
        LDI     5
        PLO     RC
ikb5:  GLO     R8
        ORI     KB_FIRE
        PLO     R8
ikn5:
        LDI     6                       ; key 6
        STR     R9
        OUT     2
        DEC     R9
        B3      ikh6
        B4      ikh6
        BR      ikn6
ikh6:  GLO     RC
        XRI     NONE
        BNZ     ikb6
        LDI     6
        PLO     RC
ikb6:  GLO     R8
        ORI     KB_RIGHT
        PLO     R8
ikn6:
        LDI     7                       ; key 7
        STR     R9
        OUT     2
        DEC     R9
        B3      ikh7
        B4      ikh7
        BR      ikn7
ikh7:  GLO     RC
        XRI     NONE
        BNZ     ikb7
        LDI     7
        PLO     RC
ikb7:  GLO     R8
        ORI     KB_DOWN|KB_LEFT
        PLO     R8
ikn7:
        LDI     8                       ; key 8
        STR     R9
        OUT     2
        DEC     R9
        B3      ikh8
        B4      ikh8
        BR      ikn8
ikh8:  GLO     RC
        XRI     NONE
        BNZ     ikb8
        LDI     8
        PLO     RC
ikb8:  GLO     R8
        ORI     KB_DOWN
        PLO     R8
ikn8:
        LDI     9                       ; key 9
        STR     R9
        OUT     2
        DEC     R9
        B3      ikh9
        B4      ikh9
        BR      ikn9
ikh9:  GLO     RC
        XRI     NONE
        BNZ     ikb9
        LDI     9
        PLO     RC
ikb9:  GLO     R8
        ORI     KB_DOWN|KB_RIGHT
        PLO     R8
ikn9:
        SEX     R2
        FJMP    in_post

; in_poll, the rest: KHELD, KNEW, KDIG, KEV from R8.0 and RC.0.
        RFIT    107
in_post:
        GETV    KHELD                   ; KNEW |= held & ~before
        PUTV    KPREV
        GLO     R8
        PUTV    KHELD
        GETV    KPREV
        XRI     0FFH
        STR     R2
        GLO     R8
        AND
        STR     R2
        GETV    KNEW
        OR
        STR     RA
        GLO     RC                      ; KDIG: a new one is an event
        STR     R2
        GETV    KDIG
        XOR
        BZ      inp5
        GLO     RC
        STR     RA
        XRI     NONE
        BZ      inp9
        GLO     RC
        PUTV    KEV
        GETV    JIFFY
        ADI     KEY_DELAY
        PUTV    KREPT
        FRET
inp5:   GLO     RC                      ; the same held: repeat it
        XRI     NONE
        BZ      inp9
        GETV    KREPT                   ; JIFFY - KREPT, signed: not yet?
        STR     R2
        GETV    JIFFY
        SM
        ANI     80H
        BNZ     inp9
        GETV    JIFFY
        ADI     KEY_RATE
        PUTV    KREPT
        GLO     RC
        PUTV    KEV
inp9:   FRET


; macros.inc -- the macros every module uses. Included before any code:
; zmac resolves forward labels and EQUs, never forward macros.

; PAGEHDR: every page's first bytes: the 55H sentinel, the BIOS's menu
; link, the name, the start (RSTENT: page 0 again, a cold start), so a
; console RESET with the page mapped comes home. Its entries follow (E_*).
PAGEHDR MACRO
        DB      55H
        DW      MENUST
        DW      RNAME
        DW      RSTENT
        ENDM

; FAR: a routine in another page (FARCALL: the caller's page comes back).
FAR     MACRO   pg, adr
        CALL    FARCALL
        DB      pg
        DW      adr
        ENDM

; GOPAGE: entry ent (E_*) of page pg (PGCALL: the caller's page comes back).
GOPAGE  MACRO   pg, ent
        LD      A,pg
        LD      HL,ent
        CALL    PGCALL
        ENDM

; SND: queue a sound or jingle (SND_*, MUS_*).
SND     MACRO   id
        LD      A,id
        CALL    SNDQUE
        ENDM

; TXT: the string at adr, line lin, column cl, colour pair pair.
TXT     MACRO   adr, lin, cl, pair
        LD      HL,adr
        LD      DE,(lin)*256+(cl)
        LD      C,pair
        CALL    TXTAT
        ENDM

; XP: an expander colour pair: fg on bg (colour indexes 0-3).
XP      MACRO   fg, bg
        LD      C,(fg)*4+(bg)
        ENDM

; FOOTB: the footer's bands under the picture (lines 84-101: the border, in
; colour 1 both sides, an ember gradient three lines a band); a band table's
; last entries, after a BLINE LINES. (look.inc FOOT0-8.)
FOOTB   MACRO
        DB      FOOT+40,0,0,FOOT0,FOOT0
        BLINE   LINES+3
        DB      FOOT+40,0,0,FOOT2,FOOT2
        BLINE   LINES+6
        DB      FOOT+40,0,0,FOOT4,FOOT4
        BLINE   LINES+9
        DB      FOOT+40,0,0,FOOT5,FOOT5
        BLINE   LINES+12
        DB      FOOT+40,0,0,FOOT6,FOOT6
        BLINE   LINES+15
        DB      FOOT+40,0,0,FOOT8,FOOT8
        ENDM

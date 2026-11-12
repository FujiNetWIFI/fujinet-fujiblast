; macros.inc -- the macros every module uses. Included before any code:
; zmac resolves forward labels and EQUs, never forward macros.

; PAGEHDR: every page's first byte: its number (C000H says which is in).
; Its entries follow (E_*, equ.inc).
PAGEHDR MACRO   n
        DB      n
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

; TXT: the string at adr, row rw, column cl, colour byte col (fg*16 + bg).
TXT     MACRO   adr, rw, cl, col
        LD      HL,adr
        LD      DE,(rw)*256+(cl)
        LD      C,col
        CALL    TXTAT
        ENDM

; PAD4: four NOPs (16 cycles): between two OUTs to the VDP with the display
; on, which need 29 cycles at the least.
PAD4    MACRO
        NOP
        NOP
        NOP
        NOP
        ENDM

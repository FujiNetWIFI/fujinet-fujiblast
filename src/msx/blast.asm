; blast.asm -- the MSX's memory map: an ASCII8 megaROM (tools/msx/mkrom.py)
;
;   4000h-9FFFh     banks 0-2, fixed: the program (code and constants)
;   A000h-BFFBh     the window: bank 3 is the art of the arena (ART1), bank 4
;                   the rest of it (ART2: src/tms9918/gfx.c switches);
;                   BFFCh-BFFFh are the FujiNet's registers in every bank
;   C000h ..        RAM: the variables, then the stack down from HIMEM

        PUBLIC  WIN_SIZE
        defc    WIN_ORG = $A000
        defc    WIN_SIZE = $1FFC

        SECTION ART1
        org     WIN_ORG
        SECTION ART2
        org     WIN_ORG

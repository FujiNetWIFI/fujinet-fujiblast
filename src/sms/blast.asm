; blast.asm -- the SMS's memory map: a Sega-mapper image (tools/sms/mkrom.py)
; (from M.U.L.E.'s src/sms/mule.asm)
;
;   0000h-7FD7h     banks 0 and 1, fixed in slots 0 and 1: the program
;                   (z88dk's crt0, the code, the constants)
;   7FD8h-7FFFh     the "FUJI" claim (7FDCh) and the SEGA header (7FF0h)
;   8000h-AFFFh     slot 2: bank 2 (the power-on one) is the arena's art
;                   (ART1: gen/sms/gfxart1.c), bank 3 the menus' (ART2)
;   B000h-BFFFh     the FujiNet cartridge's mailbox, over slot 2 whatever
;                   bank is in it (so each bank's last 4K is never seen)
;   C000h-DFEFh     RAM: the variables, then the stack down from DFF0h

        PUBLIC  WIN_SIZE
        defc    WIN_ORG = $8000
        defc    WIN_SIZE = $3000

        SECTION ART1
        org     WIN_ORG
        SECTION ART2
        org     WIN_ORG

; Slot 2's bank switch (gfx.c): its own LD (FFFFh),A, which is what the
; cartridge counts to tell a Sega mapper's image from others
; (tools/sms/mkrom.py checks it still would). sms_slot2 takes the bank in
; L (__z88dk_fastcall).

        SECTION code_user

        PUBLIC  _sms_slot2

_sms_slot2:
        ld      a,l
        ld      ($FFFF),a
        ret

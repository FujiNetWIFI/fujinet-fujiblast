; fbsel.asm -- FBSEL.COM, the last line of the FujiBlast disk's AUTOEXEC.BAT
; (the Makefile's msdos/disk-post): types FUJIBVGA (a VGA or MCGA: the
; 256-colour build, src/msdos/vga) or FUJIBLST (anything else: CGA, EGA,
; PCjr, Tandy) into the BIOS's keyboard buffer and ends, so COMMAND.COM runs
; the game as if typed, with nothing of this left in memory and nothing of
; the batch file left to run after it. From M.U.L.E.'s tools/dos/mulesel.asm.
;
; A VGA or MCGA BIOS answers INT 10h AX=1A00h with AL=1Ah and the active
; display in BL: 07h/08h a VGA (mono/colour), 0Bh/0Ch an MCGA (mono/colour).
; An older BIOS leaves AL alone.
.8086
                NAME    fbsel

_TEXT           SEGMENT BYTE PUBLIC 'CODE'
                ASSUME  CS:_TEXT, DS:_TEXT, ES:NOTHING
                ORG     100h
start:          cld
                mov     si, offset cga
                mov     ax, 1A00h
                int     10h
                cmp     al, 1Ah
                jne     sel
                cmp     bl, 07h
                je      vga
                cmp     bl, 08h
                je      vga
                cmp     bl, 0Bh
                je      vga
                cmp     bl, 0Ch
                jne     sel
vga:            mov     si, offset vganame
sel:            mov     ax, 40h
                mov     es, ax
                mov     di, 1Eh                 ; the buffer, 001E..003D
                cli
                mov     es:[1Ah], di            ; its head
key:            lodsb
                or      al, al
                jz      done
                xor     ah, ah
                cmp     al, 0Dh
                jne     put
                mov     ah, 1Ch                 ; ENTER's scan code
put:            stosw
                jmp     key
done:           mov     es:[1Ch], di            ; its tail
                sti
                mov     ax, 4C00h
                int     21h
cga             DB      'FUJIBLST', 0Dh, 0
vganame         DB      'FUJIBVGA', 0Dh, 0
_TEXT           ENDS
                END     start

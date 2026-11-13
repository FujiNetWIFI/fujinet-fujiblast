; ---------------------------------------------------------------------------
; equ.inc -- the console, the cart, the screen's layout.
;
; The cart is fujinet-firmware pico/channelf at protocol 2 (APPBANK pages and
; the tick, fuji_mailbox.h): M.U.L.E.'s clients/channelf equ.inc. The
; server's numbers are build/proto.inc (src/proto.h).
; ---------------------------------------------------------------------------

; --- the BIOS --------------------------------------------------------------
PUSHK   EQU 00107H              ; push K onto the K-stack (clobbers r7, A)
POPK    EQU 0011EH              ; pop it back
; The BIOS's K-stack: KU, KL at the scratch register r59 points to, r59 += 2.
; The BIOS starts it at r40 (9 levels); nothing here uses r16-r39, so it
; starts at r16: 21 levels (M.U.L.E.: a 10th level at r40 overwrote r59).
KSTK0   EQU 16
BCLRSCR EQU 000D0H              ; clear the screen to value r3 >> 6, palette 0

; --- the cart: its status page ($FC00) -------------------------------------
FNACKSQ EQU 0FC00H              ; echoes SEQ when the reply is ready
FNSTATE EQU 0FC01H
FNERR   EQU 0FC02H              ; FN_ERR_* of the last transaction
FNRCMD  EQU 0FC03H              ; $06 ACK / $15 NAK
FNRXLO  EQU 0FC04H              ; reply length, little-endian
FNRXHI  EQU 0FC05H
FNMAG0  EQU 0FC09H              ; 'F'
FNMAG1  EQU 0FC0AH              ; 'N'
FNPVER  EQU 0FC0BH              ; 2: APPBANK and the tick
FNTICK  EQU 0FC0EH              ; 60 Hz count, LE: read HI, LO, HI

FNREPLY EQU 0F800H              ; the whole 1K reply, read in place

; --- console -> cart: stores ------------------------------------------------
FNREG   EQU 0FD00H              ; store V at +n: register n = V
FNBANK  EQU 0FD80H              ; store anything at +n: page n at $4800
FNTX    EQU 0FF00H              ; store anywhere in the page: append

FR_DEV  EQU 000H
FR_CMD  EQU 001H
FR_NPAR EQU 002H
FR_DRST EQU 005H
FR_SEQ  EQU 010H

FD_FUJI EQU 070H
FD_NET  EQU 071H

; N: commands (fujinet-firmware NDevice)
NC_OPEN EQU 04FH                ; 'O'
NC_CLOS EQU 043H                ; 'C'
NC_READ EQU 052H                ; 'R'
NC_WRIT EQU 057H                ; 'W'
NC_STAT EQU 053H                ; 'S'
NM_RW   EQU 00CH                ; open mode: read and write

; appkeys (the FujiNet lobby's: creator 1, app 1)
FC_AKOPN EQU 0DCH               ; OPEN_APPKEY
FC_AKRD EQU 0DDH                ; READ_APPKEY
FC_AKWR EQU 0DEH                ; WRITE_APPKEY

; --- the page window -------------------------------------------------------
PGWIN   EQU 04800H
PGSIZE  EQU 02000H

; --- the console's I/O ports -----------------------------------------------
; port 0: bits 6,5 the VRAM write strobe ($60 then $50); bits 0-3 the
;         console buttons when read (bit 6 clear)
; port 1: pixel value in bits 7-6 (complemented); the right controller when read
; port 4: column (complemented); the left controller when read
; port 5: row in bits 5-0 (complemented); bits 7-6 the tone (SNDB holds them)

; --- pixel values as port-1 codes: what PIX holds ---------------------------
CV0     EQU 0C0H                ; value 0: the row's background
CV1     EQU 080H                ; 1: blue (white in palette 0)
CV2     EQU 040H                ; 2: red
CV3     EQU 000H                ; 3: green

; --- the arena on the screen (tools/chf/drawart_chf.py has the same) --------
TW      EQU 6                   ; a tile, pixels
TH      EQU 5
AX0     EQU 8                   ; the arena's top-left pixel
AY0     EQU 5
AX1     EQU AX0+13*TW           ; first column past it (the right wall)
AY1     EQU AY0+11*TH           ; first row past it
PANX    EQU 88                  ; the panel's first text column
PALCOL  EQU 125                 ; the palette's two columns

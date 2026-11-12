; bands.inc -- the screens' band tables and kept colours (resident: the raster
; handler reads them whatever page is mapped). An entry: HORCB, R3, R2, R1,
; L1, the next band's scanline (BLINE) or VBLLN after the last (equ.inc).

; a plain screen: one band, all of it the right palette; white on black
B_PLAIN: DB     0,07H,05H,03H,00H,VBLLN
P_PLAIN: DB     00H,00H,00H,00H         ; L0, L2, L3, R0

; ---- the arena -------------------------------------------------------------------------
; Lines 0-6 the HUD (all of it the arena's palette: the seats' icons look as
; their bombers do), then bands down the arena: bytes 0-27 (the walls, the
; arena) the left palette, bytes 28-39 (the stats panel) the right, which
; each of the powers' rows colours its own way (its icon and figure in 4
; new colours). Rows that look alike share a band: each interrupt costs
; about 300 cycles. (A look with FLOORB would need a band a row again.)
P_ARENA: DB     C_DARK,C_WARM,C_BRIGHT,C_DARK   ; L0, L2, L3, R0
ARH     EQU     40              ; HORCB: all of the line left
ARP     EQU     PANB            ; bytes from 28 right
B_ARENA: DB     ARH,C_DARK,C_DARK,C_DARK,C_HUD
        BLINE   ATOP
        DB      ARP,07H,76H,05H,C_FLOOR         ; rows 0-2: the clock, the round
        BLINE   ATOP+21
        DB      ARP,76H,52H,07H,C_FLOOR         ; row 3: fire
        BLINE   ATOP+28
        DB      ARP,06H,6BH,07H,C_FLOOR         ; row 4: bombs
        BLINE   ATOP+35
        DB      ARP,0D7H,0D3H,07H,C_FLOOR       ; row 5: speed
        BLINE   ATOP+42
        DB      ARP,7FH,6BH,07H,C_FLOOR         ; row 6: kick
        BLINE   ATOP+49
        DB      ARP,07H,4CH,07H,C_FLOOR         ; row 7: glove
        BLINE   ATOP+56
        DB      ARP,07H,76H,05H,C_FLOOR         ; rows 8-10: "ROUND n"
        IF      FOOTER
        BLINE   LINES
        FOOTB
        ENDIF
        DB      VBLLN

; ---- the text screens -------------------------------------------------------------------
; All of a line the right palette (HORCB 0): R0 the ground (set by SETPAL),
; R1-R3 a band's own. Text is fg on colour 0 (XP 1-3,0).
P_TEXT: DB      C_DARK,C_WARM,C_BRIGHT,C_DARK   ; L0, L2, L3, R0

; the title: the logo (lines 4-19) a gradient two lines a band, white
; highlights and an ember-dark shadow; the bombers' lane (lines 50-56) the
; arena's colours
TLOGO   EQU     4
TLANEY  EQU     50
B_TITLE: DB     0,07H,76H,05H,0
        BLINE   TLOGO
        DB      0,07H,7EH,49H,0
        BLINE   TLOGO+2
        DB      0,07H,7DH,49H,0
        BLINE   TLOGO+4
        DB      0,7FH,75H,49H,0
        BLINE   TLOGO+6
        DB      0,7FH,6CH,49H,0
        BLINE   TLOGO+8
        DB      0,7EH,6BH,49H,0
        BLINE   TLOGO+10
        DB      0,76H,63H,41H,0
        BLINE   TLOGO+12
        DB      0,75H,5AH,41H,0
        BLINE   TLOGO+14
        DB      0,6CH,52H,41H,0
        BLINE   TLOGO+16
        DB      0,07H,0D5H,76H,0                ; FOR FUJINET, 2-4 PLAYERS
        BLINE   TLANEY
        DB      0,C_BRIGHT,C_WARM,C_FLOOR,0     ; the lane
        BLINE   TLANEY+TXH
        DB      0,07H,76H,4CH,0                 ; PRESS FIRE, the tag
        DB      VBLLN

; a message: white, yellow, cyan on black
B_MSG:  DB      0,07H,76H,0D5H,0,VBLLN

; the waiting room and the trophies: rows 0-1 the heading; each seat's row
; (2, 4, 6, 8) a strip of its own colour (R1) under its bomber and name; the
; rows under them (the trophies) gold; rows 10-11 what next
RSTRIP0 EQU     0F2H            ; the seats' strips: blue, green, purple, teal
RSTRIP1 EQU     0A2H
RSTRIP2 EQU     2BH
RSTRIP3 EQU     0C3H
B_ROOM: DB      0,7EH,07H,0D5H,0                ; the heading: yellow, white, cyan
        BLINE   2*TXH
        DB      0,C_BRIGHT,C_WARM,RSTRIP0,0     ; seat 0
        BLINE   3*TXH
        DB      0,7EH,75H,00H,0                 ; its trophies: gold (their ground dark)
        BLINE   4*TXH
        DB      0,C_BRIGHT,C_WARM,RSTRIP1,0
        BLINE   5*TXH
        DB      0,7EH,75H,00H,0
        BLINE   6*TXH
        DB      0,C_BRIGHT,C_WARM,RSTRIP2,0
        BLINE   7*TXH
        DB      0,7EH,75H,00H,0
        BLINE   8*TXH
        DB      0,C_BRIGHT,C_WARM,RSTRIP3,0
        BLINE   9*TXH
        DB      0,7EH,75H,00H,0
        BLINE   10*TXH
        DB      0,07H,76H,0D5H,0                ; what next: white, yellow, cyan
        IF      FOOTER
        BLINE   LINES
        FOOTB
        ENDIF
        DB      VBLLN

; the name and the rooms: the heading yellow; the list white (a room
; playing: green), the cursor and the letter cyan; the hints at the foot
B_MENU: DB      0,7EH,07H,0D5H,0                ; rows 0-1
        BLINE   2*TXH
        DB      0,07H,0A4H,0D5H,0               ; rows 2-9
        BLINE   10*TXH
        DB      0,07H,76H,0D5H,0                ; rows 10-11
        IF      FOOTER
        BLINE   LINES
        FOOTB
        ENDIF
        DB      VBLLN

; look.inc -- the arena's colours: the spike's looks (build.sh LOOK=n).
;
; The arena is the left palette and stays put: L0 dark (the frame, walls,
; shades, bombs, visors), L1 floor (a band may change it: FLOORB, every
; other row), L2 warm, L3 bright. The seats are made of those (a bitmap
; has no colour of its own for each bomber): the CGA's and M.U.L.E.'s
; scheme, a pattern byte for each role (index * 55H), the fourth seat
; striped. Colour bytes are hue<<3 | luma from MAME's palette
; (tools/astro/astropal.py).

        IF      LOOK == 0       ; a green field: black, green, red, white
C_DARK  EQU     00H
C_FLOOR EQU     0A2H
C_FLOORB EQU    0A2H
C_WARM  EQU     52H
C_BRIGHT EQU    07H
C_HUD   EQU     04H             ; the HUD's ground
        ENDIF
        IF      LOOK == 1       ; a blue field: black, blue, red, yellow (the Arcadia's)
C_DARK  EQU     00H
C_FLOOR EQU     0F1H
C_FLOORB EQU    0F1H
C_WARM  EQU     52H
C_BRIGHT EQU    76H
C_HUD   EQU     0F9H
        ENDIF
        IF      LOOK == 2       ; a lawn: striped greens, orange, white
C_DARK  EQU     00H
C_FLOOR EQU     0A2H
C_FLOORB EQU    0A3H
C_WARM  EQU     6BH
C_BRIGHT EQU    07H
C_HUD   EQU     04H
        ENDIF

; the seats: body (even rows, odd rows), visor
SB0E    EQU     0FFH            ; bright, a dark visor
SB0O    EQU     0FFH
SV0     EQU     00H
SB1E    EQU     0AAH            ; warm, a dark visor
SB1O    EQU     0AAH
SV1     EQU     00H
SB2E    EQU     00H             ; dark, a bright visor
SB2O    EQU     00H
SV2     EQU     0FFH
SB3E    EQU     0AAH            ; warm and bright stripes, a dark visor
SB3O    EQU     0FFH
SV3     EQU     00H

; the footer under the arena: a gradient of embers, 2 lines a band
FOOT0   EQU     48H
FOOT1   EQU     49H
FOOT2   EQU     51H
FOOT3   EQU     5AH
FOOT4   EQU     62H
FOOT5   EQU     6BH
FOOT6   EQU     74H
FOOT7   EQU     7DH
FOOT8   EQU     7EH

; a blast: the frame (colour 0) glows: an ember, then a darker one
FLASHN  EQU     6               ; frames
FLASHC  EQU     60H             ; the first three: a dark red
FLASHD  EQU     70H             ; the last three: a darker brown

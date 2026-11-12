; look.inc -- the arena's colours: one ground for the whole screen and one
; set of four inks (BGCOL b3: 0 white/cyan/magenta/blue, 1 yellow/green/
; red/black), the sprites any of the eight. LOOK (build.sh LOOK=n) picks
; one of the spike's looks.
;
;   TC_*    a tile code's cell (src/proto.h T_*): ink | glyph
;   WALLC   the walls' cell    STATI0/1 a power's letter, off / on
;   TXTI    the text's ink     HUDI0-3 each seat's HUD ink
;   LISTI1  a room playing, in the room list
;   SPRC0-3 each seat's sprite colour
;   LOGOI0-2 the title logo's inks, its three rows

        IFNDEF  LOOK
LOOK    EQU     1
        ENDIF

        IF      LOOK == 0       ; a green field: yellow, (green), red, black
LOOKBG  EQU     $08|CGREEN
I_YEL   EQU     INK0
I_RED   EQU     INK2
I_BLK   EQU     INK3
TC_PILLAR EQU   I_BLK|GSOLID
TC_BLOCK EQU    I_RED|GBRICK
TC_BURN  EQU    I_YEL|GBRICK
TC_BOMB  EQU    I_BLK|GBOMB
TC_DUD   EQU    I_YEL|GBOMB
TC_FLAMEC EQU   I_YEL|GSOLID
TC_FLAMEH EQU   I_YEL|GFLAMEH
TC_FLAMEV EQU   I_YEL|GFLAMEV
I_PWR   EQU     I_RED
LISTI1  EQU     I_RED
WALLC   EQU     I_BLK|GSOLID
TXTI    EQU     I_YEL
STATI0  EQU     I_BLK
STATI1  EQU     I_YEL
HUDI0   EQU     I_YEL
HUDI1   EQU     I_YEL
HUDI2   EQU     I_YEL
HUDI3   EQU     I_YEL
SPRC0   EQU     CWHITE
SPRC1   EQU     CCYAN
SPRC2   EQU     CMAGEN
SPRC3   EQU     CBLUE
LOGOI0  EQU     I_YEL
LOGOI1  EQU     I_YEL
LOGOI2  EQU     I_RED

        ELSEIF  LOOK == 1       ; a blue field: yellow, green, red, black
LOOKBG  EQU     $08|CBLUE
I_YEL   EQU     INK0
I_GRN   EQU     INK1
I_RED   EQU     INK2
I_BLK   EQU     INK3
TC_PILLAR EQU   I_BLK|GSOLID
TC_BLOCK EQU    I_RED|GBRICK
TC_BURN  EQU    I_YEL|GBRICK
TC_BOMB  EQU    I_BLK|GBOMB
TC_DUD   EQU    I_GRN|GBOMB
TC_FLAMEC EQU   I_YEL|GSOLID
TC_FLAMEH EQU   I_YEL|GFLAMEH
TC_FLAMEV EQU   I_YEL|GFLAMEV
I_PWR   EQU     I_GRN
LISTI1  EQU     I_GRN
WALLC   EQU     I_BLK|GSOLID
TXTI    EQU     I_YEL
STATI0  EQU     I_BLK
STATI1  EQU     I_YEL
HUDI0   EQU     I_YEL
HUDI1   EQU     I_GRN
HUDI2   EQU     I_RED
HUDI3   EQU     I_YEL
SPRC0   EQU     CWHITE
SPRC1   EQU     CGREEN
SPRC2   EQU     CRED
SPRC3   EQU     CYELLOW
LOGOI0  EQU     I_YEL
LOGOI1  EQU     I_YEL
LOGOI2  EQU     I_RED

        ELSE                    ; a black field: white, cyan, magenta, blue
LOOKBG  EQU     $00|CBLACK
I_WHT   EQU     INK0
I_CYN   EQU     INK1
I_MAG   EQU     INK2
I_BLU   EQU     INK3
TC_PILLAR EQU   I_BLU|GSOLID
TC_BLOCK EQU    I_MAG|GBRICK
TC_BURN  EQU    I_WHT|GBRICK
TC_BOMB  EQU    I_WHT|GBOMB
TC_DUD   EQU    I_CYN|GBOMB
TC_FLAMEC EQU   I_WHT|GSOLID
TC_FLAMEH EQU   I_CYN|GFLAMEH
TC_FLAMEV EQU   I_CYN|GFLAMEV
I_PWR   EQU     I_CYN
LISTI1  EQU     I_CYN
WALLC   EQU     I_BLU|GSOLID
TXTI    EQU     I_WHT
STATI0  EQU     I_BLU
STATI1  EQU     I_WHT
HUDI0   EQU     I_WHT
HUDI1   EQU     I_CYN
HUDI2   EQU     I_MAG
HUDI3   EQU     I_WHT
SPRC0   EQU     CYELLOW
SPRC1   EQU     CCYAN
SPRC2   EQU     CMAGEN
SPRC3   EQU     CWHITE
LOGOI0  EQU     I_WHT
LOGOI1  EQU     I_CYN
LOGOI2  EQU     I_MAG
        ENDIF

TC_FLOOR EQU    0
TC_FIRE  EQU    I_PWR|(GALPHA+5)
TC_BOMBUP EQU   I_PWR|(GALPHA+1)
TC_SPEED EQU    I_PWR|(GALPHA+18)
TC_KICK  EQU    I_PWR|(GALPHA+10)
TC_GLOVE EQU    I_PWR|(GALPHA+6)

;
; hw.inc -- the chips and the graphics memory (fujiblst.cfg; atari.h has
; the same for C).
;
GTIA    = $D000
POKEY   = $D200
ANTIC   = $D400

CHARSETS = $9000        ; four 1K arena charsets, the animation's frames
MENUCH  = $A000         ; the menus' charset
SCREEN  = $A400         ; 25 rows x 40
PMG     = $B800         ; PMBASE: the players drawn twice over (gfxa.s)
PMG2    = $B000
MISSILES = PMG + $0300
PLAYERS = PMG + $0400

ROWS    = 25
ANIM    = $FF           ; a row's charset: the arena's, this frame's

HPOSP0  = GTIA + $00
HPOSM0  = GTIA + $04
SIZEP0  = GTIA + $08
SIZEM   = GTIA + $0C
COLPM0  = GTIA + $12
COLPF0  = GTIA + $16
COLBK   = GTIA + $1A
PRIOR   = GTIA + $1B
GRACTL  = GTIA + $1D

DMACTL  = ANTIC + $00
DLISTL  = ANTIC + $02
PMBASE  = ANTIC + $07
CHBASE  = ANTIC + $09
WSYNC   = ANTIC + $0A
NMIEN   = ANTIC + $0E

AUDF1   = POKEY + $00
AUDC1   = POKEY + $01

VDSLST  = $0200
SDMCTL  = $022F
SDLSTL  = $0230
GPRIOR  = $026F
PCOLR0  = $02C0
COLOR0  = $02C4
COLOR4  = $02C8
CHBAS   = $02F4
ATRACT  = $004D
RTCLOK  = $0014
CRITIC  = $0042
KEYDEL  = $02F1
SETVBV  = $E45C
XITVBV  = $E462
SYSVBV  = $E45F

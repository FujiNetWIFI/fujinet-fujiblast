;
; hw.inc -- the chips and the graphics memory (fujiblst.cfg; atari.h has
; the same for C). The 5200 (src/atari5200, fujiblst-a52.cfg) assembles the
; same files: the same ANTIC, GTIA and POKEY at other addresses, the
; graphics in its 16K RAM, and the crt0's zero page where the OS's shadows
; are.
;
.ifdef __ATARI5200__
GTIA    = $C000
POKEY   = $E800
CHARSETS = $2000        ; four 1K arena charsets, the animation's frames
MENUCH  = $1C00         ; the menus' charset
SCREEN  = $1800         ; 25 rows x 40
PMG     = $3800         ; PMBASE: the players drawn twice over (gfxa.s)
PMG2    = $3000
.else
GTIA    = $D000
POKEY   = $D200
CHARSETS = $9000        ; four 1K arena charsets, the animation's frames
MENUCH  = $A000         ; the menus' charset
SCREEN  = $A400         ; 25 rows x 40
PMG     = $B800         ; PMBASE: the players drawn twice over (gfxa.s)
PMG2    = $B000
.endif
ANTIC   = $D400
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

.ifdef __ATARI5200__
; the crt0's (src/atari5200/crt0.s) vectors and zero-page shadows
VVBLKI  = $0202
VVBLKD  = $0204
VDSLST  = $0206
ATRACT  = $04
SDLSTL  = $05
SDMCTL  = $07
PCOLR0  = $08
COLOR0  = $0C
COLOR4  = $10
.else
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
.endif

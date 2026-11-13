; sound.inc -- the cues (src/platform-specific/sound.h) on the Studio II's
; one tone: each a rhythm of (jiffies on, jiffies off) pairs, the ISR
; keeping Q on while RD.0 counts down (M.U.L.E.'s snd.inc). A new cue
; replaces the one playing; snd_tick runs once a pass.

SND_DROP   EQU  0
SND_BOOM   EQU  1
SND_PICKUP EQU  2
SND_KICK   EQU  3
SND_THROW  EQU  4
SND_FIZZLE EQU  5
SND_DIE    EQU  6
SND_BEEP   EQU  7
SND_GO     EQU  8
SND_SELECT EQU  9
MUS_TITLE  EQU  10
MUS_ROUND  EQU  11
MUS_WIN    EQU  12
MUS_GAME   EQU  13

; snd_play: cue D, unless muted. Keeps R9 (callers loop on it).
        RFIT    51
snd_play:
        PLO     R8
        GETV    FLAGS
        ANI     FL_MUTE
        BNZ     sp9
        GLO     R8
        PUTV    SNDID
        SHL
        ADI     LO(SNDTAB)
        PLO     R8
        LDI     HI(SNDTAB)
        PHI     R8
        LDA     R8
        PUTV    SNDPH
        LDN     R8
        PUTV    SNDPL
        GETV    JIFFY
        PUTV    SNDT
        FJMP    snd_tick
sp9:    FRET

; snd_tick: every pass: the next pair when its time comes.
        RFIT    71
snd_tick:
        GETV    SNDPH
        BZ      stk9
        GETV    SNDT                    ; JIFFY - SNDT, signed: not yet?
        STR     R2
        GETV    JIFFY
        SM
        ANI     80H
        BNZ     stk9
        GETV    SNDPH
        PHI     R8
        GETV    SNDPL
        PLO     R8
        LDA     R8                      ; on
        BZ      stk8
        PLO     RD                      ; the ISR's beep counter
        STR     R2
        LDA     R8                      ; + off
        ADD
        STR     R2
        GETV    JIFFY
        ADD
        PUTV    SNDT
        GLO     R8
        PUTV    SNDPL
        GHI     R8
        PUTV    SNDPH
stk9:   FRET
stk8:   SETV    SNDPH, 0
        FRET

; the cues: build/studio2/snd.inc (tools/s2/mksnd_s2.py)
        INCLUDE "snd.inc"

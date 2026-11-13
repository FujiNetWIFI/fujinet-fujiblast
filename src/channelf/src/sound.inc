; ---------------------------------------------------------------------------
; sound.inc -- the tones (port 5 bits 7-6: $40 1 kHz, $80 500 Hz, $C0 a
; 120 Hz buzz; no volume). A tone rings and decays in about 9 ms and is
; struck only when the bits change; so a sound is a list of steps (tone,
; jiffies, strike: 0 once, 1 every jiffy, 2 every other), from
; tools/chf/mksnd_chf.py (SNDTAB, in the art page: RAM). SNDB is the tone now
; out: the engine sends it with every row (no INS 5), and every port-5 write
; here keeps the row's bits (INS 5).
;
; A new sound replaces the one playing (an effect cuts a jingle). The tick
; runs once a pass by DT, between the drawings.
; ---------------------------------------------------------------------------

SND_DROP   EQU 0
SND_BOOM   EQU 1
SND_PICKUP EQU 2
SND_KICK   EQU 3
SND_THROW  EQU 4
SND_FIZZLE EQU 5
SND_DIE    EQU 6
SND_BEEP   EQU 7
SND_GO     EQU 8
SND_SELECT EQU 9
MUS_TITLE  EQU 10
MUS_ROUND  EQU 11
MUS_WIN    EQU 12
MUS_GAME   EQU 13

; SNDOUT -- tone r8 out: silence first if it is struck again (the bits must
;   change), then the tone; SNDB = it.  Leaf.  Clobbers A, r7, DC0.
SNDOUT: INS 5
        NI 03FH
        LR 7,A
        OUTS 5                  ; (silence: the next tone is struck)
        LR A,8
        STB SNDB
        AS 7
        OUTS 5
        POP

; SNDINIT -- nothing playing, the tone off.  Leaf.  Clobbers A, DC0.
SNDINIT: CLR
        STB SNDB
        STB SNDID+1
        LI 0FFH
        STB SNDID
        INS 5
        NI 03FH
        OUTS 5
        POP

; SNDPLAY -- sound r0 (none while muted).  Leaf.  Clobbers A, r0, r7, r8,
;   Q, DC0.
SNDPLAY: LDB SNDMUTE
        NI 0FFH
        BNZ SNP9
        LR A,0
        STB SNDID
        SL 1
        DCI SNDTAB
        ADC
        LM
        LR QU,A
        LM
        LR QL,A
        LR A,QU                 ; its first step
        STB SNDPTR
        LR A,QL
        STB SNDPTR+1
        CLR
        STB SNDT                ; (the tick takes the step at once)
SNP9:   POP

; SNDTICK -- a pass of the sound: DT jiffies off the step; at its end the
;   next step struck; the step's tone struck again as its rule says.
SNDTICK: LDB SNDID              ; (nothing playing: out before ENTER)
        CI 0FFH
        BNZ SNT0
        POP
SNT0:   ENTER
        LDB DT
        STB SNDN
SNT1:   LDB SNDT                ; the step over?
        NI 0FFH
        BNZ SNT3
        DCI SNDPTR              ; the next step: tone, jiffies, strike
        LM
        LR QU,A
        LM
        LR QL,A
        LR DC,Q
        LM
        LR 8,A                  ; tone
        LM
        LR 6,A                  ; jiffies
        LM
        LR 5,A                  ; strike
        LR Q,DC                 ; (the next step: kept before any STB moves DC0)
        LR A,5
        STB SNDMODE
        LR A,6
        NI 0FFH
        BZ SNTEND               ; (0 jiffies: the end)
        STB SNDT
        LR A,QU
        STB SNDPTR
        LR A,QL
        STB SNDPTR+1
        LR A,8
        STB SNDTONE
        PI SNDOUT
        CLR
        STB SNDAGE
        BR SNT4
SNT3:   AI -1                   ; a jiffy of it
        STB SNDT
        INCB SNDAGE
        LDB SNDMODE             ; struck again?
        NI 0FFH
        BZ SNT4
        CI 1
        BZ SNT3A
        LDB SNDAGE
        NI 1
        BNZ SNT4
SNT3A:  LDB SNDTONE
        NI 0FFH
        BZ SNT4
        LR 8,A
        PI SNDOUT
SNT4:   DECB SNDN
        BNZ SNT1
SNTX:   LEAVE
SNTEND: LI 0FFH                 ; over: silence
        STB SNDID
        CLR
        LR 8,A
        PI SNDOUT
        LEAVE

; SNDTMUT -- sound off, or on again (TIME).  Leaf.  Clobbers A, DC0.
SNDTMUT: LDB SNDMUTE
        XI 1
        STB SNDMUTE
        LI 0FFH
        STB SNDID
        CLR
        STB SNDB
        INS 5
        NI 03FH
        OUTS 5
        POP

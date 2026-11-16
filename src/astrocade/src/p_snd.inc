; p_snd.inc -- the sound: three tones and the noise of the custom I/O chip,
; a list of steps on each (tools/astro/mksnd_astro.py: snd.inc). No family
; client had used more than tone A; this one plays the jingles in three
; parts (the tune on A, a harmony on B, a bass on C) and lets the effects
; overlap, each on its own channels (a blast's noise never cuts a step on A).
;
; SNDTICK runs from FRAME (and the mailbox's waits): the frames since the
; last tick are stepped (8 at most: a slow pass keeps the time), then all
; eight registers are written at once (OTIR to the block port). The master
; oscillator is fixed (SNDMO) and nothing modulates it, so no voice is ever
; retuned by another's effect.
;
; A channel (SNDCH, 7 bytes each, A B C N): its list (0: off), the frames
; left of its step, its volume, decay, the decay's count, its tone period.

        RET                     ; E_ENTER
        NOP
        NOP
        RET                     ; E_STATE
        NOP
        NOP
        JP      SNDTICK         ; E_TICK

CHSZ    EQU     7

; SNDTICK: one jiffy of sound, called by the frame interrupt (SNDISR in core.inc,
; page 8 mapped, the interrupted page put back after): the queued sounds
; started, the loop and each channel stepped, the chip written. It runs
; between any two instructions of the program, so it keeps the stack shallow
; and touches only the sound's own bytes.
SNDTICK: LD     A,(FLAGS)       ; muted: all off
        AND     FL_MUTE
        JP      NZ,SNMUTE
        LD      HL,SNDQ         ; the queue: two
        LD      A,(HL)
        LD      (HL),0FFH
        CALL    SNSTART
        LD      HL,SNDQ2
        LD      A,(HL)
        LD      (HL),0FFH
        CALL    SNSTART
        CALL    MUSCTL          ; the loop started or stopped
        LD      A,(MUSBAR)      ; (the loop going: something is playing)
        CP      0FDH
        JR      C,SQT1A
        LD      HL,SNDCH+1  ; anything playing? (each channel's list, high byte)
        LD      A,(HL)
        LD      DE,CHSZ
        ADD     HL,DE
        OR      (HL)
        ADD     HL,DE
        OR      (HL)
        ADD     HL,DE
        OR      (HL)
        JR      NZ,SQT1A
        LD      HL,SNDSIL       ; no: silence, once
        LD      A,(HL)
        OR      A
        RET     NZ
        LD      (HL),1
        LD      HL,SNDSH
        LD      B,8
SQT1B:  LD      (HL),0
        INC     HL
        DJNZ    SQT1B
        JP      SNOUT
SQT1A:  XOR     A
        LD      (SNDSIL),A
        CALL    MUSFEED
        CALL    SNSTEP
        CALL    MUSADV
; the registers, 7 down to 0
        LD      IX,SNDCH
        LD      A,(IX+3*CHSZ+3) ; 7: the noise's volume (no modulation mask)
        RLCA
        RLCA
        RLCA
        RLCA
        AND     0F0H
        LD      (SNDSH),A
        LD      E,A
        LD      A,(IX+1*CHSZ+3) ; 6: B's volume, A's
        RLCA
        RLCA
        RLCA
        RLCA
        AND     0F0H
        OR      (IX+0*CHSZ+3)
        LD      (SNDSH+1),A
        LD      A,E             ; 5: the noise mixed in, C's volume
        OR      A
        LD      A,(IX+2*CHSZ+3)
        JR      Z,SQT3
        OR      20H
SQT3:    LD      (SNDSH+2),A
        XOR     A               ; 4: no vibrato
        LD      (SNDSH+3),A
        LD      A,(IX+2*CHSZ+6) ; 3, 2, 1: the periods
        LD      (SNDSH+4),A
        LD      A,(IX+1*CHSZ+6)
        LD      (SNDSH+5),A
        LD      A,(IX+0*CHSZ+6)
        LD      (SNDSH+6),A
        LD      A,SNDMO         ; 0: the master
        LD      (SNDSH+7),A
SNOUT:  LD      HL,SNDSH
        LD      BC,8*256+SNDBX  ; (B after OUTI's DEC: registers 7 .. 0)
        OTIR
        RET

; SNMUTE: every channel stopped, the chip silent.
SNMUTE: LD      HL,SNDCH
        LD      B,4*CHSZ
SQM1:    LD      (HL),0
        INC     HL
        DJNZ    SQM1
        LD      HL,SNDSH
        LD      B,8
SQM2:    LD      (HL),0
        INC     HL
        DJNZ    SQM2
        LD      A,0FFH
        LD      (SNDQ),A
        LD      (SNDQ2),A
        LD      (MUSBAR),A      ; (the loop is off until a round asks)
        JR      SNOUT

; SNSTART: sound A (0FFH: none) on its channels, cutting what was there.
SNSTART: CP     0FFH
        RET     Z
        CP      MUS_TITLE
        JR      C,SQS0
        PUSH    AF              ; a jingle has B and C: the loop stops
        LD      A,0FEH
        LD      (MUSBAR),A
        POP     AF
SQS0:    LD      L,A
        LD      H,0
        ADD     HL,HL
        LD      DE,SNDTAB
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        EX      DE,HL           ; HL: its entry: channel, list, ... 0FFH
SQS1:    LD      A,(HL)
        CP      0FFH
        RET     Z
        INC     HL
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        PUSH    HL
        LD      L,A             ; the channel: SNDCH + 7 ch
        ADD     A,A
        ADD     A,A
        ADD     A,A
        SUB     L
        LD      L,A
        LD      H,0
        LD      BC,SNDCH
        ADD     HL,BC
        LD      (HL),E          ; its list
        INC     HL
        LD      (HL),D
        INC     HL
        LD      (HL),0          ; its step: fetched next
        INC     HL
        LD      (HL),0          ; quiet till then
        POP     HL
        JR      SQS1

; SNSTEP: a frame of each channel: its next step fetched when one ends,
; the volume's decay.
SNSTEP: LD      IX,SNDCH
        LD      B,4
SQP1:    LD      L,(IX+0)
        LD      H,(IX+1)
        LD      A,H
        OR      L
        JR      Z,SQP9           ; off
        LD      A,(IX+2)
        OR      A
        JR      NZ,SQP3
        LD      A,(HL)          ; the next step: T, volume, frames, decay
        LD      (IX+6),A
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        OR      A
        JR      NZ,SQP2
        LD      (IX+0),A        ; frames 0: the end
        LD      (IX+1),A
        LD      (IX+3),A
        JR      SQP9
SQP2:    LD      (IX+2),A
        LD      A,(HL)
        INC     HL
        LD      (IX+4),A
        LD      (IX+5),A
        LD      (IX+0),L
        LD      (IX+1),H
        LD      A,(IX+6)        ; (a rest: T 0 on a tone, quiet)
        OR      A
        JR      NZ,SQP2A
        LD      A,B
        CP      1               ; (the noise has no T)
        JR      Z,SQP2A
        LD      C,0
SQP2A:   LD      (IX+3),C
SQP3:    DEC     (IX+2)          ; the step's frame
        LD      A,(IX+4)        ; the decay
        OR      A
        JR      Z,SQP9
        DEC     (IX+5)
        JR      NZ,SQP9
        LD      (IX+5),A
        LD      A,(IX+3)
        CP      2
        JR      C,SQP9
        DEC     (IX+3)
SQP9:    LD      DE,CHSZ
        ADD     IX,DE
        DJNZ    SQP1
        RET


; ---- the round's music loop (tools/mkmus_astro.py: music.inc) -----------------------
; MUSBAR: the bar playing (MUSORD, 7 bytes a bar: jiffies, the lists of B, N, C),
; or 0FDH start asked (LOOP 1), 0FEH stop asked (LOOP 0, a jingle), 0FFH off.
; MUSJ: the jiffy in the bar. Nothing else is kept: a channel that is idle (an
; effect has ended) or at a bar's start walks the bar's list to jiffy MUSJ, so
; the loop keeps time under the effects and comes back mid-bar. A music list
; is one that lies in MUSBEG-MUSEND.

; MUSCTL: the start and stop asked for. Clobbers A, B, DE, HL, IX.
MUSCTL: LD      A,(MUSBAR)
        CP      0FDH
        RET     C               ; going
        JR      Z,MZSTA
        CP      0FEH
        RET     NZ              ; off
        LD      IX,SNDCH        ; stop: the music lists off every channel
        LD      B,4
MZS1:   LD      L,(IX+0)
        LD      H,(IX+1)
        LD      A,H
        OR      L
        JR      Z,MZS2
        LD      DE,MUSBEG       ; (is it the music's?)
        OR      A
        SBC     HL,DE
        JR      C,MZS2
        LD      DE,MUSEND-MUSBEG
        OR      A
        SBC     HL,DE
        JR      NC,MZS2
        XOR     A
        LD      (IX+0),A
        LD      (IX+1),A
        LD      (IX+2),A
        LD      (IX+3),A
MZS2:   LD      DE,CHSZ
        ADD     IX,DE
        DJNZ    MZS1
        LD      A,0FFH
        LD      (MUSBAR),A
        RET
MZSTA:  LD      A,(FLAGS)       ; start: not while the loop or all sound is off
        AND     FL_MUTE+FL_NOLP
        LD      A,0FFH
        JR      NZ,MZS3
        XOR     A
        LD      (MUSJ),A
MZS3:   LD      (MUSBAR),A
        RET

; MUSFEED: before a jiffy's step, each music channel (B, C, N: the order of a
; bar's record) started where the bar is, if it is idle or the bar has just
; begun and it was playing music. No calls below it (the interrupt's stack).
; Clobbers A, BC, DE, HL, IX.
MUSFEED: LD     A,(MUSBAR)
        CP      0FDH
        RET     NC
        MUSENT
        INC     HL              ; (the length)
        LD      (MUSPT),HL
        LD      IX,SNDCH+1*CHSZ
        LD      A,3
        LD      (MUSCN),A
MZF1:   LD      HL,(MUSPT)      ; this channel's list for the bar
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      (MUSPT),HL
        LD      L,(IX+0)
        LD      H,(IX+1)
        LD      A,H
        OR      L
        JR      Z,MZC2          ; idle: start it
        LD      A,(MUSJ)
        OR      A
        JR      NZ,MZC9         ; mid-bar, busy
        LD      BC,MUSBEG       ; at the bar's start: only a music list restarts
        OR      A
        SBC     HL,BC
        JR      C,MZC9
        LD      BC,MUSEND-MUSBEG
        OR      A
        SBC     HL,BC
        JR      NC,MZC9
MZC2:   LD      A,(MUSJ)        ; walk the list DE to jiffy MUSJ
        LD      B,A
        EX      DE,HL
MZC3:   LD      A,(HL)          ; T
        LD      (IX+6),A
        INC     HL
        LD      C,(HL)          ; volume
        INC     HL
        LD      A,(HL)          ; frames
        INC     HL
        OR      A
        JR      Z,MZC8          ; (past the end: idle)
        LD      D,A
        LD      A,B
        CP      D
        JR      C,MZC4          ; this step
        SUB     D
        LD      B,A
        INC     HL              ; (its decay)
        JR      MZC3
MZC4:   LD      A,D
        SUB     B
        LD      (IX+2),A        ; the frames left of it
        LD      A,(HL)
        INC     HL
        LD      (IX+4),A
        LD      (IX+5),A
        LD      (IX+0),L
        LD      (IX+1),H
        LD      (IX+3),C
        JR      MZC9
MZC8:   XOR     A
        LD      (IX+0),A
        LD      (IX+1),A
        LD      (IX+3),A
MZC9:   LD      DE,CHSZ
        ADD     IX,DE
        LD      HL,MUSCN
        DEC     (HL)
        JR      NZ,MZF1
        RET

; MUSADV: after a jiffy's step, the next jiffy of the bar, the next bar.
MUSADV: LD      A,(MUSBAR)
        CP      0FDH
        RET     NC
        MUSENT
        LD      A,(MUSJ)
        INC     A
        CP      (HL)
        JR      C,MZA1
        XOR     A
        LD      (MUSJ),A
        LD      A,(MUSBAR)
        INC     A
        CP      MUSNB
        JR      C,MZA2
        LD      A,MUSLP
MZA2:   LD      (MUSBAR),A
        RET
MZA1:   LD      (MUSJ),A
        RET

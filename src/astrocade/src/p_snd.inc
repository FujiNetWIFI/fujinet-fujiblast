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

; SNDTICK: the queued sounds started, the frames stepped, the chip written.
SNDTICK: LD     A,(FLAGS)       ; muted: all off
        AND     FL_MUTE
        JP      NZ,SNMUTE
        LD      HL,(FRAMES)     ; the frames since the last tick
        LD      DE,(SNDLF)
        LD      (SNDLF),HL
        OR      A
        SBC     HL,DE
        LD      A,H
        OR      A
        JR      NZ,SQT0
        LD      A,L
        OR      A
        RET     Z
        CP      9
        JR      C,SQT1
SQT0:    LD      A,8
SQT1:    PUSH    AF
        LD      HL,SNDQ         ; the queue: two
        LD      A,(HL)
        LD      (HL),0FFH
        CALL    SNSTART
        LD      HL,SNDQ2
        LD      A,(HL)
        LD      (HL),0FFH
        CALL    SNSTART
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
        POP     BC              ; no: silence, once
        LD      HL,SNDSIL
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
        POP     BC              ; B: the frames
SQT2:    PUSH    BC
        CALL    SNSTEP
        POP     BC
        DJNZ    SQT2
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
        JR      SNOUT

; SNSTART: sound A (0FFH: none) on its channels, cutting what was there.
SNSTART: CP     0FFH
        RET     Z
        LD      L,A
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


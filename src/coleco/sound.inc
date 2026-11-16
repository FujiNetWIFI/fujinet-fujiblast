; sound.inc -- the effects (voice 1: tone channel 1, or the noise channel)
; and jingles (voice 0: tone channel 0) on the SN76489 (port FFH), stepped
; a frame at a time by the NMI (SNDNMI), so a note ends on time however long
; the game is busy. src/adam/sound.c, transcribed; the tunes are its own
; (tools/coleco/mksnd_coleco.py, snd.inc). Fixed.
;
; A note: a divisor word (a rest if 0; bit 15 the noise channel, its control
; in the low bits), frames, an attenuation (0 loud, 15 off); 0 frames ends
; the tune. A voice is a pointer to its next note (0: none) and the frames
; left of the one playing.

SNPORT  EQU     0FFH

; SNDINI: every channel quiet.
SNDINI: LD      A,9FH
        OUT     (SNPORT),A
        LD      A,0BFH
        OUT     (SNPORT),A
        LD      A,0DFH
        OUT     (SNPORT),A
        LD      A,0FFH
        OUT     (SNPORT),A
        RET

; SNDNMI: one frame of both voices (from the NMI: every register is ours).
SNDNMI: LD      IX,SNDV0
        LD      B,0
        CALL    SNDSTEP
        LD      IX,SNDV1
        LD      B,1
        CALL    SNDSTEP
        JP      MUSNMI
; SNDSTEP: voice B (record IX): this frame's step.
SNDSTEP: LD     L,(IX+0)
        LD      H,(IX+1)
        LD      A,H
        OR      L
        RET     Z
        LD      A,(IX+2)
        CP      2
        JR      C,SSNEXT
        DEC     A
        LD      (IX+2),A
        RET
SSNEXT: LD      E,(HL)          ; the note: divisor, frames, attenuation
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      A,(HL)
        INC     HL
        LD      (IX+0),L
        LD      (IX+1),H
        LD      H,A             ; H = the attenuation
        LD      A,C
        OR      A
        JR      NZ,SSPUT
        LD      (IX+0),A        ; the end: the voice free, quiet
        LD      (IX+1),A
SSQUIET: LD     A,B
        OR      A
        LD      A,9FH           ; voice 0: tone 0 off
        JR      Z,SSQ1
        LD      A,0BFH          ; voice 1: tone 1 off, and the noise if an effect had it
        OUT     (SNPORT),A
        LD      A,(FXNOI)
        OR      A
        RET     Z
        XOR     A
        LD      (FXNOI),A
        LD      A,0FFH
SSQ1:   OUT     (SNPORT),A
        RET
SSPUT:  LD      (IX+2),C
        BIT     7,D
        JR      NZ,SSNOI
        LD      A,D
        OR      E
        JR      Z,SSQUIET       ; a rest
        LD      L,0             ; L = the channel's bits
        LD      A,B
        OR      A
        JR      Z,SSP1
        LD      L,20H
SSP1:   LD      A,E             ; the low 4 bits
        AND     0FH
        OR      80H
        OR      L
        OUT     (SNPORT),A
        LD      A,E             ; the high 6: (d >> 4) & 3FH
        RRCA
        RRCA
        RRCA
        RRCA
        AND     0FH
        LD      C,A
        LD      A,D
        RLCA
        RLCA
        RLCA
        RLCA
        AND     0F0H
        OR      C
        AND     3FH
        OUT     (SNPORT),A
        LD      A,H
        AND     0FH
        OR      90H
        OR      L
        OUT     (SNPORT),A
        LD      A,B
        OR      A
        RET     Z
        LD      A,(FXNOI)       ; (the effects' voice: the noise off, if an effect had it)
        OR      A
        RET     Z
        XOR     A
        LD      (FXNOI),A
        LD      A,0FFH
        OUT     (SNPORT),A
        RET
SSNOI:  LD      A,1
        LD      (FXNOI),A
        LD      A,0BFH          ; tone 1 off, the noise on
        OUT     (SNPORT),A
        LD      A,E
        AND     7
        OR      0E0H
        OUT     (SNPORT),A
        LD      A,H
        AND     0FH
        OR      0F0H
        OUT     (SNPORT),A
        RET

; SNDQUE: start sound or jingle A (SND_*, MUS_*) now (none while muted). An
; effect goes on voice 1, a jingle on voice 0. Preserves all but A.
SNDQUE: PUSH    HL
        PUSH    DE
        LD      E,A
        LD      HL,FLAGS
        BIT     2,(HL)          ; FL_MUTE
        JR      NZ,SQ9
        LD      A,E
        ADD     A,A
        LD      L,A
        LD      H,0
        PUSH    DE
        LD      DE,SNDTAB
        ADD     HL,DE
        LD      E,(HL)
        INC     HL
        LD      D,(HL)          ; DE = the tune
        POP     HL              ; L = the id
        LD      A,L
        CP      10              ; the effects are 0-9
        LD      HL,SNDV1
        JR      C,SQ1
        LD      HL,SNDV0
SQ1:    CALL    VLOCK           ; (the NMI would step it half set)
        PUSH    HL
        PUSH    DE
        JR      C,SQ2
        CALL    MUSOFF          ; the jingle has tone 0
SQ2:    POP     DE
        POP     HL
        LD      (HL),E
        INC     HL
        LD      (HL),D
        INC     HL
        LD      (HL),1
        CALL    VFREE
SQ9:    POP     DE
        POP     HL
        RET

; SNDOFF: every voice off now. Clobbers A, HL.
SNDOFF: CALL    VLOCK
        XOR     A
        LD      HL,SNDV0
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        INC     HL
        LD      (HL),A
        LD      (FXNOI),A
        LD      (MUSV0),A       ; the loop off (its pointers' high bytes are enough)
        LD      (MUSV0+1),A
        LD      (MUSV1),A
        LD      (MUSV1+1),A
        CALL    SNDINI
        JP      VFREE

; ---- the round's music (music/play.txt, tools/mkmus_sms.py --zmac) ---------------------
; Two slots of steps A, B, frames (music.inc). Slot 0 (ch1) is tone 2 (the
; bass and kick) or the noise channel (snare, hat: skipped while an effect has
; the noise); slot 1 (ch2, the chatter) is tone 0. A, B:
;   ch1  B = 0 silent; B = 40H|N>>4 tone 2, A = N&15 | att<<4; B = 80H noise, A = ctl | att<<4
;   ch2  B = 0 silent; B = 40H|N>>4 tone 0, A = N&15 | att<<4
; Frames 1-252, or FEH call (one level), FDH return, FFH jump, A B the address.
; src/sms/sound.c mus_step, transcribed.

; MUSNMI: one frame of both slots (from SNDNMI: every register is ours).
MUSNMI: LD      IX,MUSV0
        LD      B,0
        CALL    MUSSTEP
        LD      IX,MUSV1
        LD      B,1
; MUSSTEP: slot B (record IX): this frame's step.
MUSSTEP: LD     L,(IX+0)
        LD      H,(IX+1)
        LD      A,H
        OR      L
        RET     Z
        LD      A,(IX+4)
        CP      2
        JR      C,MQ1
        DEC     A
        LD      (IX+4),A
        RET
MQ1:    LD      E,(HL)          ; the step: A, B, frames
        INC     HL
        LD      D,(HL)
        INC     HL
        LD      C,(HL)
        INC     HL
        LD      A,C
        CP      0FEH
        JR      NZ,MQ2
        LD      (IX+2),L        ; a call: the next step is where we come back to
        LD      (IX+3),H
        EX      DE,HL
        JR      MQ1
MQ2:    CP      0FDH
        JR      NZ,MQ3
        LD      L,(IX+2)
        LD      H,(IX+3)
        JR      MQ1
MQ3:    CP      0FFH
        JR      NZ,MQ4
        EX      DE,HL
        JR      MQ1
MQ4:    LD      (IX+0),L        ; the step: E = A, D = B, C = frames
        LD      (IX+1),H
        LD      (IX+4),C
        LD      A,B
        OR      A
        JR      NZ,MQCH2
        BIT     7,D
        JR      NZ,MQDRUM
        LD      A,(FXNOI)       ; the bass: the noise off (a drum's), unless an effect's
        OR      A
        JR      NZ,MQB1
        LD      A,0FFH
        OUT     (SNPORT),A
MQB1:   LD      A,D
        OR      A
        JR      Z,MQB2
        LD      A,E
        AND     0FH
        OR      0C0H
        OUT     (SNPORT),A
        LD      A,D
        AND     3FH
        OUT     (SNPORT),A
        LD      A,E
        RRCA
        RRCA
        RRCA
        RRCA
        AND     0FH
        OR      0D0H
        OUT     (SNPORT),A
        RET
MQB2:   LD      A,0DFH
        OUT     (SNPORT),A
        RET
MQDRUM: LD      A,0DFH          ; tone 2 off, the noise on
        OUT     (SNPORT),A
        LD      A,(FXNOI)
        OR      A
        RET     NZ
        LD      A,E
        AND     7
        OR      0E0H
        OUT     (SNPORT),A
        LD      A,E
        RRCA
        RRCA
        RRCA
        RRCA
        AND     0FH
        OR      0F0H
        OUT     (SNPORT),A
        RET
MQCH2:  LD      A,D             ; the chatter: tone 0
        OR      A
        JR      Z,MQC2
        LD      A,E
        AND     0FH
        OR      80H
        OUT     (SNPORT),A
        LD      A,D
        AND     3FH
        OUT     (SNPORT),A
        LD      A,E
        RRCA
        RRCA
        RRCA
        RRCA
        AND     0FH
        OR      90H
        OUT     (SNPORT),A
        RET
MQC2:   LD      A,9FH
        OUT     (SNPORT),A
        RET

; MUSOFF: both slots off and quiet (the NMI locked out: VLOCK). Clobbers A, HL.
MUSOFF: XOR     A
        LD      (MUSV0),A
        LD      (MUSV0+1),A
        LD      (MUSV1),A
        LD      (MUSV1+1),A
        LD      A,0DFH          ; tone 2 off, and tone 0
        OUT     (SNPORT),A
        LD      A,9FH
        OUT     (SNPORT),A
        LD      A,(FXNOI)
        OR      A
        RET     NZ
        LD      A,0FFH
        OUT     (SNPORT),A
        RET

; SNDLOOP: A = 0 the loop off, else on: both slots from the top on the same
; frame. Playing already (and no jingle), it plays on; muted, it stays off.
; Preserves all but A.
SNDLOOP: PUSH   HL
        PUSH    DE
        LD      E,A
        LD      HL,FLAGS
        LD      A,(HL)
        AND     FL_MUTE+FL_NOLP
        JR      Z,SL1
        LD      E,0
SL1:    LD      A,E
        OR      A
        JR      Z,SL2
        LD      HL,(MUSV0)
        LD      A,H
        OR      L
        JR      Z,SL2
        LD      HL,(SNDV0)      ; (a jingle on tone 0 ends the loop)
        LD      A,H
        OR      L
        JR      NZ,SL2
        JR      SL9             ; going on: leave it
SL2:    CALL    VLOCK
        XOR     A
        LD      (SNDV0),A
        LD      (SNDV0+1),A
        CALL    MUSOFF
        LD      A,E
        OR      A
        JR      Z,SL3
        LD      HL,MLOOP1
        LD      (MUSV0),HL
        LD      HL,MLOOP2
        LD      (MUSV1),HL
        XOR     A
        LD      (MUSV0+2),A
        LD      (MUSV0+3),A
        LD      (MUSV1+2),A
        LD      (MUSV1+3),A
        INC     A
        LD      (MUSV0+4),A
        LD      (MUSV1+4),A
SL3:    CALL    VFREE
SL9:    POP     DE
        POP     HL
        RET

        INCLUDE "music.inc"
        INCLUDE "snd.inc"

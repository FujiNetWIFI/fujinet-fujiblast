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
        LD      A,0BFH          ; voice 1: tone 1 and the noise off
        OUT     (SNPORT),A
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
        LD      A,0FFH          ; (the effects' voice: the noise off)
        OUT     (SNPORT),A
        RET
SSNOI:  LD      A,0BFH          ; tone 1 off, the noise on
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
        CALL    SNDINI
        JP      VFREE

        INCLUDE "snd.inc"

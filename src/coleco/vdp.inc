; vdp.inc -- the TMS9918A's ports. Fixed.
;
; Every address and data burst is inside VLOCK/VFREE (core.inc): the NMI
; reads the status, which resets the address latch. Data accesses with the
; display on are 29 cycles apart at the least (the VDP needs 8 us between
; them while it draws Graphics II); the safe loops here are 50. With the
; display off (BLANKED) OTIR's 21 is enough.

; VREG: register B = C. Clobbers A.
VREG:   CALL    VLOCK
        LD      A,C
        OUT     (VDPC),A
        LD      A,B
        OR      80H
        OUT     (VDPC),A
        JP      VFREE

; VWA: the write address HL. Clobbers A. (Inside VLOCK.)
VWA:    LD      A,L
        OUT     (VDPC),A
        LD      A,H
        OR      40H
        OUT     (VDPC),A
        RET

; VWRITE: BC bytes from HL to VRAM DE. Clobbers A,BC,DE,HL.
VWRITE: CALL    VLOCK
        PUSH    HL
        EX      DE,HL
        CALL    VWA
        POP     HL
        LD      A,(BLANKED)
        OR      A
        JR      NZ,VWF
VWS:    LD      A,B
        OR      C
        JP      Z,VFREE
VWS1:   LD      A,(HL)
        OUT     (VDPD),A
        INC     HL
        DEC     BC
        LD      A,B
        OR      C
        JR      NZ,VWS1
        JP      VFREE
VWF:    LD      D,B
        LD      E,C
        LD      C,VDPD
        LD      A,D
        OR      A
        JR      Z,VWF2
VWF1:   LD      B,0
        OTIR
        DEC     D
        JR      NZ,VWF1
VWF2:   LD      A,E
        OR      A
        JP      Z,VFREE
        LD      B,E
        OTIR
        JP      VFREE

; VFILL: DE bytes of A at VRAM HL. Clobbers A,BC,DE,HL.
VFILL:  LD      C,A
        LD      A,D
        OR      E
        RET     Z
        CALL    VLOCK
        CALL    VWA
VFL1:   LD      A,C
        OUT     (VDPD),A
        DEC     DE
        LD      A,D
        OR      E
        JR      NZ,VFL1
        JP      VFREE

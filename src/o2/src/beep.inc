; beep.inc -- beep: queue sound A (SND_*) for p9, which plays it from its
; table; the vblank counts it down and stops it. SNDCNT: bit 7 a sound
; queued (its id below), else the frames it has left. The soft ones (SND_SOFT
; on) never cut another sound; an effect cuts a jingle; a jingle (SND_JINGLE
; on) is asked for in JNOTE. Main program. Clobbers A, r0, r1.
beep:
    mov r0,a
    add a,#256-SND_JINGLE
    jnc bp_fx
    mov a,r0                    ; a jingle: p9 plays it
    mov r1,#JNOTE
    movx @r1,a
    ret
bp_fx:
    mov a,r0
    add a,#256-SND_SOFT
    jnc bp_hard
    mov r1,#SNDCNT              ; soft: only if nothing plays
    mov a,@r1
    jnz bp_x
    jmp bp_q
bp_hard:
    clr a                       ; an effect: the jingle stops
    mov r1,#JNOTE
    movx @r1,a
bp_q:
    mov a,r0
    orl a,#80h
    mov r1,#SNDCNT
    mov @r1,a
bp_x:
    ret

' ===========================================================================
' isr.bas -- the frame handler (ON FRAME GOSUB frame_isr) and the jiffy
' clock (plat.h's). IntyBASIC allows one handler, so the clock, the
' controller, the sound and the arena's animation all hang off it. It can
' run before the program's start-up (every variable is 0 then: ready says
' when it may do more than count), and it must stay short and shallow: the
' stack is 24 words.
' ===========================================================================

    DIM #jiffy                  ' jiffies since boot: 60 a second (PAL counts every 5th twice)
    UNSIGNED #jiffy
    DIM pal5, ready, isr_t, isr_p, anim_on
    DIM #isr_a

frame_isr: PROCEDURE
    #jiffy = #jiffy + 1
    isr_t = 1
    IF NTSC = 0 THEN
        pal5 = pal5 + 1
        IF pal5 >= 5 THEN pal5 = 0 : #jiffy = #jiffy + 1 : isr_t = 2
    END IF
    IF ready = 0 THEN RETURN
    CALL READ_INPUT
    GOSUB snd_isr
    IF isr_t = 2 THEN GOSUB snd_isr
    ' The arena's animation: its cards redefined, a page every 8 jiffies,
    ' a group a frame (bombs; flames; powerups), so the copy stays small.
    IF anim_on THEN
        isr_p = (FRAME / 8) AND 3
        isr_t = FRAME AND 7
        IF isr_t = 0 THEN
            #isr_a = isr_p * (N_ANIM * 4)
            DEFINE ALTERNATE G_ANIM, 3, VARPTR gram_anim(#isr_a)
        ELSEIF isr_t = 1 THEN
            #isr_a = isr_p * (N_ANIM * 4) + 12
            DEFINE ALTERNATE G_ANIM + 3, 7, VARPTR gram_anim(#isr_a)
        ELSEIF isr_t = 2 THEN
            #isr_a = isr_p * (N_ANIM * 4) + 40
            DEFINE ALTERNATE G_ANIM + 10, 5, VARPTR gram_anim(#isr_a)
        END IF
    END IF
isr_end:
END

' frame_wait: the next frame. owed: the jiffies the logic owes (1-4; a
' frame missed is made up, as the server times our walking); frame_count
' is advanced by the caller, a jiffy at a time.
    DIM #seen, #frame_count, owed
    UNSIGNED #seen, #frame_count
frame_wait: PROCEDURE
    WAIT
    #v = #jiffy - #seen
    IF #v > 30 THEN #seen = #jiffy - 1 : #v = 1      ' a stall: don't race to catch up
    IF #v > 4 THEN #v = 4
    owed = #v
END

' a jiffy of the logic's clock
jiffy_step: PROCEDURE
    #seen = #seen + 1
    #frame_count = #seen
END

' ===========================================================================
' sound.bas -- the PSG (sound.h's): the jingles on channel A, the effects on B
' (with the noise generator for the booms), the round's music on A and C. The sounds
' are the Atari's own (gen/intv/snd.bas, from src/apple2/sndtab.s): steps
' of AUDF, AUDC, jiffies. The frame handler plays a step at a time.
'
' AUDF is POKEY's divider; the PSG's period for the same pitch is
' 3.5 (AUDF + 1) at an NTSC console's 1.79 MHz, 4 (AUDF + 1) at PAL's
' 2 MHz. AUDC's low nibble is the volume; $8x is noise (its period
' AUDF * 7 / 32 NTSC, AUDF / 4 PAL), anything else a tone.
' ===========================================================================

    CONST SND_DROP   = 0
    CONST SND_BOOM   = 1
    CONST SND_PICKUP = 2
    CONST SND_KICK   = 3
    CONST SND_THROW  = 4
    CONST SND_FIZZLE = 5
    CONST SND_DIE    = 6
    CONST SND_BEEP   = 7
    CONST SND_GO     = 8
    CONST SND_SELECT = 9
    CONST MUS_TITLE = 0
    CONST MUS_ROUND = 1
    CONST MUS_WIN   = 2
    CONST MUS_GAME  = 3

    DIM #snd_pa, #snd_pb        ' each voice's next step in snd_data, + 1 (0: quiet)
    DIM snd_la, snd_lb          ' jiffies left of its step
    DIM snd_off                 ' muted: bit 0 all, bit 1 the loop (0 -> 2 -> 3 -> 0)
    DIM sn_f, sn_c, sn_v        ' (the frame handler's)
    DIM #sn_i, #sn_p

    DIM fx_nb, fx_np, mus_na, mus_np, mx_m, mx_n

' snd_mixw: the mixer and the noise period (the PSG has one of each): $38
' is all tones on, no noise; A's noise (a drum) turns A's tone off, B's noise
' (a boom) B's. Both at once share B's period.
snd_mixw: PROCEDURE
    mx_m = $38 : mx_n = mus_np
    IF mus_na THEN mx_m = $31
    IF fx_nb THEN
        mx_m = (mx_m AND $EF) OR 2
        mx_n = fx_np
    END IF
    IF mx_n = 0 THEN mx_n = 1
    SOUND 4, mx_n, mx_m
END

snd_isr: PROCEDURE
    IF #snd_pa THEN
        IF snd_la THEN snd_la = snd_la - 1
        IF snd_la = 0 THEN
            #sn_i = #snd_pa - 1
            sn_f = snd_data(#sn_i) : sn_c = snd_data(#sn_i + 1) : snd_la = snd_data(#sn_i + 2)
            IF snd_la = 0 THEN
                #snd_pa = 0
                SOUND 0, 1, 0
            ELSE
                #snd_pa = #snd_pa + 3
                IF snd_off AND 1 THEN sn_v = 0 ELSE sn_v = sn_c AND 15
                IF NTSC THEN #sn_p = ((sn_f + 1) * 7) / 2 ELSE #sn_p = (sn_f + 1) * 4
                SOUND 0, #sn_p, sn_v
            END IF
        END IF
    END IF
    IF #snd_pb THEN
        IF snd_lb THEN snd_lb = snd_lb - 1
        IF snd_lb = 0 THEN
            #sn_i = #snd_pb - 1
            sn_f = snd_data(#sn_i) : sn_c = snd_data(#sn_i + 1) : snd_lb = snd_data(#sn_i + 2)
            IF snd_lb = 0 THEN
                #snd_pb = 0
                SOUND 1, 1, 0
                fx_nb = 0
                GOSUB snd_mixw
            ELSE
                #snd_pb = #snd_pb + 3
                IF snd_off AND 1 THEN sn_v = 0 ELSE sn_v = sn_c AND 15
                IF (sn_c AND $F0) = $80 THEN
                    IF NTSC THEN #sn_p = (sn_f * 7) / 32 ELSE #sn_p = sn_f / 4
                    IF #sn_p > 31 THEN #sn_p = 31
                    fx_nb = 1 : fx_np = #sn_p   ' noise on B, its tone off
                    GOSUB snd_mixw
                    SOUND 1, 1, sn_v
                ELSE
                    IF NTSC THEN #sn_p = ((sn_f + 1) * 7) / 2 ELSE #sn_p = (sn_f + 1) * 4
                    fx_nb = 0
                    GOSUB snd_mixw
                    SOUND 1, #sn_p, sn_v
                END IF
            END IF
        END IF
    END IF
END

' ---------------------------------------------------------------------------
' The round's music (gen/intv/music.bas, from music/play.txt): ch1 (drums and
' bass) on voice A, ch2 (the robots' chatter) on voice C, which an effect on
' B never takes. Steps of three decles in mus_d (period index, volume + noise
' << 4, jiffies; 253 calls, 254 jumps, 0 returns), a channel's order a list
' of calls to the patterns' subroutines. The jingles share voice A, so a
' jingle (snd_music) or snd_stop ends the loop.
' ---------------------------------------------------------------------------
    CONST MUS_CALL = 253
    CONST MUS_JUMP = 254
    DIM mus_on
    DIM #mp(2), #mr(2), ml(2)   ' each channel's next step, where a call returns to, jiffies left
    DIM mc, mq, mnz, mvl
    DIM #mi, #mv, #mw, #mper

' mus_isr: a jiffy of the loop (the frame handler's, twice on PAL's fifth frame)
mus_isr: PROCEDURE
    IF mus_on = 0 THEN RETURN
    FOR mc = 0 TO 1
        IF ml(mc) THEN ml(mc) = ml(mc) - 1
        IF ml(mc) = 0 THEN
            DO
                #mi = #mp(mc)
                mq = mus_d(#mi + 2)
                IF mq = 0 THEN
                    #mp(mc) = #mr(mc)
                ELSEIF mq = MUS_CALL THEN
                    #mr(mc) = #mi + 3
                    #mp(mc) = mus_d(#mi) + 256 * mus_d(#mi + 1)
                ELSEIF mq = MUS_JUMP THEN
                    #mp(mc) = mus_d(#mi) + 256 * mus_d(#mi + 1)
                ELSE
                    EXIT DO
                END IF
            LOOP WHILE 1
            ml(mc) = mq
            #mp(mc) = #mi + 3
            #mw = mus_d(#mi)
            #mv = mus_d(#mi + 1)
            mnz = #mv / 16
            mvl = #mv AND 15
            IF snd_off THEN mvl = 0
            IF NTSC THEN
                #mper = mus_nlo(#mw) + 256 * mus_nhi(#mw)
            ELSE
                #mper = mus_plo(#mw) + 256 * mus_phi(#mw)
            END IF
            IF mc = 0 THEN
                IF mnz THEN
                    mus_na = 1 : mus_np = mnz
                    SOUND 0, 1, mvl
                ELSE
                    mus_na = 0
                    SOUND 0, #mper, mvl
                END IF
                GOSUB snd_mixw
            ELSE
                SOUND 2, #mper, mvl
            END IF
        END IF
    NEXT mc
END

' snd_loop_on: the round's music from the top (a no-op while it plays); both
' channels start on the same jiffy
snd_loop_on: PROCEDURE
    IF mus_on THEN RETURN
    ASM DIS
    #snd_pa = 0
    #mp(0) = MUS_LOOP1 : #mp(1) = MUS_LOOP2
    ml(0) = 0 : ml(1) = 0
    mus_on = 1
    ASM EIS
END

snd_loop_off: PROCEDURE
    IF mus_on = 0 THEN RETURN
    ASM DIS
    mus_on = 0 : mus_na = 0
    ASM EIS
    SOUND 2, 1, 0
    IF #snd_pa = 0 THEN SOUND 0, 1, 0
    GOSUB snd_mixw
END

    DIM sid
' snd_play: effect sid on B (it takes over from the one playing).
snd_play: PROCEDURE
    ASM DIS
    #snd_pb = snd_fx(sid) + 1 : snd_lb = 0
    ASM EIS
END

' snd_music: jingle sid on A.
snd_music: PROCEDURE
    GOSUB snd_loop_off
    ASM DIS
    #snd_pa = snd_mus(sid) + 1 : snd_la = 0
    ASM EIS
END

snd_stop: PROCEDURE
    GOSUB snd_loop_off
    ASM DIS
    #snd_pa = 0 : #snd_pb = 0
    ASM EIS
    SOUND 0, 1, 0
    SOUND 1, 1, 0
    fx_nb = 0
    GOSUB snd_mixw
END

' all sound -> the effects (no music) -> none -> all
snd_toggle_mute: PROCEDURE
    IF snd_off = 0 THEN
        snd_off = 2
    ELSEIF snd_off = 2 THEN
        snd_off = 3
    ELSE
        snd_off = 0
    END IF
    IF snd_off AND 1 THEN
        SOUND 0, 1, 0 : SOUND 1, 1, 0
    END IF
    IF snd_off THEN
        IF mus_on THEN SOUND 0, 1, 0 : SOUND 2, 1, 0
    END IF
END

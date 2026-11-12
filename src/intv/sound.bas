' ===========================================================================
' sound.bas -- two voices of the PSG (sound.h's): the jingles on channel A,
' the effects on B (with the noise generator for the booms). The sounds
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
    DIM snd_off                 ' muted
    DIM sn_f, sn_c, sn_v        ' (the frame handler's)
    DIM #sn_i, #sn_p

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
                IF snd_off THEN sn_v = 0 ELSE sn_v = sn_c AND 15
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
                SOUND 4, 1, $38
            ELSE
                #snd_pb = #snd_pb + 3
                IF snd_off THEN sn_v = 0 ELSE sn_v = sn_c AND 15
                IF (sn_c AND $F0) = $80 THEN
                    IF NTSC THEN #sn_p = (sn_f * 7) / 32 ELSE #sn_p = sn_f / 4
                    IF #sn_p > 31 THEN #sn_p = 31
                    SOUND 4, #sn_p, $2A         ' noise on B, its tone off
                    SOUND 1, 1, sn_v
                ELSE
                    IF NTSC THEN #sn_p = ((sn_f + 1) * 7) / 2 ELSE #sn_p = (sn_f + 1) * 4
                    SOUND 4, 1, $38
                    SOUND 1, #sn_p, sn_v
                END IF
            END IF
        END IF
    END IF
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
    ASM DIS
    #snd_pa = snd_mus(sid) + 1 : snd_la = 0
    ASM EIS
END

snd_stop: PROCEDURE
    ASM DIS
    #snd_pa = 0 : #snd_pb = 0
    ASM EIS
    SOUND 0, 1, 0
    SOUND 1, 1, 0
    SOUND 4, 1, $38
END

snd_toggle_mute: PROCEDURE
    snd_off = 1 - snd_off
    IF snd_off THEN SOUND 0, 1, 0 : SOUND 1, 1, 0
END

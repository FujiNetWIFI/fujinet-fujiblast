;
; sndtab.s -- FujiBlast's sounds for the Mockingboard (mock.s): the Atari's
; own (src/atari/sound.c), steps of AUDF, AUDC, jiffies; 0 jiffies ends.
; In aux RAM after the art (FUJIBLST.GFX).
;
        .export _s_drop, _s_boom, _s_pickup, _s_kick, _s_throw, _s_fizzle
        .export _s_die, _s_beep, _s_go, _s_select
        .export _m_title, _m_round, _m_win, _m_game

G4 = 81
A4 = 72
B4 = 64
C5 = 60
D5 = 53
E5 = 47
G5 = 40
A5 = 35
C6 = 29

.macro  N f, l                  ; a note, and its fall
        .byte f, $A8, l - 3, f, $A4, 3
.endmacro
.macro  R l                     ; a rest
        .byte 0, $A0, l
.endmacro
.macro  END
        .byte 0, 0, 0
.endmacro

        .segment "AUXSND"

_s_drop:   .byte $C0, $A8, 2, $E0, $A6, 2, $F0, $A3, 3
           END
_s_boom:   .byte 40, $8F, 3, 60, $8E, 3, 80, $8C, 4, 100, $8A, 4, 140, $88, 5
           .byte 180, $86, 6, 220, $84, 6, 255, $82, 8
           END
_s_pickup: .byte G4, $AA, 3, B4, $AA, 3, D5, $AA, 3, G5, $AA, 5
           END
_s_kick:   .byte 30, $C8, 2, 60, $C6, 2
           END
_s_throw:  .byte 120, $84, 2, 90, $86, 2, 60, $86, 2, 40, $84, 3
           END
_s_fizzle: .byte 20, $84, 6, 25, $83, 8, 30, $82, 10, 35, $81, 10
           END
_s_die:    .byte 60, $AA, 4, 72, $AA, 4, 85, $A9, 4, 100, $A8, 5, 121, $A7, 6
           .byte 144, $A6, 6, 170, $A5, 8, 200, $A4, 10
           END
_s_beep:   .byte A4, $A8, 6
           END
_s_go:     .byte A5, $AA, 16
           END
_s_select: .byte C5, $A6, 2
           END

_m_title:  N G4, 8
           N C5, 8
           N E5, 8
           N G5, 16
           N E5, 8
           N G5, 24
           R 8
           N A5, 8
           N G5, 8
           N E5, 8
           N C6, 32
           END
_m_round:  N C5, 6
           N C5, 6
           N G5, 18
           END
_m_win:    N E5, 8
           N G5, 8
           N C6, 24
           R 4
           N G5, 8
           N C6, 32
           END
_m_game:   N C5, 8
           N E5, 8
           N G5, 8
           N C6, 16
           N A5, 8
           N C6, 16
           N G5, 8
           N E5, 8
           N G5, 8
           N C6, 40
           END

; ui.inc -- text on the cart's graphics layer (PTEXT: 4 pixels a character;
; 12 lines, or 5 with TM_SHORT), numbers, and the messages.

; txt_at: the NUL-ended string at R8 (any page: ROM, the cart's RAM) at x
; R9.0, y R9.1, in mode TXM; at most RC.0 characters (txt_at16: 16).
        RFIT    38
txt_at16:
        LDI     16
        PLO     RC
txt_at: PARS
        GLO     R9
        PD
        GHI     R9
        PD
        GETV    TXM
        PD
ta1:    LDA     R8
        BZ      ta9
        PD
        DEC     RC
        GLO     RC
        BNZ     ta1
ta9:    GOP     TOP_PTEXT
        FRET

; txt_ctr: the string at R8 centred on line R9.1 (mode TXM).
        RFIT    24
txt_ctr:
        GLO     R8
        PLO     RC
        GHI     R8
        PHI     RC
        LDI     0
        PLO     R9
tc1:    LDA     RC
        BZ      tc2
        INC     R9
        GLO     R9
        XRI     16
        BNZ     tc1
tc2:    GLO     R9                      ; x = 32 - 2 n
        SHL
        SDI     32
        PLO     R9
        FJMP    txt_at16

; txt_num: the number D (0-255) at x R9.0, y R9.1 (mode TXM).
        RFIT    80
txt_num:
        PLO     R8                      ; the number
        LDI     LO(NUMB)
        PLO     RA
        GLO     R8                      ; hundreds
        SMI     100
        BNF     tn2
        LDI     0
        PHI     R8
tn1:    GLO     R8
        SMI     100
        BNF     tn1a
        PLO     R8
        GHI     R8
        ADI     1
        PHI     R8
        BR      tn1
tn1a:   GHI     R8
        ADI     '0'
        STR     RA
        INC     RA
        LDI     1                       ; (tens then even if 0)
        PHI     RC
        BR      tn3
tn2:    LDI     0
        PHI     RC
tn3:    LDI     0                       ; tens
        PHI     R8
tn4:    GLO     R8
        SMI     10
        BNF     tn5
        PLO     R8
        GHI     R8
        ADI     1
        PHI     R8
        BR      tn4
tn5:    GHI     R8
        BNZ     tn6
        GHI     RC
        BZ      tn7
tn6:    GHI     R8
        ADI     '0'
        STR     RA
        INC     RA
tn7:    GLO     R8                      ; ones
        ADI     '0'
        STR     RA
        INC     RA
        LDI     0
        STR     RA
        LDR     R8, NUMB
        FJMP    txt_at16

; ---------------------------------------------------------------------------
; msg_show -- message MSGID (b6-0) on a cleared screen; it stays MSGT
;   jiffies, or (b7 of MSGW) until 5. Its lines: MSGTAB.
; ---------------------------------------------------------------------------
M_CONN   EQU    0
M_JOIN   EQU    1
M_NOANS  EQU    2
M_LOST   EQU    3
M_NOROOM EQU    4
M_VERSION EQU   5
M_QUIET  EQU    6
M_NOCART EQU    7

        RFIT    83
msg_show:
        OPN     TOP_CLS
        SETV    TXM, TM_SET
        GETV    MSGID
        ANI     7FH
        SHL
        SHL
        ADI     LO(MSGTAB)
        PLO     RC
        LDI     HI(MSGTAB)
        PHI     RC
        LDA     RC                      ; the first line
        PHI     R8
        LDA     RC
        PLO     R8
        LDA     RC
        PHI     R9                      ; (the second: in T0/T1)
        LDN     RC
        PLO     R9
        GHI     R9
        PUTV    T0
        GLO     R9
        PUTV    T1
        LDI     42
        PHI     R9
        FCALL   txt_ctr
        GETV    T0
        BZ      msg1
        PHI     R8
        GETV    T1
        PLO     R8
        SETV    TXM, TM_SET|TM_SHORT
        LDI     62
        PHI     R9
        FCALL   txt_ctr
msg1:   FRET

; msg_wait: the message shown, then N (D) jiffies, or until 5 if D is 0.
        RFIT    88
msg_wait:
        PUTV    MRC
        FCALL   msg_show
        GETV    MRC
        BNZ     mw1
        SETV    TXM, TM_SET|TM_SHORT
        LDR     R8, s_press5
        LDI     100
        PHI     R9
        FCALL   txt_ctr
        SETV    KNEW, 0
mw0:    FCALL   frame
        FCALL   in_poll
        FCALL   snd_tick
        GETV    KNEW
        ANI     KB_FIRE
        BZ      mw0
        SETV    KNEW, 0
        FRET
mw1:    FCALL   frame
        FCALL   in_poll
        FCALL   snd_tick
        GETV    DT
        STR     R2
        GETV    MRC
        SM
        STR     RA
        BZ      mw2
        BDF     mw1
mw2:    FRET

        PFIT    32
MSGTAB: DW      s_conn, 0, s_join, 0, s_noans, 0, s_lost, 0
        DW      s_noroom, 0, s_version, s_version2, s_quiet, 0, s_nocart, s_nocart2
s_conn: DB      "CONNECTING", 0
s_join: DB      "JOINING", 0
s_noans: DB     "NO ANSWER", 0
s_lost: DB      "LINK LOST", 0
s_noroom: DB    "NO SUCH ROOM", 0
s_version: DB   "OLD VERSION", 0
s_version2: DB  "UPDATE THE GAME", 0
s_quiet: DB     "BYE", 0
s_nocart: DB    "NO FUJINET", 0
s_nocart2: DB   "PROTOCOL 2 CART", 0
s_press5: DB    "PRESS 5", 0

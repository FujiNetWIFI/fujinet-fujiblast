' ===========================================================================
' memory.bas -- the cartridge RAM map, colours, and the variables and
' helpers every file shares. IntyBASIC is single-pass, so a CONST, DIM or
' DEF FN comes before its first use: this file is included first.
'
' IntyBASIC has about 220 8-bit and 50 16-bit variables, and 24 words of
' stack shared with the frame handler. Everything tabular lives in the
' FujiNet cartridge's RAM, $8000-$9BFF, 8 bits a location (fujinet.bas says
' why it stops there), reached with PEEK and POKE (rb() masks a read) and
' from netasm.asm.
' ===========================================================================

    ' ---- cartridge RAM ------------------------------------------------------
    CONST ST       = $8000      ' 247: the state (game.h's st), patched as it comes
    CONST NBUF     = $8100      ' 136: the frame decoded (type, payload, sums),
    CONST NBUF_END = $8188      ' and 256 bytes of slack after, for a garbled one
    CONST TXQ      = $8290      ' 64: encoded frames waiting to go out
    CONST RAWB     = $82D0      ' 32: a frame being encoded: type, payload
    CONST TABLES   = $82F0      ' 192: the room list, TABLE_REC a room
    CONST AKBUF    = $83B0      ' 72: an AppKey read or written
    CONST MYNAME   = $8400      ' 9: our name, 0 ended
    CONST SRVTBL   = $8410      ' 9: the table (room), 0 ended
    CONST SRVHOST  = $8420      ' 64: the server's host, 0 ended
    CONST STXT     = $8460      ' 48: text built for the screen, 0 ended
    CONST TEST_HOOK = $8FF0     ' 8: the rigs' (input.bas, test.bas)

    ' ---- Intellivision colours ----------------------------------------------
    CONST C_BLACK    = 0
    CONST C_BLUE     = 1
    CONST C_RED      = 2
    CONST C_TAN      = 3
    CONST C_DKGREEN  = 4
    CONST C_GREEN    = 5
    CONST C_YELLOW   = 6
    CONST C_WHITE    = 7
    CONST C_GREY     = 8
    CONST C_CYAN     = 9
    CONST C_ORANGE   = 10
    CONST C_BROWN    = 11
    CONST C_PINK     = 12
    CONST C_LTBLUE   = 13
    CONST C_YELGREEN = 14
    CONST C_PURPLE   = 15

    ' A card's colours in foreground/background mode: foreground 0-7, any
    ' background (bit 13 is background bit 2, bit 12 bit 3: stic.txt).
    DEF FN fgbg(f, b) = ((f) + ((b) AND 3) * $200 + ((b) AND 4) * $800 + ((b) AND 8) * $200)
    ' A cartridge RAM byte (and a mailbox one: their upper bits aren't 0).
    DEF FN rb(a) = (PEEK(a) AND 255)

    CONST NONE = 255

    ' Main-loop scratch (never the frame handler's)
    DIM i, j, k, n, c, r, t, v
    DIM #a, #v, #w
    UNSIGNED #v, #w

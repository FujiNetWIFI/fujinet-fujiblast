' ===========================================================================
' draw.bas -- the screen (gfx.h's): 20x12 cards in foreground/background
' mode (each card one of 8 foreground colours on any of 16 backgrounds),
' text in the GROM's letters, the arena's tiles in GRAM (gen/intv/gfx.bas,
' from gfx/intv/*.png), and the bombers: two MOBs each, the visor (MOB 2s)
' in front of the body (MOB 2s + 1).
'
' The arena: the HUD on row 0 (play.bas), the tiles at columns 4-16 of rows
' 1-11 (a card a tile: an Intellivision card is as big on the screen as
' the Atari's 8x16 tile), walls at columns 3 and 17, the player's stats at
' columns 0-2, the round at 18-19.
' ===========================================================================

    CONST AR_COL = 4
    CONST AR_ROW = 1
    CONST MOB_VIS  = $200       ' X: shown
    CONST MOB_ZX   = $400       ' X: twice as wide
    CONST MOB_ZY2  = $100       ' Y: twice as tall (a row a pixel)
    CONST MOB_FLIP = $400       ' Y: mirrored

    ' text colours (a GROM letter's card is (ASCII - 32) * 8 plus these)
    CONST CW_WHITE  = fgbg(C_WHITE, C_BLACK)
    CONST CW_YELLOW = fgbg(C_YELLOW, C_BLACK)
    CONST CW_TAN    = fgbg(C_TAN, C_BLACK)
    CONST CW_RED    = fgbg(C_RED, C_BLACK)
    CONST CW_GREEN  = fgbg(C_GREEN, C_BLACK)
    CONST CW_BLUE   = fgbg(C_BLUE, C_BLACK)

    DIM gram_set                ' the per-screen GRAM loaded: 0 none, 1 the arena's, 2 the logo
    DIM sp_n, sp_x, sp_y, sp_f, sp_s, sp_big, sp_dy
    DIM #sp_x, #sp_y, #sp_v, #sp_f
    DIM #p, #cw                 ' where text goes, and its colour word
    DIM sd_lf(SEATS), sd_lx(SEATS), sd_ly(SEATS)     ' what SEATS_DRAW (seats.asm) last drew

' gfx_init: the bombers' cards, once.
gfx_init: PROCEDURE
    DEFINE 0, 16, gram_spr
    WAIT
    DEFINE 16, 5, VARPTR gram_spr(64)
    WAIT
    gram_set = 0
END

' gfx_begin: a new screen: blank, no bombers.
gfx_begin: PROCEDURE
    GOSUB spr_hide_all
    CLS
END

' The arena's cards (GRAM 22-44), animated from then on.
gfx_arena_gram: PROCEDURE
    IF gram_set = 1 THEN RETURN
    anim_on = 0
    DEFINE G_TILE0, 16, gram_arena
    WAIT
    DEFINE G_TILE0 + 16, 7, VARPTR gram_arena(64)
    WAIT
    gram_set = 1
    anim_on = 1
END

' The logo's (GRAM 22-57), over the arena's.
gfx_logo_gram: PROCEDURE
    IF gram_set = 2 THEN RETURN
    anim_on = 0
    DEFINE G_LOGO, 16, gram_logo
    WAIT
    DEFINE G_LOGO + 16, 16, VARPTR gram_logo(64)
    WAIT
    DEFINE G_LOGO + 32, 4, VARPTR gram_logo(128)
    WAIT
    gram_set = 2
END

' gfx_logo: FUJIBLAST on rows 1-2, a letter 2x2 cards.
gfx_logo: PROCEDURE
    GOSUB gfx_logo_gram
    FOR i = 0 TO 8
        #a = (G_LOGO + i * 4) * 8
        #BACKTAB(21 + i * 2) = CW_LOGO_TOP + #a
        #BACKTAB(22 + i * 2) = CW_LOGO_TOP + #a + 8
        #BACKTAB(41 + i * 2) = CW_LOGO_BOTTOM + #a + 16
        #BACKTAB(42 + i * 2) = CW_LOGO_BOTTOM + #a + 24
    NEXT i
END

' gfx_cstr: the 0-ended text at #a (cartridge RAM), n characters at most,
' at #p in colour #cw, in capitals (the GROM has no small letters).
gfx_cstr: PROCEDURE
    k = 0
    WHILE k < n
        c = rb(#a + k)
        IF c = 0 THEN EXIT WHILE
        IF c >= 96 THEN c = c - 32
        IF c < 32 THEN c = 32
        #BACKTAB(#p + k) = (c - 32) * 8 + #cw
        k = k + 1
    WEND
END

' gfx_clen: n = the length of the 0-ended text at #a, (n at most).
gfx_clen: PROCEDURE
    k = 0
    WHILE k < n
        IF rb(#a + k) = 0 THEN EXIT WHILE
        k = k + 1
    WEND
    n = k
END

' gfx_num: v in decimal at #p, colour #cw: 1 digit, or 2 if v > 9 (n = digits used).
gfx_num: PROCEDURE
    IF v > 9 THEN
        #BACKTAB(#p) = (16 + v / 10) * 8 + #cw
        #BACKTAB(#p + 1) = (16 + v % 10) * 8 + #cw
        n = 2
    ELSE
        #BACKTAB(#p) = (16 + v) * 8 + #cw
        n = 1
    END IF
END

' gfx_clear_row: row r blank.
gfx_clear_row: PROCEDURE
    FOR i = 0 TO 19
        #BACKTAB(r * 20 + i) = 0
    NEXT i
END

' ---- the bombers --------------------------------------------------------------

' spr_put: bomber sp_n (0-3) at screen pixel sp_x, sp_y (0-159, 0-95),
' frame sp_f (FR_*), in seat sp_s's colour; sp_big: twice the size.
spr_put: PROCEDURE
    #sp_x = sp_x + 8 + MOB_VIS
    #sp_y = sp_y + 8
    #sp_f = fr_flags(sp_f)
    #sp_v = fr_visor(sp_f)
    sp_dy = fr_vdy(sp_f)
    IF sp_big THEN #sp_x = #sp_x + MOB_ZX : #sp_f = #sp_f + MOB_ZY2 : sp_dy = sp_dy * 2
    SPRITE sp_n * 2 + 1, #sp_x, #sp_y + #sp_f, fr_body(sp_f) + seat_col(sp_s)
    sd_lf(sp_n) = 254
    IF #sp_v THEN
        SPRITE sp_n * 2, #sp_x, #sp_y + sp_dy + (#sp_f AND (MOB_FLIP + MOB_ZY2)), #sp_v + VISOR_COL
    ELSE
        SPRITE sp_n * 2, 0, 0, 0
    END IF
END

' spr_arena: bomber sp_n at arena units sp_x, sp_y (16 a tile).
spr_arena: PROCEDURE
    sp_x = AR_COL * 8 + sp_x / 2
    sp_y = AR_ROW * 8 + sp_y / 2
    sp_big = 0
    GOSUB spr_put
END

spr_hide: PROCEDURE
    SPRITE sp_n * 2, 0, 0, 0
    SPRITE sp_n * 2 + 1, 0, 0, 0
    sd_lf(sp_n) = 254
END

spr_hide_all: PROCEDURE
    FOR sp_n = 0 TO 3
        GOSUB spr_hide
        sd_lf(sp_n) = 254                ' (SEATS_DRAW: draw it again)
    NEXT sp_n
END

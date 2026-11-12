' ===========================================================================
' move.bas -- the player's own walking (src/move.c): its state. The walking
' itself is walk.asm's (WALK), a jiffy at a time.
' ===========================================================================

    DIM mv_x, mv_y, mv_dir, mv_acc, mv_bump, pend_tile
    DIM wk_lv, wk_d, moved      ' walk.asm's
    DIM wc, wr, ct

' centre_tile: ct = the tile under the middle of wc, wr (units).
centre_tile: PROCEDURE
    wc = (wc + UNIT / 2) / 16
    wr = (wr + UNIT / 2) / 16
    IF wc >= ARENA_W THEN wc = ARENA_W - 1
    IF wr >= ARENA_H THEN wr = ARENA_H - 1
    ct = wr * ARENA_W + wc
END


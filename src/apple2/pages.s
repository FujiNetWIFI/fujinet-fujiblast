;
; pages.s -- the room the HGR pages take in main RAM ($2000-$5FFF). It is
; zeros in the file, so loading the program blanks them; until the display
; is on, plat.c reads FUJIBLST.GFX through it on its way to aux RAM.
;
        .segment "HGRPAGES"
        .res    $4000

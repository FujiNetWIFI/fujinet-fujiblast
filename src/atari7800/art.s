;
; art.s -- the character blocks' images (tools/mkgfx_a78.py), in the
; banks the window shows one at a time: gfx.c copies them into cart RAM.
; And REPL: the bombers' pixel pages (and nothing else), the same in every
; bank so MARIA can read them whichever one is in.
;
        .export _art_txt, _art_big, _art_gfx, _art_a0, _art_a1

        .segment "ART5"
_art_txt:  .incbin "gen/atari7800/art_txt.bin"
_art_big:  .incbin "gen/atari7800/art_big.bin"
_art_gfx:  .incbin "gen/atari7800/art_gfx.bin"
_art_a0:   .incbin "gen/atari7800/art_anim.bin", 0, 2048

        .segment "ART6"
_art_a1:   .incbin "gen/atari7800/art_anim.bin", 2048, 6144

        .segment "REPL"
        .incbin "gen/atari7800/repl.bin"

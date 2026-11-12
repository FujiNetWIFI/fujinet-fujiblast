; p_art.inc -- the art page: no code, only the pictures, at the places
; artequ.inc's A_* name for every assembly (fixed code borrows this page:
; VBSYNC for the bombers' frames and the arena's animation, UDCSET for a
; screen's own glyphs). tools/arc/mkgfx_arc.py from gfx/arcadia.
        RETC,UN                 ; (no entries)
        INCLUDE "art.inc"       ; (ORG ARTBASE)

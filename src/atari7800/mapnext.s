;
; mapnext.s -- map_next (gfx.h, PLAT_MAP_NEXT): the first tile from `from`
; on where the map and what's drawn differ, or TILES. play_state's scan of
; all 143 tiles in C took a good part of a frame each state.
;
; uint8_t map_next(const uint8_t *map, const uint8_t *drawn, uint8_t from)
; (cc65: from in A; drawn, then map, on the C stack)
;
        .export _map_next
        .import popax
        .importzp ptr1, ptr2, tmp1

TILES = 13 * 11

        .code
.proc   _map_next
        sta     tmp1
        jsr     popax           ; drawn
        sta     ptr2
        stx     ptr2+1
        jsr     popax           ; map
        sta     ptr1
        stx     ptr1+1
        ldy     tmp1
@l:     cpy     #TILES
        bcs     @r
        lda     (ptr1),y
        cmp     (ptr2),y
        bne     @r
        iny
        bne     @l
@r:     tya
        ldx     #0
        rts
.endproc

*
* mapnext.asm -- map_next (gfx.h, PLAT_MAP_NEXT) for the CoCo builds:
* uint8_t map_next(const uint8_t *map, const uint8_t *drawn, uint8_t from)
* the first tile from `from` on where map and drawn differ, or TILES (143).
* CMOC: arguments on the stack (map, drawn, from: a word whose low byte is
* the value), result in B; U is kept.
*
        section code

        export  _map_next

TILES   equ     143

_map_next
        pshs    u
        ldb     9,s             from (after U and the return address: map 4,s, drawn 6,s, from 8,s)
        cmpb    #TILES
        bhs     @none
        clra
        ldx     4,s
        leax    d,x             map + from
        ldu     6,s
        leau    d,u             drawn + from
        negb
        addb    #TILES          tiles left
@l      lda     ,x+
        cmpa    ,u+
        bne     @found
        decb
        bne     @l
@none   ldb     #TILES
        puls    u,pc
@found  negb
        addb    #TILES          TILES - left = the index
        puls    u,pc

        endsect

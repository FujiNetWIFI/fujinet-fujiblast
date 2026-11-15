;
; mapnext.asm -- map_next (gfx.h, PLAT_MAP_NEXT), the CGA and VGA builds'.
;
; map_next(SI map, DI drawn, DX from): AL = the first tile from DX on whose
; map entry differs from what is drawn, or TILES (143). repe cmpsb skips
; the many that match ~10x faster than a C loop.
;
        .8086

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT

TILES   EQU     143
        PUBLIC  map_next_
map_next_ PROC  NEAR
        cld
        mov     ax, ds
        mov     es, ax
        xor     dh, dh
        add     si, dx
        add     di, dx
        mov     cx, TILES
        sub     cx, dx
        jbe     mn_none
        repe    cmpsb
        je      mn_none                 ; all the same
        mov     ax, TILES - 1
        sub     ax, cx
        ret
mn_none:
        mov     al, TILES
        ret
map_next_ ENDP

_TEXT   ENDS
        END

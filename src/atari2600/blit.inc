; blit.inc -- one blit through the cartridge's single slot, then wait for it.
; In: A = source low, X = source high, Y = transform; CBD/CBD+1 = destination,
; CBC = count.  The cartridge has ONE blit slot and no queue: FNBGEN is bumped
; after the blit landed, so poll it before the next (emulation does the blit
; inside the store and hides a missing poll; hardware does not).
FBLIT:  sta     FNRSEL+FH_BSL
        stx     FNRSEL+FH_BSH
        lda     CBD
        sta     FNRSEL+FH_BDL
        lda     CBD+1
        sta     FNRSEL+FH_BDH
        lda     CBC
        sta     FNRSEL+FH_BCNT
        lda     FNBGEN
        sta     MTMP
        tya
        sta     FNRSEL+FH_BGO
        ldx     #0
FBL1:   lda     FNBGEN
        cmp     MTMP
        bne     FBL2
        dex
        bne     FBL1
FBL2:   rts

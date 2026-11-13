; stub.asm -- an unused bank: back to the cold start.
        CPU     6502
        INCLUDE "vcs.inc"
        INCLUDE "fujinet.inc"
        INCLUDE "fbdefs.inc"
        INCLUDE "../../build/a26/tail.inc"
        ORG     $1000
        lda     #ENCOLD
        sta     MENT
        lda     #BKBOOT
        jmp     MGOTO
        END

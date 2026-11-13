; mtail.asm -- the fixed half's code: trampoline, shared transport, cold stub.
; $1800-$1F1F is the cartridge's mailbox; the client owns $1F20-$1FFB and the
; vectors. tools/a26/mktail.py reads the addresses back out of the listing.
        CPU     6502
        INCLUDE "vcs.inc"
        INCLUDE "fujinet.inc"
        INCLUDE "fbdefs.inc"
        INCLUDE "tailcore.inc"
        END

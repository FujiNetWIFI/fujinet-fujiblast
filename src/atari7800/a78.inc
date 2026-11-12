;
; a78.inc -- FujiBlast on the 7800: the hardware and the memory map
; (DESIGN.md; the C side is a78hw.h).
;
; MARIA only (never TIA: a store to $00-$1F rewrites INPTCTRL, which the
; FujiNet cartridge keeps unlocked)
BACKGRND = $20
WSYNC    = $24
MSTAT    = $28          ; bit 7: vertical blank
DPPH     = $2C
DPPL     = $30
CHARBASE = $34
OFFSET   = $38
CTRL     = $3C
P0C1     = $21          ; palette n colour c: $21 + 4n + (c - 1)

POKEY    = $0450
SWCHA    = $0280
SWCHB    = $0282
CTLSWA   = $0281
CTLSWB   = $0283
INPT0    = $08
INPT1    = $09
INPT4    = $0C
BANKSEL  = $8000

ZONES    = 25           ; rows of 8 lines
NDLI     = 28           ; DLIs a screen: a zone each at most, and the frame's
CT_TEXT  = $4B          ; DMA on, border = BACKGRND, 320A, 1-byte characters
CT_CELL  = $58          ; DMA on, border, 160 modes, 2-byte characters

/* OS1.3 CreateProc expects a BCPL segment list, not a C entry pointer. */
        .text
        .balign 4
        .long 16
        .globl _fb_worker_seglist
_fb_worker_seglist:
        .long 0
        jsr _fb_worker_entry
        rts

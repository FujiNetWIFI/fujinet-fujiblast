; p3.inc -- page 3: the overlays' swap, by farcall (O_ARENA, O_TEXT). The
; fixed half's $600-$7FF is the display engine of one kind of screen: the
; text screens' (ov_text.inc: the image boots with it) or the arena's
; (ov_arena.inc). The fixed half is cartridge RAM, so a swap is a copy:
; the display and the interrupts off, an overlay's two pages POKEd in from
; this page (ovdata.inc: each with its copy stub, MOVP reading only its own
; page), then the interrupts on with the counter idle until the vblank arms
; the new overlay's bands. About 4000 cycles: a blank frame. The caller then
; sets its screen up (OBJTAB, ODIRTY, SPRTAB).

    PAGE_HEAD
    db e_arena&0FFh             ; O_ARENA
    db e_text&0FFh              ; O_TEXT
e_arena:
    jmp ov_arena
e_text:
    jmp ov_text

; ov_arena: the arena's overlay in, PAL's band timing poked into it.
; Clobbers A, r0-r6.
ov_arena:
    call ov_off
    mov r2,#6
    clr a
    FCALL poke_at
    mov r4,#arena6&0FFh
    mov r6,#OV_N6
    mov r0,#FN_W_REGDATA
    call arena6_c
    mov r2,#7
    clr a
    FCALL poke_at
    mov r4,#arena7&0FFh
    mov r6,#OV_N7
    mov r0,#FN_W_REGDATA
    call arena7_c
    mov r1,#MODE
    mov a,@r1
    jb7 oa_pal
    jmp ov_on
oa_pal:
    mov r6,#0                   ; (page, address, PAL value) x 4
oa_p:
    mov a,r6
    add a,#paltab&0FFh
    movp a,@a
    mov r2,a
    inc r6
    mov a,r6
    add a,#paltab&0FFh
    movp a,@a
    FCALL poke_at
    inc r6
    mov a,r6
    add a,#paltab&0FFh
    movp a,@a
    FCALL poke_b
    inc r6
    mov a,r6
    xrl a,#4*3
    jnz oa_p
    jmp ov_on

; ov_text: the text screens' overlay in. Clobbers A, r0-r6.
ov_text:
    call ov_off
    mov r2,#6
    clr a
    FCALL poke_at
    mov r4,#text6&0FFh
    mov r6,#OV_N6
    mov r0,#FN_W_REGDATA
    call text6_c
    mov r2,#7
    clr a
    FCALL poke_at
    mov r4,#text7&0FFh
    mov r6,#OV_N7
    mov r0,#FN_W_REGDATA
    call text7_c
    jmp ov_on

; ov_off: the interrupts and the counter stopped, the display off (the VDC
; takes no object writes while it's on: the caller's set-up follows).
; Clobbers A, r0.
ov_off:
    dis i
    dis tcnti
    stop tcnt
    mov a,#P1_VDC
    outl p1,a
    mov r0,#vdc_control
    clr a
    movx @r0,a
    mov a,#P1_MAIN
    outl p1,a
    ret

; ov_on: the counter cleared and idle (T = 0: the vblank arms it before it
; could overflow -- it counts display lines only), the interrupts on.
ov_on:
    jtf oo_c                    ; (clears the timer flag)
oo_c:
    clr a
    mov t,a
    strt cnt
    en tcnti
    en i
    ret

; paltab: the arena's band timing on PAL (equ.inc GPRENP...).
paltab:
    db GW_PRE>>8, GW_PRE&0FFh, GPRENP
    db GW_NOP>>8, GW_NOP&0FFh, GNOPNP
    db GO_PRE>>8, GO_PRE&0FFh, GPRENP
    db VB_FIRST>>8, VB_FIRST&0FFh, 256-BL_FIRSTP

    if $ > 900h
    error "page 3's page $8 has run out"
    endif

    include "ovdata.inc"

    PAGE_TAIL

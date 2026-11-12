; p0.inc -- page 0: the boot. The console set up (the VDC, RAM, PAL or not),
; the FujiNet cartridge there, the lobby's keys read; then the title (p8),
; one way.

    PAGE_HEAD
    db e_boot&0FFh              ; entry 0: the boot (core.inc start)
e_boot:
    jmp boot

boot:
    call vdc_reset
    call ram_clear
    call ints_on
    FARC PG_TLIB,L_CLEAR
    call pal_check
    mov r6,#30                  ; a test scene asked for (a rig's poke)?
bt_t:
    mov r1,#TESTHK
    mov a,@r1
    anl a,#0F0h
    xrl a,#0A0h
    jz bt_test
    call fw_boot
    djnz r6,bt_t
    mov a,#FN_R_MAGIC0&0FFh     ; a FujiNet cartridge, speaking protocol 2?
    call mb_get
    xrl a,#'F'
    jnz bt_nocart
    mov a,#FN_R_MAGIC1&0FFh
    call mb_get
    xrl a,#'N'
    jnz bt_nocart
    mov a,#FN_R_PROTO_VER&0FFh
    call mb_get
    add a,#256-FN_PROTO_VER
    jnc bt_nocart
    mov r0,#FN_W_REGSEL         ; the reply's window at its start
    mov a,#FN_REG_RXWIN
    movx @r0,a
    inc r0
    clr a
    movx @r0,a
    FARC PG_NCTL,K_LOAD
    GOTO PG_MENU,E_TITLE

bt_test:
    GOTO PG_TEST,E_RUN
bt_nocart:
    mov a,#s_nocart&0FFh
    call t0_put
bt_stop:
    call fw_boot
    jmp bt_stop

; ram_clear: external RAM from TMP0 on to 0, KLAST and KEY to none, internal
; RAM $20-$38 to 0. Clobbers A, r0, r2.
ram_clear:
    mov r0,#TMP0
    mov r2,#80h-TMP0
    clr a
rcx_l:
    movx @r0,a
    inc r0
    djnz r2,rcx_l
    mov a,#K_NONE
    mov r0,#KLAST
    movx @r0,a
    mov r0,#KEY
    movx @r0,a
    mov a,#NONE
    mov r0,#MYSEAT
    movx @r0,a
    mov r0,#020h
    mov r2,#FRAMES-020h
    clr a
rci_l:
    mov @r0,a
    inc r0
    djnz r2,rci_l
    ret

; pal_check: a PAL console (MODE_PAL: the swap pokes the arena's band
; timing for it): its frame is a third longer, so a frame of a tight loop
; counts more passes -- with the vblank and the bands running, as they always
; are. The count is left in TMP1:TMP2 (low, high). Clobbers A, r0-r6.
pal_check:
    call fw_boot                ; (a frame to settle)
    call fw_boot
    mov r1,#FRAMES
    mov a,@r1
    mov r2,a
    mov r4,#0
    mov r5,#0
pc_w:                           ; to a frame's start
    mov a,@r1
    xrl a,r2
    jz pc_w
    mov a,@r1
    mov r2,a
pc_l:                           ; 8 cycles a pass
    inc r4
    mov a,r4
    jnz pc_n
    inc r5
pc_n:
    mov a,@r1
    xrl a,r2
    jz pc_l
    mov r0,#TMP1
    mov a,r4
    movx @r0,a
    inc r0
    mov a,r5
    movx @r0,a
    mov a,r4                    ; PALPASS or more: PAL
    add a,#(10000h-PALPASS)&0FFh
    mov a,r5
    addc a,#(10000h-PALPASS)>>8
    jnc pc_x
    mov r1,#MODE
    mov a,@r1
    orl a,#MODE_PAL
    mov @r1,a
pc_x:
    ret

; fw_boot: until the next vblank. Clobbers A, r1, r2.
fw_boot:
    mov r1,#FRAMES
    mov a,@r1
    mov r2,a
fb_l:
    mov a,@r1
    xrl a,r2
    jz fb_l
    ret

ints_on:
    clr a
    mov t,a
    strt cnt
    en tcnti
    en i
    ret

; t0_put: T0 = the string at offset A of page $8, white. Clobbers A, r0-r7.
t0_put:
    mov r2,a
    mov r1,#LINEBUF
    mov r0,#16
    mov a,#' '
tp_s:
    movx @r1,a
    inc r1
    djnz r0,tp_s
    mov r1,#LINEBUF
    mov a,r2
    mov r0,a
tp_c:
    mov a,r0
    movp a,@a
    jz tp_x
    movx @r1,a
    inc r1
    inc r0
    jmp tp_c
tp_x:
    mov a,#C_WHITE
    mov r0,#TXCOL
    movx @r0,a
    FARC PG_TLIB,L_T0
    ret

s_nocart: db "   NO FUJINET",0

    if $ > 900h
    error "page 0's page $8 has run out"
    endif

; ===========================================================================
    org 900h
    include "vdcreset.inc"
    if $ > 0A00h
    error "page 0's page $9 has run out"
    endif

    PAGE_TAIL

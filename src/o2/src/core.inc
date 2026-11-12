; core.inc -- the fixed half's page $4: resident under either overlay
; ($6-$7: ov_arena.inc or the text screens'). The vectors, the start,
; page switching, the mailbox's POKE, the characters' table and writer, the
; sound's countdown.
;
; The 8048 holds A11 at 0 for a whole interrupt routine, so interrupt code
; and every table it reads with MOVP sit in the fixed half ($400-$7FF), each
; table in the 256-byte page of the code that reads it. The fixed half is
; cartridge RAM: the main program writes it through POKE (poke_at, poke_b)
; and never touches the VDC's object registers itself (sprite shapes aside:
; the VDC takes those with the display on).

; ISR_IN / ISR_VDC / ISR_CTL / ISR_OUT: the frame round every interrupt.
; Register bank 1; P1 saved in r6, A in r7.
ISR_IN  macro
    sel rb1
    mov r7,a
    endm
ISR_VDC macro
    in a,p1
    mov r6,a
    mov a,#P1_VDC
    outl p1,a
    endm
ISR_CTL macro val               ; the control register: foreground, grid
    mov r0,#vdc_control
    mov a,#val
    movx @r0,a
    endm
ISR_OUT macro
    mov a,r6
    outl p1,a
    mov a,r7
    retr
    endm

OV_VBL  equ 700h                ; an overlay's vblank entry
OV_TIM  equ 702h                ; ... and its counter's

    org 400h
    jmp start                   ; $400 reset
    jmp OV_VBL                  ; $402 external IRQ: the VDC's vblank
    jmp OV_TIM                  ; $404 counter overflow: a band is due
    jmp start                   ; $406 (the BIOS's own vblank chain: unused)
    jmp start                   ; $408 (after the BIOS's game select: unused)
    retr                        ; $40A
    nop

; start: a console RESET does not reset the cartridge, so the page mapped and
; the fixed half survive it; the boot (page 0) puts back what it needs.
start:                          ; (a reset leaves interrupts off, the counter
    mov a,#P1_MAIN              ; stopped)
    outl p1,a
    clr a
    call sel_page               ; page 0
st_boot:
    jmp 800h                    ; its entry 0 (A = 0): the boot

; sel_page: map page A at $800-$F1F. Main program, P1 = P1_MAIN.
; Clobbers A, r0, r1.
sel_page:
    mov r1,a
    mov r0,#CURPG
    movx @r0,a
    mov r0,#FN_W_REGSEL
    mov a,#FN_REG_CODEBANK
    movx @r0,a
    inc r0
    mov a,r1
    movx @r0,a
    ret

; farcall: entry A of page r7, then the caller's page back. From a page:
;     mov r7,#PG_x / mov a,#E_y / FCALL farcall
; One level: an entry it runs may not farcall. Results come back in r2-r5 or
; external RAM (A and r0, r1, r6 are spent getting home). Clobbers r6.
farcall:
    mov r6,a
    mov r0,#CURPG
    movx a,@r0
    mov r0,#OLDPG
    movx @r0,a
    mov a,r7
    call sel_page
    mov a,r6
    call 800h                   ; the page's dispatcher (page.inc)
    sel mb0
    mov r0,#OLDPG
    movx a,@r0
    call sel_page
    ret

; poke_at: aim POKE at the fixed half's address r2:A (r2 = $4-$7). Leaves the
; register file selecting POKE, so each poke_b after it is one MOVX (and
; inline, `mov r0,#FN_W_REGDATA` once, then `movx @r0,a` a byte).
; Clobbers A, r0, r1.
poke_at:
    mov r1,a
    mov r0,#FN_W_REGSEL
    mov a,#FN_REG_POKE_LO
    movx @r0,a
    inc r0
    mov a,r1
    movx @r0,a
    dec r0
    mov a,#FN_REG_POKE_HI
    movx @r0,a
    inc r0
    mov a,r2
    add a,#0FCh                 ; - 4: offset from $400
    movx @r0,a
; poke_sel: select POKE again, the pointer where it was (a reply read or a
; page switch selects another register). Clobbers A, r0.
poke_sel:
    mov r0,#FN_W_REGSEL
    mov a,#FN_REG_POKE
    movx @r0,a
    ret

; poke_b: store A at the POKE pointer and advance it. Clobbers r0.
poke_b:
    mov r0,#FN_W_REGDATA
    movx @r0,a
    ret

; poke_n: r6 copies of A at the POKE pointer. Clobbers r0, r6.
poke_n:
    call poke_b
    djnz r6,poke_n
    ret

; snd_vbl: the vblank's sound, A = SNDCNT (nonzero), r1 at it. A queued one
; waits for the main loop; a playing one counts down and stops at 0. VDC
; selected. Clobbers A, r0.
snd_vbl:
    jb7 sv_x
    dec a
    mov @r1,a
    jnz sv_x
    mov r0,#vdc_soundctrl
    movx @r0,a
sv_x:
    ret

; OBJTAB: the characters, in the order of the VDC's registers but the quads
; compact -- 12 singles x (Y, X, pointer, control), then 4 quads x (Y, X,
; 4 x (pointer, control)). In the arena quads 0-1 are its pieces' and 2-3 the
; HUD.
OBJTAB:
    db 48 dup (0)
OBJQ:
    db 40 dup (0)

; obj_write: one dirty group of OBJTAB into the VDC (A = ODIRTY, r1 at it):
; the singles first, then quads 0-1, then quads 2-3. VDC selected,
; foreground off. Up to 48 x 9 cycles. Clobbers A, r0-r3.
obj_write:
    jb0 ow_sing
    jb1 ow_qa
    anl a,#(~OD_QH)&0FFh
    mov @r1,a
    mov r1,#(OBJQ+20)&0FFh
    mov r0,#vdc_quad2
    jmp ow_q2
ow_qa:
    anl a,#(~OD_QA)&0FFh
    mov @r1,a
    mov r1,#OBJQ&0FFh
    mov r0,#vdc_quad0
ow_q2:
    mov r3,#2
ow_q:                           ; a quad: Y, X, then 4 x (pointer, control)
    mov r2,#2
ow_yx:
    mov a,r1
    movp a,@a
    movx @r0,a
    inc r0
    inc r1
    djnz r2,ow_yx
    mov r2,#4
ow_qc:
    mov a,r1
    movp a,@a
    movx @r0,a                  ; pointer
    inc r0
    inc r1
    mov a,r1
    movp a,@a
    movx @r0,a                  ; control
    inc r1
    inc r0
    inc r0
    inc r0                      ; (the next character's Y and X: unused)
    djnz r2,ow_qc
    dec r0
    dec r0                      ; the next quad
    djnz r3,ow_q
    ret
ow_sing:
    anl a,#(~OD_SING)&0FFh
    mov @r1,a
    mov r1,#OBJTAB&0FFh
    mov r0,#vdc_char0
    mov r2,#48
ow_l:
    mov a,r1
    movp a,@a
    movx @r0,a
    inc r0
    inc r1
    djnz r2,ow_l
    ret

    if $ > 500h
    error "page $4 has left the page"
    endif

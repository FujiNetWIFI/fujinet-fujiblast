; p9.inc -- page 9: a frame's input and sound, by farcall (IO_FRAME): after
; the frame's last band (frame_sync), the sound queued is started, the
; keyboard (BIOS readkey: only in that quiet stretch) and the stick read,
; the stick's directions repeating while held; M toggles the sound.

    PAGE_HEAD
    db e_frame&0FFh             ; IO_FRAME
e_frame:
    jmp io_frame

io_frame:
    xget FLAGS                  ; muted: nothing plays
    jb0 io_mute
    xget JNOTE                  ; a jingle: asked for, then its notes
    jz io_go
    add a,#256-40h
    jc io_next
    xget JNOTE                  ; asked for: its first note
    add a,#(jstart-SND_JINGLE)&0FFh
    movp a,@a
    add a,#40h
    movx @r0,a
io_next:
    mov r1,#SNDCNT              ; the note before over: the next
    mov a,@r1
    jnz io_go
    xget JNOTE
    add a,#(jnotes-40h)&0FFh
    movp a,@a
    inc a
    jz io_end
    dec a
    orl a,#80h
    mov r1,#SNDCNT
    mov @r1,a
    xinc JNOTE
    jmp io_go
io_mute:
    mov r1,#SNDCNT
    mov a,@r1
    jb7 io_mq
    jmp io_end
io_mq:
    clr a
    mov @r1,a
io_end:
    clr a
    xset JNOTE
io_go:
    call in_poll
    xget KEY
    xrl a,#K_M
    jnz iof_x
    xget FLAGS
    xrl a,#FL_MUTE
    movx @r0,a
    clr a                       ; (and any sound now cut)
    mov r1,#SNDCNT
    mov @r1,a
    mov a,#P1_VDC
    outl p1,a
    mov r0,#vdc_soundctrl
    clr a
    movx @r0,a
    mov a,#P1_MAIN
    outl p1,a
iof_x:
    ret

; the jingles: each one's first note in jnotes, then the notes, 0FFh ends
jstart: db jn_title-jnotes, jn_round-jnotes, jn_win-jnotes, jn_game-jnotes
jnotes:
jn_title: db N_LO, N_MID, N_HI, N_TOP, N_HI, N_END, 0FFh
jn_round: db N_MID, N_MID, N_END, 0FFh
jn_win:   db N_HI, N_TOP, N_HI, N_END, 0FFh
jn_game:  db N_LO, N_MID, N_HI, N_TOP, N_HI, N_TOP, N_END, 0FFh

    if $ > 900h
    error "page 9's page $8 has run out"
    endif

    org 900h
    include "input.inc"
    if $ > 0A00h
    error "page 9's page $9 has run out"
    endif

    org 0A00h
    include "snd.inc"
    if $ > 0B00h
    error "page 9's page $A has run out"
    endif

    PAGE_TAIL

;
; fbasm.asm -- fbnet.c's hot loop: a FujiBus reply taken from the receive
; ring (uartisr.asm fills it), its SLIP undone, its header kept, its data
; put where the request said, its checksum summed. In C this was ~80
; instructions a byte, the most of any byte the game handles.
;
; Small model, near; DS = SS = DGROUP. The reply's state is fbnet.c's
; (_fbc_*); fbnet.c judges the reply when this says it is done.
;
        .8086

DGROUP  GROUP   _DATA, _BSS

_DATA   SEGMENT WORD PUBLIC 'DATA'
_DATA   ENDS
_BSS    SEGMENT WORD PUBLIC 'BSS'
_BSS    ENDS

        EXTRN   _fb_rring:BYTE
        EXTRN   _fb_tring:BYTE
        EXTRN   _fb_thead:BYTE
        EXTRN   _fb_rhead:BYTE
        EXTRN   _fb_rtail:BYTE
        EXTRN   _fbc_hdr:BYTE           ; the reply's 6-byte header
        EXTRN   _fbc_dst:WORD           ; where its data goes
        EXTRN   _fbc_dmax:WORD          ; and how much of it fits
        EXTRN   _fbc_got:WORD           ; bytes so far (the header's included)
        EXTRN   _fbc_sum:WORD           ; their sum, carries folded in (a byte)
        EXTRN   _fbc_inframe:BYTE       ; between its ENDs
        EXTRN   _fbc_esc:BYTE           ; an escape came last

SL_END  EQU     0C0h
SL_ESC  EQU     0DBh
SL_ESCEND EQU   0DCh
SL_ESCESC EQU   0DDh

_TEXT   SEGMENT BYTE PUBLIC 'CODE'
        ASSUME  CS:_TEXT, DS:DGROUP, SS:DGROUP

; fb_collect: the bytes waiting in the ring (255 a call at most). AL = 0:
; the reply isn't all here; 1: it is (an END after some bytes, or its
; length reached): judge it.
        PUBLIC  fb_collect_
fb_collect_ PROC NEAR
        push    si
        push    di
        push    bp
        mov     bl, _fb_rtail
        xor     bh, bh
        mov     dx, _fbc_got
        mov     cl, BYTE PTR _fbc_sum
        xor     ch, ch                  ; bytes this call
        mov     di, _fbc_dst
        mov     si, _fbc_dmax
        mov     ah, _fbc_esc
fc_next:
        cmp     bl, _fb_rhead
        je      fc_wait
        mov     al, _fb_rring[bx]
        inc     bl
        cmp     al, SL_END
        je      fc_end
        cmp     _fbc_inframe, 0
        je      fc_next                 ; (before its opening END)
        test    ah, ah
        jnz     fc_escaped
        cmp     al, SL_ESC
        jne     fc_store
        mov     ah, 1
        jmp     fc_next
fc_escaped:
        xor     ah, ah
        cmp     al, SL_ESCEND
        jne     fc_esc2
        mov     al, SL_END
        jmp     fc_store
fc_esc2:
        cmp     al, SL_ESCESC
        jne     fc_store
        mov     al, SL_ESC
fc_store:
        mov     bp, dx
        cmp     dx, 6
        jae     fc_data
        mov     _fbc_hdr[bp], al        ; (bp: SS, which is DGROUP)
        cmp     dx, 4
        je      fc_counted              ; the checksum itself counts as 0
        jmp     fc_sum
fc_data:
        sub     bp, 6
        cmp     bp, si
        jae     fc_sum                  ; (past what fits: dropped)
        mov     [bp + di], al
fc_sum:
        add     cl, al
        adc     cl, 0
fc_counted:
        inc     dx
        cmp     dx, 6
        jb      fc_more
        cmp     dx, WORD PTR _fbc_hdr + 2
        je      fc_done                 ; its length: whole
fc_more:
        inc     ch
        jnz     fc_next
fc_wait:
        xor     al, al
        jmp     fc_out
fc_end:
        cmp     _fbc_inframe, 0
        je      fc_open
        test    dx, dx
        jnz     fc_done                 ; an END inside: short of its length
fc_open:
        mov     _fbc_inframe, 1         ; the opening END
        jmp     fc_next
fc_done:
        mov     al, 1
fc_out:
        mov     _fb_rtail, bl
        mov     _fbc_got, dx
        mov     BYTE PTR _fbc_sum, cl
        mov     BYTE PTR _fbc_sum + 1, 0
        mov     _fbc_esc, ah
        pop     bp
        pop     di
        pop     si
        ret
fb_collect_ ENDP

; fb_request(AL command, AH fields, SI data, CX its length, 25 at most): a
; FujiBus request (to N1:), its header and checksum made here, SLIP-framed
; into the send ring, which is empty (the last reply is in). fbnet.c then
; lets it go (fb_sent).
DEV_NET EQU     71h
        PUBLIC  fb_request_
fb_request_ PROC NEAR
        push    di
        mov     di, ax                  ; (AL command, AH fields)
        mov     dl, DEV_NET             ; the checksum: the bytes added,
        add     dl, al                  ; carries back in
        adc     dl, 0
        mov     dh, cl
        add     dh, 6                   ; the length (the header's 6 too)
        add     dl, dh
        adc     dl, 0
        add     dl, ah
        adc     dl, 0
        push    si
        push    cx
        jcxz    rq_sum
rq_ck:  add     dl, [si]
        adc     dl, 0
        inc     si
        loop    rq_ck
rq_sum: pop     cx
        pop     si
        mov     bl, _fb_thead
        xor     bh, bh
        mov     al, SL_END
        call    rq_raw
        mov     al, DEV_NET
        call    rq_put
        mov     ax, di
        call    rq_put                  ; the command
        mov     al, dh
        call    rq_put                  ; the length
        xor     al, al
        call    rq_put
        mov     al, dl
        call    rq_put                  ; the checksum
        mov     ax, di
        mov     al, ah
        call    rq_put                  ; the fields
        jcxz    rq_end
rq_data:
        lodsb
        call    rq_put
        loop    rq_data
rq_end: mov     al, SL_END
        call    rq_raw
        mov     _fb_thead, bl
        pop     di
        ret
rq_put: cmp     al, SL_END              ; SLIP: END and ESC escaped
        jne     rq_p1
        mov     al, SL_ESC
        call    rq_raw
        mov     al, SL_ESCEND
        jmp     rq_raw
rq_p1:  cmp     al, SL_ESC
        jne     rq_raw
        call    rq_raw
        mov     al, SL_ESCESC
rq_raw: mov     _fb_tring[bx], al
        inc     bl
        and     bl, 3Fh
        ret
fb_request_ ENDP

_TEXT   ENDS
        END

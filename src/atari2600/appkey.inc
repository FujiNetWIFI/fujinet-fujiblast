; appkey.inc -- the two appkeys, from Combat's cbappk.inc (the OPEN payload's
; sixth, reserved byte is mandatory). Soft: a missing key leaves the default.
AKNAMEK EQU     0
AKROOMK EQU     $10

AKOPEN: sta     MTMP2           ; the key
        lda     #FNDEVF
        sta     FNDEV
        lda     #FCAKOPN
        sta     FNCMD
        lda     #0
        sta     FNNPR
        jsr     FNBEG
        lda     #AKCREAT
        sta     FNTX
        lda     #0
        sta     FNTX
        lda     #AKAPP
        sta     FNTX
        lda     MTMP2
        sta     FNTX
        lda     #AKMRD
        sta     FNTX
        lda     #0
        sta     FNTX
        jmp     AKGO
AKREAD: lda     #FNDEVF
        sta     FNDEV
        lda     #FCAKRD
        sta     FNCMD
        lda     #0
        sta     FNNPR
        jsr     FNBEG
        jmp     AKGO
AKCLOSE: lda    #FNDEVF
        sta     FNDEV
        lda     #FCAKCLS
        sta     FNCMD
        lda     #0
        sta     FNNPR
        jsr     FNBEG
        jsr     AKGO
        lda     #0
        rts
AKGO:   jsr     FNGO
        cmp     #FNEOK
        bne     AKG9
        jsr     FNACK
AKG9:   rts

; AKNAME -- key A is the player's name: up to 8 bytes into NAME.
AKNAME: jsr     AKOPEN
        bne     AKN9
        jsr     AKREAD
        bne     AKN8
        lda     FNRPLY+1
        bne     AKN8
        lda     FNRPLY
        beq     AKN8
        sta     KT0
        ldx     #0
AKN1:   cpx     KT0
        bcs     AKN8
        cpx     #8
        bcs     AKN8
        lda     FNRPLY+2,x
        beq     AKN8
        sta     NAME,x
        inx
        bne     AKN1
AKN8:   jsr     AKCLOSE
AKN9:   rts

; AKURL -- key A is "tcp://host:port/?table=id": the host (scheme and query
; cut off) into path buffer PB_HOST, the table id into TABLE.
AKURL:  jsr     AKOPEN
        bne     AKU9
        jsr     AKREAD
        bne     AKU8
        lda     FNRPLY+1
        bne     AKU8
        lda     FNRPLY
        beq     AKU8
        sta     KT0
        lda     #PB_HOST
        sta     FNRSEL+FH_PATHO
        lda     #FP_RST
        sta     FNRSEL+FH_PATHO
        ldx     #0
AKU1:   cpx     KT0             ; find "://"
        bcs     AKU2
        lda     FNRPLY+2,x
        cmp     #':'
        bne     AKU1N
        lda     FNRPLY+3,x
        cmp     #'/'
        bne     AKU1N
        lda     FNRPLY+4,x
        cmp     #'/'
        bne     AKU1N
        inx
        inx
        inx
        jmp     AKU3
AKU1N:  inx
        bne     AKU1
AKU2:   ldx     #0              ; no scheme: the value is a host
AKU3:   cpx     KT0
        bcs     AKU8
        lda     FNRPLY+2,x
        beq     AKU8
        cmp     #'?'
        beq     AKU5
        sta     FNRSEL+FH_PATHC
        inx
        bne     AKU3
AKU5:   inx                     ; the query: its first '=' starts the table id
        cpx     KT0
        bcs     AKU8
        lda     FNRPLY+2,x
        cmp     #'='
        bne     AKU5
        inx
        ldy     #0
AKU6:   cpx     KT0
        bcs     AKU8
        cpy     #8
        bcs     AKU8
        lda     FNRPLY+2,x
        beq     AKU8
        cmp     #'&'
        beq     AKU8
        sta     TABLE,y
        iny
        inx
        bne     AKU6
AKU8:   jsr     AKCLOSE
AKU9:   rts

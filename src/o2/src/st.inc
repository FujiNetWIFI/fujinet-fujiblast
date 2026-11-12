; st.inc -- the fixed half's page $5: the state the server pushes
; (docs/PROTOCOL.md: 247 bytes, proto.inc's OFF_* layout), resident under
; either overlay. The link POKEs patches straight into it; a page reads it
; through st_get (MOVP reads only the page the code is in).

    org 500h
ST:
    db STATE_SIZE dup (0)

; st_get: A = st[A]. From a page: FCALL st_get. No register touched.
st_get:
    movp a,@a
    ret

    if $ > 600h
    error "page $5 has left the page"
    endif

' ===========================================================================
' fujinet.bas -- the FujiNet cartridge's mailbox: a transaction waited on
' (fn_transact) or posted and looked for later (fn_post, fn_check), the
' network device's open and close, and AppKeys.
'
' From syn-ack-wire's Intellivision client (client/src/intv/fujinet.bas),
' itself from M.U.L.E.'s (clients/intv/fujinet.bas), with M.U.L.E.'s AppKey
' write. The mailbox layout is fujinet-firmware's
' pico/intellivision/firmware/include/fuji_mailbox.h.
'
' MEMATTR stops at $9BFF, short of the mailbox at $9C00: jzIntv's --fujinet
' peripheral registers its handler for $9C00-$9FFF after the cartridge's
' RAM, and whichever registered first answers, so declaring $8000-$9FFF
' would hide the mailbox in the emulator. The real cartridge maps that
' window itself, whatever the .cfg says.
' ===========================================================================

    ASM MEMATTR $8000, $9BFF, "+RWN"

    CONST FN_MAGIC0     = $9C00
    CONST FN_MAGIC1     = $9C01
    CONST FN_SEQ        = $9C03
    CONST FN_ACKSEQ     = $9C04
    CONST FN_DEVICE     = $9C05
    CONST FN_CMD        = $9C06
    CONST FN_NPARAM     = $9C07
    CONST FN_TXLEN_LO   = $9C08
    CONST FN_TXLEN_HI   = $9C09
    CONST FN_ERR        = $9C0B
    CONST FN_RXLEN_LO   = $9C0C
    CONST FN_RXLEN_HI   = $9C0D
    CONST FN_REPLY_CMD  = $9C0E
    CONST FN_PARAM_SIZE = $9C10
    CONST FN_PARAM_VAL  = $9C20
    CONST FN_TX         = $9C40     ' 256 bytes
    CONST FN_RX         = $9D40     ' 512 bytes: the most one READ returns

    CONST FUJICMD_ACK = $06

    ' The Fuji device: AppKeys
    CONST FUJI_DEVICEID        = $70
    CONST FUJICMD_OPEN_APPKEY  = $DC
    CONST FUJICMD_CLOSE_APPKEY = $DB
    CONST FUJICMD_READ_APPKEY  = $DD
    CONST FUJICMD_WRITE_APPKEY = $DE

    ' The network device, N1:
    CONST NET_DEVICEID  = $71
    CONST NETCMD_OPEN   = $4F
    CONST NETCMD_CLOSE  = $43
    CONST NETCMD_READ   = $52
    CONST NETCMD_WRITE  = $57
    CONST NETCMD_STATUS = $53
    CONST OPEN_MODE_RW  = $0C       ' read and write; no translation: the protocol is binary

    ' fn_ok: 1 = the last transaction was ACKed, 0 = timed out or NAKed,
    ' 255 = posted and not answered yet (fn_check). #mb_err: FN_ERR then.
    ' 16-bit on purpose: IntyBASIC 1.4.2 dropped "AND 255" when the target of
    ' "x = PEEK(...) AND 255" was 8-bit.
    DIM fn_ok, #mb_err
    DIM mb_dev, mb_cmd, mb_nparam, mb_seq
    DIM #fn_txlen
    DIM #fn_t                   ' frames (fn_transact) or looks (fn_check) waited
    DIM pm_i, pm_size, #pm_val  ' fn_param

' fn_wait_mailbox: up to 3 s for the cartridge's magic bytes. fn_ok = 1 if
' they came.
fn_wait_mailbox: PROCEDURE
    #fn_t = 0
    WHILE (rb(FN_MAGIC0) <> 70 OR rb(FN_MAGIC1) <> 78) AND #fn_t < 180
        #fn_t = #fn_t + 1
        WAIT
    WEND
    IF #fn_t >= 180 THEN fn_ok = 0 ELSE fn_ok = 1
END

' fn_post: start the transaction staged in mb_dev, mb_cmd, mb_nparam,
' #fn_txlen and the TX buffer. SEQ always follows the cartridge's own ACKSEQ:
' a console reset clears IntyBASIC's variables but not the cartridge.
fn_post: PROCEDURE
    POKE FN_DEVICE, mb_dev
    POKE FN_CMD, mb_cmd
    POKE FN_NPARAM, mb_nparam
    POKE FN_TXLEN_LO, #fn_txlen AND 255
    POKE FN_TXLEN_HI, #fn_txlen / 256
    mb_seq = rb(FN_ACKSEQ) + 1
    IF mb_seq = 0 THEN mb_seq = 1
    POKE FN_SEQ, mb_seq
    #fn_t = 0
END

' fn_check: fn_ok = 255 while the cartridge hasn't answered (900 looks at
' most), else as fn_transact leaves it.
fn_check: PROCEDURE
    IF rb(FN_ACKSEQ) <> mb_seq THEN
        #fn_t = #fn_t + 1
        fn_ok = 255
        IF #fn_t >= 900 THEN fn_ok = 0 : #mb_err = 0
        RETURN
    END IF
    fn_ok = 1
    IF rb(FN_REPLY_CMD) <> FUJICMD_ACK THEN fn_ok = 0 : #mb_err = rb(FN_ERR)
END

' fn_transact: post, then wait for the answer (15 s at most).
fn_transact: PROCEDURE
    GOSUB fn_post
    DO
        WAIT
        GOSUB fn_check
    LOOP WHILE fn_ok = 255
END

' fn_param: parameter pm_i, pm_size bytes (1 or 2) of #pm_val, little-endian.
fn_param: PROCEDURE
    POKE FN_PARAM_SIZE + pm_i, pm_size
    POKE FN_PARAM_VAL + pm_i * 4, #pm_val AND 255
    IF pm_size > 1 THEN POKE FN_PARAM_VAL + pm_i * 4 + 1, #pm_val / 256
END

' fn_open: open the devicespec staged in the TX buffer (#fn_txlen bytes)
' for reading and writing, untranslated.
fn_open: PROCEDURE
    mb_dev = NET_DEVICEID : mb_cmd = NETCMD_OPEN : mb_nparam = 2
    pm_i = 0 : pm_size = 1 : #pm_val = OPEN_MODE_RW : GOSUB fn_param
    pm_i = 1 : pm_size = 1 : #pm_val = 0 : GOSUB fn_param
    GOSUB fn_transact
END

fn_close: PROCEDURE
    mb_dev = NET_DEVICEID : mb_cmd = NETCMD_CLOSE : mb_nparam = 0
    #fn_txlen = 0
    GOSUB fn_transact
END

' fn_status_post: a STATUS. Its answer: bytes waiting (2), connected, error.
fn_status_post: PROCEDURE
    mb_dev = NET_DEVICEID : mb_cmd = NETCMD_STATUS : mb_nparam = 2
    pm_i = 0 : pm_size = 1 : #pm_val = 0 : GOSUB fn_param
    pm_i = 1 : pm_size = 1 : #pm_val = 0 : GOSUB fn_param
    #fn_txlen = 0
    GOSUB fn_post
END

' AppKey ak_key of creator 1, app 1 (the FujiNet game lobby's): ak_open
' with ak_mode 0 (read) or 1 (write), then ak_read or ak_write, then
' ak_close. The open takes exactly 6 bytes (creator lo, hi, app, key, mode,
' 0): 5 times out. A read's answer starts with its length, 2 bytes,
' little-endian; ak_read puts the text at AKBUF, 0 ended, ak_len long.
    DIM ak_key, ak_mode, ak_len

ak_open: PROCEDURE
    mb_dev = FUJI_DEVICEID : mb_cmd = FUJICMD_OPEN_APPKEY : mb_nparam = 0
    POKE FN_TX, 1
    POKE FN_TX + 1, 0
    POKE FN_TX + 2, 1
    POKE FN_TX + 3, ak_key
    POKE FN_TX + 4, ak_mode
    POKE FN_TX + 5, 0
    #fn_txlen = 6
    GOSUB fn_transact
END

ak_read: PROCEDURE
    ak_len = 0
    mb_dev = FUJI_DEVICEID : mb_cmd = FUJICMD_READ_APPKEY : mb_nparam = 0
    #fn_txlen = 0
    GOSUB fn_transact
    IF fn_ok THEN
        #v = rb(FN_RX) + rb(FN_RX + 1) * 256
        IF #v > 64 THEN #v = 64
        ' (a FOR loop runs once even when it shouldn't at all)
        WHILE ak_len < #v
            c = rb(FN_RX + 2 + ak_len)
            IF c = 0 THEN EXIT WHILE
            POKE AKBUF + ak_len, c
            ak_len = ak_len + 1
        WEND
    END IF
    POKE AKBUF + ak_len, 0
END

' ak_write: AKBUF's ak_len bytes.
ak_write: PROCEDURE
    mb_dev = FUJI_DEVICEID : mb_cmd = FUJICMD_WRITE_APPKEY : mb_nparam = 0
    CALL COPYB(AKBUF, FN_TX, ak_len)
    #fn_txlen = ak_len
    GOSUB fn_transact
END

ak_close: PROCEDURE
    mb_dev = FUJI_DEVICEID : mb_cmd = FUJICMD_CLOSE_APPKEY : mb_nparam = 0
    #fn_txlen = 0
    GOSUB fn_transact
END

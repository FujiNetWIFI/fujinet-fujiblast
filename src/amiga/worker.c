/* The sole owner of the released NIO library and its broker reply port.
 * Foreground calls only copy bounded buffers under Forbid. DOS/IO calls
 * always happen after Permit. Control RPCs block only outside active play.
 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <exec/ports.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>
#include <fujinet-nio.h>
#include "nio/settings.h"
#include "worker.h"
#include "queue.h"
#include "perf.h"
#include "proto.h"
#include "stream.h"
#include "history_store.h"
#define RX_SIZE 2048
static struct MsgPort *commands, *replies;
static struct fb_queue tx;
static uint8_t rx[RX_SIZE];
static unsigned rx_head, rx_count;
static volatile uint8_t broken, ended;
static fn_handle_t handle;
static uint32_t roff, woff;
static uint8_t opened;
/* Opt-in worker-only disk I/O. Prefer RAM: to avoid sending diagnostics
 * through the NIO device being diagnosed. No logging under Forbid. */
static char log_path[128];
static FILE *log_file;
static unsigned long log_bytes, reads, writes, read_bytes, write_bytes, io_started, last_summary;
static unsigned tx_high, rx_high, notes_pending, bad_frames;
static unsigned long milliseconds(void)
{
    struct DateStamp d;
    if (!log_path[0]) return 0;
    DateStamp(&d);
    return ((unsigned long)d.ds_Days * 86400UL + d.ds_Minute * 60UL) * 1000UL
           + (unsigned long)d.ds_Tick * 20UL;
}
void fb_stream_pause(unsigned ticks) { Delay(ticks); }
void fb_stream_event(const char *event, uint8_t rc, unsigned wanted,
                     unsigned done, unsigned attempt)
{
    uint8_t stage = 0, result = 0, cause = 0, native = 0;
    uint16_t status = 0;
    fn_write_diagnostics_t wd = {0};
    int n;
    if (!log_path[0]) return;
    if (strstr(event, "error") || !strcmp(event, "open") || !strcmp(event, "init")) {
        fn_amiga_transport_last_broker_detail(&stage, &result);
        fn_amiga_transport_last_broker_cause(&cause);
        fn_amiga_transport_last_serial_detail(&native, &status);
        if (!strncmp(event, "write", 5)) fn_write_get_last_diagnostics(&wd);
    }
    if (log_file && log_bytes > 60000) {
        fclose(log_file); log_file = 0; log_bytes = 0;
    }
    if (!log_file) log_file = fopen(log_path, "w");
    if (!log_file) return;
    n = fprintf(log_file,
        "netlog=1 ms=%lu io_ms=%lu event=%s rc=%u want=%u done=%u attempt=%u "
        "broker=%u/%u cause=%u serial=%u status=%u "
        "exchanges=%u tr_retry=%u svc_retry=%u zero=%u response=%u/%u/%u "
        "roff=%lu woff=%lu reads=%lu writes=%lu rxbytes=%lu txbytes=%lu "
        "txq=%u txhigh=%u rxq=%u rxhigh=%u badframes=%u\n",
        milliseconds(), milliseconds() - io_started, event, rc, wanted, done, attempt,
        stage, result, cause, native, status, wd.exchanges, wd.transport_retry,
        wd.service_retry, wd.zero_accepted, wd.response_length,
        wd.response_device, wd.response_command, (unsigned long)roff,
        (unsigned long)woff, reads, writes, read_bytes, write_bytes,
        tx.count, tx_high, rx_count, rx_high, bad_frames);
    if (n > 0) log_bytes += n;
    fflush(log_file);
}
void amiga_tcp_note(uint8_t reason)
{
    Forbid();
    if (reason == FB_NET_BAD_FRAME) ++bad_frames;
    else notes_pending |= 1u << reason;
    Permit();
}
static void log_notes(void)
{
    unsigned mask;
    Forbid(); mask = notes_pending; notes_pending = 0; Permit();
    if (mask & (1u << FB_NET_IDLE)) fb_stream_event("game-idle-timeout", 0, 900, 0, 0);
    if (mask & (1u << FB_NET_FRAME_OVERFLOW)) fb_stream_event("frame-overflow", 0, 64, 0, 0);
    if (mask & (1u << FB_NET_QUEUE_FULL)) fb_stream_event("tx-queue-full", 0, FB_TX_SLOTS, 0, 0);
}
extern ULONG fb_worker_seglist;
enum { INIT, OPEN, CLOSE, KEY_READ, KEY_WRITE, HISTORY_READ, HISTORY_WRITE, STOP };
struct request {
    struct Message msg;
    unsigned command;
    const char *host;
    uint8_t *data;
    uint16_t port, size;
    uint8_t key, result;
};
static uint8_t rpc(struct request *r)
{
    r->msg.mn_Node.ln_Type = NT_MESSAGE;
    r->msg.mn_Length = sizeof(*r);
    r->msg.mn_ReplyPort = replies;
    PutMsg(commands, &r->msg);
    WaitPort(replies);
    GetMsg(replies);
    return r->result;
}
static void flush(void)
{
    uint8_t buf[256], rc;
    uint16_t written = 0, began;
    unsigned n;
    Forbid();
    n = fb_queue_take(&tx, buf, sizeof buf);
    Permit();
    if (!n || !opened || broken)
        return;
    began = amiga_perf_clock();
    io_started = milliseconds();
    ++writes;
    rc = fb_stream_write(handle, woff, buf, n, &written);
    amiga_perf_add(PERF_WRITE, began);
    woff += written;
    write_bytes += written;
    if (rc != FN_OK || written != n)
        broken = 1;
}
static void close_stream(void)
{
    while (tx.count && opened && !broken)
        flush();
    if (opened) {
        log_notes();
        fb_stream_event("close", broken, 0, ended, 0);
        fn_close(handle);
    }
    opened = 0;
    Forbid();
    memset(&tx, 0, sizeof tx);
    rx_count = rx_head = 0;
    Permit();
}
void fb_worker_entry(void)
{
    struct Process *self = (struct Process *)FindTask(0);
    struct request *r;
    /* Bootstrap once on the process port, then reserve that port for DOS.
     * Delay and file I/O use pr_MsgPort internally: mixing our RPC messages
     * with DOS replies can make either side consume the other's message. */
    WaitPort(&self->pr_MsgPort);
    r = (struct request *)GetMsg(&self->pr_MsgPort);
    commands = CreatePort(0, 0);
    if (!commands) {
        r->result = 0;
        Forbid(); ReplyMsg(&r->msg); return;
    }
    io_started = milliseconds();
    {
        uint8_t rc = fn_init();
        r->result = rc == FN_OK;
        fb_stream_event("init", rc, 0, 0, 0);
    }
    ReplyMsg(&r->msg);
    for (;;) {
        log_notes();
        while ((r = (struct request *)GetMsg(commands))) {
            r->result = 1;
            switch (r->command) {
            case OPEN:
                close_stream();
                broken = ended = 0;
                roff = woff = 0;
                io_started = milliseconds();
                { uint8_t rc = fn_tcp_open(&handle, r->host, r->port);
                  opened = rc == FN_OK;
                  fb_stream_event("open", rc, r->port, 0, 0); }
                r->result = opened;
                break;
            case CLOSE:
                close_stream();
                break;
            case KEY_READ:
                io_started = milliseconds();
                r->result = fb_settings_read(r->key, &r->size, r->data);
                if (!r->result) fb_stream_event("key-read-error", FN_ERR_UNKNOWN, r->key, r->size, 0);
                break;
            case KEY_WRITE:
                io_started = milliseconds();
                r->result = fb_settings_write(r->key, r->size, r->data);
                if (!r->result) fb_stream_event("key-write-error", FN_ERR_UNKNOWN, r->key, r->size, 0);
                break;
            case HISTORY_READ:
                r->result = fb_history_read(r->data, &r->size);
                break;
            case HISTORY_WRITE:
                r->result = fb_history_write(r->data, r->size);
                break;
            case STOP:
                close_stream();
                fn_shutdown();
                if (log_file) { fclose(log_file); log_file = 0; }
                /* Parent may unload our code after the reply. Stay forbidden
                 * through the return into DOS's process-exit trampoline. */
                DeletePort(commands);
                Forbid();
                ReplyMsg(&r->msg);
                return;
            }
            ReplyMsg(&r->msg);
        }
        if (!opened || broken || ended) {
            WaitPort(commands);
        } else {
            uint8_t buf[FB_READ_SIZE], flags = 0, rc;
            unsigned delay = 1;
            uint16_t got = 0, began;
            unsigned i, space;
            flush();
            Forbid();
            space = RX_SIZE - rx_count;
            Permit();
            if (space >= sizeof buf && !broken) {
                began = amiga_perf_clock();
                io_started = milliseconds();
                ++reads;
                rc = fb_stream_read(handle, roff, buf, &got, &flags);
                amiga_perf_add(PERF_READ, began);
                if (rc == FN_OK && got <= sizeof buf) {
                    roff += got;
                    read_bytes += got;
                    if (!got) delay = 3;
                    Forbid();
                    for (i = 0; i < got; ++i)
                        rx[(rx_head + rx_count++) % RX_SIZE] = buf[i];
                    if (rx_count > rx_high) rx_high = rx_count;
                    Permit();
                    if (flags & FN_READ_EOF) {
                        ended = 1;
                        fb_stream_event("tcp-eof", 0, 0, got, 0);
                    }
                } else if (rc != FN_ERR_NOT_READY && rc != FN_ERR_BUSY) {
                    broken = 1;
                    fb_stream_event("read-fatal", rc, sizeof buf, got, 0);
                } else delay = 3;
            }
            if ((unsigned long)(milliseconds() - last_summary) >= 10000UL) {
                last_summary = milliseconds();
                fb_stream_event("summary", 0, 0, 0, 0);
            }
            /* Yield even for an empty, immediately returning device. */
            Delay(delay);
        }
    }
}
int amiga_worker_init(void)
{
    struct request r = {0};
    if (commands)
        return 1;
    {
        const char *path = getenv("FUJIBLAST_NETLOG");
#if defined(AMIGA_LIVE) || defined(AMIGA_ACCEPT)
        if (!path) path = "FB:network.log";
#endif
        if (path && *path && strcmp(path, "OFF")) {
            strncpy(log_path, path, sizeof log_path - 1);
            log_path[sizeof log_path - 1] = 0;
        }
    }
    replies = CreatePort(0, 0);
    if (!replies)
        return 0;
    commands =
        CreateProc((UBYTE *)"FujiBlast network", 0, (BPTR)((ULONG)&fb_worker_seglist >> 2), 16384);
    if (!commands) {
        DeletePort(replies);
        replies = 0;
        return 0;
    }
    r.command = INIT;
    {
        uint8_t ok = rpc(&r);
        if (!commands) { DeletePort(replies); replies = 0; }
        return ok;
    }
}
void amiga_worker_shutdown(void)
{
    struct request r = {0};
    if (!commands)
        return;
    r.command = STOP;
    rpc(&r);
    commands = 0;
    DeletePort(replies);
    replies = 0;
}
uint8_t amiga_tcp_open(const char *host, uint16_t port)
{
    struct request r = {0};
    r.command = OPEN;
    r.host = host;
    r.port = port;
    return commands && rpc(&r);
}
void amiga_tcp_close(void)
{
    struct request r = {0};
    if (!commands)
        return;
    r.command = CLOSE;
    rpc(&r);
}
uint8_t amiga_tcp_send(const uint8_t *data, unsigned n)
{
    int ok;
    /* COBS first nonzero byte following its code is the protocol type. */
    int move = n > 2 && data[0] > 1 && data[1] == SC_MOVE;
    Forbid();
    ok = !broken && fb_queue_put(&tx, data, n, move);
    if (!ok && !broken) {
        broken = 1;
        notes_pending |= 1u << FB_NET_QUEUE_FULL;
    }
    if (tx.count > tx_high) tx_high = tx.count;
    Permit();
    return ok;
}
unsigned amiga_tcp_read(uint8_t *data, unsigned n)
{
    unsigned i;
    Forbid();
    if (n > rx_count)
        n = rx_count;
    for (i = 0; i < n; ++i)
        data[i] = rx[(rx_head + i) % RX_SIZE];
    rx_head = (rx_head + n) % RX_SIZE;
    rx_count -= n;
    Permit();
    return n;
}
uint8_t amiga_tcp_lost(void) { return broken || (ended && !rx_count); }
uint8_t amiga_key_read(uint8_t key, uint16_t *n, uint8_t *data)
{
    struct request r = {0};
    r.command = KEY_READ;
    r.key = key;
    r.data = data;
    if (!commands || !rpc(&r))
        return 0;
    *n = r.size;
    return 1;
}
uint8_t amiga_key_write(uint8_t key, uint16_t n, uint8_t *data)
{
    struct request r = {0};
    r.command = KEY_WRITE;
    r.key = key;
    r.data = data;
    r.size = n;
    return commands && rpc(&r);
}
uint8_t amiga_history_read(uint8_t *data, uint16_t *size)
{
    struct request r = {0};
    r.command = HISTORY_READ;
    r.data = data;
    *size = 0;
    if (!commands || !rpc(&r)) return 0;
    *size = r.size;
    return 1;
}
uint8_t amiga_history_write(uint8_t *data, uint16_t size)
{
    struct request r = {0};
    r.command = HISTORY_WRITE;
    r.data = data;
    r.size = size;
    return commands && rpc(&r);
}

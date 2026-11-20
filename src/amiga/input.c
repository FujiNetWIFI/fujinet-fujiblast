/* OS key messages plus the standard port-2 digital joystick. */
#include <stdint.h>
#include <string.h>
#include <devices/inputevent.h>
#include <devices/console.h>
#include <proto/exec.h>
#include <proto/console.h>
#include <clib/alib_protos.h>
#include "amiga.h"
#include "input.h"
#include "controls.h"
uint8_t inp_dir, inp_btn, ev_btn, ev_key;
uint8_t inp_hook, inp_hook_dir, inp_hook_btn, inp_hook_key;
static uint8_t held[128], prev_btn, latch, key;
static struct MsgPort *port;
static struct IOStdReq *req;
static uint8_t opened;
struct Device *ConsoleDevice;
void input_init(void)
{
    port = CreatePort(0, 0);
    if (port)
        req = (struct IOStdReq *)CreateExtIO(port, sizeof(*req));
    if (req && !OpenDevice((UBYTE *)"console.device", (ULONG)-1, (struct IORequest *)req, 0)) {
        opened = 1;
        ConsoleDevice = req->io_Device;
    }
    if (!opened)
        amiga_fail("Cannot open console.device");
}
void input_close(void)
{
    if (opened)
        CloseDevice((struct IORequest *)req);
    if (req)
        DeleteExtIO((struct IORequest *)req);
    if (port)
        DeletePort(port);
    req = 0;
    port = 0;
    opened = 0;
}
void input_poll(void)
{
    struct IntuiMessage *m;
    while ((m = (struct IntuiMessage *)GetMsg(amiga_win->UserPort))) {
        if (m->Class == IDCMP_INACTIVEWINDOW)
            memset(held, 0, sizeof held);
        if (m->Class == IDCMP_RAWKEY) {
            unsigned code = m->Code & 127;
            held[code] = !(m->Code & 128);
            if (!(m->Code & 128)) {
                struct InputEvent ie;
                char out[8];
                LONG n;
                memset(&ie, 0, sizeof ie);
                ie.ie_Class = IECLASS_RAWKEY;
                ie.ie_Code = m->Code;
                ie.ie_Qualifier = m->Qualifier;
                ie.ie_EventAddress = m->IAddress ? *(APTR *)m->IAddress : 0;
                n = RawKeyConvert(&ie, (UBYTE *)out, sizeof out, 0);
                if (code == 0x50) {
                    key = KEY_SETUP;
                } else if (n == 1) {
                    uint8_t k = out[0];
                    if (k >= 'a' && k <= 'z')
                        k -= 32;
                    key = k;
                }
            }
        }
        ReplyMsg((struct Message *)m);
    }
    inp_dir = amiga_directions(held, *(volatile uint16_t *)0xdff00c);
    inp_btn = held[0x40] || held[0x44] || !(*(volatile uint8_t *)0xbfe001 & 0x80);
    if (inp_hook) {
        inp_dir = inp_hook_dir;
        inp_btn = inp_hook_btn;
        key = inp_hook_key;
        inp_hook_key = 0;
    }
    if (inp_btn && !prev_btn)
        latch = 1;
    prev_btn = inp_btn;
}
void input_take(void)
{
    ev_btn = latch;
    ev_key = key;
    latch = key = 0;
}

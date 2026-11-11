#!/usr/bin/env python3
"""dropproxy.py LISTEN_PORT SERVER_PORT -- a TCP proxy that loses the
client's first frame (its bytes up to the first 0 after a non-0 byte), as a
real FujiNet does while it dials the server after netstream comes on. The
client must still get in (it says HELLO again until answered)."""
import asyncio
import sys


async def pipe(r, w, drop_first):
    dropping, seen = drop_first, False
    try:
        while data := await r.read(4096):
            if dropping:
                out = bytearray()
                for b in data:
                    if dropping:
                        seen = seen or b != 0
                        if seen and b == 0:
                            dropping = False
                            print("dropped the first frame", flush=True)
                        continue
                    out.append(b)
                data = bytes(out)
            if data:
                w.write(data)
                await w.drain()
    finally:
        w.close()


async def handle(cr, cw):
    sr, sw = await asyncio.open_connection("127.0.0.1", int(sys.argv[2]))
    await asyncio.gather(pipe(cr, sw, True), pipe(sr, cw, False), return_exceptions=True)


async def main():
    srv = await asyncio.start_server(handle, "127.0.0.1", int(sys.argv[1]))
    async with srv:
        await srv.serve_forever()


asyncio.run(main())

#!/usr/bin/env python3
"""cutproxy.py LISTEN_PORT SERVER_PORT SECONDS -- a TCP proxy that hangs up
on every client once, SECONDS after it starts (a link lost in the middle
of a game), then carries on: the client must say CONNECTION LOST, dial
again and get its seat back."""
import asyncio
import sys

conns = []


async def pipe(r, w):
    try:
        while data := await r.read(4096):
            w.write(data)
            await w.drain()
    except (ConnectionError, asyncio.CancelledError):
        pass
    finally:
        w.close()


async def handle(cr, cw):
    sr, sw = await asyncio.open_connection("127.0.0.1", int(sys.argv[2]))
    conns.append((cw, sw))
    await asyncio.gather(pipe(cr, sw), pipe(sr, cw))


async def cut(after):
    await asyncio.sleep(after)
    for cw, sw in conns:
        cw.close()
        sw.close()
    print("cut %d links" % len(conns), flush=True)
    conns.clear()


async def main():
    srv = await asyncio.start_server(handle, "127.0.0.1", int(sys.argv[1]))
    asyncio.get_running_loop().create_task(cut(float(sys.argv[3])))
    async with srv:
        await srv.serve_forever()


asyncio.run(main())

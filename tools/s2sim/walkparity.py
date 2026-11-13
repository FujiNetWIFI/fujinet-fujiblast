#!/usr/bin/env python3
"""walkparity.py -- the Studio II walker against src/move.c (walkref.c):
offline scene 1 (src/test.c's canned map), a DEBUG build, a long walk of
ways and diagonals; the TRACE ring read every 25 frames, the jiffies put in
order and replayed.

    tools/s2sim/walkparity.py OUTDIR [IMAGE]

IMAGE defaults to build/studio2-dbg/fujiblst.st2 (DEBUG=1 OUT=build/studio2-dbg/obj
R2R=build/studio2-dbg src/studio2/build.sh); build/walkref must exist.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")


def main():
    out = sys.argv[1]
    image = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "build", "studio2-dbg", "fujiblst.st2")
    lst = os.path.join(os.path.dirname(image), "obj", "fujiblst.lst")
    # a walk: ways and diagonals, into walls and round pillars (lane slides)
    walk = [("6", 30), ("3", 40), ("8", 30), ("9", 50), ("6", 20), ("2", 25), ("1", 40), ("4", 30),
            ("7", 40), ("8", 40), ("3", 30), ("6", 40), ("2", 60), ("9", 30), ("4", 50), ("1", 30),
            ("8", 20), ("6", 25), ("2", 30), ("7", 35), ("6", 30), ("8", 45), ("4", 20), ("2", 40)]
    lines = ["test 1", "wait 30", "speek ST+88 143"]
    for key, n in walk:
        while n > 0:
            k = min(n, 25)
            lines.append("down %s" % key)
            lines.append("wait %d" % k)
            lines.append("speek TRACE 250")
            lines.append("peek TRP")
            n -= k
        lines.append("up %s" % key)
        lines.append("wait 3")
        lines.append("speek TRACE 250")
        lines.append("peek TRP")
    scen = os.path.join(out, "walk.txt")
    os.makedirs(out, exist_ok=True)
    open(scen, "w").write("\n".join(lines) + "\n")
    env = dict(os.environ, S2SIM_LST=lst)
    r = subprocess.run([sys.executable, os.path.join(HERE, "s2sim.py"), scen, out, image],
                       capture_output=True, text=True, env=env)
    mapb, entries, last = None, [], 0
    ring = None
    for line in r.stdout.splitlines():
        m = re.match(r"SPEEK ([0-9A-F]+): (.*)", line)
        if m:
            data = bytes(int(x, 16) for x in m.group(2).split())
            if len(data) == 143:
                mapb = data
            else:
                ring = data
            continue
        m = re.match(r"PEEK [0-9A-F]+: ([0-9A-F]+)", line)
        if m and ring is not None:
            trp = int(m.group(1), 16)
            n = (trp - last) % 250 // 5
            for k in range(n):
                o = (last + 5 * k) % 250
                entries.append(ring[o:o + 5])
            last = trp
            ring = None
    print("walkparity: %d jiffies traced" % len(entries))
    w = subprocess.run([os.path.join(ROOT, "build", "walkref"), mapb.hex(),
                        b"".join(entries).hex()], capture_output=True, text=True)
    print(w.stdout.strip()[-2000:])
    return w.returncode


if __name__ == "__main__":
    sys.exit(main())

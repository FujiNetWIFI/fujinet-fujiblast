#!/usr/bin/env python3
"""h89sim.py -- drive the H89 build in fujinet-go-h89-desktop's headless H89
(M.U.L.E.'s tools/h89sim, for FujiBlast).

usage: h89sim.py DISK MAPFILE SCENARIO OUTDIR

Boots Heath CP/M 2.2.04 (64K) from H17 drive 1 with DISK (r2r/h89-cpm/
fujiblst.h8d) in drive 2 and runs B:FUJIBLST. H89SIM_OS=hdos: HDOS 2.0 from
drive 1, DISK is r2r/h89-hdos/fujiblst.h8d, MOUNT SY1: and RUN
SY1:FUJIBLST.ABS; H89SIM_FN0=1 runs it from the FujiNet's drive FN0: with
FN: loaded instead (H89SIM_BOOT a boot disk with FN.DVD on it: fujinet-h89
tests/make-hdos-boot.sh; H89SIM_LP=1 loads LP: too).

The scenes are TEST_SCENE builds (make h89-cpm TEST_SCENE=n, then touch
src/test.c to go back): they start the scene at once. The scenario, one
command a line (# comments); times are in frames of the game's clock
(frame_count):

    start                       wait for the game's clock to run
    wait N                      let N frames pass
    snap NAME                   OUTDIR/NAME.png (the H19's picture, once the
                                screen is all sent) and NAME.txt (its text)
    hold DIRS                   the stick through inp_hook: u d l r, or none
    press                       the button, through inp_hook
    key CHAR|NUM                a key through inp_hook (RETURN 13, ESC 27)
    type TEXT                   type on the H19's keyboard (\\r, \\x20)
    keyname NAME                press a key the rig names: esc, up, f1...
    peek SYMBOL [N]             print N bytes at a symbol (from the map)
    peekw SYMBOL                a 16-bit word
    poke SYMBOL VALUE
    rate N                      over N frames: draw passes, passes made up,
                                bytes to the terminal, a second each
    sleep MS                    let the machine run MS milliseconds
    pcprof N                    the functions the clock interrupt found the
                                CPU in over N frames (an H89_PROF=1 build)

The machine is h89_session_run (its in-process FujiNet) driven step by step
through --stdin. H89SIM_RIG is fujinet-go-h89-desktop
(~/Workspace/fujinet-go-h89-desktop-main), H89SIM_BOOT the boot disk (its
tools/disks/boot/cpm-2.2.04-64k.h8d). Offline it runs with --warp unless
H89SIM_NOWARP=1. H89SIM_FUJINET=1 is a live game: no warp, and the
FujiNet's SD gets the appkeys first -- H89SIM_NAME (key 0) and H89SIM_ROOM
(key 10h, tcp://host:port/?table=id). Never run two at once: the rig's
ports are fixed.
"""
import os
import re
import subprocess
import sys
import time

RIG = os.environ.get("H89SIM_RIG", os.path.expanduser("~/Workspace/fujinet-go-h89-desktop-main"))
HDOS = os.environ.get("H89SIM_OS") == "hdos"
FN0 = HDOS and os.environ.get("H89SIM_FN0") == "1"
BOOT = os.environ.get("H89SIM_BOOT", os.path.join(RIG, "tools/disks/boot/hdos-2.0.h17disk" if HDOS
                                                  else "tools/disks/boot/cpm-2.2.04-64k.h8d"))
LIVE = os.environ.get("H89SIM_FUJINET") == "1"
# H89SIM_RUN: the command that starts the game (CP/M; B:FUJIBLST), e.g.
# FUJIBLST with H89SIM_BOOT=r2r/h89-cpm/fujiblst-boot.h8d
DIRBITS = {"u": 1, "d": 2, "l": 4, "r": 8}      # src/platform-specific/input.h


def symbols(path):
    syms = {}
    for line in open(path):
        m = re.match(r"(\S+)\s+= \$([0-9A-F]+) ;", line)
        if m:
            syms.setdefault(m.group(1), int(m.group(2), 16))
    return syms


def runner():
    for p in ("build/core/h89_session_run", "build-gnome/core/h89_session_run"):
        if os.path.exists(os.path.join(RIG, p)):
            return os.path.join(RIG, p)
    raise SystemExit("h89sim: no h89_session_run in " + RIG)


def fn0_steps(home, disk):
    """The steps that run FUJIBLST.ABS from the FujiNet's drive FN0:, with FN: loaded."""
    import shutil
    import urllib.request
    shutil.copy(disk, os.path.join(home, "data/fujinet/SD/FUJIBLST.H8D"))
    for st in ("wait 3000", "type \\x20", "wait 500", "type \\x20", "prompt ACTION? <BOOT>",
               "type \\r", "prompt DATE (DD-MMM-YY)?", "type 13-NOV-80\\r", "prompt >"):
        yield st
    urllib.request.urlopen("http://127.0.0.1:11513/mount?hostslot=0&deviceslot=0&mode=1"
                           "&filename=/FUJIBLST.H8D").read()
    yield "type LOAD FN:\\r"
    yield "wait 3000"
    yield "prompt >"
    if os.environ.get("H89SIM_LP") == "1":
        yield "type LOAD LP:\\r"
        yield "wait 3000"
        yield "prompt >"
    yield "type MOUNT FN0:\\r"
    yield "until MOUNTED ON FN0:"
    yield "prompt >"
    yield "type RUN FN0:FUJIBLST.ABS\\r"


class H89:
    def __init__(self, home, disk, warp=True):
        cmd = [runner(), "--roms", os.path.join(RIG, "tools/roms"), "--home", home, "--timeout", "120"]
        if warp:
            cmd.append("--warp")         # (a machine option: before the steps)
        cmd += ["--disk", "h17:1=" + os.path.join(home, "boot.h8d"), "--disk", "h17:2=" + disk, "--stdin"]
        self.p = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True, bufsize=1)
        self.log = open(os.path.join(home, "rig.log"), "w")

    def step(self, s):
        """Run one step; its output lines (FujiNet's log prefix taken off)."""
        self.p.stdin.write(s + "\n")
        self.p.stdin.flush()
        out = []
        while True:
            line = self.p.stdout.readline()
            if not line:
                raise SystemExit("h89sim: the rig went away during: " + s)
            self.log.write(line)
            line = line.rstrip("\n")
            if line.startswith("[fujinet] "):
                line = line[10:]
            # (FujiNet's log shares the output: a step's marker can come at
            # the end of one of its lines)
            if line.endswith("@@ok") or line.endswith("@@fail"):
                if line.endswith("@@fail"):
                    print("h89sim: step failed: " + s, file=sys.stderr)
                return out
            out.append(line)

    def mem(self, addr, n):
        b = []
        while n > 0:
            k = min(n, 128)
            got = []
            for line in self.step("debug mem 0x%X %d" % (addr, k)):
                m = re.match(r"^[0-9A-F]{4}\s+((?:[0-9A-F]{2} )+)", line + " ")
                if m:
                    got += [int(x, 16) for x in m.group(1).split()]
            b += got[:k]
            addr += k
            n -= k
        return b

    def poke(self, addr, *vals):
        self.step("debug poke 0x%X %s" % (addr, " ".join("%d" % v for v in vals)))

    def close(self):
        try:
            self.p.stdin.close()
            self.p.wait(timeout=20)
        except Exception:
            self.p.kill()


def main():
    disk, mapfile, scen, out = sys.argv[1:5]
    out = os.path.abspath(out)
    os.makedirs(out, exist_ok=True)
    home = os.path.join(out, "home")
    os.makedirs(os.path.join(home, "data/fujinet/SD/FujiNet"), exist_ok=True)
    with open(BOOT, "rb") as f, open(os.path.join(home, "boot.h8d"), "wb") as g:
        g.write(f.read())
    sd = os.path.join(home, "data/fujinet/SD/FujiNet")
    for key, var in (("00010100.key", "H89SIM_NAME"), ("00010110.key", "H89SIM_ROOM")):
        path = os.path.join(sd, key)
        if os.environ.get(var):
            open(path, "w").write(os.environ[var])
        elif os.path.exists(path):
            os.remove(path)
    syms = symbols(mapfile)

    def sym(n):
        """A symbol (C names without their _), +offset, or a number."""
        base, _, off = n.partition("+")
        if re.match(r"^0x[0-9A-Fa-f]+$", base) or base.isdigit():
            a = int(base, 0)
        else:
            a = syms["_" + base] if "_" + base in syms else syms[base]
        return a + (int(off, 0) if off else 0)

    warp = not LIVE and not os.environ.get("H89SIM_NOWARP")
    m = H89(home, os.path.abspath(disk), warp=warp)

    def word(n):
        b = m.mem(sym(n), 2)
        return b[0] | b[1] << 8

    def frames():
        return word("frame_count")

    def wait_frames(n):
        t = (frames() + n) & 0xFFFF
        while ((frames() - t) & 0xFFFF) >= 0x8000:
            m.step("wait 50")

    try:
        m.step("until H:")
        m.step("type B\\r")
        if HDOS:
            # (it asks the date on the disk's first boot: a fresh copy each time)
            for st in ("wait 3000", "type \\x20", "wait 500", "type \\x20", "prompt ACTION? <BOOT>",
                       "type \\r", "prompt DATE (DD-MMM-YY)?", "type 13-NOV-80\\r", "prompt >",
                       "type MOUNT SY1:\\r", "wait 2000", "prompt >", "type RUN SY1:FUJIBLST.ABS\\r") \
                    if not FN0 else fn0_steps(home, disk):
                m.step(st)
        else:
            m.step("until A>")
            m.step("wait 500")
            m.step("prompt A>")
            m.step("type %s\\r" % os.environ.get("H89SIM_RUN", "B:FUJIBLST"))
        lines = [l.split("#")[0].strip() for l in open(scen)]
        lines = [l for l in lines if l]
        for line in lines:
            w = line.split()
            c = w[0]
            if c == "start":
                t0 = time.time()
                while frames() < 30:
                    if time.time() - t0 > 240:
                        raise SystemExit("h89sim: the game never started")
                    m.step("wait 200")
            elif c == "wait":
                wait_frames(int(w[1]))
            elif c == "snap":
                t0 = time.time()
                while m.mem(sym("h89_idle"), 1) != [1] and time.time() - t0 < 20:
                    m.step("wait 50")
                ppm = os.path.join(out, w[1] + ".ppm")
                m.step("ppm " + ppm)
                txt = "\n".join(l for l in m.step("screen") if not l.startswith("----")
                                and not re.match(r"\d\d:\d\d:\d\d\.\d+ > |CF: dev:", l)) + "\n"
                # (the rig writes the graphics characters' UTF-8 as \xHH)
                txt = re.sub(r"(?:\\x[0-9A-Fa-f]{2})+",
                             lambda g: bytes.fromhex(g.group(0).replace("\\x", "")).decode("utf-8", "replace"), txt)
                open(os.path.join(out, w[1] + ".txt"), "w").write(txt)
                try:
                    from PIL import Image
                    Image.open(ppm).save(os.path.join(out, w[1] + ".png"))
                    os.remove(ppm)
                except Exception as e:
                    print("h89sim: no PNG: %s" % e, file=sys.stderr)
                print("SNAP %s frame %d" % (w[1], frames()), file=sys.stderr)
            elif c == "hold":
                bits = 0 if w[1] == "none" else sum(DIRBITS[ch] for ch in w[1])
                m.poke(sym("inp_hook"), 1)
                m.poke(sym("inp_hook_dir"), bits)
            elif c == "press":
                m.poke(sym("inp_hook"), 1)
                m.poke(sym("inp_hook_btn"), 1)
                wait_frames(3)
                m.poke(sym("inp_hook_btn"), 0)
            elif c == "key":
                k = int(w[1]) if w[1].isdigit() and len(w[1]) > 1 else ord(w[1][0])
                m.poke(sym("inp_hook"), 1)
                m.poke(sym("inp_hook_key"), k)
                wait_frames(2)
            elif c == "peek":
                n = int(w[2]) if len(w) > 2 else 1
                print("PEEK %s %s" % (w[1], " ".join("%02X" % b for b in m.mem(sym(w[1]), n))),
                      file=sys.stderr)
            elif c == "peekw":
                print("PEEKW %s %d" % (w[1], word(w[1])), file=sys.stderr)
            elif c == "poke":
                m.poke(sym(w[1]), int(w[2], 0))
            elif c == "rate":
                n = int(w[1])
                names = ("h89_drawn", "h89_skip", "h89_passes", "tx_count", "h89_flushes", "h89_sweeps", "h89_cells", "h89_queued", "tx_waits", "h89_idle_ticks", "h89_draw_ticks", "nst_bytes")
                a = [word(x) for x in names]
                f0 = frames()
                wait_frames(n)
                f1 = frames()
                b = [word(x) for x in names]
                secs = ((f1 - f0) & 0xFFFF) / 60.0
                d = [((y - x) & 0xFFFF) / secs for x, y in zip(a, b)]
                print("RATE over %.1f s: drawn %.1f/s, made up %.1f/s, passes %.1f/s, "
                      "line %.0f B/s, flushes %.1f/s, rows %.0f/s, cells %.0f/s, queued %.0f a flush, "
                      "ring full %.0f/s, idle %.0f%%, drawing %.0f%% (%.0f ms a draw), net %.0f B/s; stall %d"
                      % (secs, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] / max(d[4], 0.1), d[8], d[9] / 5.0,
                         d[10] / 5.0, 2.0 * d[10] / max(d[0], 0.1), d[11],
                         m.mem(sym("h89_stall"), 1)[0]),
                      file=sys.stderr)
                m.poke(sym("h89_stall"), 0)
            elif c == "pcprof":
                import bisect
                import collections
                funcs = sorted((a, n) for n, a in syms.items() if n.startswith("_") or n.startswith("l_"))
                addrs = [a for a, n in funcs]
                hist = collections.Counter()
                raw = collections.Counter()
                ring, at_sym = sym("h89_prof"), sym("h89_prof_at")
                prev = m.mem(at_sym, 1)[0]
                t = (frames() + int(w[1])) & 0xFFFF
                while ((frames() - t) & 0xFFFF) >= 0x8000:
                    m.step("wait 150")
                    at = m.mem(at_sym, 1)[0]
                    b = m.mem(ring, 512)
                    i = prev
                    while i != at:
                        pc = b[2 * i] | b[2 * i + 1] << 8
                        k = bisect.bisect_right(addrs, pc) - 1
                        name = (re.sub(r"^l_(.*)_\d+$", r"_\1", funcs[k][1]) if k >= 0 and pc < 0xD000
                                else "(os %04X)" % (pc & 0xFF00))
                        hist[name] += 1
                        raw[pc] += 1
                        i = (i + 1) & 255
                    prev = at
                n = sum(hist.values()) or 1
                print("PCPROF %d samples" % n, file=sys.stderr)
                for name, v in hist.most_common(30):
                    print("  %5.1f%% %s" % (100.0 * v / n, name), file=sys.stderr)
                print("  PCs: " + " ".join("%04X:%d" % (a, v) for a, v in raw.most_common(int(os.environ.get("H89SIM_PCS", "16")))), file=sys.stderr)
            elif c == "raw":
                for l in m.step(line.split(None, 1)[1]):
                    if not re.match(r"\d\d:\d\d:\d\d\.\d+ > ", l):
                        print("RAW " + l, file=sys.stderr)
            elif c == "sleep":
                m.step("wait " + w[1])
            elif c == "keyname":
                m.step("key " + w[1])
            elif c == "type":
                m.step("type " + line.split(None, 1)[1])
            else:
                raise SystemExit("h89sim: unknown command: " + line)
    finally:
        m.close()


if __name__ == "__main__":
    main()

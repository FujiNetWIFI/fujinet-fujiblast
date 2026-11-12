#!/usr/bin/env python3
"""intvsim.py -- FujiBlast's Intellivision ROM headless in jzIntv: a
scenario of presses, the screen as text and as pictures, the stack and
the time each pass takes. After M.U.L.E.'s and syn-ack-wire's
(fujinet-multiplayer-mule clients/intv/tools/intvsim.py, syn-ack-wire
client/tools/intvsim.py).

It drives jzIntv's debugger (-d) over a pipe: a breakpoint on frame_isr
makes each 'r' one frame. Pictures are drawn here from BACKTAB, GRAM, the
GROM and IntyBASIC's MOB shadow (_mobs), in foreground/background mode,
as a TV would show them (8 px of border round 160 x 96, scanlines
doubled). The controller goes through input.bas's TEST_HOOK ($8FF0 = $A5,
$8FF1 = the controller's byte, active high, as CONT reads it).

usage: intvsim.py ROM SCENARIO OUTDIR     (SCENARIO "-": stdin; the .sym beside the ROM)
Scenario lines ('#' starts a comment):
    test SCENE [ARG]        (first line only) test.bas's offline scene SCENE, no network
    wait N                  run N frames
    snap NAME               OUTDIR/NAME.png, and the screen as text
    text                    the screen as text
    key K                   a keypad key, held 4 frames: 0-9, C (CLEAR), E (ENTER)
    btn T|L|R               a button: top, lower left, lower right
    disc D [FRAMES]         the disc (N NE E SE S SW W NW), held 4 frames (or FRAMES)
    hold HEX N              the controller's byte HEX for N frames
    set HEX                 the controller's byte from now on (set 0 lets go)
    until TEXT [MAX]        run until the screen shows TEXT (MAX frames, 3600)
    untilnot TEXT [MAX]     until it doesn't
    untilvar NAME VAL [MAX] until IntyBASIC variable NAME (low byte) is VAL
    film PREFIX EVERY COUNT a picture every EVERY frames, COUNT times
    vars NAME...            IntyBASIC variables
    trace N EVERY NAME...   them every EVERY frames, N times
    peek ADDR N             N words from ADDR (hex)
    poke ADDR VAL...        write them (hex address, decimal values)
    where                   the stack words in the frame handler (the interrupted PC is among them)
    stack                   the most of IntyBASIC's 24-word stack used: its free words were marked $BEEF
                            at the start, and the highest one overwritten since is reported
    prof A B N [VAR...]     N frames, timing each pass from label A to label B (CPU cycles, the frame
                            handler's included): the average, the most, and how many took a frame or more;
                            with VARs, the passes grouped by their values at B
Environment: JZINTV_DIR, JZINTV, FUJINET (host:port; none: no FujiNet), PAL (1: a PAL console).
"""
import os
import re
import subprocess
import sys

JZ = os.environ.get("JZINTV_DIR", os.path.expanduser("~/Workspace/jzintv-20200712-src"))
JZINTV = os.environ.get("JZINTV", os.path.join(JZ, "bin", "linux", "jzintv"))
FUJINET = os.environ.get("FUJINET", "")
HOOK = 0x8FF0
KEYPAD = {"1": 0x81, "2": 0x41, "3": 0x21, "4": 0x82, "5": 0x42, "6": 0x22, "7": 0x84, "8": 0x44,
          "9": 0x24, "C": 0x88, "0": 0x48, "E": 0x28}
BUTTON = {"T": 0xA0, "L": 0x60, "R": 0xC0}
DISC = {"N": 0x04, "NE": 0x16, "E": 0x02, "SE": 0x13, "S": 0x01, "SW": 0x19, "W": 0x08, "NW": 0x1C}
FRAME_CYCLES = 14934

PAL = [(0x00, 0x00, 0x00), (0x00, 0x2D, 0xFF), (0xFF, 0x3D, 0x10), (0xC9, 0xCF, 0xAB),
       (0x38, 0x6B, 0x3F), (0x00, 0xA7, 0x56), (0xFA, 0xEA, 0x50), (0xFF, 0xFC, 0xFF),
       (0xBD, 0xAC, 0xC8), (0x24, 0xB8, 0xFF), (0xFF, 0xB4, 0x1F), (0x54, 0x6E, 0x00),
       (0xFF, 0x4E, 0x57), (0xA4, 0x96, 0xFF), (0x75, 0xCC, 0x80), (0xB5, 0x1A, 0x58)]
REGS = re.compile(r"^ ([0-9A-F]{4}) ([0-9A-F]{4}) ([0-9A-F]{4}) ([0-9A-F]{4}) ([0-9A-F]{4}) "
                  r"([0-9A-F]{4}) ([0-9A-F]{4}) ([0-9A-F]{4}) .*?(\d+)\s*$", re.M)


class Jz:
    def __init__(self, rom, sym):
        env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", SDL_JOYSTICK_HIDAPI="0")
        args = [JZINTV, "-d", "-e", os.path.join(JZ, "rom", "exec.bin"), "-g", os.path.join(JZ, "rom", "grom.bin"),
                "-s0", "-r1"]
        if FUJINET:
            args.append("--fujinet=" + FUJINET)
        if os.environ.get("PAL") == "1":
            args.append("--pal")
        self.p = subprocess.Popen(args + [rom], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, env=env, bufsize=0)
        self.buf = b""
        self.read_prompt()
        self.cmd("l " + sym)
        self.syms = {}
        for ln in open(sym):
            a, _, n = ln.strip().partition(" ")
            if n and re.match(r"^[0-9A-Fa-f]+$", a):
                self.syms[n] = int(a, 16)
        self.isr = self.syms["label_FRAME_ISR"]
        self.cmd("b label_FRAME_ISR")
        self.grom = open(os.path.join(JZ, "rom", "grom.bin"), "rb").read()
        self.frames = 0
        self.sp = self.low_sp = 0
        self.prof = None            # (pc A, pc B, [cycles at A or None], [pass costs])

    def read_prompt(self):
        while True:
            i = self.buf.find(b"\n> ")
            if i < 0 and self.buf.startswith(b"> "):
                self.buf = self.buf[2:]
                return ""
            if i >= 0:
                out, self.buf = self.buf[:i + 1], self.buf[i + 3:]
                return out.decode("latin1")
            ch = os.read(self.p.stdout.fileno(), 65536)
            if not ch:
                raise SystemExit("jzIntv exited:\n" + self.buf.decode("latin1")[-2000:])
            self.buf += ch

    def cmd(self, c):
        self.p.stdin.write((c + "\n").encode())
        self.p.stdin.flush()
        return self.read_prompt()

    def run(self, n=1):
        """n frames (n stops in the frame handler)."""
        done = 0
        while done < n:
            out = self.cmd("r")
            m = re.findall(r"Hit breakpoint at \$([0-9A-F]{4})", out)
            pc = int(m[-1], 16) if m else None
            regs = REGS.findall(out)
            cyc = int(regs[-1][8]) if regs else None
            if regs:
                self.sp = int(regs[-1][6], 16)
            if pc == self.isr or pc is None:
                if regs:
                    self.low_sp = max(self.low_sp, self.sp)
                self.frames += 1
                done += 1
            if self.prof and cyc is not None:
                a, b, start, costs, names = self.prof
                if pc == a:
                    start[0] = cyc
                elif pc == b and start[0] is not None:
                    tag = tuple(self.mem(self.addr(v), 1)[0] & 0xFFFF for v in names)
                    costs.append((cyc - start[0], tag))
                    start[0] = None

    def mem(self, addr, n):
        out = self.cmd("m %x %x" % (addr, n))
        words = {}
        for ln in out.splitlines():
            m = re.match(r"^([0-9A-F]{4}):((?:\s+[0-9A-F]{4}\*?){1,8})", ln)
            if m:
                a = int(m.group(1), 16)
                for i, t in enumerate(re.findall(r"[0-9A-F]{4}", m.group(2))):
                    words[a + i] = int(t, 16)
        return [words.get(addr + i, 0) for i in range(n)]

    def poke(self, addr, val):
        self.cmd("e %x %x" % (addr, val))

    def addr(self, name):
        n = name.upper()
        for s in ("var_" + n, "var_&" + n, "array_" + n, "array_&" + n, "label_" + n, name):
            if s in self.syms:
                return self.syms[s]
        raise SystemExit("no variable " + name)

    def var(self, name):
        return self.mem(self.addr(name), 1)[0]

    def screen_text(self):
        bt = self.mem(0x200, 240)
        rows = []
        for r in range(12):
            s = ""
            for c in range(20):
                w = bt[r * 20 + c]
                if w & 0x800:
                    s += "#"
                else:
                    card = (w >> 3) & 0x3F
                    s += chr(card + 32)
            rows.append(s)
        return rows

    def render(self, path):
        from PIL import Image
        bt = self.mem(0x200, 240)
        gram = [w & 0xFF for w in self.mem(0x3800, 512)]
        mobs = self.mem(self.syms["_mobs"], 24)
        border = self.mem(self.syms["_border_color"], 1)[0] & 15
        img = Image.new("RGB", (176, 208), PAL[border])
        px = img.load()

        def card_bits(g, n):
            if g:
                return gram[(n & 63) * 8:(n & 63) * 8 + 8]
            return self.grom[n * 8:n * 8 + 8]

        fgmask = [[False] * 192 for _ in range(160)]
        for r in range(12):
            for c in range(20):
                w = bt[r * 20 + c]
                g = bool(w & 0x800)
                n = (w >> 3) & 0x3F
                fg = w & 7
                bg = (w >> 9 & 3) | ((w >> 13) & 1) << 2 | ((w >> 12) & 1) << 3
                bits = card_bits(g, n)
                for y in range(8):
                    for x in range(8):
                        on = (bits[y] >> (7 - x)) & 1
                        col = PAL[fg] if on else PAL[bg]
                        for s in (0, 1):
                            px[8 + c * 8 + x, 8 + (r * 8 + y) * 2 + s] = col
                        if on:
                            fgmask[c * 8 + x][(r * 8 + y) * 2] = True
                            fgmask[c * 8 + x][(r * 8 + y) * 2 + 1] = True
        # MOBs, 7 first so 0 ends on top
        for i in range(7, -1, -1):
            X, Y, A = mobs[i], mobs[8 + i], mobs[16 + i]
            if not X & 0x200:
                continue
            x0 = (X & 0xFF) - 8
            wide = 2 if X & 0x400 else 1
            y0 = ((Y & 0x7F) - 8) * 2
            tall = 16 if Y & 0x80 else 8
            ysz = [1, 2, 4, 8][(Y >> 8) & 3]
            xflip, yflip = Y & 0x400, Y & 0x800
            col = PAL[(A & 7) | ((A >> 9) & 8)]
            g = bool(A & 0x800)
            n = (A >> 3) & 0xFF
            if tall == 16:
                n &= ~1
            behind = A & 0x2000
            rows = []
            for k in range(tall // 8):
                rows += card_bits(g, n + k)
            if yflip:
                rows = rows[::-1]
            for ry, b in enumerate(rows):
                for rx in range(8):
                    if not (b >> (7 - rx)) & 1:
                        continue
                    xx = 7 - rx if xflip else rx
                    for dx in range(wide):
                        for dy in range(ysz):
                            sx, sy = x0 + xx * wide + dx, y0 + ry * ysz + dy
                            if 0 <= sx < 160 and 0 <= sy < 192:
                                if behind and fgmask[sx][sy]:
                                    continue
                                px[8 + sx, 8 + sy] = col
        img.resize((176 * 3, 208 * 3 // 2), Image.NEAREST).save(path)

    def press(self, byte, frames=4):
        self.poke(HOOK, 0xA5)
        self.poke(HOOK + 1, byte)
        self.run(frames)
        self.poke(HOOK + 1, 0)
        self.run(3)


def show(jz, name=None):
    if name:
        print("--- %s (frame %d) ---" % (name, jz.frames))
    for r, t in enumerate(jz.screen_text()):
        print("%2d|%s|" % (r, t))


def main():
    rom, scen, outdir = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(outdir, exist_ok=True)
    jz = Jz(rom, rom[:-4] + ".sym")
    lines = open(scen).read().splitlines() if scen != "-" else sys.stdin.read().splitlines()
    first = lines[0].split("#", 1)[0].split() if lines else []
    if first and first[0] == "test":
        jz.poke(HOOK + 2, 0x5A)
        jz.poke(HOOK + 3, 0xA5)
        jz.poke(HOOK + 4, int(first[1]))
        jz.poke(HOOK + 5, int(first[2]) if len(first) > 2 else 0)
        lines = lines[1:]
    jz.run(1)                                           # into the frame handler: past the program's start
    stack = jz.syms["STACK"]                            # IntyBASIC's 24 words: mark what isn't in use yet
    for a in range(jz.sp + 1, stack + 24):
        jz.poke(a, 0xBEEF)
    jz.poke(HOOK, 0xA5)
    jz.poke(HOOK + 1, 0)
    for ln in lines:
        ln = ln.split("#", 1)[0].strip()
        if not ln:
            continue
        op, _, arg = ln.partition(" ")
        a = arg.split()
        if op == "wait":
            jz.run(int(arg))
        elif op == "snap":
            jz.render(os.path.join(outdir, arg + ".png"))
            show(jz, arg)
        elif op == "text":
            show(jz)
        elif op == "key":
            jz.press(KEYPAD[arg.upper()])
        elif op == "btn":
            jz.press(BUTTON[arg.upper()])
        elif op == "disc":
            jz.press(DISC[a[0].upper()], int(a[1]) if len(a) > 1 else 4)
        elif op == "hold":
            jz.press(int(a[0], 16), int(a[1]))
        elif op == "set":
            jz.poke(HOOK, 0xA5)
            jz.poke(HOOK + 1, int(arg, 16))
        elif op in ("until", "untilnot"):
            parts = arg.rsplit(" ", 1)
            mx, text = 3600, arg
            if len(parts) == 2 and parts[1].isdigit():
                text, mx = parts[0], int(parts[1])
            n = 0
            while n < mx:
                jz.run(10)
                n += 10
                if any(text in t for t in jz.screen_text()) == (op == "until"):
                    break
            print("%s %r: %d frames (%s)" % (op, text, n, "ok" if n < mx else "TIMEOUT"))
        elif op == "untilvar":
            mx = int(a[2]) if len(a) > 2 else 3600
            want = int(a[1], 0)
            n = 0
            while n < mx and (jz.var(a[0]) & 0xFF) != want:
                jz.run(5)
                n += 5
            print("untilvar %s=%d: %d frames (%s)" % (a[0], want, n, "ok" if n < mx else "TIMEOUT"))
        elif op == "film":
            for i in range(int(a[2])):
                jz.run(int(a[1]))
                jz.render(os.path.join(outdir, "%s_%03d.png" % (a[0], i)))
        elif op == "vars":
            print("f%d vars" % jz.frames, " ".join("%s=%d" % (v, jz.var(v) & 0xFFFF) for v in a))
        elif op == "trace":
            for _ in range(int(a[0])):
                jz.run(int(a[1]))
                print("f%d" % jz.frames, " ".join("%s=%d" % (v, jz.var(v) & 0xFFFF) for v in a[2:]))
        elif op == "peek":
            print("peek", arg, " ".join("%04X" % w for w in jz.mem(int(a[0], 16), int(a[1]) if len(a) > 1 else 1)))
        elif op == "poke":
            for k, v in enumerate(a[1:]):
                jz.poke(int(a[0], 16) + k, int(v))
        elif op == "where":
            # the stack in the frame handler (the interrupted PC is among the words saved)
            print("where: R6=$%04X" % jz.sp, " ".join("%04X" % w for w in jz.mem(stack, max(1, jz.sp - stack + 1))))
        elif op == "stack":
            words = jz.mem(stack, 24)
            used = max([i for i, w in enumerate(words) if w != 0xBEEF] + [0])
            print("stack: %d of 24 words used at most; R6 in the frame handler at most $%04X"
                  % (used + 1, jz.low_sp))
        elif op == "prof":
            pa, pb = jz.addr(a[0]), jz.addr(a[1])
            jz.cmd("b %x" % pa)
            jz.cmd("b %x" % pb)
            jz.prof = (pa, pb, [None], [], a[3:])
            jz.run(int(a[2]))
            tagged = jz.prof[3]
            costs = [c for c, _ in tagged]
            jz.prof = None
            jz.cmd("n %x" % pa)
            jz.cmd("n %x" % pb)
            if costs:
                srt = sorted(costs)
                print("prof %s-%s: %d passes, average %d cycles, least %d, median %d, most %d, %d of a frame or more"
                      % (a[0], a[1], len(costs), sum(costs) // len(costs), srt[0], srt[len(srt) // 2], srt[-1],
                         sum(1 for c in costs if c >= FRAME_CYCLES)))
                if a[3:]:                       # by the variables' values at B
                    groups = {}
                    for c, tag in tagged:
                        groups.setdefault(tag, []).append(c)
                    for tag in sorted(groups):
                        g = sorted(groups[tag])
                        print("  %s: %d passes, median %d, most %d, %d late" % (
                            " ".join("%s=%d" % kv for kv in zip(a[3:], tag)), len(g), g[len(g) // 2], g[-1],
                            sum(1 for c in g if c >= FRAME_CYCLES)))
            else:
                print("prof %s-%s: no passes" % (a[0], a[1]))
        else:
            raise SystemExit("unknown: " + ln)
    jz.p.stdin.write(b"q\n")
    jz.p.stdin.flush()
    jz.p.wait()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""astrosim.py -- run FujiBlast's Astrocade image headless in MAME from a
scenario. (The Arcadia's tools/arcsim/arcsim.py, for MAME's astrocde with
the FujiNet cart device; symbols from the zmac listing, ASTROSIM_LST,
default build/astrocade/fujiblst.lst.)

    astrosim.py SCENARIO OUTDIR [IMAGE]     (IMAGE: r2r/astrocade/fujiblst.bin)

MAME's astrocde with `-cartslot fujinet` (grafted by fujinet-firmware's
pico/astrocade/emu/apply.sh). The script holds keypad 1 at the BIOS's menu
to start the cart (and jumps to RSTENT if that hasn't started it by frame
400). SCENARIO is a file of steps, one per line (# comments); `-` reads it
from stdin:

    wait N              N frames
    hold KEYS N         hold KEYS for N frames, then let go for 6
    press KEYS          hold 6 frames, let go 6
    down KEYS / up KEYS press and keep holding / let go (no wait)
                        (KEYS: up down left right fire 0-9 ent clr mr ce,
                        joined by +)
    snap NAME           a screenshot, OUTDIR/NAME.png
    burst NAME N        a screenshot every frame for N frames, NAME_000 ...
    text                the screen as text (TEXT lines: the 4x6 font read
                        back from the pixels, 12 rows of 7 lines)
    peek ADDR [LEN]     print memory (ADDR: a number, or a symbol from the
                        listing, NAME or NAME+n)
    poke ADDR V ...     write memory (RAM)
    until ADDR V [N]    wait (at most N frames, default 3600) until ADDR == V
    watch ADDR N [LEN]  for N frames, print ADDR (LEN bytes) when it changes
    trace N ADDR..      for N frames, print every listed byte when any changes
    test SCENE [ARG]    an offline scene (src/test.c): 1 the arena (ARG: b0
                        kick, b1 glove, b4-6 speed), 2 the waiting room,
                        3 a round's end, 4 the game's end
    pc                  the PC
    cpu FILE N          the CPU's instructions for N frames into FILE (MAME's
                        debugger trace, each line led by the page mapped then,
                        loops unfolded; needs ASTROSIM_DEBUG=1; traceprof.py)
    reset               the console's RESET (MAME runs the script again from
                        its start: a scenario tells the runs apart by state)

ASTROSIM_FUJINET=host:port: the fujinet-pc to use (default: none, offline --
never the everyday one on 9995). ASTROSIM_WAV=FILE records the sound.
ASTROSIM_SECONDS: MAME is killed (SIGKILL) past it (default 600).
ASTROSIM_CART=FILE: the cart's image instead (CONFIG's: it boots ours from the
SD). ASTROSIM_THROTTLE=1: real time (a live server's clock is wall time).
Prints its lines prefixed (SNAP PEEK TEXT WATCH UNTIL PC DONE) for a caller
to grep.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
MAME = os.path.expanduser(os.environ.get("MAME", "~/Workspace/mame"))
LST = os.environ.get("ASTROSIM_LST", os.path.join(ROOT, "build", "astrocade", "fujiblst.lst"))
FONT = os.path.join(ROOT, "src", "astrocade", "src", "font46.inc")
LINES, TXH = 84, 7

# port (by its tag's end), mask
KEYS = {
    "up": ("HANDLE", 0x01), "down": ("HANDLE", 0x02),
    "left": ("HANDLE", 0x04), "right": ("HANDLE", 0x08), "fire": ("HANDLE", 0x10),
    "ent": ("KEYPAD0", 0x20),
    "9": ("KEYPAD1", 0x04), "6": ("KEYPAD1", 0x08), "3": ("KEYPAD1", 0x10),
    "8": ("KEYPAD2", 0x04), "5": ("KEYPAD2", 0x08), "2": ("KEYPAD2", 0x10),
    "0": ("KEYPAD2", 0x20),
    "clr": ("KEYPAD3", 0x01), "mr": ("KEYPAD3", 0x02), "7": ("KEYPAD3", 0x04),
    "4": ("KEYPAD3", 0x08), "1": ("KEYPAD3", 0x10), "ce": ("KEYPAD3", 0x20),
}

LUA = r"""
local steps = { %(steps)s }
local testhk = %(testhk)d
local rstent = %(rstent)d
local cpu = manager.machine.devices[":maincpu"]
local mem = cpu.spaces["program"]
local ports = manager.machine.ioport.ports
local frames, i, waitleft = 0, 1, 0
local held, holdleft, relleft = nil, 0, 0
local untila, untilv, untilleft = 0, 0, 0
local burstname, burstk, burstleft = "", 0, 0
local watcha, watchleft, watchlen, watchlast = 0, 0, 1, ""
local teststage, testargs = 0, nil
local tracea, traceleft, tracelast = {}, 0, ""
local cpustop = nil
local booted = false

local function field(suffix, mask)
  for tag, p in pairs(ports) do
    if tag:sub(-#suffix) == suffix then
      for _, f in pairs(p.fields) do
        if f.mask == mask then return f end
      end
    end
  end
  return nil
end
local function setkeys(list, v)
  for _, k in ipairs(list) do
    local f = field(k[1], k[2])
    if f then f:set_value(v) end
  end
end
local function snap(name)
  manager.machine.video:snapshot()
  print("SNAP " .. name .. " " .. frames)
end
local function hex(a, n)
  local t = {}
  for k = 0, n - 1 do t[#t + 1] = string.format("%%02X", mem:read_u8(a + k)) end
  return table.concat(t, "")
end
local function screen()
  for l = 0, %(lines)d - 1 do print(string.format("SCREEN %%02d %%s", l, hex(0x4000 + l * 40, 40))) end
end

_G.ASTROSIM_SUB = emu.add_machine_frame_notifier(function()
  frames = frames + 1
  if not booted then
    if frames == 120 then setkeys({{"KEYPAD3", 0x10}}, 1) end
    if frames == 180 then setkeys({{"KEYPAD3", 0x10}}, 0) end
    local pc = cpu.state["PC"].value
    if frames > 60 and pc >= 0x2000 and pc < 0x3B00 then
      booted = true; print("BOOT " .. frames)
    elseif frames == 400 then
      cpu.state["PC"].value = rstent; print("BOOT forced " .. frames)
    end
    return
  end
  if cpustop and frames >= cpustop then
    manager.machine.debugger:command("trace off,maincpu"); cpustop = nil
    print("CPU " .. frames)
  end
  if burstleft > 0 then
    snap(string.format("%%s_%%03d", burstname, burstk))
    burstk = burstk + 1
    burstleft = burstleft - 1
  end
  if traceleft > 0 then
    local t = {}
    for _, a in ipairs(tracea) do t[#t + 1] = string.format("%%02X", mem:read_u8(a)) end
    local v = table.concat(t, " ")
    if v ~= tracelast then print(string.format("TRACE %%d %%s", frames, v)); tracelast = v end
    traceleft = traceleft - 1
    if traceleft > 0 then return end
  end
  if watchleft > 0 then
    local v = hex(watcha, watchlen)
    if v ~= watchlast then print(string.format("WATCH %%d %%04X %%s", frames, watcha, v)); watchlast = v end
    watchleft = watchleft - 1
    if watchleft > 0 then return end
  end
  if teststage > 0 then
    if teststage == 1 then
      if mem:read_u8(testhk + 1) == 0xC3 then
        mem:write_u8(testhk + 2, testargs[1])
        mem:write_u8(testhk + 3, testargs[2])
        mem:write_u8(testhk, 0x5A)
        teststage = 2
      end
      return
    end
    if mem:read_u8(testhk) == 0xA5 then
      print("TEST " .. frames); teststage = 0
    end
    return
  end
  if holdleft > 0 then
    holdleft = holdleft - 1
    if holdleft == 0 then setkeys(held, 0); relleft = 6 end
    return
  end
  if relleft > 0 then relleft = relleft - 1; return end
  if waitleft > 0 then waitleft = waitleft - 1; return end
  if untilleft > 0 then
    if mem:read_u8(untila) == untilv then
      print(string.format("UNTIL %%04X %%02X %%d", untila, untilv, frames)); untilleft = 0
    else
      untilleft = untilleft - 1
      if untilleft == 0 then print(string.format("UNTIL %%04X timeout (%%02X)", untila, mem:read_u8(untila))) end
      return
    end
  end
  while i <= #steps do
    local s = steps[i]; i = i + 1
    local op = s[1]
    if op == "wait" then waitleft = s[2]; return
    elseif op == "hold" then held = s[2]; setkeys(held, 1); holdleft = s[3]; return
    elseif op == "down" then setkeys(s[2], 1)
    elseif op == "up" then setkeys(s[2], 0)
    elseif op == "snap" then snap(s[2])
    elseif op == "burst" then burstname = s[2]; burstk = 0; burstleft = s[3]; return
    elseif op == "text" then screen()
    elseif op == "peek" then print(string.format("PEEK %%04X: %%s", s[2], hex(s[2], s[3]):gsub("(..)", "%%1 ")))
    elseif op == "poke" then for k = 3, #s do mem:write_u8(s[2] + k - 3, s[k]) end
    elseif op == "until" then untila, untilv, untilleft = s[2], s[3], s[4]; return
    elseif op == "watch" then watcha, watchleft, watchlen, watchlast = s[2], s[3], s[4], ""; return
    elseif op == "trace" then tracea, traceleft, tracelast = s[2], s[3], ""; return
    elseif op == "test" then testargs = { s[2], s[3] }; teststage = 1; return
    elseif op == "cpu" then
      manager.machine.debugger:command('trace ' .. s[2] .. ',maincpu,noloop,{tracelog "%%02X ",b@%(curpg)x}')
      cpustop = frames + s[3]; return
    elseif op == "pc" then print(string.format("PC %%04X", cpu.state["PC"].value))
    elseif op == "reset" then
      print("RESET " .. frames); manager.machine:soft_reset()
      i = #steps + 2; return     -- (MAME starts the script again: this run stands down)
    end
  end
  if i > #steps + 1 then return end
  print("DONE " .. frames)
  manager.machine:exit()
end)
"""


def symbols(lst):
    text = open(lst, encoding="utf-8", errors="replace").read()
    tail = text[text.rfind("Symbol Table:"):]
    syms = {}
    for name, val in re.findall(r"\b([a-z_][a-z0-9_]*)\s+=?\s*([0-9a-f]{1,5})\+?", tail):
        syms.setdefault(name.upper(), int(val, 16))
    return syms


SYMS = {}


def num(t):
    """A number ($hex, 0x.., decimal) or a symbol from the listing (NAME or NAME+n)."""
    base, _, add = t.partition("+")
    if base and (base[0].isalpha() or base[0] == "_") and not base.lower().startswith("0x"):
        if base.upper() not in SYMS:
            sys.exit("astrosim: no symbol %s" % base)
        return SYMS[base.upper()] + (int(add, 0) if add else 0)
    return int(t.replace("$", "0x"), 0)


def keys(spec):
    out = []
    for k in spec.split("+"):
        if k not in KEYS:
            sys.exit("astrosim: no key %r" % k)
        out.append('{"%s",%d}' % KEYS[k])
    return "{" + ",".join(out) + "}"


def font():
    """FONT46's glyphs (20H-5FH) as tuples of 6 high nibbles."""
    vals = [int(m, 16) for m in re.findall(r"DB\s+0?([0-9A-Fa-f]+)H", open(FONT).read())]
    return {tuple(v >> 4 for v in vals[g * 6:g * 6 + 6]): chr(0x20 + g) for g in range(64)}


def text(lines):
    """The screen's text rows from SCREEN lines: each cell's 4x6 pixels against
    the font, ink = any colour but the cell's ground (its bottom-right pixel's
    row under the glyph: line 7 of the cell)."""
    pix = {}
    for l, h in lines.items():
        row = bytes.fromhex(h)
        pix[l] = [(row[x // 4] >> (6 - 2 * (x % 4))) & 3 for x in range(160)]
    glyphs = font()
    out = []
    for r in range(LINES // TXH):
        s = ""
        for c in range(40):
            ground = pix[r * TXH + 6][c * 4] if r * TXH + 6 in pix else 0
            nib = tuple(sum((pix[r * TXH + y][c * 4 + x] != ground) << (3 - x) for x in range(4))
                        for y in range(6))
            s += glyphs.get(nib, "?" if any(nib) else " ")
        out.append("TEXT %2d |%s|" % (r, s))
    return out


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    scen, outdir = sys.argv[1], sys.argv[2]
    image = sys.argv[3] if len(sys.argv) > 3 else os.path.join(ROOT, "r2r", "astrocade", "fujiblst.bin")
    image = os.path.abspath(image)
    SYMS.update(symbols(LST))
    lines = (sys.stdin if scen == "-" else open(scen)).read().splitlines()
    steps = []
    for line in lines:
        line = line.split("#")[0].strip()
        if not line:
            continue
        w = line.split()
        op = w[0]
        if op == "wait":
            steps.append('{"wait",%d}' % int(w[1]))
        elif op == "hold":
            steps.append('{"hold",%s,%d}' % (keys(w[1]), int(w[2])))
        elif op in ("down", "up"):
            steps.append('{"%s",%s}' % (op, keys(w[1])))
        elif op == "press":
            steps.append('{"hold",%s,6}' % keys(w[1]))
        elif op == "snap":
            steps.append('{"snap","%s"}' % w[1])
        elif op == "burst":
            steps.append('{"burst","%s",%d}' % (w[1], int(w[2])))
        elif op == "cpu":
            steps.append('{"cpu","%s",%d}' % (os.path.abspath(w[1]), int(w[2])))
        elif op in ("text", "pc", "reset"):
            steps.append('{"%s"}' % op)
        elif op == "peek":
            steps.append('{"peek",%d,%d}' % (num(w[1]), int(w[2]) if len(w) > 2 else 1))
        elif op == "poke":
            steps.append('{"poke",%d,%s}' % (num(w[1]), ",".join(str(num(x)) for x in w[2:])))
        elif op == "until":
            steps.append('{"until",%d,%d,%d}' % (num(w[1]), num(w[2]), int(w[3]) if len(w) > 3 else 3600))
        elif op == "watch":
            steps.append('{"watch",%d,%d,%d}' % (num(w[1]), int(w[2]), int(w[3]) if len(w) > 3 else 1))
        elif op == "trace":
            steps.append('{"trace",{%s},%d}' % (",".join(str(num(x)) for x in w[2:]), int(w[1])))
        elif op == "test":
            a = [int(x, 0) for x in w[1:]] + [0, 0]
            steps.append('{"test",%d,%d}' % (a[0], a[1]))
        else:
            sys.exit("astrosim: no step %r" % op)
    os.makedirs(outdir, exist_ok=True)
    lua = LUA % {"steps": ",".join(steps), "testhk": SYMS["TESTHK"],
                 "rstent": SYMS["RSTENT"], "lines": LINES, "curpg": SYMS["CURPG"]}
    luafile = os.path.join(outdir, "astrosim.lua")
    open(luafile, "w").write(lua)
    env = dict(os.environ)
    env["FUJINET_TCP"] = os.environ.get("ASTROSIM_FUJINET", "127.0.0.1:9")
    env.setdefault("SDL_AUDIODRIVER", "dummy")
    throttle = [] if os.environ.get("ASTROSIM_THROTTLE") else ["-nothrottle"]
    cart = ["-cart", os.path.abspath(os.environ.get("ASTROSIM_CART") or image)]
    cmd = [os.path.join(MAME, "mame"), "astrocde", "-rompath", os.path.join(MAME, "roms"),
           "-cartslot", "fujinet"] + cart + [
           "-autoboot_script", luafile] + throttle + ["-video", "none",
           "-snapshot_directory", outdir, "-snapname", "%i",
           "-cfg_directory", os.path.join(outdir, "cfg"), "-nvram_directory", os.path.join(outdir, "nvram"),
           "-skip_gameinfo", "-nomouse"]
    if os.environ.get("ASTROSIM_DEBUG"):
        cmd += ["-debug", "-debugger", "none"]
    wav = os.environ.get("ASTROSIM_WAV")
    if wav:
        cmd += ["-wavwrite", os.path.abspath(wav)]
    else:
        cmd += ["-sound", "none"]
    secs = int(os.environ.get("ASTROSIM_SECONDS", "600"))
    r = subprocess.run(["timeout", "-s", "KILL", str(secs)] + cmd, env=env, cwd=outdir,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    screen = {}
    for l in r.stdout.splitlines():
        if l.startswith("SCREEN "):
            _, n, h = l.split()
            screen[int(n)] = h
            if int(n) == LINES - 1:
                print("\n".join(text(screen)))
                screen = {}
        else:
            print(l)
    # MAME numbers its snapshots 0000.png, ... in order: give them their names
    names = [l.split()[1] for l in r.stdout.splitlines() if l.startswith("SNAP ")]
    shots = sorted(f for f in os.listdir(outdir) if f[:4].isdigit() and f.endswith(".png"))
    for f, n in zip(shots, names):
        os.replace(os.path.join(outdir, f), os.path.join(outdir, n + ".png"))


if __name__ == "__main__":
    main()

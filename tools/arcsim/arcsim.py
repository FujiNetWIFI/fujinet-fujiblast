#!/usr/bin/env python3
"""arcsim.py -- run FujiBlast's Arcadia image headless in MAME from a
scenario. (M.U.L.E.'s clients/arcadia/tools/arcsim.py, for this build:
listings in ARCSIM_LST, default build/arcadia; the test step is
src/test.c's hook.)

    arcsim.py SCENARIO OUTDIR [IMAGE]       (IMAGE: r2r/arcadia/fujiblst.bin)

MAME's arcadia with the FujiNet cart device (`-cartslot fujinet`, grafted by
fujinet-firmware's pico/arcadia/emu/apply.sh). SCENARIO is a file of steps,
one per line (# comments); `-` reads it from stdin:

    wait N              N frames
    hold KEYS N         hold KEYS for N frames, then let go for 6
    press KEYS          hold 6 frames, let go 6
    down KEYS / up KEYS press and keep holding / let go (no wait)
                        (KEYS: up down left right fire 0-9 ent clr start opt
                        sel, joined by +)
    snap NAME           a screenshot, OUTDIR/NAME.png
    burst NAME N        a screenshot every frame for N frames, NAME_000 ...
    text                the screen as text (TEXT lines, 13 rows; '#' a
                        block-graphics row's cells; user glyphs: @ the
                        sprites', then X o = | (the arena's brick, bomb,
                        flames; a text screen's > - : and trophy))
    peek ADDR [LEN]     print memory (any address the CPU sees; ADDR may be a
                        symbol from the listings, NAME or NAME+n: the boot
                        page's first, then the other pages')
    poke ADDR V ...     write memory (console RAM)
    until ADDR V [N]    wait (at most N frames, default 3600) until ADDR == V
    watch ADDR N [LEN]  for N frames, print ADDR (LEN bytes) when it changes
    trace N ADDR.. [@ADDR=V=NAME ..]
                        for N frames, print every listed byte when any of them
                        changes (TRACE frame values); each @ADDR=V=NAME[+D] takes
                        a screenshot NAME_k D frames after ADDR becomes V
    test SCENE [ARG]    an offline scene (src/test.c): 1 the arena (ARG: b0
                        kick, b1 glove, b4-6 speed), 2 the waiting room,
                        3 a round's end, 4 the game's end
    pc                  the PC
    cpu FILE N          the CPU's instructions for N frames into FILE (MAME's
                        debugger trace; needs ARCSIM_DEBUG=1; see traceprof.py)
    reset               the console's RESET (a soft reset): MAME runs the
                        script again from its start (the frame count too),
                        so a scenario tells the runs apart by state

ARCSIM_FUJINET=host:port: the fujinet-pc to use (default: none, offline --
never the everyday one on 9995). ARCSIM_WAV=FILE records the sound.
ARCSIM_SECONDS: MAME is killed (SIGKILL) past it (default 600).
ARCSIM_CART=FILE: the cart's image instead (CONFIG's: it boots ours from the
SD; MAME's cart wants an image where the cart's firmware has CONFIG baked
in). ARCSIM_THROTTLE=1: real time (a live server's clock is wall time). Prints its
lines prefixed (SNAP PEEK TEXT WATCH UNTIL PC DONE) for a caller to grep.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MAME = os.path.expanduser(os.environ.get("MAME", "~/Workspace/mame"))
TESTHK = None   # read from the listings: the title page's test hook

# port, mask
KEYS = {
    "left": (":joysticks", 0x01), "right": (":joysticks", 0x02),
    "down": (":joysticks", 0x04), "up": (":joysticks", 0x08),
    "1": (":controller1_col1", 0x08), "4": (":controller1_col1", 0x04),
    "7": (":controller1_col1", 0x02), "clr": (":controller1_col1", 0x01),
    "fire": (":controller1_col2", 0x08), "2": (":controller1_col2", 0x08),
    "5": (":controller1_col2", 0x04), "8": (":controller1_col2", 0x02),
    "0": (":controller1_col2", 0x01),
    "3": (":controller1_col3", 0x08), "6": (":controller1_col3", 0x04),
    "9": (":controller1_col3", 0x02), "ent": (":controller1_col3", 0x01),
    "start": (":panel", 0x01), "opt": (":panel", 0x02), "sel": (":panel", 0x04),
}

LUA = r"""
local steps = { %(steps)s }
local out = "%(out)s"
local testhk = %(testhk)d
local cpu = manager.machine.devices[":maincpu"]
local mem = cpu.spaces["program"]
local ports = manager.machine.ioport.ports
local frames, i, waitleft = 0, 1, 0
local held, holdleft, relleft = nil, 0, 0
local untila, untilv, untilleft = 0, 0, 0
local burstname, burstk, burstleft = "", 0, 0
local watcha, watchleft, watchlen, watchlast = 0, 0, 1, ""
local teststage, testargs = 0, nil
local pend = {}
local tracea, tracet, traceleft, tracelast, tracek = {}, {}, 0, "", 0
local cpustop = nil

local function field(port, mask)
  local p = ports[port]
  if p == nil then return nil end
  for _, f in pairs(p.fields) do
    if f.mask == mask then return f end
  end
  return nil
end
local function setkeys(list, v)
  for _, k in ipairs(list) do
    local f = field(k[1], k[2])
    if f then f:set_value(v) end
  end
end
function snap(name)
  manager.machine.video:snapshot()
  print("SNAP " .. name .. " " .. frames)
end
local GL = " /\\#^|_|::::<><>0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.,+$@@@@Xo=|"
local function text()
  for r = 0, 12 do
    local base = 0x1800 + r * 16
    local s, gfx = "", false
    for c = 0, 15 do
      local v = mem:read_u8(base + c)
      if v == 0xC0 then gfx = true end
      if v == 0x40 then gfx = false end
      if gfx then s = s .. "#" else
        local g = v & 0x3F
        s = s .. string.sub(GL, g + 1, g + 1)
      end
    end
    print(string.format("TEXT %%2d |%%s|", r, s))
  end
end
local function hex(a, n)
  local t = {}
  for k = 0, n - 1 do t[#t + 1] = string.format("%%02X", mem:read_u8(a + k)) end
  return table.concat(t, " ")
end

_G.ARCSIM_SUB = emu.add_machine_frame_notifier(function()
  frames = frames + 1
  if cpustop and frames >= cpustop then
    manager.machine.debugger:command("trace off,maincpu"); cpustop = nil
    print("CPU " .. frames)
  end
  if burstleft > 0 then
    snap(string.format("%%s_%%03d", burstname, burstk))
    burstk = burstk + 1
    burstleft = burstleft - 1
  end
  for k = #pend, 1, -1 do
    if frames >= pend[k][1] then snap(pend[k][2]); table.remove(pend, k) end
  end
  if traceleft > 0 then
    local t = {}
    for _, a in ipairs(tracea) do t[#t + 1] = string.format("%%02X", mem:read_u8(a)) end
    local v = table.concat(t, " ")
    if v ~= tracelast then
      print(string.format("TRACE %%d %%s", frames, v))
      for _, tr in ipairs(tracet) do
        if mem:read_u8(tr[1]) == tr[2] and (tracelast == "" or tr.was ~= tr[2]) then
          tracek = tracek + 1
          pend[#pend + 1] = { frames + tr[4], string.format("%%s_%%d", tr[3], tracek) }
        end
        tr.was = mem:read_u8(tr[1])
      end
      tracelast = v
    end
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
    elseif op == "text" then text()
    elseif op == "peek" then print(string.format("PEEK %%04X: %%s", s[2], hex(s[2], s[3])))
    elseif op == "poke" then for k = 3, #s do mem:write_u8(s[2] + k - 3, s[k]) end
    elseif op == "until" then untila, untilv, untilleft = s[2], s[3], s[4]; return
    elseif op == "watch" then watcha, watchleft, watchlen, watchlast = s[2], s[3], s[4], ""; return
    elseif op == "trace" then
      tracea, tracet, traceleft, tracelast = s[2], s[3], s[4], ""; return
    elseif op == "test" then testargs = { s[2], s[3], s[4] }; teststage = 1; return
    elseif op == "cpu" then
      manager.machine.debugger:command("trace " .. s[2] .. ",maincpu")
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


SYMS = {}
LST = os.environ.get("ARCSIM_LST", os.path.join(HERE, "..", "..", "build", "arcadia"))


def load_syms(image, page=0):
    import re
    lst = os.path.join(LST, "fb_p%d.lst" % page)
    for line in open(lst, errors="replace"):
        for m in re.finditer(r"\b([A-Z_][A-Z0-9_]*)\s*:\s*([0-9A-F]+) [C-]", line):
            SYMS.setdefault(m.group(1), int(m.group(2), 16))


def num(t):
    """A number ($hex, 0x.., decimal) or a symbol from the listing (NAME or NAME+n)."""
    base, _, add = t.partition("+")
    if base and (base[0].isalpha() or base[0] == "_") and base.upper() == base and base not in ("0X",):
        if base not in SYMS:
            sys.exit("arcsim: no symbol %s" % base)
        return SYMS[base] + (int(add, 0) if add else 0)
    return int(t.replace("$", "0x"), 0)


def keys(spec):
    out = []
    for k in spec.split("+"):
        if k not in KEYS:
            sys.exit("arcsim: no key %r" % k)
        out.append('{"%s",%d}' % KEYS[k])
    return "{" + ",".join(out) + "}"


def testhook(image):
    """TESTHK from the listings (build/arcadia/fb_p*.lst)."""
    import glob
    import re
    for lst in sorted(glob.glob(os.path.join(LST, "fb_p*.lst"))):
        for line in open(lst, errors="replace"):
            m = re.search(r"\bTESTHK\s*:\s*([0-9A-F]+)\b", line)
            if m:
                return int(m.group(1), 16)
    sys.exit("arcsim: no TESTHK in the listings")


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    scen, outdir = sys.argv[1], sys.argv[2]
    image = sys.argv[3] if len(sys.argv) > 3 else os.path.join(HERE, "..", "..", "r2r", "arcadia", "fujiblst.bin")
    image = os.path.abspath(image)
    load_syms(image)                    # the boot page's first, then each page's own
    import glob
    for lst in sorted(glob.glob(os.path.join(LST, "fb_p*.lst"))):
        load_syms(image, int(lst.rsplit("_p", 1)[1].split(".")[0]))
    lines = (sys.stdin if scen == "-" else open(scen)).read().splitlines()
    steps = []
    for line in lines:
        line = line.split("#")[0].strip()
        if not line:
            continue
        w = line.split()
        op = w[0]
        if op in ("wait",):
            steps.append('{"wait",%d}' % int(w[1]))
        elif op == "hold":
            steps.append('{"hold",%s,%d}' % (keys(w[1]), int(w[2])))
        elif op in ("down", "up"):
            steps.append('{"%s",%s}' % (op, keys(w[1])))
        elif op == "press":
            steps.append('{"hold",%s,6}' % keys(w[1]))
        elif op in ("snap",):
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
            addrs = [num(x) for x in w[2:] if not x.startswith("@")]
            trig = []
            for x in w[2:]:
                if x.startswith("@"):
                    ad, v, nm = x[1:].split("=")
                    nm, _, dl = nm.partition("+")
                    trig.append('{%d,%d,"%s",%d}' % (num(ad), num(v), nm, int(dl or 0)))
            steps.append('{"trace",{%s},{%s},%d}' % (",".join(map(str, addrs)), ",".join(trig), int(w[1])))
        elif op == "test":
            a = [int(x, 0) for x in w[1:]] + [0, 0]
            steps.append('{"test",%d,%d,0}' % (a[0], a[1]))
        else:
            sys.exit("arcsim: no step %r" % op)
    os.makedirs(outdir, exist_ok=True)
    lua = LUA % {"steps": ",".join(steps), "out": outdir, "testhk": testhook(image)}
    luafile = os.path.join(outdir, "arcsim.lua")
    open(luafile, "w").write(lua)
    env = dict(os.environ)
    env["FUJINET_TCP"] = os.environ.get("ARCSIM_FUJINET", "127.0.0.1:9")
    throttle = [] if os.environ.get("ARCSIM_THROTTLE") else ["-nothrottle"]
    cart = ["-cart", os.path.abspath(os.environ.get("ARCSIM_CART") or image)]
    cmd = [os.path.join(MAME, "mame"), "arcadia", "-cartslot", "fujinet"] + cart + [
           "-autoboot_script", luafile] + throttle + ["-video", "none",
           "-snapshot_directory", outdir, "-snapname", "%i",
           "-cfg_directory", os.path.join(outdir, "cfg"), "-nvram_directory", os.path.join(outdir, "nvram"),
           "-skip_gameinfo", "-nomouse"]
    if os.environ.get("ARCSIM_DEBUG"):
        cmd += ["-debug", "-debugger", "none"]
    wav = os.environ.get("ARCSIM_WAV")
    if wav:
        cmd += ["-wavwrite", wav]
    else:
        cmd += ["-sound", "none"]
    secs = int(os.environ.get("ARCSIM_SECONDS", "600"))
    try:
        r = subprocess.run(["timeout", "-s", "KILL", str(secs)] + cmd, env=env,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        sys.stdout.write(r.stdout)
    except KeyboardInterrupt:
        pass
    # MAME numbers its snapshots 0000.png, ... in order: give them their names
    names = [l.split()[1] for l in r.stdout.splitlines() if l.startswith("SNAP ")]
    shots = sorted(f for f in os.listdir(outdir) if f[:4].isdigit() and f.endswith(".png"))
    for f, n in zip(shots, names):
        os.replace(os.path.join(outdir, f), os.path.join(outdir, n + ".png"))


if __name__ == "__main__":
    main()

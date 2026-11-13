#!/usr/bin/env python3
"""cocosim.py -- run the CoCo builds' test scenes in MAME (from tools/a2sim
and M.U.L.E.'s tools/cocosim).

usage: cocosim.py DISK.dsk MAPFILE SCENARIO OUTDIR

Boots the disk in MAME's CoCo 2 (Disk BASIC; no window, as fast as it goes),
types RUN"AUTOEXEC" (the disk's loader, which picks the build), then plays
the scenario, one command a line; times are in jiffies of the game's clock
(frame_count):

    test SCENE [ARG]        an offline scene (src/test.c): 1 arena (ARG: powers),
                            2 waiting room, 3 round end, 4 game over, 5 four walkers
    wait N                  let N jiffies pass
    snap NAME               save OUTDIR/NAME.png (MAME's own picture)
    hold DIRS               hold directions: any of u d l r, or none
    press                   tap fire
    key CHAR|NUM            a key (ENTER = 13, BREAK = 27)
    peek SYMBOL [N]         print N bytes at a symbol (from the map) or $ADDR
    poke SYMBOL VALUE
    dump FROM TO FILE       save memory (hex addresses) to OUTDIR/FILE
    trace FILE | trace off  every instruction to OUTDIR/FILE (slow)
    frames N                print frame_count and MAME's frame count for N frames
    boot                    (first) start at once, not 25 s in

MACHINE=coco3 (or coco2b, the default) picks the machine; COCO_ARTIFACT=1
(standard), 2 (reversed) or 0 sets the artifact colours; COCO_BIOS=hdbk12 and
COCO_BECKER=1 for a FujiNet (DISK "-": the FujiNet's disk); COCOWAV=FILE
records the sound. MAME's settings go in a config directory of our own
under OUTDIR, so the user's are left alone.

The input goes through the game's test hook (inp_hook in input.c).
"""
import os
import re
import subprocess
import sys

MAME = os.environ.get("MAME", os.path.expanduser("~/Workspace/mame"))
DIRBITS = {"u": 1, "d": 2, "l": 4, "r": 8}     # src/platform-specific/input.h


def symbols(mapfile):
    syms = {}
    for name, addr in re.findall(r"Symbol: (\S+) \(.*\) = ([0-9A-F]{4})", open(mapfile).read()):
        syms.setdefault(name, int(addr, 16))
    return syms


CFG = """<?xml version="1.0"?>
<mameconfig version="10">
    <system name="%s">
        <input>
            <port tag=":beckerport" type="CONFIG" mask="1" defvalue="0" value="%d" />
            <port tag=":vdg:artifacting" type="CONFIG" mask="3" defvalue="1" value="%d" />
        </input>
    </system>
</mameconfig>
"""


def main():
    disk, mapfile, scen, out = sys.argv[1:5]
    out = os.path.abspath(out)
    os.makedirs(out, exist_ok=True)
    syms = symbols(mapfile)

    def sym(n):
        if n.startswith("$"):
            return int(n[1:], 16)           # a raw address
        return syms["_" + n] if "_" + n in syms else syms[n]

    steps = []
    for line in open(scen):
        line = line.split("#")[0].strip()
        if not line:
            continue
        w = line.split()
        c = w[0]
        if c == "test":
            steps.append(("test", sym("test_hook"), int(w[1]), int(w[2], 0) if len(w) > 2 else 0))
        elif c == "wait":
            steps.append(("wait", int(w[1])))
        elif c == "snap":
            steps.append(("snap", w[1]))
        elif c == "hold":
            bits = 0 if w[1] == "none" else sum(DIRBITS[ch] for ch in w[1])
            steps.append(("poke", sym("inp_hook"), 1))
            steps.append(("poke", sym("inp_hook_dir"), bits))
        elif c == "press":
            steps.append(("poke", sym("inp_hook"), 1))
            steps.append(("poke", sym("inp_hook_btn"), 1))
            steps.append(("wait", 3))
            steps.append(("poke", sym("inp_hook_btn"), 0))
        elif c == "key":
            k = int(w[1]) if w[1].isdigit() and len(w[1]) > 1 else ord(w[1][0])
            steps.append(("poke", sym("inp_hook"), 1))
            steps.append(("poke", sym("inp_hook_key"), k))
            steps.append(("wait", 2))
        elif c == "peek":
            steps.append(("peek", w[1], sym(w[1]), int(w[2]) if len(w) > 2 else 1))
        elif c == "poke":
            steps.append(("poke", sym(w[1]), int(w[2], 0)))
        elif c == "dump":
            steps.append(("dump", int(w[1], 16), int(w[2], 16), w[3]))
        elif c == "buzz":
            steps.append(("buzz", int(w[1])))
        elif c == "trace":
            steps.append(("trace", "off" if w[1] == "off" else os.path.join(out, w[1])))
        elif c == "frames":
            steps.append(("frames", int(w[1])))
        elif c == "boot":
            steps.append(("boot",))
        elif c == "mame":
            steps.append(("mame", int(w[1])))
        else:
            sys.exit("unknown command: " + line)

    lua = ["local steps = {"]
    for s in steps:
        if s[0] in ("poke", "test"):
            lua.append('{"%s", %s},' % (s[0], ", ".join(str(x) for x in s[1:])))
        elif s[0] in ("wait", "frames"):
            lua.append('{"%s", %d},' % s)
        elif s[0] == "dump":
            lua.append('{"dump", %d, %d, "%s"},' % (s[1], s[2], os.path.join(out, s[3])))
        elif s[0] == "buzz":
            lua.append('{"buzz", %d},' % s[1])
        elif s[0] == "boot":
            lua.append('{"boot"},')
        elif s[0] == "mame":
            lua.append('{"mame", %d},' % s[1])
        elif s[0] in ("snap", "trace"):
            lua.append('{"%s", "%s"},' % s)
        elif s[0] == "peek":
            lua.append('{"peek", "%s", %d, %d},' % s[1:])
    lua.append("}")
    lua.append("""
local fc = %d
local out = "%s"
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local started = false
local i = 1
local wait_until = nil
local frames = 0
local seen = 0
local asked = false
local framedump = 0
local buzz = 0
_G.COCOSIM = emu.add_machine_frame_notifier(function()
  frames = frames + 1
  if frames > 60 * %d then manager.machine:exit() end
  if frames == %d and %s then manager.machine.natkeyboard:post('RUN"AUTOEXEC"\\n') end
  -- "mame N": MAME's own frames, whatever the banks
  if started and steps[i] ~= nil and steps[i][1] == "mame" then
    if frames >= steps[i][2] then i = i + 1 end
    if steps[i] ~= nil and steps[i][1] == "trace" then
      manager.machine.debugger:command("trace " .. steps[i][2] .. ",0"); i = i + 1
    end
    return
  end
  local now = mem:read_u16(fc)
  if framedump > 0 then
    print(string.format("FRAMES game %%d mame %%d", now, frames))
    framedump = framedump - 1
    if framedump == 0 then i = i + 1 end
    return
  end
  if not started then
    -- the first step may be the test hook: the game marks [1] $C3 and
    -- waits a third of a second for [0] $5A (never poke before: the image
    -- is still loading, and the mark must hold for two frames)
    local s = steps[1]
    if s ~= nil and s[1] == "test" then
      local h = s[2]
      if asked then
        if mem:read_u8(h) == 0xA5 then started = true; i = 2 end
        return
      end
      if mem:read_u8(h + 1) == 0xC3 and mem:read_u8(h) == 0 then
        seen = seen + 1
        if seen >= 2 then
          mem:write_u8(h + 2, s[3]); mem:write_u8(h + 3, s[4]); mem:write_u8(h, 0x5A)
          asked = true
        end
      else seen = 0 end
      return
    elseif s ~= nil and s[1] == "boot" then started = true; i = 2
    elseif frames > 60 * 30 then started = true
    else return end
  end
  while i <= #steps do
    local s = steps[i]
    if s[1] == "wait" then
      if wait_until == nil then wait_until = now + s[2] end
      if ((now - wait_until) & 0xFFFF) < 0x8000 then wait_until = nil; i = i + 1 else return end
    elseif s[1] == "mame" then
      if frames >= s[2] then i = i + 1 else return end
    elseif s[1] == "poke" then
      mem:write_u8(s[2], s[3]); i = i + 1
    elseif s[1] == "snap" then
      manager.machine.video:snapshot()
      print("SNAP " .. s[2] .. " game frame " .. now .. " mame frame " .. frames)
      local f = io.open(out .. "/snaps.txt", "a"); f:write(s[2] .. "\\n"); f:close()
      i = i + 1
      return
    elseif s[1] == "buzz" then
      -- the DAC toggled each MAME frame (a test of the audio path)
      buzz = buzz + 1
      mem:write_u8(0xFF20, (buzz & 1) ~= 0 and 0xFC or 0)
      if buzz >= s[2] then buzz = 0; i = i + 1 else return end
    elseif s[1] == "dump" then
      local f = io.open(s[4], "wb")
      for a = s[2], s[3] do f:write(string.char(mem:read_u8(a))) end
      f:close(); i = i + 1
    elseif s[1] == "trace" then
      if s[2] == "off" then manager.machine.debugger:command("trace off")
      else manager.machine.debugger:command("trace " .. s[2] .. ",0") end
      i = i + 1
    elseif s[1] == "frames" then
      framedump = s[2]; return
    elseif s[1] == "peek" then
      local t = {}
      for k = 0, s[4] - 1 do t[#t + 1] = string.format("%%02X", mem:read_u8(s[3] + k)) end
      print("PEEK " .. s[2] .. " " .. table.concat(t, " ") .. "  (game frame " .. now .. ", mame frame " .. frames .. ")")
      i = i + 1
    end
  end
  manager.machine:exit()
end)
""" % (sym("frame_count"), out, int(os.environ.get("COCOSIM_SECONDS", "600")),
       int(os.environ.get("COCO_TYPE_AT", "90")), "false" if disk == "-" and not os.environ.get("COCO_TYPE_AT") else "true"))
    luafile = os.path.join(out, "scenario.lua")
    open(luafile, "w").write("\n".join(lua))
    snapdir = os.path.join(out, "raw")
    subprocess.run(["rm", "-rf", snapdir, os.path.join(out, "snaps.txt")])
    machine = os.environ.get("MACHINE", "coco2b")
    cfgdir = os.path.join(out, "cfg")
    os.makedirs(cfgdir, exist_ok=True)
    cfg = CFG % (machine, 1 if os.environ.get("COCO_BECKER") else 0, int(os.environ.get("COCO_ARTIFACT", "1")))
    if machine.startswith("coco3"):       # the monitor: COCO_MONITOR=0 composite, 1 RGB (the default)
        cfg = cfg.replace("</input>", '    <port tag=":screen_config" type="CONFIG" mask="1" defvalue="0" value="%d" />\n        </input>'
                          % int(os.environ.get("COCO_MONITOR", "1")))
    open(os.path.join(cfgdir, machine + ".cfg"), "w").write(cfg)
    # MAME 0.289 wants the VDG's character ROM, which its ROM set lacks
    # ("needredump"): a blank one in a path of our own does for the graphics
    for dev, name, size in (("mc6847t1", "mc6847t1_charset_needredump.bin", 0x480),
                            ("gime_ntsc", "gime_charset.bin", 0x280), ("gime_pal", "gime_charset.bin", 0x280)):
        os.makedirs(os.path.join(out, "romx", dev), exist_ok=True)
        open(os.path.join(out, "romx", dev, name), "wb").write(bytes(size))
    ext = "fdc"
    if os.environ.get("COCO_BIOS"):             # e.g. hdbk12 (HDB-DOS, DriveWire)
        ext = "fdc,bios=" + os.environ["COCO_BIOS"]
    flop = [] if disk == "-" else ["-flop1", os.path.abspath(disk)]
    cmd = ["./mame", machine, "-rompath", "roms;" + os.path.join(out, "romx"), "-ext", ext] + flop + [
           "-video", "none", "-sound", "none", "-nothrottle", "-snapshot_directory", snapdir,
           "-cfg_directory", cfgdir, "-nvram_directory", os.path.join(out, "nvram"),
           "-autoboot_script", luafile, "-skip_gameinfo"]
    if disk == "-":                             # against a real server: its clock is real time
        cmd.remove("-nothrottle")
    if any(x[0] == "trace" for x in steps):
        cmd[2:2] = ["-debug", "-debugger", "none"]
    if os.environ.get("COCOWAV"):
        k = cmd.index("-sound")
        cmd[k + 1] = "auto"
        cmd += ["-wavwrite", os.path.abspath(os.environ["COCOWAV"])]
    r = subprocess.run(cmd, cwd=MAME, capture_output=True, text=True, timeout=900)
    if os.environ.get("COCOSIM_VERBOSE"):
        print(r.stdout[-3000:], r.stderr[-3000:])
    for line in r.stdout.splitlines():
        if line.startswith(("PEEK", "SNAP", "FRAMES")):
            print(line)
    if r.returncode:
        print(r.stdout[-2000:], r.stderr[-2000:])
    names = []
    if os.path.exists(os.path.join(out, "snaps.txt")):
        names = open(os.path.join(out, "snaps.txt")).read().split()
    d = os.path.join(snapdir, machine)
    shots = sorted(os.listdir(d)) if os.path.isdir(d) else []
    for n, f in zip(names, shots):
        os.replace(os.path.join(d, f), os.path.join(out, n + ".png"))
        print(os.path.join(out, n + ".png"))


if __name__ == "__main__":
    main()

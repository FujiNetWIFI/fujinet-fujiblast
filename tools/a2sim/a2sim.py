#!/usr/bin/env python3
"""a2sim.py -- run the Apple II build's test scenes in MAME (from M.U.L.E.'s
tools/a2sim).

usage: a2sim.py DISK.po MAPFILE SCENARIO OUTDIR

Boots the disk in MAME's enhanced //e (128K; no window, as fast as it
goes), then plays the scenario, one command a line; times are in jiffies of
the game's clock (frame_count):

    test SCENE [ARG]        an offline scene (src/test.c): 1 arena (ARG: powers),
                            2 waiting room, 3 round end, 4 game over
    wait N                  let N jiffies pass
    snap NAME               save OUTDIR/NAME.png (MAME's own picture)
    hold DIRS               hold directions: any of u d l r, or none
    press                   tap fire
    key CHAR|NUM            a key (RETURN = 13, ESC = 27)
    peek SYMBOL [N]         print N bytes at a symbol (from the map) or $ADDR
    poke SYMBOL VALUE
    trace FILE | trace off  every instruction to OUTDIR/FILE (slow)
    frames N                print frame_count and MAME's frame count for N frames
    boot                    (first) start at power-on, not 30 s in
    mame N                  until MAME's frame N (then a trace, if next, starts at once)

MACHINE=apple2c (or apple2e) picks another machine; MOCK=1 puts a
Mockingboard in slot 4 (apple2ee has one there anyway), NOMOCK=1 empties
the slot; A2WAV=FILE records the sound.

The input goes through the game's test hook (inp_hook in input.c). Lua sees
memory as the CPU does, so frames where the game has aux RAM switched in
(RAMRD or RAMWRT) are let go by.
"""
import os
import re
import subprocess
import sys

MAME = os.environ.get("MAME", os.path.expanduser("~/Workspace/mame"))
DIRBITS = {"u": 1, "d": 2, "l": 4, "r": 8}     # src/platform-specific/input.h


def symbols(mapfile):
    syms = {}
    text = open(mapfile).read()
    part = text.split("Exports list by name:")[1].split("Exports list by value:")[0]
    for name, addr in re.findall(r"(\S+)\s+([0-9A-F]{6})\s+\S+", part):
        syms[name] = int(addr, 16)
    return syms


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
_G.A2SIM = emu.add_machine_frame_notifier(function()
  frames = frames + 1
  if frames > 60 * 900 then manager.machine:exit() end
  -- "mame N": MAME's own frames, whatever the banks
  if started and steps[i] ~= nil and steps[i][1] == "mame" then
    if frames >= steps[i][2] then i = i + 1 end
    if steps[i] ~= nil and steps[i][1] == "trace" then
      manager.machine.debugger:command("trace " .. steps[i][2] .. ",0"); i = i + 1
    end
    return
  end
  if (mem:read_u8(0xC013) & 0x80) ~= 0 or (mem:read_u8(0xC014) & 0x80) ~= 0 then return end
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
      if mem:read_u8(h + 1) == 0xC3 and mem:read_u8(h) == 0 and (mem:read_u8(0xC01A) & 0x80) == 0 then
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
    elseif s[1] == "poke" then
      mem:write_u8(s[2], s[3]); i = i + 1
    elseif s[1] == "snap" then
      manager.machine.video:snapshot()
      print("SNAP " .. s[2] .. " game frame " .. now .. " mame frame " .. frames)
      local f = io.open(out .. "/snaps.txt", "a"); f:write(s[2] .. "\\n"); f:close()
      i = i + 1
      return
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
""" % (sym("frame_count"), out))
    luafile = os.path.join(out, "scenario.lua")
    open(luafile, "w").write("\n".join(lua))
    snapdir = os.path.join(out, "raw")
    subprocess.run(["rm", "-rf", snapdir, os.path.join(out, "snaps.txt")])
    machine = os.environ.get("MACHINE", "apple2ee")
    cmd = ["./mame", machine, "-rompath", "roms", "-flop1", os.path.abspath(disk),
           "-video", "none", "-sound", "none", "-nothrottle", "-snapshot_directory", snapdir,
           "-autoboot_script", luafile, "-skip_gameinfo"]
    if os.environ.get("MOCK"):
        cmd[2:2] = ["-sl4", "mockingboard"]
    if os.environ.get("NOMOCK"):                # (apple2ee has one in slot 4 by default)
        cmd[2:2] = ["-sl4", ""]
    if any(x[0] == "trace" for x in steps):
        cmd[2:2] = ["-debug", "-debugger", "none"]
    if os.environ.get("A2WAV"):
        k = cmd.index("-sound")
        cmd[k + 1] = "auto"
        cmd += ["-wavwrite", os.path.abspath(os.environ["A2WAV"])]
    r = subprocess.run(cmd, cwd=MAME, capture_output=True, text=True, timeout=900)
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

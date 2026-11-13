-- parity.lua -- the walker's trace: once the round is in play, hold a scripted
-- run of directions and log my position every frame (and the map), for
-- replaying through the server's Walker (internal/game/move.go).
local zp = dofile(os.getenv("A26_ZP") or "build/a26/zp.lua")
local sp = manager.machine.devices[":maincpu"].spaces["program"]
local out = io.open(os.getenv("A26_TRACE") or "/tmp/a26trace.txt", "w")
local F = { up = "P1 Up", down = "P1 Down", left = "P1 Left", right = "P1 Right" }
local function field(n) return manager.machine.ioport.ports[":joyport1:joy:JOY"].fields[F[n]] end
local function rd(k, o) return sp:read_u8(zp[k] + (o or 0)) end
local script = {}
for seg in (os.getenv("A26_SCRIPT") or "left:100,up:60,right:100,down:60,left:80,up:80"):gmatch("[^,]+") do
    local d, n = seg:match("(%a+):(%d+)")
    for i = 1, tonumber(n) do script[#script + 1] = d end
end
local started, idx, cur = false, 0, nil
local mapdone = false
_G._par = emu.add_machine_frame_notifier(function()
    if not started then
        if rd("PHASE") == 2 and rd("MYSEAT") < 4 then started = true else return end
    end
    local me = rd("MYSEAT")
    if not mapdone then
        mapdone = true
        for r = 0, 10 do
            local a = 0x1800 + (r >> 1) * 128 + 83 + 13 * (r & 1)
            local t = {}
            for c = 0, 12 do t[#t + 1] = tostring(sp:read_u8(a + c)) end
            out:write("MAP " .. table.concat(t, " ") .. "\n")
        end
    end
    out:write(string.format("P %d %s %d %d %d %d\n", idx, cur or "-", rd("BX", me), rd("BY", me), rd("MACC"), rd("MSPD")))
    if cur then field(cur):set_value(0) end
    idx = idx + 1
    cur = script[idx]
    if cur then field(cur):set_value(1) end
    if idx > #script + 5 then out:flush() end
end)
emu.add_machine_stop_notifier(function() out:flush() out:close() end)

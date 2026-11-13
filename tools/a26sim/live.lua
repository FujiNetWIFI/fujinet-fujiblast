-- live.lua -- a headless MAME run of the 2600 client: snapshots at chosen
-- seconds, zero-page values logged. Env: A26_SNAPS="5,10,20" (seconds),
-- A26_SHOW="PHASE,LFLAGS,MYSEAT" (zp names), A26_SECS.
local dir = os.getenv("A26_SIM") or "."
local zp = dofile(os.getenv("A26_ZP") or "build/a26/zp.lua")
local sp = manager.machine.devices[":maincpu"].spaces["program"]
local snaps = {}
for s in (os.getenv("A26_SNAPS") or "10"):gmatch("[^,]+") do snaps[math.floor(tonumber(s) * 60)] = true end
local show = {}
for s in (os.getenv("A26_SHOW") or "PHASE,LFLAGS,MYSEAT,DST,RLEFT,LREQ"):gmatch("[^,]+") do show[#show + 1] = s end
local FIELDS = {
    fire = "P1 Button 1", up = "P1 Up", down = "P1 Down", left = "P1 Left", right = "P1 Right",
}
local function field(name)
    if name == "reset" then return manager.machine.ioport.ports[":SWB"].fields["Reset Game"] end
    return manager.machine.ioport.ports[":joyport1:joy:JOY"].fields[FIELDS[name]]
end
-- A26_KEYS="sec:name:frames,..." holds an input; A26_WANDER=1 walks about
local keys = {}
for s in (os.getenv("A26_KEYS") or ""):gmatch("[^,]+") do
    local t, nm, f = s:match("([%d%.]+):(%a+):(%d+)")
    if t then keys[#keys + 1] = { at = math.floor(tonumber(t) * 60), name = nm, frames = tonumber(f), left = 0 } end
end
local pokes = {}
for s in (os.getenv("A26_POKE") or ""):gmatch("[^,]+") do
    local t, t2, nm, v = s:match("([%d%.]+)%-?([%d%.]*):(%w+):(%d+)")
    if t then pokes[#pokes + 1] = { at = math.floor(tonumber(t) * 60), to = t2 ~= "" and math.floor(tonumber(t2) * 60) or nil, name = nm, val = tonumber(v) } end
end
local wander = os.getenv("A26_WANDER")
local wdir, wfire = nil, 0
math.randomseed(7)
local n = 0
local function rd(a) return sp:read_u8(a) end
_G._live = emu.add_machine_frame_notifier(function()
    n = n + 1
    if n % 60 == 0 then
        local t = {}
        for _, k in ipairs(show) do
            local nm, off = k:match("^(%w+)%+(%d+)$")
            if not nm then nm, off = k, 0 end
            t[#t + 1] = string.format("%s=%02X", k, zp[nm] and rd(zp[nm] + tonumber(off)) or 0xEE)
        end
        print(string.format("[%4ds] %s", n // 60, table.concat(t, " ")))
    end
    for _, k in ipairs(keys) do
        if n == k.at then field(k.name):set_value(1); k.left = k.frames end
        if k.left > 0 then
            k.left = k.left - 1
            if k.left == 0 then field(k.name):set_value(0) end
        end
    end
    for _, p in ipairs(pokes) do
        if n == p.at or (p.to and n > p.at and n <= p.to) then sp:write_u8(zp[p.name], p.val) end
    end
    if wander and n > 600 then
        if n % 45 == 0 then
            for _, d in ipairs({ "up", "down", "left", "right" }) do field(d):set_value(0) end
            wdir = ({ "up", "down", "left", "right" })[math.random(4)]
            field(wdir):set_value(1)
        end
        field("fire"):set_value((n % 100 == 0) and 1 or 0)
    end
    if snaps[n] then
        manager.machine.video:snapshot()
        if os.getenv("A26_MAP") then
            for r = 0, 10 do
                local a = 0x1800 + (r >> 1) * 128 + 83 + 13 * (r & 1)
                local t = {}
                for c = 0, 12 do t[#t + 1] = string.format("%X", sp:read_u8(a + c) & 15) end
                print("MAP r" .. r .. " " .. table.concat(t))
            end
        end
    end
end)

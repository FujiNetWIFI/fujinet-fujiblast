-- frames.lua -- every frame must be 262 lines.
--
-- Not by counting frames spent in a bank: that measures where the program is,
-- not whether anything is on screen. Not by sampling the screen either:
-- screen:pixel() reads a bitmap MAME updates on its own schedule. What is
-- unambiguous is the program's own VSYNC. Tap the write, measure the gap to
-- the previous one, and anything but 262 lines is a frame that moved the
-- picture -- a poll that blanked, a recompose that overran, a kernel line
-- that silently took two.
--
--   DRIVE_LUA=emu/drive.lua ./run.sh frames
--
-- Without DRIVE_LUA it boots and sits wherever it lands.
if os.getenv("DRIVE_LUA") then dofile(os.getenv("DRIVE_LUA")) end

local NLINES = tonumber(os.getenv('A26_LINES') or '262')
local FRAME = (NLINES == 312) and (1 / 50.0) or (1 / 59.92)
local LINE  = FRAME / NLINES
local hist  = {}
local last, lastbank, n, bad = nil, 0, 0, 0

local function report()
    local keys = {}
    for k in pairs(hist) do keys[#keys+1] = k end
    table.sort(keys)
    local out = {}
    for _, k in ipairs(keys) do out[#out+1] = string.format("%d:%d", k, hist[k]) end
    print("LINES " .. table.concat(out, " "))
    print(string.format("FRAMES: %d of %d were not 262 lines", bad, n))
end

local sp = manager.machine.devices[":maincpu"].spaces["program"]
local zp = dofile(os.getenv("A26_ZP") or "build/a26/zp.lua")
local function z(k) return sp:read_u8(zp[k]) end
local sw, lastsw, lastsw2 = {}, {}, {}
_G._swtap = sp:install_write_tap(0x1D80, 0x1D8F, "bank", function(off, data, mask)
    sw[#sw + 1] = string.format("%d@%.0f", off - 0x1D80, (manager.machine.time:as_double() - (_G._t0 or 0)) / (1 / 59.92 / 262))
end)
_G._tw = sp:install_write_tap(0x296, 0x296, "tim64t", function(off, data, mask)
    sw[#sw + 1] = string.format("T%d@%.0f", data, (manager.machine.time:as_double() - (_G._t0 or 0)) / (1 / 59.92 / 262))
end)
local hi = 0
_G._intim = sp:install_read_tap(0x284, 0x284, "intim", function(off, data, mask)
    if data >= 64 and hi < 12 then hi = hi + 1; print(string.format("INTIM=%02X read at pc=%04X", data, manager.machine.devices[":maincpu"].state["PC"].value)) end
end)
local gen_reads, last_reads = 0, 0
_G._genrd = sp:install_read_tap(0x1F19, 0x1F19, "blitgen", function(off, data, mask) gen_reads = gen_reads + 1 end)
_G._frames = sp:install_write_tap(0x00, 0x00, "vsync", function(off, data, mask)
    if (data & 0x02) == 0 then return end          -- VSYNC going ON only
    local t = manager.machine.time:as_double()
    lastsw2 = lastsw; lastsw, sw = sw, {}
    _G._t0 = t
    local bank = 0
    if last then
        local gap = t - last
        if n == 100 then print(string.format('GAP %.4f ms', gap * 1000)) end
        n = n + 1
        local lines = math.floor(gap / LINE + 0.5)
        hist[lines] = (hist[lines] or 0) + 1
        if lines ~= NLINES then
            bad = bad + 1
            _G._big = (_G._big or 0)
            if (lines > NLINES or lines < NLINES - 7) and t > 6 and _G._big < 8 then _G._big = _G._big + 1; print(string.format("BIG %d lines at %.2fs: prev[%s] this[%s]  [REQ=%X LEFT=%02X CROW=%02X CREG=%02X]", lines, t, table.concat(lastsw2, " "), table.concat(lastsw, " "), z("LREQ"), z("RLEFT"), z("CROW"), z("CREG"))) end
            if bad <= 60 then
                print(string.format("BAD FRAME #%d: %d lines at %.2fs  reads=%d prev: REQ=%X LEFT=%02X DST=%X CROW=%02X CREG=%02X LF=%02X ACT=%X",
                                    bad, lines, t, last_reads, z("LREQ"), z("RLEFT"), z("DST"), z("CROW"), z("CREG"), z("LFLAGS"), z("LACT")))
            end
        end
        if n % 600 == 0 then report() end
    end
    last_reads, gen_reads = gen_reads, 0
    last, lastbank = t, bank
end)
_G._frames_stop = emu.add_machine_stop_notifier(function() report() end)

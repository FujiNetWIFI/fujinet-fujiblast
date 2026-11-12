-- probe.lua -- let the probe run 120 frames, snapshot, exit.
if _G.__probe then return end
_G.__probe = { n = 0 }
local P = _G.__probe
P.sub = emu.add_machine_frame_notifier(function()
  P.n = P.n + 1
  if P.n == 600 then
    manager.machine.screens[":screen"]:snapshot()
    manager.machine:exit()
  end
end)

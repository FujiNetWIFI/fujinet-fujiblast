#!/usr/bin/env bash
# shot.sh -- run a .a52 in MAME a bit and snapshot: shot.sh CART.a52 OUTDIR [FRAMES] [a5200|a5200a]
set -euo pipefail
CART=$(realpath "$1"); OUT=$(realpath -m "$2"); N=${3:-600}; SYS=${4:-a5200}
MAME=${MAME:-$HOME/Workspace/mame-a5200}
HERE=$(cd "$(dirname "$0")" && pwd)
rm -rf "$OUT"; mkdir -p "$OUT/cfg"
cat > "$OUT/shot.lua" <<L
if _G.__shot then return end
_G.__shot = { n = 0 }
local P = _G.__shot
P.sub = emu.add_machine_frame_notifier(function()
  P.n = P.n + 1
  if P.n == $N then
    local mem = manager.machine.devices[":maincpu"].spaces["program"]
    for spec in string.gmatch("${DUMP:-}", "%S+") do
      local a, n = string.match(spec, "(%x+):(%d+)")
      local t = {}
      for i = 0, tonumber(n) - 1 do t[#t + 1] = string.format("%02X", mem:read_u8(tonumber(a, 16) + i)) end
      print(spec .. " " .. table.concat(t, " "))
    end
    manager.machine.screens[":screen"]:snapshot()
    manager.machine:exit()
  end
end)
L
( cd "$MAME" && SDL_VIDEODRIVER=dummy FUJINET_TCP=${FUJINET_TCP:-127.0.0.1:1} \
  ./a5200 $SYS -rompath "$HOME/Workspace/mame/roms" -cartslot fujinet -cart "$CART" \
  -snapshot_directory "$OUT" -cfg_directory "$OUT/cfg" -nvram_directory "$OUT/cfg" \
  -autoboot_script "$OUT/shot.lua" -video none -sound none -nothrottle -seconds_to_run 60 2>&1 | grep -v '^$' | tail -20 )
find "$OUT" -name '*.png' | head -1

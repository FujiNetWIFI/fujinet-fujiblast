#!/usr/bin/env bash
# spike.sh [LOOK] -- build the arena spike (build/a26spike/spike.bin) and
# snapshot it in MAME (build/a26spike/snap/), plus the 262-line gate.
#   FRAMES=1 runs only the line gate.
set -euo pipefail
cd "$(dirname "$0")/../.."
LOOK=${1:-1}
OUT=build/a26spike
rm -rf "$OUT/snap"; mkdir -p "$OUT/snap"
python3 tools/a26/mkspike.py "$OUT" "$LOOK"
( cd src/atari2600/spike && ~/asl/asl -q -L -i "$PWD/../../../$OUT" -i . spike.asm )
mv src/atari2600/spike/spike.p "$OUT/spike.p"
mv -f src/atari2600/spike/spike.lst "$OUT/spike.lst"
~/asl/p2bin "$OUT/spike.p" "$OUT/spike.bin" -r '$F000-$FFFF' -l 0 >/dev/null
ls -l "$OUT/spike.bin"
MAME=${MAME:-$HOME/Workspace/mame}
export SDL_VIDEODRIVER=dummy
cd "$MAME"
if [ -n "${FRAMES:-}" ]; then
  exec ./mame a2600 -cart "$OLDPWD/$OUT/spike.bin" -video none -sound none -seconds_to_run 6 \
       -autoboot_script "$OLDPWD/tools/a26sim/frames.lua" -nvram_directory "$OLDPWD/$OUT/nvram" \
       -cfg_directory "$OLDPWD/$OUT/cfg"
fi
SHOT_FRAMES=${SHOT_FRAMES:-90} exec ./mame a2600 -cart "$OLDPWD/$OUT/spike.bin" -video none -sound none \
     -seconds_to_run 5 -autoboot_script "$OLDPWD/tools/a26sim/shot.lua" \
     -snapshot_directory "$OLDPWD/$OUT/snap" -nvram_directory "$OLDPWD/$OUT/nvram" \
     -cfg_directory "$OLDPWD/$OUT/cfg"

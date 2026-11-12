#!/usr/bin/env bash
# run.sh -- build the 160B probe, run it in MAME (NTSC), check the snapshot.
#   tools/a78sim/probe160b/run.sh [a7800 a7800p]
set -euo pipefail
cd "$(dirname "$0")"
HERE=$(pwd)
B=$HERE/build
MAME=${MAME:-$HOME/Workspace/mame-a7800}
mkdir -p "$B" "$B/cfg"
python3 gen.py "$B/data.bin" "$B/probe.json"
ca65 -o "$B/probe.o" probe.s
ld65 -C probe.cfg -m "$B/probe.map" -o "$B/code.bin" "$B/probe.o"
cat "$B/data.bin" "$B/code.bin" > "$B/probe.bin"
python3 ../mka78.py "$B/probe.bin" "$B/probe.a78" PROBE160B
for sys in ${@:-a7800}; do
    rm -rf "$B/snap-$sys"; mkdir -p "$B/snap-$sys"
    ( cd "$MAME" && SDL_VIDEODRIVER=dummy FUJINET_TCP=127.0.0.1:1 \
      ./a7800 "$sys" -rompath "$HOME/Workspace/mame/roms" -cartslot fujinet -cart "$B/probe.a78" \
        -snapshot_directory "$B/snap-$sys" -cfg_directory "$B/cfg" -nvram_directory "$B/cfg" \
        -autoboot_script "$HERE/probe.lua" -video none -sound none -nothrottle -seconds_to_run 30 \
        2>&1 | grep -v '^$' | tail -5 )
    python3 check.py "$B/probe.json" "$(find "$B/snap-$sys" -name '*.png' | head -1)" "$sys"
done

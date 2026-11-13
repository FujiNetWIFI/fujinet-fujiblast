#!/usr/bin/env bash
# run.sh LUA [SECS] -- run build/a26/fujiblst.bin in the FujiNet-patched MAME
# headless with tools/a26sim/LUA.lua.  Env: MAME (tree, default ~/Workspace/mame),
# IMAGE, OUT (snapshot dir), SHOT_FRAMES, FUJINET_TCP (BoIP, default 127.0.0.1:9995).
# MAME must run FROM its own tree or -autoboot_script is ignored; never pkill
# mame by name (other sessions run it too).
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
LUA=${1:?lua script name}
SECS=${2:-8}
IMAGE=${IMAGE:-$HERE/build/a26/fujiblst.bin}
OUT=${OUT:-$HERE/build/a26/snap}
MAME=${MAME:-$HOME/Workspace/mame}
rm -rf "$OUT"; mkdir -p "$OUT" "$HERE/build/a26/nvram" "$HERE/build/a26/cfg"
export SDL_VIDEODRIVER=dummy
export FUJINET_TCP="${FUJINET_TCP:-127.0.0.1:9995}"
export A26_SIM="$HERE/tools/a26sim"
cd "$MAME"
exec ./mame a2600 -cartslot fujinet -cart "$IMAGE" -video none -sound none \
    -seconds_to_run "$SECS" -autoboot_script "$HERE/tools/a26sim/$LUA.lua" \
    -snapshot_directory "$OUT" -nvram_directory "$HERE/build/a26/nvram" \
    -cfg_directory "$HERE/build/a26/cfg"

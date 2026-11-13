#!/usr/bin/env bash
# build.sh [make args...] -- a clean 5200 build (make doesn't see CFLAGS change): build.sh TEST_SCENE=1
cd "$(dirname "$0")/../.."
rm -rf build/fujiblst/atari5200 r2r/atari5200
make atari5200 "$@" 2>&1 | grep -E "Error|error:|overflow|Warning|mkrom: ROM" | head -20
ls -la r2r/atari5200/fujiblst.a52 2>&1 | tail -1

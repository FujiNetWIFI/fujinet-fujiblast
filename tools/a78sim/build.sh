#!/usr/bin/env bash
# build.sh [make args...] -- a clean 7800 build (make doesn't see CFLAGS change): build.sh TEST_SCENE=1
cd "$(dirname "$0")/../.."
rm -rf build/fujiblst/atari7800 r2r/atari7800
make atari7800 "$@" 2>&1 | grep -E "Error|error:|overflow|Warning|mkrom: (REPL|OVER)" | head -20
ls -la r2r/atari7800/fujiblst.a78 2>&1 | tail -1

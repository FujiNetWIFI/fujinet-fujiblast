#!/usr/bin/env bash
# Interactive NIO workspace session; owns and cleans up its emulator and services.
set -euo pipefail
GAME_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
WORKSPACE="${NIO_WORKSPACE:-$(cd -- "$GAME_ROOT/../.." && pwd)}"
source "$WORKSPACE/scripts/env.sh"
cd "$GAME_ROOT"
if [[ ! -x server/fujiblastd ]]; then
    echo "Build the local server first: (cd server && go build -o fujiblastd ./cmd/fujiblastd)" >&2
    exit 1
fi
exec python3 tools/amiga/accept.py --play "$@"

#!/usr/bin/env bash
# rig.sh -- the Adam client live: a private server, bots and FujiNet-PC (the
# Adam build) with adamcore (FujiNet Go Adam's core), all in a network
# namespace of their own (unshare -rn), so nothing reaches the real server.
#
#   tools/adamrig/rig.sh OUTDIR
#
# Uses r2r/adam/fujiblst.ddp as built (make adam). Starts fujiblastd on
# 9910 and BOTS bots (default 3) at TABLE (default dev1: the first is host and
# starts once START players are in, default BOTS + 1); the lobby's appkeys are
# seeded: name NAME (ADM), server tcp://127.0.0.1:9910/?table=TABLE (ROOM=
# overrides, ROOM= empty: none). KEYS (see run-adam.sh) drives the stick;
# FRAMES, EVERY and WAV are run-adam.sh's. EXTRA: a command run in the namespace in the
# background (a proxy in front of the server: tools/rigs, with ROOM= through it). The pictures are in OUTDIR/shots.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$(realpath -m "$1")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-adam}")
NAME=${NAME-ADM}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9910/?table=$TABLE}
BOTS=${BOTS:-3}
START=${START:-$((BOTS + 1))}
ROUNDS=${ROUNDS:-2}
mkdir -p "$RIG/keys" "$OUT/shots"
( cd "$HERE/server" && go build -o "$RIG/fujiblastd" ./cmd/fujiblastd && go build -o "$RIG/blastbot" ./cmd/blastbot )
rm -f "$RIG"/keys/*.key
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/keys/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/keys/00010110.key"
export RIG OUT HERE TABLE BOTS START ROUNDS KEYS FRAMES EVERY WAV EMUARGS EXTRA
exec unshare -rn bash -c '
    ip link set lo up
    "$RIG/fujiblastd" -stream :9910 -seed 7 -bots "" > "$OUT/server.log" 2>&1 &
    srv=$!
    bots=
    trap "kill $srv \$bots 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9910 " && break; sleep 0.25; done
    if [ "$BOTS" -gt 0 ]; then
        "$RIG/blastbot" -addr 127.0.0.1:9910 -table "$TABLE" -n "$BOTS" -start "$START" \
            -rounds "$ROUNDS" > "$OUT/bots.log" 2>&1 &
        bots=$!
    fi
    if [ -n "${EXTRA:-}" ]; then bash -c "$EXTRA" > "$OUT/extra.log" 2>&1 & bots="$bots $!"; fi
    APPKEYS="$RIG/keys" SHOTDIR="$OUT/shots" "$HERE/tools/adamrig/run-adam.sh" "$HERE/r2r/adam/fujiblst.ddp" > "$OUT/run.log" 2>&1 || true
    tail -5 "$OUT/run.log"
'

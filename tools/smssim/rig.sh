#!/usr/bin/env bash
# rig.sh -- the SMS client live: a private server, bots, FujiNet and MAME,
# all in a network namespace of their own (unshare -rn).
#
#   tools/smssim/rig.sh SCENARIO OUTDIR
#
# Uses r2r/sms/fujiblst.sms as built (a production build: no
# TEST_SCENE) and r2r/sms/fujiblst.map. Starts fujiblastd on 9910, BOTS
# bots (default 3) at TABLE (default dev1; the first bot is host and starts
# once START players are in, default BOTS + 1), a copy of fujinet-pc-rs232
# (BoIP 9988) with the lobby's appkeys seeded: name NAME (SMS), server
# tcp://127.0.0.1:9910/?table=TABLE (ROOM= overrides; ROOM= empty: none).
# Then smssim.py on SCENARIO, throttled (the server's clock is wall time).
# EXTRA: a command run in the
# namespace (in the background) before MAME, e.g. a proxy in front of the
# server. The copied fujinet-pc config loses its cloud sections (tokens).
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SCEN=$(realpath "$1")
OUT=$(realpath -m "$2")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-sms}")
NAME=${NAME-SMS}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9910/?table=$TABLE}
BOTS=${BOTS:-3}
START=${START:-$((BOTS + 1))}
ROUNDS=${ROUNDS:-2}
SECS=${SECS:-300}
CART=${CART:-$HERE/r2r/sms/fujiblst.sms}
MAP=${MAP:-$HERE/r2r/sms/fujiblst.map}
FNPC_DIST=${FNPC_DIST:-$HOME/Workspace/fujinet-pc-rs232/build/dist}
mkdir -p "$RIG" "$OUT"
( cd "$HERE/server" && go build -o "$RIG/fujiblastd" ./cmd/fujiblastd && go build -o "$RIG/blastbot" ./cmd/blastbot )

rm -rf "$RIG/fn"; mkdir -p "$RIG/fn/SD/FujiNet"
cp -a "$FNPC_DIST/fujinet" "$FNPC_DIST/run-fujinet" "$FNPC_DIST/data" "$RIG/fn/"
python3 - "$FNPC_DIST/fnconfig.ini" "$RIG/fn/fnconfig.ini" <<'PY'
import re, sys
out, sec = [], ""
for line in open(sys.argv[1]).read().splitlines():
    m = re.match(r"\[(.*)\]", line)
    if m:
        sec = m.group(1)
    if sec in ("GoogleDrive", "S3", "OneDrive"):
        continue
    if sec == "General" and line.startswith("configenabled="):
        line = "configenabled=0"
    if sec == "BOIP" and line.startswith("port="):
        line = "port=9988"
    if sec == "BOIP" and line.startswith("enabled="):
        line = "enabled=1"
    out.append(line)
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/fn/SD/FujiNet/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/fn/SD/FujiNet/00010110.key"

export RIG OUT HERE SCEN CART MAP TABLE BOTS START ROUNDS EXTRA SECS
exec unshare -rn bash -c '
    ip link set lo up
    "$RIG/fujiblastd" -stream :9910 -seed 7 -bots "" > "$RIG/server.log" 2>&1 &
    srv=$!
    ( cd "$RIG/fn" && exec setsid ./run-fujinet -u http://127.0.0.1:8013 -c fnconfig.ini -s SD ) \
        > "$RIG/fn.log" 2>&1 &
    fn=$!
    bots=
    trap "kill -- -$fn $srv \$bots 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9910 " && break; sleep 0.25; done
    for i in $(seq 40); do ss -ltn | grep -q ":9988 " && break; sleep 0.25; done
    if [ "$BOTS" -gt 0 ]; then
        "$RIG/blastbot" -addr 127.0.0.1:9910 -table "$TABLE" -n "$BOTS" -start "$START" \
            -rounds "$ROUNDS" > "$RIG/bots.log" 2>&1 &
        bots=$!
    fi
    if [ -n "${EXTRA:-}" ]; then bash -c "$EXTRA" > "$RIG/extra.log" 2>&1 & bots="$bots $!"; fi
    sleep 1
    FUJINET_TCP=127.0.0.1:9988 SMSSIM_THROTTLE=1 \
        timeout "$SECS" python3 "$HERE/tools/smssim/smssim.py" "$CART" "$MAP" "$SCEN" "$OUT" 2>&1 | tail -30
'

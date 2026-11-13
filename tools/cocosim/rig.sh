#!/usr/bin/env bash
# rig.sh -- a CoCo client live: a private server, bots, fujinet-pc (the CoCo
# build, DriveWire over the Becker port 65504) and MAME's CoCo, all in a
# network namespace of their own (unshare -rn). The CoCo's counterpart of
# tools/a2rig/rig.sh.
#
#   tools/cocosim/rig.sh SCRIPT OUTDIR
#
# Uses r2r/coco/fujiblst.dsk as built. Starts fujiblastd on 9910, BOTS bots
# (default 3) at TABLE (default dev1; the first bot is host and starts once
# START players are in, default BOTS + 1), and fujinet-pc with the disk in
# its slot 1 and the lobby's appkeys seeded: name NAME (CC), server
# tcp://127.0.0.1:9910/?table=TABLE (ROOM= overrides, ROOM= empty: none).
# MACHINE=coco3 runs the CoCo 3 build. SCRIPT is cocosim.py's language;
# the game is booted by HDB-DOS from the FujiNet's disk (AUTOEXEC.BAS).
# Never the production server or the lobby.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SCRIPT=$(realpath "$1")
OUT=$(realpath -m "$2")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-coco}")
FNPC=${FNPC:-$HOME/Workspace/fujinet-pc-coco/build/dist}
NAME=${NAME-CC}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9910/?table=$TABLE}
BOTS=${BOTS:-3}
START=${START:-$((BOTS + 1))}
ROUNDS=${ROUNDS:-2}
MACHINE=${MACHINE:-coco2b}
WEB=${WEB:-8123}
MAP=$HERE/build/fujiblst-coco.map
[ "$MACHINE" = coco3 ] && MAP=$HERE/build/fujiblst-coco3.map
mkdir -p "$RIG" "$OUT"
( cd "$HERE/server" && go build -o "$RIG/fujiblastd" ./cmd/fujiblastd && go build -o "$RIG/blastbot" ./cmd/blastbot )

# FujiNet's root: a copy of the CoCo build's, with our disk
rm -rf "$RIG/fn"; mkdir -p "$RIG/fn/SD/FujiNet"
cp -a "$FNPC/fujinet" "$FNPC/data" "$RIG/fn/"
python3 - "$FNPC/fnconfig.ini" "$RIG/fn/fnconfig.ini" <<'PY'
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
    out.append(line)
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
cp "$HERE/r2r/coco/fujiblst.dsk" "$RIG/fn/SD/fujiblst.dsk"
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/fn/SD/FujiNet/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/fn/SD/FujiNet/00010110.key"

export RIG OUT HERE TABLE BOTS START ROUNDS EXTRA MACHINE WEB MAP SCRIPT
exec unshare -rn bash -c '
    ip link set lo up
    "$RIG/fujiblastd" -stream :9910 -seed 7 -bots "" > "$RIG/server.log" 2>&1 &
    srv=$!
    pids="$srv"
    trap "kill $pids 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9910 " && break; sleep 0.25; done
    if [ "$BOTS" -gt 0 ]; then
        "$RIG/blastbot" -addr 127.0.0.1:9910 -table "$TABLE" -n "$BOTS" -start "$START" \
            -rounds "$ROUNDS" > "$RIG/bots.log" 2>&1 &
        pids="$pids $!"
    fi
    cd "$RIG/fn"
    ./fujinet -c fnconfig.ini -s SD -u "http://127.0.0.1:$WEB" > "$RIG/fujinet.log" 2>&1 &
    pids="$pids $!"
    for i in $(seq 40); do ss -ltn | grep -q ":65504 " && break; sleep 0.25; done
    sleep 1
    curl -s -X POST "http://127.0.0.1:$WEB/api/v1/drives/1/mount" -H "Content-Type: application/json" \
        -d "{\"path\":\"/fujiblst.dsk\",\"host_slot\":0,\"mode\":\"r\"}" > "$RIG/mount.log" 2>&1 || true
    cd "$HERE"
    COCO_BECKER=1 COCO_BIOS=${COCO_BIOS:-hdbk12} COCOSIM_SECONDS=${COCOSIM_SECONDS:-300} \
        python3 tools/cocosim/cocosim.py - "$MAP" "$SCRIPT" "$OUT" > "$OUT/cocosim.log" 2>&1
    echo "cocosim exit $?" >> "$OUT/cocosim.log"
    tail -20 "$OUT/cocosim.log"
'

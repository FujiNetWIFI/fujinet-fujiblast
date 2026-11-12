#!/usr/bin/env bash
# rig.sh -- the Intellivision client live: a private server, bots, a
# fujinet-pc (the RS232 build) on BoIP, and jzIntv (the FujiNet-patched
# build) driven by tools/intvsim/intvsim.py, all in a namespace of their
# own (unshare -rnm: no network but its own, and no gamepads: a pad on the
# desk would play a seat). The INTV counterpart of tools/dosrig/rig.sh.
#
#   tools/intvrig/rig.sh SCENARIO OUTDIR
#
# Uses r2r/intv/fujiblst.rom and its .sym as built (BUILD=1: `make intv`
# first; ROM= another, e.g. build/intv-rig/fujiblst.rom from
# `make -C src/intv rig`, whose default server is the rig's) and builds the server and the bots into RIG. Starts
# fujiblastd on 9910, BOTS bots (default 3) at TABLE (default dev1; the
# first bot is host and starts once START players are in, default
# BOTS + 1), and a copy of fujinet-pc-rs232's dist (FNPC; never the live
# one) with the lobby's appkeys seeded: name NAME (INTV), server
# tcp://127.0.0.1:9910/?table=TABLE (ROOM= overrides, ROOM= empty: none).
# The scenario is intvsim.py's. EXTRA: a command run in the namespace first
# (e.g. tools/rigs/dropproxy.py). WINDOW=1: jzIntv's window instead, to
# play (no scenario). Never the production server or the lobby.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SCEN=$(realpath -m "$1")
OUT=$(realpath -m "$2")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-intv}")
FNPC=${FNPC:-$HOME/Workspace/fujinet-pc-rs232/build/dist}
JZ=${JZINTV_DIR:-$HOME/Workspace/jzintv-20200712-src}
NAME=${NAME-INTV}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9910/?table=$TABLE}
BOTS=${BOTS:-3}
START=${START:-$((BOTS + 1))}
ROUNDS=${ROUNDS:-2}
BOIP=9995
ROM=$(realpath "${ROM:-$HERE/r2r/intv/fujiblst.rom}")
mkdir -p "$RIG" "$OUT"

if [ "${BUILD:-0}" = 1 ]; then
    ( cd "$HERE" && make intv > "$RIG/build.log" 2>&1 ) || { tail -20 "$RIG/build.log"; exit 1; }
fi
( cd "$HERE/server" && go build -o "$RIG/fujiblastd" ./cmd/fujiblastd && go build -o "$RIG/blastbot" ./cmd/blastbot )

# FujiNet: the binary and its data, a fresh SD with the appkeys, the config
# without the cloud sections, CONFIG off, BoIP on our port
rm -rf "$RIG/fn"; mkdir -p "$RIG/fn/SD/FujiNet"
cp "$FNPC/fujinet" "$RIG/fn/"
cp -a "$FNPC/data" "$RIG/fn/"
python3 - "$FNPC/fnconfig.ini" "$RIG/fn/fnconfig.ini" "$BOIP" <<'PY'
import re, sys
out, sec = [], ""
for line in open(sys.argv[1]).read().replace("\r", "").splitlines():
    m = re.match(r"\[(.*)\]", line)
    if m:
        sec = m.group(1)
    if sec in ("GoogleDrive", "S3", "OneDrive") or re.match(r"(Host|Mount)\d", sec):
        continue
    if sec == "General" and line.startswith("configenabled="):
        line = "configenabled=0"
    if sec == "BOIP" and line.startswith("host="):
        line = "host=127.0.0.1"
    if sec == "BOIP" and line.startswith("port="):
        line = "port=" + sys.argv[3]
    out.append(line)
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/fn/SD/FujiNet/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/fn/SD/FujiNet/00010110.key"

export RIG OUT TABLE BOTS START ROUNDS EXTRA BOIP HERE SCEN JZ ROM
export WINDOW=${WINDOW:-}
exec unshare -rnm bash -c '
    ip link set lo up
    # no gamepads: a pad on the desk would play (SDL opens /dev/input and hidraw)
    mount -t tmpfs tmpfs /dev/input 2>/dev/null || true
    for h in /dev/hidraw*; do [ -e "$h" ] && mount --bind /dev/null "$h" 2>/dev/null || true; done
    "$RIG/fujiblastd" -stream :9910 -seed 7 -bots "" > "$RIG/server.log" 2>&1 &
    srv=$!
    ( cd "$RIG/fn" && exec ./fujinet -u http://127.0.0.1:8001 > "$RIG/fujinet.log" 2>&1 ) &
    fn=$!
    bots=
    trap "kill -9 $srv $fn \$bots 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9910 " && ss -ltn | grep -q ":$BOIP " && break; sleep 0.25; done
    if [ "$BOTS" -gt 0 ]; then
        "$RIG/blastbot" -addr 127.0.0.1:9910 -table "$TABLE" -n "$BOTS" -start "$START" \
            -rounds "$ROUNDS" > "$RIG/bots.log" 2>&1 &
        bots=$!
    fi
    if [ -n "${EXTRA:-}" ]; then bash -c "$EXTRA" > "$RIG/extra.log" 2>&1 & bots="$bots $!"; fi
    if [ -n "$WINDOW" ]; then
        SDL_AUDIODRIVER=${SDL_AUDIODRIVER:-pulseaudio} "$JZ/bin/linux/jzintv" -z 3 -s0 \
            -e "$JZ/rom/exec.bin" -g "$JZ/rom/grom.bin" --fujinet=127.0.0.1:$BOIP \
            "$ROM" > "$RIG/jzintv.log" 2>&1 || true
    else
        JZINTV_DIR=$JZ FUJINET=127.0.0.1:$BOIP python3 "$HERE/tools/intvsim/intvsim.py" \
            "$ROM" "$SCEN" "$OUT"
    fi
'

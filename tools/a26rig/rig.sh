#!/usr/bin/env bash
# rig.sh -- the Atari 2600 client live: a private server, bots, a fujinet-pc
# (the RS232 build) on BoIP, and MAME's a2600 with the FujiNet cart, all in a
# namespace of their own (unshare -rnm: no network but its own, no gamepads).
#
#   tools/a26rig/rig.sh OUTDIR [SECS]
#
# Env: NAME (A26) ROOM (tcp://127.0.0.1:9910/?table=TABLE; ROOM= empty: no key)
# TABLE (dev1) BOTS (3) START ROUNDS EXTRA (a command run in the namespace
# first, e.g. tools/rigs/dropproxy.py) A26_SNAPS A26_SHOW (see live.lua)
# BUILD=1 builds the image first with DEFSRV=127.0.0.1:9910. MAME runs
# throttled: the cart's transactions take real time. Never the production
# server or the lobby; never pkill mame by name.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$(realpath -m "$1")
SECS=${2:-40}
RIG=$(realpath -m "${RIG:-$HERE/build/rig-a26}")
FNPC=${FNPC:-$HOME/Workspace/fujinet-pc-rs232/build/dist}
NAME=${NAME-A26}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9910/?table=$TABLE}
BOTS=${BOTS:-3}
START=${START:-$((BOTS + 1))}
ROUNDS=${ROUNDS:-2}
BOIP=9995
ROM=$(realpath -m "${ROM:-$HERE/build/a26/fujiblst.bin}")
MAME=${MAME:-$HOME/Workspace/mame}
mkdir -p "$RIG" "$OUT"

if [ "${BUILD:-0}" = 1 ]; then
    DEFSRV=127.0.0.1:9910 "$HERE/tools/a26/build.sh" > "$RIG/build.log" 2>&1 || { tail -20 "$RIG/build.log"; exit 1; }
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
    if sec == "BOIP" and line.startswith("enabled="):
        line = "enabled=1"
    out.append(line)
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/fn/SD/FujiNet/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/fn/SD/FujiNet/00010110.key"

export RIG OUT TABLE BOTS START ROUNDS EXTRA BOIP HERE SECS ROM MAME
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
    rm -rf "$OUT/snap"; mkdir -p "$OUT/snap" "$RIG/nvram" "$RIG/cfg"
    export SDL_VIDEODRIVER=dummy FUJINET_TCP=127.0.0.1:$BOIP A26_SIM="$HERE/tools/a26sim" A26_ZP="$HERE/build/a26/zp.lua"
    cd "$MAME" && ./mame ${MACHINE:-a2600} -cartslot fujinet -cart "$ROM" -video none ${SNDOPT:--sound none} \
        -seconds_to_run "$SECS" -autoboot_script "$HERE/tools/a26sim/${LUA:-live}.lua" \
        -snapshot_directory "$OUT/snap" -nvram_directory "$RIG/nvram" -cfg_directory "$RIG/cfg"
'

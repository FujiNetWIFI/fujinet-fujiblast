#!/usr/bin/env bash
# rig.sh -- the MS-DOS client live: a private server, bots, a fujinet-pc
# (the RS232 build) on BoIP, and MAME's IBM PC with FUJINET.SYS on COM1,
# its null modem a socket to that FujiNet (tools/dossim/dossim.py), all in
# a network namespace of their own (unshare -rn). The DOS counterpart of
# tools/a2rig/rig.sh.
#
#   tools/dosrig/rig.sh SCENARIO OUTDIR
#
# Uses r2r/msdos/fujiblst.exe and build/fujiblst-msdos.map as built (BUILD=1:
# `defoogi make msdos` first) and builds the server and the bots into RIG.
# Starts fujiblastd on 9910, BOTS bots (default 3) at TABLE (default dev1;
# the first bot is host and starts once START players are in, default
# BOTS + 1), and a copy of fujinet-pc-rs232's dist (FNPC; never the live
# one) with the lobby's appkeys seeded: name NAME (DOS), server
# tcp://127.0.0.1:9910/?table=TABLE (ROOM= overrides, ROOM= empty: none).
# BAUD (default 115200) is FUJINET.SYS's and the null modem's, PORT its
# FUJI_PORT (1; the PCjr's own port is 2: DOSSIM_MACHINE=ibmpcjr). The
# scenario is dossim.py's. EXTRA: a command run in the namespace first
# (e.g. tools/rigs/dropproxy.py). Never the production server or the lobby.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SCEN=$(realpath "$1")
OUT=$(realpath -m "$2")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-msdos}")
FNPC=${FNPC:-$HOME/Workspace/fujinet-pc-rs232/build/dist}
BOOT=${BOOT:-$HOME/Vintage/IBM 5160 PCXT/fn-dos21.img}
NAME=${NAME-DOS}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9910/?table=$TABLE}
BOTS=${BOTS:-3}
START=${START:-$((BOTS + 1))}
ROUNDS=${ROUNDS:-2}
BAUD=${BAUD:-115200}
PORT=${PORT:-1}
BOIP=1987
mkdir -p "$RIG" "$OUT"

if [ "${BUILD:-0}" = 1 ]; then
    ( cd "$HERE" && defoogi make msdos > "$RIG/build.log" 2>&1 ) || { tail -20 "$RIG/build.log"; exit 1; }
fi
( cd "$HERE/server" && go build -o "$RIG/fujiblastd" ./cmd/fujiblastd && go build -o "$RIG/blastbot" ./cmd/blastbot )

# FujiNet: the binary and its data, a fresh SD with the appkeys, the config
# without the cloud sections, BoIP on our port
rm -rf "$RIG/fn"; mkdir -p "$RIG/fn/SD/FujiNet"
cp "$FNPC/fujinet" "$RIG/fn/"
cp -a "$FNPC/data" "$RIG/fn/"
python3 - "$FNPC/fnconfig.ini" "$RIG/fn/fnconfig.ini" "$BOIP" "$BAUD" <<'PY'
import re, sys
out, sec = [], ""
for line in open(sys.argv[1]).read().splitlines():
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
    if sec == "Serial" and line.startswith("baud="):
        line = "baud=" + sys.argv[4]
    out.append(line)
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/fn/SD/FujiNet/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/fn/SD/FujiNet/00010110.key"

# the boot disk: FUJINET.SYS on COM1 (MAME's isa2:com:serport0) at BAUD
cp "$BOOT" "$RIG/boot.img"
printf 'DEVICE=FUJINET.SYS FUJI_PORT=%s FUJI_BPS=%s\r\n' "$PORT" "$BAUD" > "$RIG/config.sys"
mcopy -o -i "$RIG/boot.img" "$RIG/config.sys" ::CONFIG.SYS

export RIG OUT TABLE BOTS START ROUNDS EXTRA BAUD BOIP HERE SCEN
exec unshare -rn bash -c '
    ip link set lo up
    "$RIG/fujiblastd" -stream :9910 -seed 7 -bots "" > "$RIG/server.log" 2>&1 &
    srv=$!
    ( cd "$RIG/fn" && exec ./fujinet > "$RIG/fujinet.log" 2>&1 ) &
    fn=$!
    bots=
    trap "kill $srv $fn \$bots 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9910 " && ss -ltn | grep -q ":$BOIP " && break; sleep 0.25; done
    if [ "$BOTS" -gt 0 ]; then
        "$RIG/blastbot" -addr 127.0.0.1:9910 -table "$TABLE" -n "$BOTS" -start "$START" \
            -rounds "$ROUNDS" > "$RIG/bots.log" 2>&1 &
        bots=$!
    fi
    if [ -n "${EXTRA:-}" ]; then bash -c "$EXTRA" > "$RIG/extra.log" 2>&1 & bots="$bots $!"; fi
    DOSSIM_BOOT="$RIG/boot.img" DOSSIM_FUJINET=127.0.0.1:$BOIP DOSSIM_BAUD=$BAUD \
        python3 "$HERE/tools/dossim/dossim.py" "$HERE/r2r/msdos/fujiblst.exe" \
        "$HERE/build/fujiblst-msdos.map" "$SCEN" "$OUT"
'

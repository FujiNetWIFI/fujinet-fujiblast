#!/usr/bin/env bash
# rig.sh -- the Atari client live: a private server, bots, FujiNet and asim
# over NetSIO, all in a network namespace of their own (unshare -rn).
# (From M.U.L.E.'s tools/atarisim/rig.sh.)
#
#   tools/atarisim/rig.sh ASIM_SCRIPT OUTDIR
#
# Uses r2r/atari/fujiblst.atr as built (BUILD=1: `defoogi make atari`
# first) and builds the server and the bots into RIG. Starts fujiblastd on
# 9910, BOTS bots (default 3) at TABLE (default dev1; the first bot is host
# and starts once START players are in, default BOTS + 1), a copy of
# fujinet-pc-atari (NetSIO 9997) with the disk in slot 1 from its SD and
# the lobby's appkeys seeded: name NAME (ATR), server
# tcp://127.0.0.1:9910/?table=TABLE (ROOM= overrides, ROOM= empty: none).
# Then runs asim on ASIM_SCRIPT in real time.
#
# The script is asim's own language (asim.c): it starts with "wait 120"
# and "coldstart" (FujiNet is there by then); @symbol[+n] stands for that
# address in build/fujiblst-atari.map, in hex. XL=1: an 800XL; PAL=1: a
# PAL machine. EXTRA: a command run in the namespace (in the background)
# before asim, e.g. a proxy in front of the server. The ports
# are the namespace's alone. The copied fujinet-pc config loses its cloud
# sections (they hold tokens). Kills what it started when asim ends.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SCRIPT=$(realpath "$1")
OUT=$(realpath -m "$2")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-atari}")
NAME=${NAME-ATR}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9910/?table=$TABLE}
BOTS=${BOTS:-3}
START=${START:-$((BOTS + 1))}
ROUNDS=${ROUNDS:-2}
FNPC_DIST=${FNPC_DIST:-$HOME/Workspace/fujinet-pc-atari/build/dist}
ASIM=$HERE/tools/atarisim/asim
mkdir -p "$RIG" "$OUT"

if [ "${BUILD:-0}" = 1 ]; then
    ( cd "$HERE" && defoogi make atari > "$RIG/build.log" 2>&1 ) || { tail -20 "$RIG/build.log"; exit 1; }
fi
( cd "$HERE/server" && go build -o "$RIG/fujiblastd" ./cmd/fujiblastd && go build -o "$RIG/blastbot" ./cmd/blastbot )

# the script, its symbols resolved
python3 - "$HERE/build/fujiblst-atari.map" "$SCRIPT" "$OUT/script.txt" <<'PY'
import re, sys
t = open(sys.argv[1]).read().split("Exports list by name:")[1].split("Exports list by value:")[0]
s = {n: int(a, 16) for n, a in re.findall(r"(\S+)\s+([0-9A-F]{6})\s+\S+", t)}
def addr(m):
    n = m.group(1)
    return "%x" % ((s["_" + n] if "_" + n in s else s[n]) + int(m.group(2) or "0", 0))
src = open(sys.argv[2]).read()
open(sys.argv[3], "w").write(re.sub(r"@(\w+)(?:\+(\w+))?", lambda m: addr(m), src))
PY

# fujinet-pc
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
    if sec == "Mount1" and line.startswith("hostslot="):
        line = "hostslot=1"                     # (1-based)
    if sec == "Mount1" and line.startswith("path="):
        line = "path=/fujiblst.atr"
    out.append(line)
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
cp "$HERE/r2r/atari/fujiblst.atr" "$RIG/fn/SD/fujiblst.atr"
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/fn/SD/FujiNet/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/fn/SD/FujiNet/00010110.key"

export RIG OUT ASIM XL PAL TABLE BOTS START ROUNDS EXTRA
exec unshare -rn bash -c '
    ip link set lo up
    "$RIG/fujiblastd" -stream :9910 -seed 7 > "$RIG/server.log" 2>&1 &
    srv=$!
    ( cd "$RIG/fn" && exec setsid ./run-fujinet -u http://127.0.0.1:8013 -c fnconfig.ini -s SD ) \
        > "$RIG/fn.log" 2>&1 &
    fn=$!
    bots=
    trap "kill -- -$fn $srv \$bots 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9910 " && break; sleep 0.25; done
    if [ "$BOTS" -gt 0 ]; then
        "$RIG/blastbot" -addr 127.0.0.1:9910 -table "$TABLE" -n "$BOTS" -start "$START" \
            -rounds "$ROUNDS" > "$RIG/bots.log" 2>&1 &
        bots=$!
    fi
    if [ -n "${EXTRA:-}" ]; then bash -c "$EXTRA" > "$RIG/extra.log" 2>&1 & bots="$bots $!"; fi
    sleep 1
    "$ASIM" ${XL:+-xl} ${PAL:+-pal} -netsio 9997 - "$OUT/script.txt" "$OUT" > "$OUT/asim.log" 2>&1 || true
    grep -v "^$" "$OUT/asim.log" | tail -40
'

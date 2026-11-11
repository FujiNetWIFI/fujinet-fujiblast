#!/usr/bin/env bash
# rig.sh -- the Apple II client live: a private server, bots, and a2rig
# (AppleWin's core with an in-process FujiNet on SmartPort over SLIP), all
# in a network namespace of their own (unshare -rn). The Apple II's
# counterpart of tools/atarisim/rig.sh.
#
#   tools/a2rig/rig.sh SCRIPT OUTDIR
#
# Uses r2r/apple2/fujiblst.po as built (BUILD=1: `make apple2` first) and
# builds the server and the bots into RIG. Starts fujiblastd on 9910, BOTS
# bots (default 3) at TABLE (default dev1; the first bot is host and starts
# once START players are in, default BOTS + 1), and a2rig with the disk in
# FujiNet's slot 1 from its SD and the lobby's appkeys seeded: name NAME
# (A2), server tcp://127.0.0.1:9910/?table=TABLE (ROOM= overrides, ROOM=
# empty: none). FujiNet is fujinet-go-apple2-desktop's libfujinet.so (A2D).
#
# The script is a2rig's language (a2rig.c); @symbol[+n] stands for that
# address in build/fujiblst-apple2.map, in hex. FAST=1 runs frames flat
# out. EXTRA: a command run in the namespace (in the background) before
# a2rig, e.g. a proxy in front of the server. Kills what it started when
# a2rig ends. Never the production server or the lobby.
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SCRIPT=$(realpath "$1")
OUT=$(realpath -m "$2")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-apple2}")
A2D=${A2D:-$HOME/Workspace/fujinet-go-apple2-desktop}
FNOUT=$A2D/tools/fujinet/work/out
NAME=${NAME-A2}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9910/?table=$TABLE}
BOTS=${BOTS:-3}
START=${START:-$((BOTS + 1))}
ROUNDS=${ROUNDS:-2}
A2RIG=$HERE/tools/a2rig/a2rig
mkdir -p "$RIG" "$OUT"

if [ "${BUILD:-0}" = 1 ]; then
    ( cd "$HERE" && make apple2 > "$RIG/build.log" 2>&1 ) || { tail -20 "$RIG/build.log"; exit 1; }
fi
make -s -C "$HERE/tools/a2rig" a2rig
( cd "$HERE/server" && go build -o "$RIG/fujiblastd" ./cmd/fujiblastd && go build -o "$RIG/blastbot" ./cmd/blastbot )

# the script, its symbols resolved
python3 - "$HERE/build/fujiblst-apple2.map" "$SCRIPT" "$OUT/script.txt" <<'PY'
import re, sys
t = open(sys.argv[1]).read().split("Exports list by name:")[1].split("Exports list by value:")[0]
s = {n: int(a, 16) for n, a in re.findall(r"(\S+)\s+([0-9A-F]{6})\s+\S+", t)}
def addr(m):
    n = m.group(1)
    return "0x%x" % ((s["_" + n] if "_" + n in s else s[n]) + int(m.group(2) or "0", 0))
src = open(sys.argv[2]).read()
open(sys.argv[3], "w").write(re.sub(r"@(\w+)(?:\+(\w+))?", lambda m: addr(m), src))
PY

# FujiNet's root: the desktop's runtime tree, booting our disk
rm -rf "$RIG/fn"; mkdir -p "$RIG/fn/SD/FujiNet"
cp -a "$FNOUT/data" "$RIG/fn/"
python3 - "$FNOUT/fnconfig.ini" "$RIG/fn/fnconfig.ini" <<'PY'
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
    if sec == "BOIP" and line.startswith("host="):
        line = "host=127.0.0.1"
    if sec == "BOIP" and line.startswith("port="):
        line = "port=1985"
    out.append(line)
out += ["", "[Mount1]", "hostslot=1", "path=/fujiblst.po", "mode=r"]
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
cp "$HERE/r2r/apple2/fujiblst.po" "$RIG/fn/SD/fujiblst.po"
[ -n "$NAME" ] && printf '%s' "$NAME" > "$RIG/fn/SD/FujiNet/00010100.key"
[ -n "$ROOM" ] && printf '%s' "$ROOM" > "$RIG/fn/SD/FujiNet/00010110.key"

export RIG OUT A2RIG FNOUT TABLE BOTS START ROUNDS EXTRA FAST
exec unshare -rn bash -c '
    ip link set lo up
    "$RIG/fujiblastd" -stream :9910 -seed 7 > "$RIG/server.log" 2>&1 &
    srv=$!
    bots=
    trap "kill $srv \$bots 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9910 " && break; sleep 0.25; done
    if [ "$BOTS" -gt 0 ]; then
        "$RIG/blastbot" -addr 127.0.0.1:9910 -table "$TABLE" -n "$BOTS" -start "$START" \
            -rounds "$ROUNDS" > "$RIG/bots.log" 2>&1 &
        bots=$!
    fi
    if [ -n "${EXTRA:-}" ]; then bash -c "$EXTRA" > "$RIG/extra.log" 2>&1 & bots="$bots $!"; fi
    "$A2RIG" ${FAST:+-fast} -fn "$FNOUT/libfujinet.so" "$RIG/fn" "$OUT/script.txt" "$OUT" \
        > "$OUT/a2rig.log" 2>&1
    echo "a2rig exit $?" >> "$OUT/a2rig.log"
    grep -v "^$" "$OUT/a2rig.log" | tail -40
'

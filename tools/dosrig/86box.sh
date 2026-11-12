#!/usr/bin/env bash
# 86box.sh -- FujiBlast's speed on a real 4.77 MHz 8088 with CGA: 86Box's
# IBM 5150 (cycle-exact, with CGA's wait states), headless, playing live
# against bots, as tools/dosrig/rig.sh sets them up (the same variables).
#
#   tools/dosrig/86box.sh SECONDS OUTDIR
#
# Runs build/fujiblst-stats.exe (`defoogi make msdos STATS=1`, copied there:
# it plays by itself and appends a line to A:\STATS.TXT every 10 s:
# jiffies, passes drawn, passes late, fbnet's counters (bad replies, lost
# replies, resends, reads lost) and UART overruns; then, in tenths of a per
# cent of the time, drawing, waiting for the clock and the link's decoding,
# and the bytes received; and A:\PROF.BIN, where the timer's interrupt
# found the CPU (tools/dosrig/prof86.py)). VM is the 86Box machine
# copied (default ~/Vintage/IBM 5150 PC); FujiNet is its COM2 (BoIP).
# 86Box's serial port takes a byte every bit time (ten times a real one),
# so BAUD 9600 (the default) already runs the link faster than a real
# 9600. SHOTS=N: 86Box on a private Xvnc display (:93), the screen grabbed
# every N seconds into OUTDIR (else it runs with no display at all).
set -euo pipefail
HERE=$(cd "$(dirname "$0")/../.." && pwd)
SECS=$1
OUT=$(realpath -m "$2")
RIG=$(realpath -m "${RIG:-$HERE/build/rig-86box}")
VM=${VM:-$HOME/Vintage/IBM 5150 PC}
FNPC=${FNPC:-$HOME/Workspace/fujinet-pc-rs232/build/dist}
BOOT=${BOOT:-$HOME/Vintage/IBM 5160 PCXT/fn-dos21.img}
EXE=${EXE:-$HERE/build/fujiblst-stats.exe}
NAME=${NAME-DOS}
TABLE=${TABLE:-dev1}
ROOM=${ROOM-tcp://127.0.0.1:9910/?table=$TABLE}
BOTS=${BOTS:-3}
START=${START:-$((BOTS + 1))}
ROUNDS=${ROUNDS:-9}
BAUD=${BAUD:-9600}
SHOTS=${SHOTS:-}
BOIP=1988
mkdir -p "$RIG" "$OUT"
( cd "$HERE/server" && go build -o "$RIG/fujiblastd" ./cmd/fujiblastd && go build -o "$RIG/blastbot" ./cmd/blastbot )

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

# the floppy: DOS, FUJINET.SYS on COM2 at BAUD, the game run by AUTOEXEC.BAT
# (OFFLINE=1: no FUJINET.SYS, and the game plays the arena's test scene)
cp "$BOOT" "$RIG/boot.img"
printf 'DEVICE=FUJINET.SYS FUJI_PORT=2 FUJI_BPS=%s\r\n' "$BAUD" > "$RIG/config.sys"
printf '@ECHO OFF\r\nFUJIBLST\r\n' > "$RIG/autoexec.bat"
mcopy -o -i "$RIG/boot.img" "$RIG/config.sys" ::CONFIG.SYS
mcopy -o -i "$RIG/boot.img" "$RIG/autoexec.bat" ::AUTOEXEC.BAT
mcopy -o -i "$RIG/boot.img" "$EXE" ::FUJIBLST.EXE
mdel -i "$RIG/boot.img" ::STATS.TXT 2>/dev/null || true
[ "${OFFLINE:-0}" = 1 ] && mdel -i "$RIG/boot.img" ::CONFIG.SYS

# the machine: the VM's settings, headless, our port and floppy
rm -rf "$RIG/vm"; mkdir -p "$RIG/vm"
[ -d "$VM/nvr" ] && cp -a "$VM/nvr" "$RIG/vm/"
CFG=$VM/86box.cfg; [ -f "$CFG" ] || CFG=$VM/86Box.cfg
python3 - "$CFG" "$RIG/vm/86box.cfg" "$BOIP" "$RIG/boot.img" <<'PY'
import re, sys
out, sec = [], ""
for line in open(sys.argv[1]).read().splitlines():
    m = re.match(r"\[(.*)\]", line)
    if m:
        sec = m.group(1)
    if line.startswith(("fdd_01_fn", "fdd_0", "vid_renderer")) and "history" not in line:
        continue
    if sec.startswith("FujiNet (BoIP)") and line.startswith("port"):
        line = "port = " + sys.argv[3]
    out.append(line)
    if sec == "General" and m:
        out.append("vid_renderer = qt_software")
    if sec == "Floppy and CD-ROM drives" and m:
        out.append("fdd_01_fn = " + sys.argv[4])
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY

export RIG OUT TABLE BOTS START ROUNDS BAUD BOIP SECS SHOTS
unshare -rn bash -c '
    ip link set lo up
    "$RIG/fujiblastd" -stream :9910 -seed 7 -bots "" > "$RIG/server.log" 2>&1 &
    srv=$!
    ( cd "$RIG/fn" && exec ./fujinet > "$RIG/fujinet.log" 2>&1 ) &
    fn=$!
    trap "kill $srv $fn \$bots 2>/dev/null; true" EXIT
    for i in $(seq 40); do ss -ltn | grep -q ":9910 " && ss -ltn | grep -q ":$BOIP " && break; sleep 0.25; done
    "$RIG/blastbot" -addr 127.0.0.1:9910 -table "$TABLE" -n "$BOTS" -start "$START" \
        -rounds "$ROUNDS" > "$RIG/bots.log" 2>&1 &
    bots=$!
    if [ -n "$SHOTS" ]; then
        Xvnc :93 -SecurityTypes None -geometry 1024x800 -depth 24 -rfbunixpath "$RIG/v.sock" \
            -nolisten tcp > "$RIG/xvnc.log" 2>&1 &
        xv=$!
        trap "kill $srv $fn \$bots $xv 2>/dev/null; true" EXIT
        sleep 1
        ( n=0; while sleep "$SHOTS"; do n=$((n + 1));
            import -display :93 -window root "$OUT/shot$(printf %03d $n).png" 2>/dev/null; done ) &
        sh=$!
        unset WAYLAND_DISPLAY
        DISPLAY=:93 QT_QPA_PLATFORM=xcb timeout -s KILL "$SECS" 86Box -N -P "$RIG/vm" > "$RIG/86box.log" 2>&1 || true
        kill $sh 2>/dev/null || true
    else
        QT_QPA_PLATFORM=offscreen timeout -s KILL "$SECS" 86Box -N -P "$RIG/vm" > "$RIG/86box.log" 2>&1 || true
    fi
'
mtype -i "$RIG/boot.img" ::STATS.TXT > "$OUT/stats.txt" 2>/dev/null || echo "(no STATS.TXT)" > "$OUT/stats.txt"
mcopy -o -i "$RIG/boot.img" ::PROF.BIN "$OUT/prof.bin" 2>/dev/null || true
cat "$OUT/stats.txt"
[ -f "$OUT/prof.bin" ] && python3 "$HERE/tools/dosrig/prof86.py" "$OUT/prof.bin" "$HERE/build/fujiblst-stats.map" | head -25

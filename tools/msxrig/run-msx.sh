#!/usr/bin/env bash
# Run the MSX client in openMSX with FujiNet-PC (the MSX build that mounts user ROMs: ~/Workspace/
# fn-msx-romtype/build/dist, the Go MSX desktop's pin), CONFIG on: the ROM is mounted from the SD folder and
# booted with `b`, as a user does. openMSX (the FujiNet extension) takes the cartridge's link on BoIP 1985
# and FujiNet-PC listens there. Your own fnconfig.ini is not touched: a scratch copy is used.
#
#   tools/msxrig/run-msx.sh r2r/msxrom/fujiblst[ASCII8].rom
#
# APPKEYS=dir      the lobby's AppKeys: dir's *.key into SD/FujiNet/
# KEYS=script      lines "SECONDS CMD ARG", SECONDS from the start: type TEXT (\n is RETURN, \e ESCAPE),
#                  down|up|tap KEY (u d l r the arrows, s SPACE, e ESCAPE, 1 F1)
# SECS=n           run n seconds (default 60), a picture every EVERY seconds (default 5) into SHOTDIR
# BOOTAT=n         the second CONFIG gets its `b` (default 12)
# PEEKS="SYM ..."  print these symbols' words (from the map next to the ROM) at the end
# NOPNG=1         leave the .vram dumps unpainted
# WAV=file.wav     the sound, recorded
set -e
ROM=$(realpath "$1")
HERE=$(cd "$(dirname "$0")" && pwd)
FNPC=${FNPC:-$HOME/Workspace/fn-msx-romtype/build/dist}
SECS=${SECS:-60}
EVERY=${EVERY:-5}
BOOTAT=${BOOTAT:-12}
WORK=$(mktemp -d "${TMPDIR:-/tmp}/fujiblast-msx.XXXXXX")
SHOTDIR=$(realpath -m "${SHOTDIR:-$WORK/shots}")
mkdir -p "$WORK/sd/FujiNet" "$SHOTDIR"
cp "$ROM" "$WORK/sd/fujiblst[ASCII8].rom"
[ -n "$APPKEYS" ] && { cp "$APPKEYS"/*.key "$WORK/sd/FujiNet/" 2>/dev/null || true; }
tr -d '\r' < "$FNPC/fnconfig.ini" | awk '
     /^\[/ { skip = /^\[Mount[0-9]+\]/ }
     /^configenabled=/ { $0 = "configenabled=1" }
     /^boot_mode=/ { $0 = "boot_mode=0" }
     !skip
     END { print "[Mount1]"; print "hostslot=1"; print "path=/fujiblst[ASCII8].rom"; print "mode=r" }' > "$WORK/fnconfig.ini"

MAPFILE="${ROM%/*}/fujiblst.map"
python3 - "$WORK/run.tcl" "$SHOTDIR" "$SECS" "$EVERY" "$BOOTAT" "${KEYS:-}" "${WAV:-}" "$MAPFILE" "${PEEKS:-}" <<'PY'
import re, sys
out, shots, secs, every, bootat, keys, wav, mapfile, peeks = sys.argv[1:10]
syms = dict(re.findall(r"^(\S+)\s+=\s+\$([0-9A-Fa-f]{4})", open(mapfile).read(), re.M))
KEY = {"e": (7, 0x04), "u": (8, 0x20), "d": (8, 0x40), "l": (8, 0x10), "r": (8, 0x80), "s": (8, 0x01), "1": (6, 0x20)}
ev = [(float(bootat), 'type b')]
if keys:
    for line in open(keys):
        line = line.rstrip("\n")
        if not line.strip() or line.startswith("#"):
            continue
        at, cmd, _, arg = line.partition(" ")[0], *line.partition(" ")[2].partition(" ")
        if cmd == "type":
            ev.append((float(at), "type {%s}" % arg.replace("\\n", "\r").replace("\\e", "\x1b")))
        else:
            row, bit = KEY[arg]
            if cmd in ("down", "tap"):
                ev.append((float(at), "keymatrixdown %d 0x%02X" % (row, bit)))
            if cmd == "up":
                ev.append((float(at), "keymatrixup %d 0x%02X" % (row, bit)))
            if cmd == "tap":
                ev.append((float(at) + 0.15, "keymatrixup %d 0x%02X" % (row, bit)))
ev.sort(key=lambda e: e[0])
t = ["set t0 [openmsx_info realtime]", "set shot 0", "set evs {"]
t += ["  {%s {%s}}" % (a, c) for a, c in ev]
t += ["}", "set next 0",
      "proc vram {n} { set f [open %s/shot$n.vram wb]; fconfigure $f -translation binary; "
      "puts -nonewline $f [debug read_block VRAM 0 16384]; close $f }" % shots,
      "proc tick {} {",
      "  global t0 evs next shot",
      "  set now [expr {[openmsx_info realtime] - $t0}]",
      "  while {[llength $evs] && [lindex $evs 0 0] <= $now} { eval [lindex $evs 0 1]; set evs [lrange $evs 1 end] }",
      "  if {$now >= $shot * %s} { vram $shot; incr shot }" % every,
      "  if {$now >= %s} { %s exit }" % (secs, " ".join(
          'puts stderr "PEEK %s [peek16 0x%s]";' % (n, syms["_" + n]) for n in peeks.split())),
      "  after time 0.05 tick",
      "}"]
if wav:
    t.append("record start -audioonly %s" % wav)
t += ["after time 0.05 tick"]
open(out, "w").write("\n".join(t) + "\n")
PY

cleanup() { kill $FN 2>/dev/null || true; }
trap cleanup EXIT
( cd "$FNPC" && exec ./fujinet -u 127.0.0.1:8010 -c "$WORK/fnconfig.ini" -s "$WORK/sd" ) > "$WORK/fujinet.log" 2>&1 &
FN=$!
sleep 2
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy openmsx -machine C-BIOS_MSX1 -ext FujiNet \
    -command "set renderer none; set mute on" -script "$WORK/run.tcl" > "$WORK/openmsx.log" 2>&1 || true
for v in "$SHOTDIR"/shot*.vram; do
    [ -n "${NOPNG:-}" ] && break
    python3 "$HERE/../tmsshot.py" "$v" "${v%.vram}.png"
done
echo "shots in $SHOTDIR; logs in $WORK"

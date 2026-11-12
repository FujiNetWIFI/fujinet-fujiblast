#!/bin/sh
# Run the Adam client in an emulator with FujiNet-PC (an ADAM build: ~/Workspace/fujinet-pc-adam/build/dist by
# default), which serves the image as the Adam's first disk, CONFIG off, so the Adam boots it, and gives it the
# network. The emulator takes the AdamNet link on BoIP: it listens, and FujiNet-PC dials it. Your own
# fnconfig.ini is not touched: a scratch copy is used, with its own BoIP and web ports and its own SD folder.
#
#   tools/run-adam.sh build/adam/syn-ack-wire-adam.ddp    adamcore (FujiNet Go Adam's core), headless, in real
#                                                         time: FRAMES frames (default 3600), a picture every
#                                                         EVERY frames (default 300) into SHOTDIR (PNG)
#   KEYS=script ...                                       keys from a script on adamcore's keyboard: lines
#                                                         "SECONDS TEXT", the text's \n RETURN, \b BACKSPACE,
#                                                         \e ESCAPE, \1-\6 SmartKeys I-VI, \u \d \l \r arrows;
#                                                         controller 1: [U] [D] [L] [R] the stick, [A] [B] left
#                                                         and right fire, [*] [#] [0]-[9] the keypad, [*A] [*B]
#                                                         [*#] together (an adamcore-cli that takes FE hi lo on
#                                                         stdin as the controller's word: ADAMCORE=its path)
#   WAV=file.wav ...                                      adamcore's sound too
#   EMU=adamem tools/run-adam.sh IMAGE                    ADAMEm's window instead (~/Workspace/adamem_sdl);
#                                                         SECS=n closes it after n seconds
#   APPKEYS=dir ...                                       the lobby's AppKeys: dir's *.key into SD/FujiNet/
#   INSTANCE=1 ...                                        a second Adam: BoIP 9990 + n, web page 8030 + n
#
# Start the server first:  cd ../server && python3 serve.py --host 127.0.0.1 --port 6599 --trace
# and build the client to dial it:  make adam ADAM=build/adam/t URL=N:TCP://127.0.0.1:6599/ LOBBY_KEY=0
set -e
IMAGE=$(realpath "$1")
HERE=$(cd "$(dirname "$0")" && pwd)
FUJINET_DIR=${FUJINET_DIR:-$HOME/Workspace/fujinet-pc-adam/build/dist}
ROMS=${ROMS:-$HOME/Workspace/adam-roms}
ADAMCORE=${ADAMCORE:-$HOME/Workspace/adamcore/build/adamcore-cli}
ADAMEM=${ADAMEM:-$HOME/Workspace/adamem_sdl}
EMU=${EMU:-adamcore}
INSTANCE=${INSTANCE:-0}
BOIP=$((9990 + INSTANCE))
WEB=$((8030 + INSTANCE))
FRAMES=${FRAMES:-3600}
EVERY=${EVERY:-300}
for p in $BOIP $WEB; do
    if ss -ltn | grep -q "[:.]$p "; then
        echo "port $p is in use: another INSTANCE?" >&2
        exit 1
    fi
done
WORK=$(mktemp -d "${TMPDIR:-/tmp}/fujiblast-adam.XXXXXX")
SHOTDIR=${SHOTDIR:-$WORK/shots}
mkdir -p "$WORK/sd/FujiNet" "$SHOTDIR"
SHOTDIR=$(realpath "$SHOTDIR")
cp "$IMAGE" "$WORK/sd/fujiblst.ddp"
[ -n "$APPKEYS" ] && { cp "$APPKEYS"/*.key "$WORK/sd/FujiNet/" 2>/dev/null || true; }
tr -d '\r' < "$FUJINET_DIR/fnconfig.ini" | awk -v boip=$BOIP '
     /^\[/ { skip = /^\[(Mount[0-9]+|BOIP)\]/ }
     /^configenabled=/ { $0 = "configenabled=0" }
     /^boot_mode=/ { $0 = "boot_mode=0" }
     !skip
     END { print "[Mount1]"; print "hostslot=1"; print "path=/fujiblst.ddp"; print "mode=r"; print ""
           print "[BOIP]"; print "enabled=1"; print "host=127.0.0.1"; print "port=" boip }' |
    sed 's/$/\r/' > "$WORK/fnconfig.ini"

# the emulator listens first; FujiNet-PC dials it once it is up
case $EMU in
adamcore)
    keys() {                            # KEYS's lines, at their times, on adamcore's keyboard (--stdin-keys)
        [ -n "$KEYS" ] || { sleep $((FRAMES / 60 + 2)); return; }
        python3 - "$KEYS" $((FRAMES / 60 + 2)) <<'PY'
import sys, time
start = time.time()
esc = {"n": b"\r", "b": b"\b", "e": b"\x1b", "u": b"\xa0", "r": b"\xa1", "d": b"\xa2", "l": b"\xa3", "\\": b"\\"}
for line in open(sys.argv[1]):
    if not line.strip() or line.startswith("#"):
        continue
    at, _, text = line.rstrip("\n").partition(" ")
    time.sleep(max(0, float(at) - (time.time() - start)))
    out, i = [], 0                      # keys, and the controller's words (hi: stick, left fire; lo: keypad, right fire)
    while i < len(text):
        c = text[i]
        if c == "\\" and i + 1 < len(text):
            i += 1
            c = text[i]
            out.append(bytes([0x80 + int(c)]) if c in "123456" else esc[c])
        elif c == "[" and "]" in text[i:]:
            j = text.index("]", i)
            hi, lo = 0x7F, 0x7F
            press = text[i + 1:j]
            for q in (["*", press[1:]] if press[:1] == "*" and len(press) > 1 else [press]):
                if q in ("U", "R", "D", "L"):
                    hi &= ~{"U": 1, "R": 2, "D": 4, "L": 8}[q]
                elif q == "A":
                    hi &= ~0x40
                elif q == "B":
                    lo &= ~0x40
                else:                   # the keypad: 0-9, * 10, # 11
                    lo = (lo & 0x40) | 0x30 | {"*": 10, "#": 11}.get(q, int(q) if q.isdigit() else 15)
            out.append(bytes([0xFE, hi, lo]))
            i = j
        else:
            out.append(c.encode())
        i += 1
    for b in out:                       # one a frame or so: the keyboard's pace
        sys.stdout.buffer.write(b)
        sys.stdout.buffer.flush()
        time.sleep(0.12)
        if b[0] == 0xFE:                # a press, then let go
            sys.stdout.buffer.write(b"\xfe\x7f\x7f")
            sys.stdout.buffer.flush()
            time.sleep(0.12)
time.sleep(max(0, float(sys.argv[2]) - (time.time() - start)))    # then quiet till the emulator stops
PY
    }
    keys | "$ADAMCORE" -adam -os7 "$ROMS/OS7.rom" -eos "$ROMS/EOS.rom" -wp "$ROMS/WP.rom" -fujinet $BOIP \
        --throttle --stdin-keys $EMUARGS --frames "$FRAMES" --ppm-every "$EVERY" "$SHOTDIR/f" \
        --dump-ram "$WORK/ram.bin" --dump-vram "$WORK/vram.bin" ${WAV:+--wav "$WAV"} > "$WORK/emu.log" 2>&1 &
    ;;
adamem)
    (cd "$ADAMEM" && exec ${SECS:+timeout $SECS} ./adamem -fujinet $BOIP) > "$WORK/emu.log" 2>&1 &
    ;;
*)
    echo "EMU: adamcore or adamem" >&2
    exit 1
    ;;
esac
EMUPID=$!
sleep 0.5
(cd "$FUJINET_DIR" && exec stdbuf -oL ./fujinet -c "$WORK/fnconfig.ini" -s "$WORK/sd" -u "http://127.0.0.1:$WEB") \
    > "$WORK/fujinet.log" 2>&1 &
FN=$!
trap 'set +e; kill $FN 2>/dev/null; kill $EMUPID 2>/dev/null; echo "logs: $WORK"' EXIT INT TERM
wait $EMUPID || true
if [ "$EMU" = adamcore ]; then
    for f in "$SHOTDIR"/f*.ppm; do
        [ -e "$f" ] && magick "$f" "${f%.ppm}.png" && rm "$f"
    done
    echo "pictures: $SHOTDIR"
fi

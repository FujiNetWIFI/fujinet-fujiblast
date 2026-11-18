#!/bin/sh
# mkfnlib.sh SRC OUT -- build fujinet-lib-experimental (SRC, its
# add-atari7800 branch: bus/atari7800, the cartridge's mailbox) for the 7800
# into OUT/fujinet-a78.lib, headers in OUT/include, compiled here (not in
# SRC's own build). The lib's crt0 is left out (src/atari7800/crt0.s is the
# game's).
set -e
src=$1
out=$2
[ -d "$src/bus/atari7800" ] || { echo "mkfnlib.sh: no $src/bus/atari7800 (fujinet-lib-experimental's add-atari7800 branch)" >&2; exit 1; }
rm -rf "$out"
mkdir -p "$out/obj" "$out/include"
cp -r "$src"/include/. "$out/include/"
cp "$src"/bus/atari7800/*.h "$out/include/"
( cd "$src" && git rev-parse --short HEAD 2>/dev/null || echo unknown ) > "$out/VERSION"
objs=
for c in "$src"/common/*.c "$src"/bus/atari7800/*.c "$src"/bus/atari7800/*.s; do
    b=$(basename "$c")
    [ "$b" = crt0.s ] && continue
    o="$out/obj/${b%.*}.o"
    # what only the game's boot calls goes in a switched bank (src/atari7800
    # linker config): the appkeys in bank 2 with the game's appkey.c
    # (OVERLAY2, reached through bank.c), finding the cartridge in bank 0
    # (OVERLAY1). The fixed bank is full
    seg=
    case "${b%.*}" in
        appkey_atari7800|fuji_appkey) seg="--code-name OVERLAY2 --rodata-name OVL2RO" ;;
        fuji_atari7800) seg="--code-name OVERLAY1 --rodata-name OVL1RO" ;;
    esac
    case "$c" in
        *.s) ca65 -t atari7800 -o "$o" "$c" ;;
        *)   cl65 -t atari7800 -O --cpu 6502 -DBUILD_ATARI7800 $seg \
                  -I "$out/include" -I "$src/bus/atari7800" -c -o "$o" "$c" ;;
    esac
    objs="$objs $o"
done
ar65 a "$out/fujinet-a78.lib" $objs

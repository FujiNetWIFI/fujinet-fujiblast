#!/bin/sh
# fnlib_msx.sh REPO COMMIT OUT -- fujinet-lib for the MSX, built with zsdcc (the client's compiler)
# from a clean export of COMMIT of REPO (FujiNetWIFI/fujinet-lib, main): OUT/fujinet-msx.lib and
# its headers in OUT/include. The released MSX lib is sccz80, whose arguments come in the other
# order, so it is rebuilt here from the sources the library's own Makefile uses for msx (common,
# bus/msx, bus/fujiversal). The bus is MSX-UNAPI: fuji_bus_call finds the FujiNet's UNAPI ROM
# through EXTBIO and enters it with CALSLT (bus/msx/fujinet-unapi-*.c). One local fix:
#   - network_open sends the URL with its NUL (the firmware takes the string without a length).
# OUT must be inside this repository: the container (tools/z88dk) sees nothing else.
set -e
repo=$1
commit=$2
out=$3
[ -n "$out" ] || { echo "usage: fnlib_msx.sh REPO COMMIT OUT" >&2; exit 1; }
here=$(cd "$(dirname "$0")" && pwd)
rm -rf "$out"
mkdir -p "$out/export" "$out/src" "$out/obj" "$out/include"
git -C "$repo" archive "$commit" | tar -x -C "$out/export"
cp "$out"/export/common/*.c "$out"/export/bus/msx/*.c "$out"/export/bus/msx/*.h \
   "$out"/export/bus/fujiversal/*.c "$out"/export/bus/fujiversal/*.h "$out/src/"
cp -r "$out"/export/include/. "$out/include/"
cp "$out"/export/bus/msx/*.h "$out/include/"
rm -rf "$out/include/cc65"
sed -i.orig 's/^#define NETWORK_OPEN_LEN strlen(devicespec)$/#define NETWORK_OPEN_LEN (strlen(devicespec) + 1)/' \
    "$out/include/fujinet-network.h"
grep -q 'strlen(devicespec) + 1' "$out/include/fujinet-network.h" || {
    echo "fnlib_msx.sh: NETWORK_OPEN_LEN not patched" >&2; exit 1; }
cmds=
objs=
for c in "$out"/src/*.c; do
    o="$out/obj/$(basename "$c").o"
    objs="$objs $o"
    cmds="$cmds zcc +msx -subtype=rom -compiler=sdcc -SO3 -DBUILD_MSX -I$out/include -I$out/src -c -o $o $c || exit 1;"
done
"$here/z88dk" sh -c "$cmds"
"$here/z88dk" z80asm -x$out/fujinet-msx.lib $objs

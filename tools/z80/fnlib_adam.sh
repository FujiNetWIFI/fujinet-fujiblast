#!/bin/sh
# fnlib_adam.sh REPO COMMIT OUT -- fujinet-lib for the Adam, built with zsdcc (the client's
# compiler) from a clean export of COMMIT of REPO (fujinet-lib-experimental): OUT/fujinet-adam.lib
# and its headers in OUT/include. The released Adam library is an sccz80 build, which takes its
# arguments in the other order. As fujinet-multiplayer-mule builds it (tools/z80/mkfnlib.sh):
# common/*.c and bus/adam/*, without <stdio.h> (debug only). OUT must be inside this repository:
# the container (tools/z88dk) sees nothing else.
set -e
repo=$1
commit=$2
out=$3
[ -n "$out" ] || { echo "usage: fnlib_adam.sh REPO COMMIT OUT" >&2; exit 1; }
rm -rf "$out"
mkdir -p "$out/export" "$out/src" "$out/obj"
git -C "$repo" archive "$commit" | tar -x -C "$out/export"
cp "$out"/export/common/*.c "$out"/export/bus/adam/* "$out/src/"
cp -r "$out/export/include" "$out/include"
rm -rf "$out/include/cc65"
cp "$out/export/bus/adam/fujinet-bus-adam.h" "$out/include/"
sed -i -e '/#include <stdio.h>/d' "$out"/src/*.c
cmds=
objs=
for c in "$out"/src/*.c; do
    o="$out/obj/$(basename "$c" .c).o"
    objs="$objs $o"
    cmds="$cmds zcc +coleco -subtype=adam -compiler=sdcc -SO3 -DBUILD_ADAM -I$out/include -I$out/src -c -o $o $c || exit 1;"
done
"$(dirname "$0")/z88dk" sh -c "$cmds z80asm -x$out/fujinet-adam.lib $objs"

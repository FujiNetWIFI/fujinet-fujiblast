#!/bin/sh
# fnlib_msx.sh REPO COMMIT OUT -- fujinet-lib for the MSX, built with zsdcc (the client's compiler)
# from a clean export of COMMIT of REPO (fujinet-lib-experimental): OUT/fujinet-msx.lib and its
# headers in OUT/include. As fujinet-multiplayer-mule builds it (tools/z80/mkfnlib.sh), with the
# local fixes it needs:
#   - bus/msx: portio.s and timeout.s are replaced by tools/z80/msx-portio.c (the asm uses IX and
#     IY, which zsdcc keeps, and port_get_until left a word on the stack when it timed out);
#     VDP_IS_PAL reads the byte at 002Bh (it tested the address).
#   - bus/msx: network_bus_read/write use the renamed NETCMD_READ/WRITE and report a failed call
#     as 0 bytes, as bus/msdos does; it has no appkey calls, so bus/msdos's are added.
#   - bus/msx: fuji_bus_call reads its 8-bit aux arguments with va_arg(ap, uint8_t), but zsdcc
#     passes some of them as one byte: every aux argument is now passed as an int and read as one.
#   - network_open sends the URL with its NUL (the firmware takes the string without a length).
#   - no <stdio.h> (debug only).
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
cp "$out"/export/common/*.c "$out"/export/bus/msx/* "$out/src/"
cp "$out"/export/bus/msdos/appkey.c "$out/src/"
cp -r "$out"/export/include/. "$out/include/"
rm -rf "$out/include/cc65"
rm -f "$out/src/portio.s" "$out/src/timeout.s"
cp "$here/msx-portio.c" "$out/src/portio.c"
cat > "$out/src/portio.h" <<'H'
/* portio.h, for tools/z80/msx-portio.c (fnlib_msx.sh). */
#include <stdint.h>
extern int port_getc(void);
extern int port_getc_timeout(uint16_t timeout);
extern int port_discard_until(uint8_t c, uint16_t timeout);
extern uint16_t port_getbuf(void *buf, uint16_t len, uint16_t timeout);
extern uint16_t port_get_until(void *buf, uint16_t maxlen, uint8_t c, uint16_t timeout);
extern void port_putc(uint8_t c);
extern uint16_t port_putbuf(void *buf, uint16_t len);
#define VDP_IS_PAL (*(unsigned char *) 0x002b & 0x80)
H
sed -i -e 's/NETCALL_B12_RV(FUJICMD_READ,/if (!NETCALL_B12_RV(NETCMD_READ,/' \
       -e 's/NETCALL_B12_D(FUJICMD_WRITE,/if (!NETCALL_B12_D(NETCMD_WRITE,/' \
       -e 's/^\(  if (!NETCALL_B12_[RVD]*(.*, buffer, length)\);$/\1)\n    return 0;/' \
       -e 's/va_arg(ap, uint8_t)/(uint8_t) va_arg(ap, int)/' \
       "$out/src/fujinet-bus-msx.c"
python3 - "$out/include" <<'P'
import re, sys
d = sys.argv[1]
p = d + "/fujinet-bus-ezvar.h"
s = open(p).read()
s = "\n".join(re.sub(r"\b(a[1-4])\b", r"(int)(\1)", l) if "fuji_bus_call(" in l or l.lstrip().startswith(("a", "(")) and "reply" not in l else l
              for l in s.split("\n"))
open(p, "w").write(s)
p = d + "/fujinet-endian.h"
s = open(p).read()
s = s.replace("#define NATIVE_SPLIT_U16(w) U16_LSB(w), U16_MSB(w)",
              "#define NATIVE_SPLIT_U16(w) (int)U16_LSB(w), (int)U16_MSB(w)")
s = s.replace("  U16_LSB(U32_LSW(l)), U16_MSB(U32_LSW(l)),                             \\\n"
              "    U16_LSB(U32_MSW(l)), U16_MSB(U32_MSW(l))",
              "  (int)U16_LSB(U32_LSW(l)), (int)U16_MSB(U32_LSW(l)),                   \\\n"
              "    (int)U16_LSB(U32_MSW(l)), (int)U16_MSB(U32_MSW(l))")
open(p, "w").write(s)
P
sed -i 's/^#define NETWORK_OPEN_LEN strlen(devicespec)$/#define NETWORK_OPEN_LEN (strlen(devicespec) + 1)/' \
    "$out/include/fujinet-network.h"
grep -q 'strlen(devicespec) + 1' "$out/include/fujinet-network.h" || {
    echo "fnlib_msx.sh: NETWORK_OPEN_LEN not patched" >&2; exit 1; }
sed -i -e '/#include <stdio.h>/d' "$out"/src/*.c
cmds=
objs=
for c in "$out"/src/*.c; do
    o="$out/obj/$(basename "$c").o"
    objs="$objs $o"
    cmds="$cmds zcc +msx -subtype=rom -compiler=sdcc -SO3 -DBUILD_MSX -I$out/include -I$out/src -c -o $o $c || exit 1;"
done
"$here/z88dk" sh -c "$cmds z80asm -x$out/fujinet-msx.lib $objs"

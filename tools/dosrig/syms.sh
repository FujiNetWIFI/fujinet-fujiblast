#!/bin/sh
# syms.sh MAP OUT -- every function's address, static ones too: each object's
# labels (wdis) placed by a public one the map has. For prof86.py -s OUT.
set -e
map=$1
out=$2
here=$(cd "$(dirname "$0")/../.." && pwd)
tools=$here/tools/dos
: > "$out.tmp"
for o in $(find "$here/build/fujiblst/msdos" -name '*.o' | sort); do
    rel=${o#$here/}
    "$tools/ow" wdis "$rel" 2>/dev/null | python3 -c '
import re, sys
map = {n: int(a, 16) for a, n in re.findall(r"^0000:([0-9a-f]{4})[+*]?\s+(\S+)\s*$", open(sys.argv[1]).read(), re.M)}
seg = None
labels = []
for line in sys.stdin:
    if line.startswith("Segment:"):
        seg = line.split()[1]
    m = re.match(r"^([0-9A-F]{4})\s+([A-Za-z_][A-Za-z0-9_]*_):$", line)
    if m and seg == "_TEXT":
        labels.append((int(m.group(1), 16), m.group(2)))
base = None
for off, name in labels:
    if name in map:
        base = map[name] - off
        break
if base is not None:
    for off, name in labels:
        print("%04x %s" % (base + off, name))
' "$map" >> "$out.tmp"
done
sort -u "$out.tmp" > "$out"
rm -f "$out.tmp"
wc -l < "$out"

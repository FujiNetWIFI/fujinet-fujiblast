#!/usr/bin/env python3
"""fixreach.py -- rebuild until AS stops reporting "jump distance too big",
turning each such relative branch (BCTR BCFR BSTR BSFR BRNR BDRR BIRR) into
its absolute form. A development aid; the result is ordinary source.
(M.U.L.E.'s clients/arcadia/tools/fixreach.py.)

    fixreach.py BUILD.SH SRCDIR
"""
import os
import re
import subprocess
import sys

REL = {"BCTR": "BCTA", "BCFR": "BCFA", "BSTR": "BSTA", "BSFR": "BSFA",
       "BRNR": "BRNA", "BDRR": "BDRA", "BIRR": "BIRA", "BSNR": "BSNA"}
build, srcdir = sys.argv[1], sys.argv[2]
for _ in range(400):
    r = subprocess.run([build], capture_output=True, text=True)
    out = r.stdout + r.stderr
    hits = re.findall(r"(\w+\.inc)\((\d+)\): error: jump distance too big", out)
    if not hits:
        print(out)
        sys.exit(r.returncode)
    for f, ln in sorted(set(hits)):
        path = os.path.join(srcdir, f)
        lines = open(path).read().split("\n")
        i = int(ln) - 1
        for k, v in REL.items():
            if re.search(r"\b%s\b" % k, lines[i]):
                lines[i] = re.sub(r"\b%s\b" % k, v, lines[i])
                print("fixreach: %s:%s %s -> %s" % (f, ln, k, v))
                break
        else:
            print("fixreach: %s:%s: nothing to widen: %s" % (f, ln, lines[i]))
            sys.exit(1)
        open(path, "w").write("\n".join(lines))

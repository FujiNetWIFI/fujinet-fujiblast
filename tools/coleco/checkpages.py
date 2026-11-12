#!/usr/bin/env python3
"""checkpages.py SRCDIR -- no page may use a label that another page defines.

The pages share C000H-DFFFH (a page's 4K is at C000H or D000H, an image page
at a time): a label defined in one page, used from another, assembles fine
and reads whatever sits at that address in the page then mapped (a string
once came out as garbage that way). Fixed code (8000H, E000H) is mapped
always and may be used from anywhere; so may the equates. A page that needs
data from another page keeps its own copy.
"""
import glob
import os
import re
import sys

PAGES = ["p_boot.inc", "p_net.inc", "p_arena.inc", "p_play.inc", "p_menu.inc",
         "p_room.inc", "p_test.inc"]


def code(line):
    line = re.sub(r";.*", "", line)
    return re.sub(r'"[^"]*"', '""', line)


def main():
    src = sys.argv[1]
    defs, text = {}, {}
    for f in PAGES:
        path = os.path.join(src, f)
        text[f] = [code(l) for l in open(path, errors="replace")]
        for line in text[f]:
            m = re.match(r"([A-Za-z_][A-Za-z0-9_]*):", line)
            if m:
                defs.setdefault(m.group(1).upper(), set()).add(f)
    bad = 0
    for f in PAGES:
        mine = {k for k, v in defs.items() if f in v}
        for n, line in enumerate(text[f], 1):
            body = re.sub(r"^[A-Za-z_][A-Za-z0-9_]*:", "", line)
            for w in re.findall(r"[A-Za-z_][A-Za-z0-9_]*", body):
                u = w.upper()
                if u in defs and u not in mine:
                    print("checkpages: %s:%d uses %s, defined in %s" % (f, n, w, ", ".join(sorted(defs[u]))))
                    bad += 1
    if bad:
        sys.exit(1)
    print("checkpages: no page uses another page's labels")


if __name__ == "__main__":
    main()

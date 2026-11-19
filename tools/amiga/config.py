"""Keep the selected server between builds, without touching other platforms."""

import json
from pathlib import Path
import sys

out = Path("gen/amiga/config.h")
previous = "fujinet.online:9910"
if out.exists():
    for line in out.read_text().splitlines():
        if line.startswith("#define DEFAULT_SERVER "):
            previous = json.loads(line[len("#define DEFAULT_SERVER "):])
value = sys.argv[1] or previous
# Verified against https://lobby.fujinet.online/viewFull (FujiBlast appkey 16).
# Keep this preset independent of local SERVER overrides used by the emulator.
lobby = (sys.argv[2] if len(sys.argv) > 2 else "") or "fujinet.online:9910"
text = ("#define DEFAULT_SERVER " + json.dumps(value) + "\n"
        "#define LOBBY_SERVER " + json.dumps(lobby) + "\n")
if not out.exists() or out.read_text() != text:
    out.write_text(text)

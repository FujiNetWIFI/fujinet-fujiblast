#!/usr/bin/env python3
"""Fetch the fixed application release; no source checkout or legacy library."""

import hashlib
from pathlib import Path
import tempfile
import urllib.request
import zipfile

VERSION = "0.2.0"
NAME = f"fujinet-nio-lib-amiga-{VERSION}.zip"
URL = f"https://github.com/fujinet-nio/fujinet-nio-lib/releases/download/v{VERSION}/{NAME}"
SHA256 = "cf5c579bcc185d258bb2bbd26cbfef4b2d48a0f20eb5cd9ee298dd17c9413f6c"
DEST = Path("_cache/fujinet-nio-lib/amiga-0.2.0")


def install(dest=DEST):
    dest = Path(dest)
    dest.parent.mkdir(parents=True, exist_ok=True)
    archive = dest.parent / NAME
    if not archive.exists():
        with tempfile.TemporaryDirectory(dir=dest.parent) as temporary:
            tmp = Path(temporary) / NAME
            with urllib.request.urlopen(URL, timeout=60) as response:
                tmp.write_bytes(response.read())
            data = tmp.read_bytes()
            if hashlib.sha256(data).hexdigest() != SHA256:
                raise ValueError("NIO release checksum mismatch")
            tmp.replace(archive)
    data = archive.read_bytes()
    if hashlib.sha256(data).hexdigest() != SHA256:
        raise ValueError(f"NIO release checksum mismatch: remove {archive} and retry")
    with zipfile.ZipFile(archive) as z:
        for member in z.infolist():
            target = (dest / member.filename).resolve()
            if not target.is_relative_to(dest.resolve()):
                raise ValueError("unsafe release path")
        z.extractall(dest)
    (dest / ".verified").write_text(SHA256 + "\n")


if __name__ == "__main__":
    install()

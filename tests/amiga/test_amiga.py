import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import mkgfx_amiga as art
import vgapal

spec = importlib.util.spec_from_file_location(
    "fetch", ROOT / "tools/amiga/fetch_nio.py"
)
fetch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fetch)


class AmigaTests(unittest.TestCase):
    def compile_run(self, name, sources, default_server="localhost:9910"):
        with tempfile.TemporaryDirectory() as d:
            out = Path(d) / name
            (Path(d) / "config.h").write_text(
                f'#define DEFAULT_SERVER "{default_server}"\n#define LOBBY_SERVER "lobby.example:9910"\n'
            )
            subprocess.run(
                [
                    "cc",
                    "-std=c99",
                    "-g",
                    "-fsanitize=address,undefined",
                    "-D__AMIGA__",
                    "-Isrc",
                    "-Isrc/platform-specific",
                    "-I_cache/fujinet-nio-lib/amiga-0.2.0/include",
                    "-I" + d,
                    *sources,
                    "-o",
                    str(out),
                ],
                cwd=ROOT,
                check=True,
            )
            subprocess.run([str(out)], check=True)

    def test_sequencer(self):
        self.compile_run("sequencer", ["-DPLAT_MUSIC", "-Isrc/amiga", "tests/amiga/sequencer.c", "src/amiga/sequencer.c", "gen/amiga/music.c"])

    def test_stream_recovery(self):
        self.compile_run("stream", ["tests/amiga/stream.c", "src/amiga/stream.c"])

    def test_queue(self):
        self.compile_run("queue", ["tests/amiga/queue.c"])

    def test_turning(self):
        self.compile_run("turning", ["-ffunction-sections", "-fdata-sections",
                         "-Wl,--gc-sections", "-Igen/amiga",
                         "tests/amiga/turning.c", "src/move.c"])

    def test_controls(self):
        self.compile_run("controls", ["tests/amiga/controls.c", "src/move.c"])

    def test_transport(self):
        self.compile_run(
            "transport", ["tests/amiga/transport.c", "src/amiga/nstrans.c"]
        )

    def test_async_transport(self):
        self.compile_run(
            "async_transport",
            ["-DAMIGA_ASYNC", "tests/amiga/transport.c", "src/amiga/nstrans.c"],
        )

    def test_setup(self):
        self.compile_run("setup", ["-Isrc/amiga", "tests/amiga/setup.c", "src/amiga/setup.c", "src/nio/servers.c"])

    def test_servers(self):
        self.compile_run("servers", ["tests/amiga/servers.c", "src/nio/servers.c", "src/amiga/history_store.c", "src/nio/settings.c"])

    def test_appkeys(self):
        self.compile_run("appkey", ["tests/amiga/appkey.c", "src/appkey.c", "src/nio/servers.c"])

    def test_nio_without_amiga(self):
        self.compile_run("nio_appkey", ["-U__AMIGA__", "-DFB_NIO", "tests/amiga/appkey.c", "src/appkey.c", "src/nio/servers.c"])

    def test_invalid_default(self):
        for server in ("other.example:65536", "other.example:0", ":9910", "a" * 254):
            with self.subTest(server=server):
                self.compile_run("appkey", ["-DINVALID_DEFAULT", "tests/amiga/appkey.c", "src/appkey.c", "src/nio/servers.c"], server)

    def test_palette_seats(self):
        p = vgapal.palette()
        self.assertEqual(
            len({art.nearest(p[vgapal.SEAT + s * 16 + 5]) for s in range(4)}), 4
        )
        self.assertEqual(len(art.PALETTE), 32)
        self.assertTrue(all(0 < c < 32 for c in map(art.nearest, p)))

    def test_cached_release_and_corruption(self):
        with tempfile.TemporaryDirectory() as d:
            dest = Path(d) / "amiga"
            archive = Path(d) / fetch.NAME
            archive.write_bytes(
                (ROOT / "_cache/fujinet-nio-lib" / fetch.NAME).read_bytes()
            )
            with patch.object(
                fetch.urllib.request, "urlopen", side_effect=AssertionError("network")
            ):
                fetch.install(dest)
                self.assertTrue((dest / "fujinet-nio-amiga.a").is_file())
                archive.write_bytes(b"broken")
                with self.assertRaisesRegex(ValueError, "checksum"):
                    fetch.install(dest)


if __name__ == "__main__":
    unittest.main()

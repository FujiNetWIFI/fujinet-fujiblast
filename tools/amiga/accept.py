#!/usr/bin/env python3
"""Disposable A500/OS1.3 acceptance rig. Requires the NIO workspace and licensed media.
Run from the game root, after sourcing the workspace environment.
"""

import argparse
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import time
import threading
import selectors
import re
import signal

ROOT = Path(__file__).resolve().parents[2]


def run(*args, **kw):
    return subprocess.run(args, check=True, **kw)


def wait_for(predicate, seconds, label):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(0.25)
    raise RuntimeError("Timed out: " + label)


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def listening(port):
    # Do not connect to the single-client NIO/serial sockets as a probe.
    return f":{port:04X} " in Path("/proc/net/tcp").read_text()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--machine", choices=["a500", "a500-030"], default="a500")
    ap.add_argument(
        "--timing",
        action="store_true",
        help="Write guest FPS/UPS and stage timings to share/timing.log",
    )
    ap.add_argument("--scenes", action="store_true")
    ap.add_argument("--standard", choices=["pal", "ntsc"], default="pal")
    ap.add_argument("--audio", action="store_true", help="Exercise all tunes and effects without foreground rendering")
    ap.add_argument("--live", action="store_true")
    ap.add_argument(
        "--responsive",
        action="store_true",
        help="Live held-key test while NIO bridge is stalled for one second",
    )
    ap.add_argument(
        "--play",
        action="store_true",
        help="Play interactively against local bots; close Amiberry or Ctrl-C to stop",
    )
    ap.add_argument("--missing-broker", action="store_true")
    ap.add_argument("--scene", type=int, choices=range(6))
    args = ap.parse_args()
    if args.audio and (args.play or args.live or args.scenes or args.scene is not None or args.responsive or args.missing_broker):
        ap.error("--audio cannot be combined with other acceptance modes")
    if args.play and (
        args.live
        or args.responsive
        or args.scenes
        or args.missing_broker
        or args.scene is not None
    ):
        ap.error("--play cannot be combined with acceptance modes")

    def interrupted(*_):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, interrupted)
    if args.responsive:
        if args.scenes or args.missing_broker or args.scene is not None:
            ap.error("--responsive cannot be combined with scene/missing-broker tests")
        args.live = args.timing = True
    ws = Path(os.environ.get("NIO_WORKSPACE", ROOT.parents[1]))
    envf = ws / "local/amiga.env"
    env = json.loads(
        subprocess.check_output(
            [
                "bash",
                "-c",
                'set -a; source "$1"; python3 -c \'import os,json; print(json.dumps({k:os.environ.get(k,"") for k in ["AMIGA_WB13_KICKSTART","AMIGA_WB13_ROM_KEY","AMIGA_WB13_ADF_WORKBENCH"]}))\'',
                "bash",
                str(envf),
            ],
            env={**os.environ, "BASH_ENV": "/dev/null"},
        )
    )
    for key in ("AMIGA_WB13_KICKSTART", "AMIGA_WB13_ADF_WORKBENCH"):
        if not Path(env[key]).is_file():
            raise RuntimeError("Missing " + key)
    pkg = ws / "build/amiga-artifacts/wb13/NIO"
    niobin = ws / "repos/fujinet-nio/build/fujibus-tcp-debug/fujinet-nio"
    for f in (pkg / "fujinet-nio.device", pkg / "fujinet-load-resident", niobin):
        if not f.is_file():
            raise RuntimeError("Missing " + str(f))
    stamp = time.strftime("%Y%m%d-%H%M%S")
    work = ROOT / "build/amiga/evidence" / f"{stamp}-{args.standard}"
    share = work / "share"
    share.mkdir(parents=True)
    (work / "rom").mkdir()
    (work / "nio/fujinet-data").mkdir(parents=True)
    for name in ("fujinet-nio.device", "fujinet-load-resident"):
        shutil.copy2(pkg / name, share / name)
    shutil.copy2(env["AMIGA_WB13_KICKSTART"], work / "rom/kickstart.rom")
    if env["AMIGA_WB13_ROM_KEY"]:
        shutil.copy2(env["AMIGA_WB13_ROM_KEY"], work / "rom/rom.key")
    shutil.copy2(env["AMIGA_WB13_ADF_WORKBENCH"], work / "boot.adf")
    (work / "boot.adf").chmod(0o600)
    game_port = free_port()
    scenes = [args.scene] if args.scene is not None else list(range(6))
    if args.live or args.missing_broker or args.play or args.audio:
        scenes = []
    lines = ["FailAt 21", "Stack 32768"]
    if not args.missing_broker:
        lines += ["FB:fujinet-load-resident FB:fujinet-nio.device fujinet-nio.device"]
    else:
        with (work / "build-startup.log").open("w") as log:
            run("make", "amiga", cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        shutil.copy2(ROOT / "r2r/amiga/fujiblast-nio", share / "startup")
        lines += [
            "FB:startup >FB:startup.log",
            "If ERROR",
            'Echo >FB:expected-error "ERROR"',
            "EndIf",
        ]
    for scene in scenes:
        cmd = [
            "make",
            "-f",
            "tools/amiga/amiga.mk",
            "ACCEPT=1",
            "PERF=1" if args.timing else "PERF=",
        ]
        if scene:
            cmd.append(f"TEST_SCENE={scene}")
        with (work / f"build-{scene}.log").open("w") as log:
            run(*cmd, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        out = (
            "amiga"
            + (f"-scene{scene}" if scene else "")
            + "-accept"
            + ("-perf" if args.timing else "")
        )
        shutil.copy2(ROOT / "build" / out / "fujiblast-nio", share / f"scene{scene}")
        lines += [
            f'Echo >FB:current "scene{scene}"',
            f"FB:scene{scene} >FB:log{scene}",
            "If WARN",
            f'Echo >FB:error{scene} "ERROR"',
            "EndIf",
            f"Copy FB:screen120.bin FB:scene{scene}-120.bin",
        ]
        if scene:
            lines += [f"Copy FB:screen240.bin FB:scene{scene}-240.bin"]
    if args.audio:
        with (work / "build-audio.log").open("w") as log:
            run("make", "-f", "tools/amiga/amiga.mk", "AUDIO=1", "OUT=build/amiga-audio",
                cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        shutil.copy2(ROOT / "build/amiga-audio/fujiblast-nio", share / "audio")
        lines += ["FB:audio >FB:audio.log", "If WARN", 'Echo >FB:error-audio "ERROR"', "EndIf"]
    if args.play:
        with (work / "build-play.log").open("w") as log:
            run(
                "make",
                "-f",
                "tools/amiga/amiga.mk",
                "OUT=build/amiga-play" + ("-perf" if args.timing else ""),
                "PERF=1" if args.timing else "PERF=",
                f"SERVER=127.0.0.1:{game_port}",
                cwd=ROOT,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
        shutil.copy2(
            ROOT
            / (
                "build/amiga-play-perf/fujiblast-nio"
                if args.timing
                else "build/amiga-play/fujiblast-nio"
            ),
            share / "fujiblast-nio",
        )
        lines += ["FB:fujiblast-nio", 'Echo "To play again: Execute FB:run"']
    if args.live:
        with (work / "build-live.log").open("w") as log:
            run(
                "make",
                "-f",
                "tools/amiga/amiga.mk",
                "LIVE=1",
                "OUT=build/amiga-live"
                + ("-responsive" if args.responsive else "")
                + ("-perf" if args.timing else ""),
                "RESPONSIVE=1" if args.responsive else "RESPONSIVE=",
                "PERF=1" if args.timing else "PERF=",
                f"SERVER=127.0.0.1:{game_port}",
                cwd=ROOT,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
        shutil.copy2(
            ROOT
            / (
                "build/amiga-live"
                + ("-responsive" if args.responsive else "")
                + ("-perf" if args.timing else "")
            )
            / "fujiblast-nio",
            share / "live",
        )
        lines += [
            'Echo >FB:current "live"',
            "FB:live >FB:live.log",
            "If WARN",
            'Echo >FB:error-live "ERROR"',
            "EndIf",
        ]
    lines += ['Echo >FB:done "DONE"']
    (share / "run").write_text("\n".join(lines) + "\n")
    (work / "startup").write_text("Path C: SYS:System S: ADD\nExecute FB:run\n")
    for cmd in [
        ("delete", "s/Startup-Sequence"),
        ("write", str(work / "startup"), "s/Startup-Sequence"),
    ]:
        run("xdftool", str(work / "boot.adf"), *cmd, stdout=subprocess.DEVNULL)
    nio_port, serial_port = free_port(), free_port()
    # Use the proven reference rig's config schema, with private ports/data.
    (work / "nio/fujinet-data/fujinet.yaml").write_text(f"""fujinet:
  device_name: "fujiblast-test"
wifi:
  enabled: false
netsio:
  enabled: false
channel:
  tcp_host: "127.0.0.1"
  tcp_port: {nio_port}
  uart:
    baud_rate: 115200
    data_bits: 8
    parity: none
    stop_bits: 1
    flow_control: none
""")
    procs = []
    logs = []
    relay = None
    nio_relay = None
    pressed_key = None
    fire_at = 0.0
    fire_state = 0

    def start(name, cmd, **kw):
        log = (work / (name + ".log")).open("w")
        logs.append(log)
        p = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT, **kw)
        procs.append(p)
        return p

    try:
        if args.play:
            start(
                "server",
                [
                    str(ROOT / "server/fujiblastd"),
                    "-stream",
                    f"127.0.0.1:{game_port}",
                    "-seed",
                    "7",
                ],
            )
            wait_for(lambda: listening(game_port), 10, "game server")
        if args.live:
            server_port = free_port()
            start(
                "server",
                [
                    str(ROOT / "server/fujiblastd"),
                    "-stream",
                    f"127.0.0.1:{server_port}",
                    "-seed",
                    "7",
                    "-bots",
                    "blast1=1",
                ],
            )
            wait_for(lambda: listening(server_port), 10, "game server")
            relay = Relay(game_port, server_port)
        start("nio", [str(niobin)], cwd=work / "nio")
        wait_for(lambda: listening(nio_port), 15, "NIO listening")
        emulator = start(
            "amiberry",
            [
                "amiberry",
                "--log",
                "--config",
                str(ws / "configs/amiga/base-68000.uae"),
                "-G",
                "-r",
                str(work / "rom/kickstart.rom"),
                "-0",
                str(work / "boot.adf"),
                "-s",
                "chipset=ocs",
                "-s",
                "chipmem_size=1",
                "-s",
                "bogomem_size=2",  # UAE units are 256 KB: two gives 512 KB slow RAM.
                "-s",
                "fastmem_size=0",
                *(
                    [
                        "-s",
                        "cpu_model=68030",
                        "-s",
                        "cpu_type=68020",
                        "-s",
                        "cycle_exact=false",
                        "-s",
                        "blitter_cycle_exact=false",
                        "-s",
                        "cpu_cycle_exact=false",
                        "-s",
                        "cpu_memory_cycle_exact=false",
                        "-s",
                        "cpu_speed=max",
                        "-s",
                        "chipmem_size=4",
                        "-s",
                        "fastmem_size=2",
                    ]
                    if args.machine == "a500-030"
                    else []
                ),
                "-s",
                "ntsc=" + ("true" if args.standard == "ntsc" else "false"),
                "-s",
                f"serial_port=tcp://127.0.0.1:{serial_port}",
                "-s",
                "serial_direct=true",
                "-s",
                "serial_hardware_ctsrts=false",
                "-s",
                f"filesystem2=rw,DH1:FB:{share},0",
            ],
        )
        wait_for(lambda: listening(serial_port), 60, "Amiberry serial")
        bridge_port = nio_port
        if args.responsive:
            bridge_port = free_port()
            nio_relay = Relay(bridge_port, nio_port)
        start(
            "bridge",
            ["socat", f"TCP:127.0.0.1:{serial_port}", f"TCP:127.0.0.1:{bridge_port}"],
        )
        print("Guest running:", work, flush=True)
        if args.timing:
            print("Timing log:", share / "timing.log", flush=True)
        captured = set()
        quit_sent = False
        ipc_match = re.search(
            r"IPC: Listening on (\S+)", (work / "amiberry.log").read_text()
        )
        ipc_path = ipc_match.group(1) if ipc_match else None

        def ipc(command, *args):
            if not ipc_path:
                raise RuntimeError("No owned emulator IPC socket")
            with socket.socket(socket.AF_UNIX) as s:
                s.settimeout(3)
                s.connect(ipc_path)
                s.sendall(("\t".join((command, *args)) + "\n").encode())
                response = s.recv(4096).decode()
                if not response.startswith("OK"):
                    raise RuntimeError(response)

        if args.play:
            (work / "amiberry.sock.path").write_text((ipc_path or "") + "\n")
            print(
                "Interactive game: Space/Return to start, arrows to move, Space to bomb.",
                flush=True,
            )
            print(
                "Close Amiberry or press Ctrl-C here to stop all session processes.",
                flush=True,
            )
            while emulator.poll() is None:
                for process in procs:
                    if process is not emulator and process.poll() is not None:
                        raise RuntimeError(
                            "A session service exited; see logs in " + str(work)
                        )
                time.sleep(0.5)
            return

        def completed():
            nonlocal quit_sent, pressed_key, fire_at, fire_state
            if (
                args.responsive
                and (share / "keyboard").exists()
                and pressed_key is None
            ):
                pressed_key = (share / "keyboard").read_text().strip()
                if pressed_key:
                    ipc("SEND_KEY", pressed_key, "1")
                    nio_relay.hold_until = time.monotonic() + 1.0
                    fire_at = time.monotonic() + 0.35
                    (work / "network-stall.txt").write_text(
                        "NIO forwarding paused for 1.0 seconds after key down\n"
                    )
                else:
                    pressed_key = None
            if fire_at and time.monotonic() >= fire_at and fire_state == 0:
                ipc("SEND_KEY", "64", "1")
                fire_state = 1
            elif fire_state == 1 and time.monotonic() >= fire_at + 0.25:
                ipc("SEND_KEY", "64", "0")
                fire_state = 2
            if pressed_key and (share / "keyboard-done").exists():
                ipc("SEND_KEY", pressed_key, "0")
                (share / "keyboard-done").rename(share / "keyboard-released")
            for dump in list(share.glob("screen*.bin")) + list(
                share.glob("live-*.bin")
            ):
                if dump.stat().st_size != 40066:
                    continue
                identity = (dump.name, dump.stat().st_mtime_ns)
                if identity not in captured:
                    current = (
                        (share / "current").read_text().strip()
                        if (share / "current").exists()
                        else "unknown"
                    )
                    if ipc_path:
                        ipc(
                            "SCREENSHOT",
                            str(work / (current + "-" + dump.stem + "-display.png")),
                        )
                    captured.add(identity)
                    if current == "scene0" and not quit_sent:
                        ipc("SEND_KEY", "69", "1")
                        time.sleep(0.1)
                        ipc("SEND_KEY", "69", "0")
                        quit_sent = True
            if relay and (share / "cut").exists():
                relay.cut()
                (share / "cut").rename(share / "cut-done")
            return (share / "done").exists()

        wait_for(completed, 600, "guest completion")
        if args.audio:
            log = (share / "audio.log").read_text()
            assert "FUJIBLAST AUDIO OK" in log and not (share / "error-audio").exists(), log
        if args.missing_broker:
            assert "Load fujinet-nio.device" in (share / "startup.log").read_text()
            assert (share / "expected-error").exists()
        if args.live:
            log = (share / "live.log").read_text()
            if (
                "FUJIBLAST RESPONSIVE OK" if args.responsive else "FUJIBLAST LIVE OK"
            ) not in log:
                raise AssertionError(log)
            assert not (share / "error-live").exists(), log
            for dump in share.glob("live-*.bin"):
                render(dump)
        for scene in scenes:
            log = (share / f"log{scene}").read_text()
            assert not (share / f"error{scene}").exists(), log
            if ("FUJIBLAST QUIT OK" if scene == 0 else "FUJIBLAST SCENE OK") not in log:
                raise AssertionError(log)
            for frame in ((120,) if scene == 0 else (120, 240)):
                dump = share / f"scene{scene}-{frame}.bin"
                assert dump.stat().st_size == 40066, dump
                render(dump)
        if 5 in scenes:
            assert (share / "scene5-120.bin").read_bytes()[66:] != (
                share / "scene5-240.bin"
            ).read_bytes()[66:], "animation did not change display"
        print("PASS:", work, flush=True)
    finally:
        if nio_relay:
            nio_relay.close()
        if relay:
            relay.close()
        for p in reversed(procs):
            if p.poll() is None:
                p.terminate()
        for p in procs:
            try:
                p.wait(timeout=5)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()
        for log in logs:
            log.close()


class Relay:
    """Test-owned TCP relay: cut established connections, keep accepting reconnects."""

    def __init__(self, port, target):
        self.target = target
        self.hold_until = 0.0
        self.connections = []
        self.lock = threading.Lock()
        self.stopping = threading.Event()
        self.listener = socket.socket()
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("127.0.0.1", port))
        self.listener.listen()
        self.listener.settimeout(0.2)
        self.thread = threading.Thread(target=self.accept, daemon=True)
        self.thread.start()

    def accept(self):
        while not self.stopping.is_set():
            try:
                client, _ = self.listener.accept()
            except socket.timeout:
                continue
            except OSError:
                return
            try:
                server = socket.create_connection(("127.0.0.1", self.target), timeout=2)
            except OSError:
                client.close()
                continue
            server.settimeout(None)
            with self.lock:
                self.connections.append((client, server))
            threading.Thread(
                target=self.forward, args=(client, server), daemon=True
            ).start()

    def forward(self, a, b):
        try:
            with selectors.DefaultSelector() as sel:
                sel.register(a, selectors.EVENT_READ, b)
                sel.register(b, selectors.EVENT_READ, a)
                while not self.stopping.is_set():
                    if time.monotonic() < self.hold_until:
                        self.stopping.wait(0.01)
                        continue
                    for key, _ in sel.select(0.2):
                        data = key.fileobj.recv(4096)
                        if not data:
                            return
                        key.data.sendall(data)
        except OSError:
            pass
        finally:
            with self.lock:
                if (a, b) in self.connections:
                    self.connections.remove((a, b))
            a.close()
            b.close()

    def cut(self):
        with self.lock:
            for pair in self.connections:
                for s in pair:
                    try:
                        s.shutdown(socket.SHUT_RDWR)
                    except OSError:
                        pass

    def close(self):
        self.stopping.set()
        self.cut()
        self.listener.close()
        self.thread.join(timeout=2)


def render(path):
    import struct, sys

    sys.path.insert(0, str(ROOT / "tools"))
    import vgapal
    import zlib

    data = path.read_bytes()
    pal = struct.unpack(">32H", data[:64])
    title, arena = data[64:66]
    colours = [tuple(((v >> s) & 15) * 17 for s in (8, 4, 0)) for v in pal]
    pix = []
    for y in range(200):
        row = []
        for x in range(320):
            pen = sum(
                ((data[66 + p * 8000 + y * 40 + x // 8] >> (7 - x % 8)) & 1) << p
                for p in range(5)
            )
            rgb = colours[pen]
            if pen == 0:
                c = (vgapal.TITLE_BACK if title else vgapal.MENU_BACK)[vgapal.BAND[y]]
                rgb = tuple((((v * 63 + 127) // 255) >> 2) * 17 for v in c)
            row.append(rgb)
        pix.append(row)

    def chunk(tag, body):
        return (
            struct.pack(">I", len(body))
            + tag
            + body
            + struct.pack(">I", zlib.crc32(tag + body))
        )

    raw = b"".join(b"\0" + bytes(v for pixel in row for v in pixel) for row in pix)
    path.with_suffix(".png").write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 320, 200, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw))
        + chunk(b"IEND", b"")
    )


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("Session stopped.")

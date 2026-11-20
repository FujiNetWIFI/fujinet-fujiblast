# Amiga port scope and cross-platform validation

## Conditional implementation

Amiga-specific graphics, input, audio, network-worker and timing code lives in
`src/amiga/`. Portable NIO settings and endpoint helpers live in `src/nio/`.
Amiga enables `FB_NIO`; other clients retain their AppKey storage and buffer
limits. `FB_NIO` can support another NIO target with its own UI and transport.

Changes in shared files are guarded by `__AMIGA__` or `FB_NIO`:

- `appkey.c` and `game.h`: native AppStore, enlarged NIO endpoint buffers,
  validation and save-result handling. NIO parsing validates before changing
  active state; invalid compiled defaults fall back to `fujinet.online:9910`.
- `main.c` and `menus.c`: startup settings, F1 setup, and navigation between the
  title, setup and room list.
- `play.c`: separate HUD fields, elapsed-time movement, stable diagonal-turn
  selection and optional timing counters. The shared walker is unchanged.
- `screens.c`: Escape behavior, clipping long text and current sound state.
- `net.c`: worker diagnostic notification on idle timeout.
- `test.c` and platform headers: Amiga fixtures, declarations and constants.

Unconditional changes are braces around branches containing guarded hooks,
documentation, and the separate `amiga`/`test-amiga` Make targets. The game
server, wire protocol, master artwork, shared tune definitions, existing music
generators and other platforms' source directories are unchanged.

## AppKey finding for upstream consideration

The non-NIO AppKey serializer has a buffer-overflow hazard: a 60-character host,
port 65535 and room `blast1` produce an 86-byte URL, exceeding its 68-byte buffer
and the DEFAULT AppKey's 64-byte limit. A native sanitizer harness reproduces
the overflow in `add()`. This port does not change that behavior on OG targets.

NIO bounds checks, strict port validation and write-result handling stay inside
`FB_NIO`. Applying those policies to OG targets requires a separate decision
about accepted saved URLs and verification of each target's ROM, bank and RAM
budgets. Required dependencies are unavailable for several targets here; the
Amiga port does not claim cross-platform hardening. Thom/upstream validation is
needed for those targets.

## Build comparison

Against base `53f41889`, use matching compiler settings and
`SERVER=fujinet.online:9910 GIT_VERSION=review`. The following program binaries
are byte-for-byte identical, with zero code/data/BSS growth:

| Target | Result | Program size |
| --- | --- | --- |
| Atari 400/800 | Compile, link and ATR packaging pass | COM 28,753 bytes |
| Apple II | Compile/link pass; disk packaging needs AppleCommander `ac` | Program 45,956 bytes; graphics/overlay 40,140 bytes |
| DOS CGA | Compile, link and disk packaging pass | EXE 45,042 bytes |
| DOS VGA | Compile, link and disk packaging pass | EXE 54,174 bytes |
| CoCo 1/2 | Compile/link and region checks pass; disk packaging needs Toolshed `decb` | Low binary 15,825 bytes; high binary identical |
| CoCo 3 | Compile/link and region checks pass; disk packaging needs `decb` | Low binary 15,920 bytes; high and art binaries identical |

CoCo uses CMOC 0.1.101 and lwtools 4.25. To check CoCo 1/2 linking without the
CoCo 3 disk-packaging prerequisite:

```sh
make gen-coco SERVER=fujinet.online:9910 GIT_VERSION=review
make -f mekkogx/platforms/coco.mk r2r/coco/fujiblst.bin \
    SERVER=fujinet.online:9910 GIT_VERSION=review
```

The following targets lack dependencies on both the base and port branches:

| Target | Missing dependency / unverified check |
| --- | --- |
| Adam | `~/Workspace/fujinet-lib-experimental`, pin `a29e57f` |
| MSX ROM | Sibling `../fujinet-lib`, pin `a2c56e4` |
| Atari 7800 | `~/Workspace/fnlib-7800/bus/atari7800`; ROM/bank budget unverified |
| Atari 5200 | `~/Workspace/fnlib-5200/bus/atari5200`; 32 KB ROM budget unverified |
| NES | `~/Workspace/fujinet-lib-experimental/bus/nes`; ROM/bank budget unverified |
| SMS | `~/Workspace/fnlib-sms/common/*.c` |
| H89 CP/M / HDOS | `~/Workspace/fujinet-lib/common/*.c`; TPA budget unverified |

The unchanged assembly/IntyBASIC ports do not compile the shared C modules and
are not part of this build comparison. Binary equality does not constitute
runtime acceptance on the non-Amiga targets.

## Amiga validation

The production 68000 build uses the released NIO 0.2.0 archive. Fourteen host
test groups cover transport, queues, input, AppStore, setup, audio and assets,
including a portable `FB_NIO` build without Amiga defines. Amiberry live checks
cover name entry with empty history, large settings, chunked history, join,
movement, bombs, TCP disconnection/rejoin, results and exit. Audio tests exercise
all jingles/effects, the round loop, mute and independent interrupt timing.

The optional responsive test's 100 ms input-gap bound is not consistently met;
see [performance testing](amiga-performance.md). Emulator acceptance does not
establish physical stock-A500 performance or serial reliability.

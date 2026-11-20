# Amiga performance testing

The foreground handles input, movement and rendering while a separate process
owns network I/O. Cached planar tiles and masked blitter objects update changed
regions through a 2.5 KB chip scratch bitmap. The foreground runs at priority 1
and waits for vertical blank only when it has not already missed that frame.
Movement targets 60 logical updates per second on both PAL and NTSC.

## Profiles and instrumentation

From the NIO workspace root:

```sh
external/fujinet-fujiblast/tools/amiga/run.sh --machine a500 --timing
external/fujinet-fujiblast/tools/amiga/run.sh --machine a500-030 --timing
```

The A500 profile uses a cycle-exact 68000, OCS, 512 KB chip and 512 KB slow RAM.
The accelerated profile uses a maximum-speed 68030, OCS, 2 MB chip and 2 MB fast
RAM. It is not calibrated to a particular accelerator clock rate.

`--timing` builds an instrumented executable and writes `share/timing.log` in
the printed evidence directory. `fps` counts foreground loops; `ups` counts
applied movement ticks. Neither measures display refresh. `dropped` counts ticks
beyond the six-step catch-up cap. `max_loop_ms` records the longest loop gap.
Render/read/write totals and counts identify expensive operations; worker I/O
and foreground rendering overlap, so their durations must not be summed as
foreground blocking time. Resolution is one vertical blank.

## Focused acceptance

From the game repository after sourcing the workspace environment:

```sh
python3 tools/amiga/accept.py --responsive
python3 tools/amiga/accept.py --responsive --standard ntsc
python3 tools/amiga/accept.py --scene 5 --timing
```

The responsive test injects a held arrow and Space through Amiberry IPC while
pausing NIO forwarding for one second. Over two guest seconds it checks at least
40 foreground updates, 110 movement steps, no more than six discarded steps,
a maximum 100 ms foreground/input gap, movement and local bomb prediction.
These are test thresholds, not guaranteed runtime performance. The 100 ms gap
threshold is not consistently satisfied; input/render latency remains a
performance limitation even when movement and network recovery succeed.

Scene 5 exercises four players and explosions and compares every display pixel
against the background and sprite artwork. Initial cache creation, screen
changes, screenshot dumps and reconnect transitions are not steady-state
performance samples. Busy scenes can be limited by rendering.

Emulator configuration includes immediate blits and does not reproduce all
physical blitter, serial, cable or baud timing. Physical A500 frame rate and
subjective responsiveness need hardware validation. Logs and captures remain in
`build/amiga/evidence/`; see [playing and diagnostics](amiga-playing.md) for the
full timing-field definitions.

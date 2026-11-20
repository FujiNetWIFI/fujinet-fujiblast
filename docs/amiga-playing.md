# Play in the NIO workspace

From the workspace root:

```sh
external/fujinet-fujiblast/tools/amiga/play.sh
```

This sources the workspace environment, builds the production game against
released NIO library 0.2.0, and starts a local FujiBlast server with bots,
POSIX FujiNet NIO, a serial bridge, and visible Amiberry. It boots a private
copy of your licensed WB1.3 floppy directly into the game. Allow time for the
floppy boot. Your persistent Workbench profiles and disks are untouched.

Prerequisites are those in [the Amiga build guide](amiga.md), the workspace's
`local/amiga.env`, built WB1.3 NIO artifacts, POSIX `fujibus-tcp-debug` binary,
and `amiberry`, `socat`, and `xdftool`. Build the game server once with Go:

```sh
cd external/fujinet-fujiblast/server
go build -o fujiblastd ./cmd/fujiblastd
```

Click the emulator window. F1 on the title opens setup (see below).
Press Space/Return, enter a name and Return if unset, select
a room and press fire. `blast1`, `blast2`, and `blast3` have one, two, and three
bots respectively. As host, press fire again to start the match. Use arrows
to move and Space to drop bombs. M toggles sound; Escape leaves the room or
quits menus. After exiting the game, `Execute FB:run` restarts it.

Close Amiberry or press Ctrl-C in the launch terminal to stop all four
processes. The launcher watches the services and reports an unexpected exit.
Each invocation gets fresh settings and private ports. Logs and copied media
stay in the printed `build/amiga/evidence/<timestamp>-pal` directory; the
owned emulator IPC socket is recorded in `amiberry.sock.path` there.
Pass `--standard ntsc` to try NTSC.

## Existing Workbench profiles

For your usual desktop session, from the workspace root:

```sh
source scripts/env.sh
scripts/build.sh amiga-workbench --profile wb13-a500
# Or:
scripts/build.sh amiga-workbench --profile wb32-a1200
```

The runner starts POSIX NIO itself. When you already have a NIO process on
port 65504, add `-- --external-nio`. These commands boot Workbench, not the
game. The dedicated `play.sh` command above is the quickest game test.
See the workspace's `docs/amiga/amiberry-testing.md` for manual shares,
broker loading, and profile configuration.

## Machine profiles and game timings

`run.sh` is an alias for `play.sh`; both accept the same options:

```sh
# Cycle-exact 68000, 512 KB chip + 512 KB slow RAM (default)
external/fujinet-fujiblast/tools/amiga/run.sh --machine a500 --timing
# Accelerated 68030, 2 MB chip + 2 MB fast RAM, OCS, same WB1.3 ROM
external/fujinet-fujiblast/tools/amiga/run.sh --machine a500-030 --timing
```

The accelerated profile uses maximum emulated CPU speed, not a calibrated
25/50 MHz accelerator. Neither profile edits your Workbench configuration.
`--standard ntsc` remains available with either profile.

`--timing` builds a separate instrumented executable and prints the path to
`share/timing.log`. Run `tail -f <that-path>` in another terminal. Every five
guest seconds it appends:

- `fps`: game-loop passes per second, **not** emulator display refresh rate.
- `ups`: applied local-player 60 Hz movement steps per second. Zero is normal
  in menus, countdowns, or after death; steps can also occur while standing still.
- `dropped`: movement ticks discarded by the existing six-step catch-up cap.
- `max_loop_ms`: longest interval between game-loop passes in that window.
- `render_ms`, `read_ms`, `write_ms`: elapsed durations of calls completed in
  the window, with corresponding call counts. Divide total by calls for the
  mean. Network durations belong to the worker and overlap foreground
  rendering; **do not add them together as foreground blocking time**.

The cheap clock uses vertical blanks: 20 ms resolution on PAL, roughly 16.7 ms
on NTSC. Aggregate times locate large stalls; a zero does not mean no CPU cost.
Logging incurs one file open/write/close per window. These are guest timings,
not host wall-clock measurements. Windows containing initial screen drawing,
room changes, screenshot dumps, or reconnect tests are not steady-state FPS.
Amiberry's `GET_FPS` reports emulation refresh and can still say 50 while the
game updates only a few times per second.

## Asynchronous networking and blitter rendering

The Amiga game has two independently scheduled processes:

- The foreground process owns keyboard/joystick input, the game state, local
  movement, sound, and display. It wakes at priority 1 and waits for vertical
  blank only when it has not already missed that frame.
- The network worker owns **all** calls into released `fujinet-nio-lib 0.2.0`,
  including AppStore and initialization/shutdown. Its broker reply port is
  created and destroyed by that same process. Network waits do not block
  foreground input or movement.

The worker receives ordered, complete COBS frames in a bounded FIFO. Consecutive
unsent movement frames can be replaced with the latest position, but an action
or credit is a barrier: updates never cross it. Multiple frames are batched in
one NIO write. Queue overflow or incomplete writes cause connection loss rather
than silently dropping an action or replaying bytes already accepted.

The receive ring is bounded and backpressured. Foreground code copies at most
128 new bytes per loop, preserving modulo-256 credits and bounding parse work.
Only the foreground applies received game state. Short `Forbid` sections protect
queue copies; they never surround NIO calls, rendering, or DOS logging.

Open, close, AppStore, and shutdown are synchronous control messages outside
active gameplay. Close drains queued leave/BYE messages; shutdown waits for the
worker to close its handles and acknowledge exit before the application unloads.
A failed/pending device call must return before these control operations finish.

Sprites are **blitter objects**, with cached planar images and transparency
masks. All normal animation frames are prepared before play. A separate planar
background supplies each changed region. A 2.5 KB chip scratch bitmap composes
that region with its intersecting sprites in seat order, then copies the finished
region to the visible screen. This avoids blank intervals during restoration
while preserving overlap and erasure. Tiles and moving bombs use a cached
planar tile atlas during play. Background text and initial screen construction
still use the indexed drawing path. Screen transitions and initial cache setup
are outside the steady-state frame-rate claim.

This retains the existing 32-colour artwork and four player colour schemes;
it does not allocate the eight hardware sprite channels. The OS owns the
blitter and display. See the [Amiga graphics reference for masked blits](https://amigadev.elowar.com/read/ADCD_2.1/Includes_and_Autodocs_2._guide/node05A6.html).

### Repeat the responsiveness test

From the game directory after sourcing the workspace environment:

```sh
python3 tools/amiga/accept.py --responsive
python3 tools/amiga/accept.py --responsive --standard ntsc
python3 tools/amiga/accept.py --scene 5 --timing
```

The responsive case boots the real game, joins a local server, injects a held
arrow and Space through Amiberry's keyboard IPC, and pauses NIO bridge forwarding
for one second. Over two guest seconds it requires at least 40 foreground
updates, at least 110 movement steps, at most six discarded steps, a foreground
and input-poll gap no larger than 100 ms, actual movement, and a locally predicted
bomb. This measures responsiveness while network I/O is stalled, not merely a
successful exchange. `--responsive` enables timing automatically.

The busy scene animates four players and explosions. On completion it independently
checks every screen pixel against the planar background plus the original sprite
artwork, catching masking, overlap, clipping, and restoration errors.

See [performance testing](amiga-performance.md) for interpretation and limits.

## Live room list

“Pick a room” refreshes player counts and WAITING/PLAYING status automatically,
five seconds after each completed list. The displayed list remains usable while
the next response arrives; arrow/joystick selection, joining, mute and Escape
stay active. A complete response replaces the displayed rows together.

Selection follows the room ID if the server reorders rooms. If the selected room
is removed, selection falls back to an available row. An empty list disables
joining and continues refreshing. A disconnected stream, server error or refresh
that takes ten seconds returns to the title after an explanatory message.

## Arena scoreboard

The Amiga HUD uses forty normal-width glyphs across the top: four separately
coloured player fields and a central clock. Each player gets up to six name
characters, a blank, and the round-win count, with a further gap between fields.
This preserves bot-number suffixes such as BOT1/BOT2/BOT3. Eliminated players
show dashes in their field during play.

The game bitmap remains 320×200; the dark area below it in a PAL emulator window
is outside that bitmap. This layout adds no screen memory or arena redraw area.
The four-player scene (`python3 tools/amiga/accept.py --scene 5`) exercises three
bot names and PLAYER with distinct scores, animation, and the pixel oracle.

## Name and server setup

Press **F1** on the title screen. Up/Down or Tab moves between the player name,
server type, saved custom servers, Add server and Save setup. Return or Fire
chooses the highlighted item. Left/Right on the server-type row switches
**Lobby** / **Custom**. The star marks the selected custom server.

Custom keeps up to five entries. Add server opens a text editor; Backspace
(or Delete) on a saved entry removes it. A full list asks you to delete an entry
before adding another. Long addresses use two lines with an ellipsis when needed;
the saved value is complete. With no saved history, the current custom endpoint
populates the list.
Case variants and equivalent default-port spellings are treated as duplicates.

Labels are blue, with aligned name and server-mode values. Gold brackets mark
the selected mode; gold text and a star mark the selected custom endpoint.
The gold arrow marks keyboard/joystick focus independently of the selected choice.
Add and Save actions brighten when focused; feedback messages use red.
Choose Save setup to persist the edited list, player name and selected server.
Escape cancels the staged edits; inside a text editor it cancels just that edit.
Return accepts edited text, and Backspace erases. Names are 2–8 letters/digits.
Enter a DNS name or IPv4 address, optionally with `:port`; omitted ports default
to 9910. `tcp://host:port/` is also accepted. Use an address reachable from the
FujiNet NIO host. IPv6 and room query strings are not accepted by this editor.
Failed storage operations leave setup open for retry; invalid/unreadable history
is preserved rather than silently replaced. Saves span separate history/name/
server values: a failed save may have written an earlier value; retry completes
it, while the active in-memory endpoint changes only after its setup save succeeds.

Lobby selects the bundled `fujinet.online:9910` preset. Fire opens the game's
room browser on that server. The preset is separate from local `SERVER`
overrides used by emulator tests and can be set with
`make amiga LOBBY_SERVER=host:port`. It does not perform HTTP lobby discovery.
Saved player settings take precedence over the compiled default.

NIO settings use the released 0.2.0 native AppStore API, namespace `fujiblast`:
`player-name` holds the player name, `server` holds the selected URL including
an optional room, and `servers` holds the five-entry custom-server history.
These settings do not use the legacy AppKey API.
Save clears the selected room; leaving a room retains the server for next time.
The player name still has the game's eight-character protocol limit.

History is a 1,240-byte value: `FBS2`, one count byte, then five zero-padded
247-byte address slots. The decoder also accepts the 295-byte `FBS1` native
format with five 58-byte slots. Saving writes `FBS2`. Reads validate version,
count, termination, endpoints
and duplicates. An empty saved list remains empty on subsequent loads.
Hostnames allow up to 240 characters, matching the TCP helper in released NIO
0.2.0. This is a network-library limit, not an AppStore limit. Server/room URLs
use a 288-byte application buffer; values above 64 and 255 bytes are supported.
Long list entries are abbreviated on screen without truncating their values.

The portable implementation is in `src/nio/settings.c` and `src/nio/servers.c`.
Storage uses 256-byte transfers and 320 bytes of scratch, with checked offsets,
lengths and EOF. Writes at offset zero replace the previous value. A future
BBC NIO target can enable `FB_NIO` and compile these modules; it will need its
own UI and transport integration. On Amiga the existing worker owns all library
calls. OG firmware targets retain their original AppKey behavior.

Escape in the room list returns to the title. After a failed connection,
dismiss the message with Fire/Return to return to the title and use F1.
The compile-time server remains the fallback when no saved setting exists.

The executable for FTP is `r2r/amiga/fujiblast-nio`; no separate config file is
needed. The menu keeps its large buffers in static storage. Unreadable storage
shows “Cannot load saved servers”; malformed data shows “Invalid saved server
list”. Return retries and Escape cancels without replacing the stored history.

## Joystick cornering

The Amiga direction selector retains a diagonal's chosen turn until the stick
changes. If that turn is blocked, the other held direction advances toward the
opening. Releasing an axis selects the remaining cardinal direction.
The shared walker uses a six-unit alignment window; pure cardinal inputs outside
that window require moving closer to the passage. Tests cover four orientations,
all speeds, update batches, walls, bombs and alignment boundaries.

## Connection diagnostics and recovery

The normal executable does not log by default. To investigate a connection
problem, enable logging in an Amiga Shell before launching the game:

```text
SetEnv FUJIBLAST_NETLOG RAM:fujiblast-net.log
```

After a drop, quit the game and inspect the log:

```text
Type RAM:fujiblast-net.log
```

Copy this file to a local disk before rebooting or starting another game process.
It is flushed after each event, starts fresh for each process, and wraps at about
60 KB. Routine traffic produces one summary per ten seconds; errors, successful
recovery, open/close and EOF produce additional lines. Logging runs in the network
worker without holding the queue locks. RAM: avoids sending diagnostic writes
through the connection under investigation. Optional Shell environment settings:

```text
SetEnv FUJIBLAST_NETLOG RAM:fujiblast-net.log
SetEnv FUJIBLAST_NETLOG OFF
```

Set one of these before launching; OFF disables the log. With logging disabled,
there are no log file writes or diagnostic DateStamp calls. A custom path must be
writable and shorter than 128 characters. Prefer RAM: or a local disk over a
FujiNet-mounted disk. Guest acceptance builds instead default to `FB:network.log`
so the host keeps the evidence.

Read `event` first:

| Event | Meaning |
| --- | --- |
| `read-error` / `read-recovered` | Failed NIO read attempt / successful retry on the same connection |
| `read-fatal` | Non-retryable read failure or three failed attempts |
| `write-error` / `write-recovered` | Failed write / recovery from provably zero-progress backpressure |
| `tcp-eof` | NIO reported the remote stream has ended |
| `tx-queue-full` | The bounded outgoing queue filled |
| `frame-overflow` | An outgoing frame exceeded its encoding buffer |
| `game-idle-timeout` | No complete incoming frame for 900 foreground polls |
| `open`, `close`, `summary` | Connection lifecycle and cumulative counters |

`rc` is the NIO result (0 success, 3 busy, 4 not-ready, 5 I/O, 6 timeout,
16 transport failure). Settings helpers return a boolean, so
`key-*-error` uses 255 (unknown), with broker details alongside it.
`broker` is stage/result, `cause` the broker's detailed class, and `serial`/`status`
the native serial error/status. Current broker cause values include 1 backend
open, 2 serial I/O, 3 session I/O, 4 timeout, 5 write, 6 query, 7 read, 8 timer,
and 9 read failure after draining an overrun. Keep the raw fields when reporting.
`io_ms` measures elapsed time since the current operation began for I/O events
(including retry delays); it is not an independent duration for summary/close.
`ms` is a wrapping millisecond timestamp with 20 ms resolution.

`roff`/`woff` are confirmed application stream offsets; write-error offsets may
lag the library if it accepted a prefix before failing. Write diagnostics include
exchange count, transient responses, zero-byte accepts and response header/length.
`txq`/`rxq` are current queue fill; `txhigh`/`rxhigh` are lifetime high-water marks.
`badframes` counts rejected COBS/Fletcher frames. Read/write call and byte counts
are cumulative; offsets restart at each open.

Reads of 128 bytes retry timeout/transport/I/O failures at the same offset and
size, up to three attempts with 2/4 DOS-tick delays. This uses the NIO TCP service's
last-read replay cache; it never skips missing stream bytes. Busy/not-ready reads
remain normal and empty reads back off to three DOS ticks. Malformed lengths,
partial progress on an error, invalid handles and EOF are not hidden by retries.

Writes retry only busy/not-ready exhaustion where library diagnostics prove
**every** exchange accepted zero bytes. In released library 0.2.0, `written=0`
on an error does not prove that no prefix was sent. Ambiguous writes therefore
still trigger a reconnect, avoiding duplicated frames/actions. The existing
900-poll silence threshold is unchanged: it is about 15 seconds at 60 polls/s
and longer at lower frame rates, not a single missed update.

The worker uses a dedicated control-message port. Its DOS process port is
reserved for DOS replies, including Delay and logging. The worker creates and
owns both its control port and the library's broker reply port. Native settings
load before display and audio allocation.

Host tests inject read/write failures, verify bounded retries, and reject
ambiguous write replay. Live tests exercise connection loss, rejoin and shutdown.
Use logs from the affected physical machine to diagnose hardware disconnects.

## Amiga music and sound

The Amiga plays the original four jingles (title, round, win and game winner)
and ten effects from `src/tms9918/tunes.h`. Both voices of the gameplay score
come from `music/play.txt`, compiled by `tools/mkmus_amiga.py`; the 25.6-second
score loops during a round. Bass, percussion, robot blips, arpeggios, warbles
and pitch sweeps are preserved. No external music files are required.

Four Paula channels play two music tones, one effect tone and noise. Noise
effects temporarily take priority over music percussion; tonal effects leave
both musical voices playing. A jingle replaces the round loop at the existing
game transitions. M toggles the displayed ON/OFF state: muted music advances
silently and resumes at its current position when enabled again.

The sequencer runs from the level-3 vertical-blank handler with 60 Hz musical
timing on both PAL and NTSC. Rendering, serial waits and setup do not drive its
clock. Only changing periods/volumes are sent through synchronous `IOF_QUICK`
`ADCMD_PERVOL` calls, which the [audio.device autodoc](https://bastya.net/AmigaDevDocs/audio.html)
permits below interrupt level 5. Four continuous DMA requests use an eight-byte
square wave and 2 KB noise sample in chip RAM. Allocation and abort/reap happen
in task context; the interrupt never allocates, waits, writes logs or reads files.
If four channels cannot be allocated, the game reports audio unavailable and
continues silently. Exit stops DMA and releases all requests and chip memory.

Focused verification commands from the game directory:

```sh
make test-amiga
python3 tools/amiga/accept.py --audio
make amiga SERVER=fujinet.online:9910
```

The audio guest check runs every jingle, a complete loop including its wrap,
every effect over music, mute/resume, stop, and close/reopen. It deliberately
sleeps the foreground throughout playback and checks vblank progress and device
results. Host tests additionally cover PAL/NTSC cadence, musical durations,
concurrent voices, repeated loop-on calls, invalid IDs and loop termination.

# FujiBlast

FujiBlast is a bomb-maze game for two to four players over FujiNet. It runs on:

- the Atari 400/800/XL/XE with 48K;
- the Apple II in double hi-res (a 128K //e, //c or IIgs);
- MS-DOS on an IBM PC, XT or PCjr (or compatible) with CGA and 128K, down to a 4.77 MHz 8088;
- the Intellivision, with the FujiNet cartridge;
- the Coleco Adam (TMS9918A).

All are played with a joystick (the Intellivision's hand controller); the Apple, the PC and the Adam take the keyboard too.

## How to play

- Drop bombs and blast blocks.
- Pick up powerups:
  - Fire: a longer blast.
  - Bomb Up: one more bomb at a time.
  - Speed.
  - Kick: walk into a bomb and it slides away.
  - Glove: press fire again while standing on your own bomb to throw it.
- Once in a while a bomb is a dud. Touching it might set it off again.
- The last player standing wins the round and a trophy. After the rounds the host picked, whoever has the most trophies wins. A tie goes to sudden death.

All the art and music are original. `gfx/*.png` is the master sheet that every port starts from; `tools/drawart.py` draws it.

## Layout

| Path | What |
|---|---|
| `server/` | `fujiblastd`, the game server (Go). `blastbot` runs bots for tests and rigs. |
| `src/*.c` | The client, platform neutral. |
| `src/platform-specific/*.h` | The API each platform implements. |
| `src/atari/` | The Atari implementation: cc65, ANTIC mode 4, players and missiles, netstream. |
| `src/apple2/` | The Apple II implementation: cc65, double hi-res on both pages, a blitter in the language card, SmartPort, the Mockingboard. |
| `src/msdos/` | The MS-DOS implementation: Open Watcom, CGA's 320x200 4-colour mode, FUJINET.SYS's serial port driven by interrupt, the PC speaker. |
| `src/intv/` | The Intellivision client: the C client again in IntyBASIC, its hot loops in CP1610 assembly; the cartridge's mailbox. |
| `src/adam/` | The Coleco Adam implementation: z88dk and zsdcc, Graphics II as a character map, two sprites a bomber, the SN76489, the FujiNet's DCB driven by hand. |
| `docs/PROTOCOL.md` | The wire protocol and the state layout. |
| `tools/` | The art converter and the emulator rigs. |

## Building

```
defoogi make atari      # r2r/atari/fujiblst.atr
make server             # server/fujiblastd, server/blastbot
make proto              # src/proto.h from the server's constants
```

`make SERVER=host:port` sets the server the game uses when the lobby gave none.

```
make apple2             # r2r/apple2/fujiblst.po
```

```
defoogi make msdos      # r2r/msdos/fujiblst.img
```

```
make intv               # r2r/intv/fujiblst.rom
```

```
make adam               # r2r/adam/fujiblst.ddp
```

`make intv` builds IntyBASIC 1.5 from `~/Workspace/IntyBASIC` (`IB_SRC`) and needs `as1600` on the PATH. The art is `gfx/intv/*.png` (drawn by `tools/drawart_intv.py`, converted by `tools/mkgfx_intv.py`); the sounds are the Mockingboard's tables (`tools/mksnd_intv.py`).

`make adam` builds fujinet-lib (the pinned fujinet-lib-experimental in `~/Workspace`, `FNLIB_EXP`) with zsdcc into `build/fnlib-adam` and links the program, which must end below C800h (`tools/adam_end.py` checks); z88dk comes from the defoogi container unless installed. The art is `gfx/tms/*.png` (drawn by `tools/drawart_tms.py`, converted by `tools/mkgfx_tms.py`) and `gfx/namco.bin`. `make adam TEST_SCENE=1` builds an offline scene in (see Testing; `touch src/test.c` to go back).

The MS-DOS disk holds `FUJIBLST.EXE` and an `AUTOEXEC.BAT` that runs it. The art is `gfx/cga/*.png` (drawn by `tools/drawart_cga.py`, converted by `tools/mkgfx_cga.py`) and `gfx/namco.bin`, the Namco arcade font.

The Apple II disk holds three files:
- `FUJIBLST.SYSTEM`, our own loader (`tools/a2loader`)
- `FUJIBLST`, the program
- `FUJIBLST.GFX`, which goes to aux RAM: the art (`gfx/dhgr/*.png`, drawn by `tools/drawart_dhgr.py`, converted by `tools/mkgfx_apple2.py`), the Mockingboard's sounds and the menus' overlay

Main RAM is full: `src/apple2/fujiblst.cfg` says what goes where.

## Running a server

```
server/fujiblastd -stream :9910
server/blastbot -addr localhost:9910 -table dev1 -n 3 -start 4
```

`fujiblastd` serves tables `blast1`–`blast4` and the hidden `dev1`–`dev3`. It lists itself in the FujiNet lobby only when `FUJIBLAST_LOBBY` is set (lobby key 0x10).

## The Apple II

- **Machine:** a 128K //e (Rev B board or later), //c or IIgs.
- **Keys:** the arrows or I/J/K/L walk. Space, Open-Apple or Solid-Apple is fire. ESC leaves the table; M turns the sound off and on.
- **Joystick:** read if one is centred at start-up.
- **Sound:** a Mockingboard, if one is found in any slot, plays the Atari's effects and jingles. Without one, the speaker gives short blips.
- **Speed:**
  - The game logic runs 60 times a second, as on the Atari.
  - With four bombers moving, a 1 MHz Apple redraws about 8 to 10 times a second.
  - On a //e the blank is polled, so the game resets its clock from the server's round timer.

## MS-DOS

- **Machine:** an IBM PC, XT or PCjr, or a compatible, with CGA and 128K, and FUJINET.SYS loaded. EGA, VGA, the PCjr and the Tandy 1000 show the same CGA screen.
- **Colours:** set through the BIOS.
  - On a CGA: mode 5 (light cyan, light red and white), and a background per screen with INT 10h AH=0Bh: green in the arena, blue on the title, black elsewhere.
  - Elsewhere: mode 4, with the same four colours in the palette registers (INT 10h AH=10h).
- **Keys:** the arrows or the keypad walk; space is fire; ESC leaves the table (on the title it goes back to the lobby); M turns the sound off and on.
- **Joystick:** read if one is centred at start-up. Its axes are timed only while no reply is coming from the FujiNet.
- **Sound:** the PC speaker plays the Atari's effects and jingles, one at a time; an effect takes the speaker from a jingle.
- **Network:** FujiBus packets on FUJINET.SYS's own serial port, driven by the UART's interrupt (no call is waited on). If no port answers, the calls go through FUJINET.SYS.
- **Speed:**
  - The game logic runs 60 times a second.
  - In live play against three bots, a 4.77 MHz 8088 redraws about 30 times a second (86Box's IBM 5150).
  - Only bombs and duds animate (each animated tile costs an 8088 a redraw).

## The Intellivision

- **Machine:** an Intellivision (NTSC or PAL) with the FujiNet cartridge, which carries the ROM's RAM and the network.
- **Controls:** either hand controller. The disc walks; any side button is fire. CLEAR leaves the table; ENTER is RETURN; 0 turns the sound off and on.
- **Name:** without one from the lobby, picked a letter at a time: the disc's up and down choose, right or fire takes it, left or CLEAR takes one back, ENTER ends it. Keypad digits are taken at once.
- **Screen:** foreground/background mode, a card a tile. The tiles animate by redefining their cards; each bomber is two MOBs (its body and its visor).
- **Sound:** the PSG plays the Atari's jingles on one voice and its effects on another, with the noise generator for the blasts.
- **Speed:**
  - The game logic runs 60 times a second (a jiffy owed is made up next frame).
  - In live play against three bots, jzIntv redraws about 50 times a second.
  - The link's decoding, the walking, the bombers and the controller are CP1610 assembly (`src/intv/*.asm`); IntyBASIC alone took over two frames a pass.

## The Coleco Adam

- **Machine:** an Adam (64K) with the FujiNet, booted from the `.ddp` tape image the FujiNet mounts as a disk.
- **Controls:** controller 1 (stick and either fire button) or the keyboard: the arrows walk, SPACE is fire, RETURN, ESCAPE (the lobby) and BACKSPACE as they say, M turns the sound off and on.
- **Screen:** the TMS9918A in Graphics II used as a character map (32 x 24 cells, two colours in any 8 pixels). A tile is 2 x 2 cells, and the animated tiles' cells are redrawn in the frame's blank. Text is the Namco font, a cell a character, made in the colour wanted when first drawn. Each bomber is two 16 x 16 sprites: the seat's colour behind, a black visor and boots in front. When three bombers share lines they take turns at the front (the VDP shows four sprites a line).
- **Sound:** the SN76489, stepped by the VDP's interrupt: jingles on one tone channel, effects on another and the noise channel.
- **Network:** an N:TCP socket, its DCB driven by hand and never waited on (a status, then a read only when bytes wait, a write of what was queued), one call a frame at most. The frame layer (COBS, Fletcher sum) is the Apple II's.
- **Speed:**
  - The game logic runs 60 times a second (a pass behind is made up by one that doesn't wait or draw).
  - In live play against three bots, adamcore redraws about 34 times a second.

## Testing

```
cd server && go test ./...
make -C tools/atarisim
python3 tools/atarisim/atarisim.py r2r/atari/fujiblst.atr build/fujiblst-atari.map SCENARIO OUTDIR
tools/atarisim/rig.sh SCRIPT OUTDIR
```

- `go test` covers the rules and the protocol.
- `atarisim.py` runs the offline test scenes from `src/test.c` in libatari800.
- `rig.sh` plays live in a private network namespace: server, bots, fujinet-pc and the emulator over NetSIO.

For the Apple II:

```
python3 tools/a2sim/a2sim.py r2r/apple2/fujiblst.po build/fujiblst-apple2.map SCENARIO OUTDIR
tools/a2rig/rig.sh SCRIPT OUTDIR
```

- `a2sim.py` runs the same scenes in MAME's enhanced //e. The Mockingboard is there by default.
- `a2rig/rig.sh` plays live in a network namespace: the server, bots, and AppleWin's core with an in-process FujiNet over SmartPort, both from a built `fujinet-go-apple2-desktop`.
  - Its scripts can take screenshots, record sound and profile the 6502.
  - AppleWin answers SmartPort calls in no emulated time, so a real Apple spends a little more time on the bus than the rig shows.

For MS-DOS:

```
python3 tools/dossim/dossim.py r2r/msdos/fujiblst.exe build/fujiblst-msdos.map SCENARIO OUTDIR
tools/dosrig/rig.sh SCENARIO OUTDIR
defoogi make msdos STATS=1 && cp r2r/msdos/fujiblst.exe build/fujiblst-stats.exe && cp build/fujiblst-msdos.map build/fujiblst-stats.map
tools/dosrig/86box.sh SECONDS OUTDIR
```

- `dossim.py` runs the test scenes in MAME's IBM PC with CGA (`DOSSIM_NOFN=1`: no FUJINET.SYS), or its PCjr (`DOSSIM_MACHINE=ibmpcjr`). Scenes are in `tools/dossim/scenes`.
- `dosrig/rig.sh` plays live in a network namespace: the server, bots, a copy of fujinet-pc-rs232 on BoIP, and MAME's PC with FUJINET.SYS (`BAUD`, `PORT`).
- `dosrig/86box.sh` measures speed on 86Box's IBM 5150, whose 8088 and CGA run at a real PC's pace.
  - The `STATS=1` build plays by itself.
  - Every 10 s it adds a line to `A:\STATS.TXT`: frames drawn and late, the link's counters, and the share of time spent drawing, waiting, on logic, the link and the UART.
  - It also writes a profile, `A:\PROF.BIN`, which `tools/dosrig/prof86.py` reads.

For the Intellivision:

```
python3 tools/intvsim/intvsim.py r2r/intv/fujiblst.rom tools/intvsim/scenes/arena.txt OUTDIR
tools/intvrig/rig.sh tools/intvsim/scenes/live.txt OUTDIR
make -C src/intv rig && ROM=build/intv-rig/fujiblst.rom NAME= ROOM= TABLE=blast1 tools/intvrig/rig.sh tools/intvsim/scenes/lobbyless.txt OUTDIR
```

- `intvsim.py` runs the test scenes (`test 1`-`4`) in jzIntv (`~/Workspace/jzintv-20200712-src`, the FujiNet build): pictures with the MOBs, the screen as text, the stack's use, and `prof`, the CPU cycles between two labels.
- `intvrig/rig.sh` plays live in a namespace of its own (no gamepads either): the server, bots, a copy of fujinet-pc-rs232 on BoIP, and jzIntv's cartridge dialing it. `make -C src/intv rig` builds a ROM whose default server is the rig's.
For the Adam:

```
make adam TEST_SCENE=1                  # 1 the arena, 2 the waiting room, 3 a round's end, 4 the game's end
FRAMES=900 EVERY=300 SHOTDIR=OUTDIR tools/adamrig/run-adam.sh r2r/adam/fujiblst.ddp
KEYS=keys.txt FRAMES=2400 tools/adamrig/rig.sh OUTDIR
```

- `adamrig/run-adam.sh` runs the image in adamcore (`~/Workspace/adamcore`) headless with a copy of fujinet-pc-adam, pictures every `EVERY` frames; `KEYS` types at it (lines `SECONDS TEXT`, `\n` RETURN, `\u \d \l \r` the arrows) and `EMUARGS=--pc-hist` prints where the Z80 spends its time.
- `adamrig/rig.sh` plays live in a namespace of its own: the server, bots and that, with the lobby's appkeys seeded.
- The counters `adam_passes`, `adam_late` and `adam_skip` (src/adam/plat.c) are in the RAM dump (`/tmp/fujiblast-adam.*/ram.bin`); the map says where.

- `tools/rigs/dropproxy.py` loses the first frame; `tools/rigs/cutproxy.py` hangs up in the middle of a game (`EXTRA=`, `ROOM=` through it).

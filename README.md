# FujiBlast

FujiBlast is a bomb-maze game for two to four players over FujiNet. It runs on the Atari 400/800/XL/XE with 48K, and on the Apple II in double hi-res (a 128K //e, //c or IIgs). Both are played with a joystick; the Apple takes the keyboard too.

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

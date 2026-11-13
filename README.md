# FujiBlast

FujiBlast is a bomb-maze game for two to four players over FujiNet. It runs on:

- the Atari 400/800/XL/XE with 48K;
- the Apple II in high-res (a 128K //e, //c or IIgs: the art lives in aux RAM);
- MS-DOS on an IBM PC, XT or PCjr (or compatible) with CGA and 128K, down to a 4.77 MHz 8088;
- the Intellivision, with the FujiNet cartridge;
- the Coleco Adam (TMS9918A);
- the MSX (MSX1 and up, TMS9918A), as a FujiNet cartridge's user ROM;
- the Magnavox Odyssey 2 / Philips Videopac, with the FujiNet cartridge;
- the Emerson Arcadia 2001, with the FujiNet cartridge;
- the Bally Astrocade, with the FujiNet cartridge;
- the ColecoVision, with the FujiNet cartridge;
- the Atari 7800, with the FujiNet cartridge;
- the Atari 5200, with the FujiNet cartridge;
- the Atari 2600, with the FujiNet cartridge.

All are played with a joystick (the Intellivision's hand controller, the Arcadia's disc, the Astrocade's handle, the ColecoVision's stick, the 7800's, the 5200's); the Apple, the PC, the Adam and the MSX take the keyboard too, and the Odyssey 2's keyboard types a name.

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
| `src/apple2/` | The Apple II implementation: cc65, high-res (the menus on both pages, the arena on one), a blitter in the language card, SmartPort, the Mockingboard. |
| `src/msdos/` | The MS-DOS implementation: Open Watcom, CGA's 320x200 4-colour mode, FUJINET.SYS's serial port driven by interrupt, the PC speaker. |
| `src/intv/` | The Intellivision client: the C client again in IntyBASIC, its hot loops in CP1610 assembly; the cartridge's mailbox. |
| `src/adam/` | The Coleco Adam implementation: z88dk and zsdcc, Graphics II as a character map, two sprites a bomber, the SN76489, the FujiNet's DCB driven by hand. |
| `src/tms9918/` | What the Adam and the MSX share: the screen (`gfx.c`, Graphics II as a character map, sprites), the VDP header and the tunes. |
| `src/msx/` | The MSX implementation: the Adam's game on the BIOS's interrupt for a clock, the AY-3-8910, the FujiNet cartridge's registers driven by hand (FujiBus packets). |
| `src/o2/` | The Odyssey 2 client: the C client again in 8048 assembly, the 8244's grid as the arena's walls, the font's man as the bombers; the cartridge's mailbox. |
| `src/arcadia/` | The Emerson Arcadia 2001 client: the C client again in 2650 assembly, a character a tile, the four sprites as the bombers; the cartridge's mailbox. |
| `src/astrocade/` | The Bally Astrocade client: the C client again in Z80 assembly, a 2-bit bitmap recoloured band by band by the line interrupt, the bombers composed into it, all three tones and the noise; the cartridge's mailbox. |
| `src/coleco/` | The ColecoVision client: the Adam's screen, art and sound in Z80 assembly (the console has 1K of RAM), the Astrocade's game logic and link; the cartridge's mailbox. |
| `src/atari5200/` | The Atari 5200 client: the Atari 800's screens (`src/atari`, assembled for the 5200) on a flat 32K cart, its own crt0, analog stick and keypad, the cartridge's mailbox. |
| `src/atari7800/` | The Atari 7800 client: cc65 and ca65 on MARIA, 160B cells (12 colours a pixel) for the arena, the bombers two 160A layers, 320A Namco text; the cartridge's mailbox. `DESIGN.md` is its spec. |
| `src/atari2600/` | The Atari 2600 client: 6502 assembly (Macroassembler AS), a 32K FujiNet-cartridge image of fifteen 2K banks, the arena as the playfield (13 columns of 3 bits), the bomber you steer on player 0 and the others on player 1, moved between tile rows. |
| `src/coco/` | The TRS-80 Color Computer clients' shared half: CMOC, the bank loader (all-RAM mode, the disk read by DSKCON), the IRQ clock and keyboard, the stick, the DAC sound (CoCo 1/2), the DriveWire link (`dw_call`). |
| `src/coco2/` | The CoCo 1/2 screens: PMODE 4 artifact colour, tiles and bombers blitted by 6809 assembly. |
| `src/coco3/` | The CoCo 3 screens: the GIME's 320x192 in 16 colours (the Adam's art), the timer-driven two-voice sound. |
| `src/nes/` | The Nintendo NES client: cc65 on an MMC3 image for the FujiNet NES cartridge, a name table of 8x8 cells at 2 bits a pixel with the status bar split by the scanline interrupt, two 8x16 sprites a bomber, the APU, the cartridge's mailbox. |
| `src/h89/` | The Heathkit H89 client: z88dk and zsdcc for CP/M or HDOS, the H19 terminal's characters sent by difference over its 9600-baud line, the H89-ESP32 card's 82C55, the hot paths in Z80 assembly. |
| `src/sms/` | The Sega Master System client: z88dk and zsdcc, Mode 4 (4 bits a pixel, two 16-colour palettes) in the Adam's 32x24 layout, four 8x8 sprites a bomber, the SN76489, the frame interrupt as the clock, the cartridge's mailbox. |
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
make msxrom             # r2r/msxrom/fujiblst[ASCII8].rom
```

```
make o2                 # r2r/o2/fujiblst.bin
```

```
make arcadia            # r2r/arcadia/fujiblst.bin
```

```
make astrocade          # r2r/astrocade/fujiblst.bin
```

```
make coleco             # r2r/coleco/fujiblst.col (and .rom)
```

```
make atari7800          # r2r/atari7800/fujiblst.a78
```

```
make atari5200          # r2r/atari5200/fujiblst.a52 (the lib: tools/atari5200/mkfnlib.sh ~/Workspace/fnlib-5200 build/fnlib-a52, outside defoogi)
```

```
make coco              # r2r/coco/fujiblst.dsk: the CoCo 1/2 and the CoCo 3 builds on one disk (make coco TEST=1 keeps the test scenes in)
```

```
make sms               # r2r/sms/fujiblst.sms (the lib: tools/z80/mkfnlib.sh ~/Workspace/fnlib-sms build/fnlib-sms sms, fujinet-lib-experimental's add-sms; z88dk from defoogi)
```

```
make h89-cpm           # r2r/h89-cpm/fujiblst.h8d: FUJIBLST.COM on an H17 disk (the lib: tools/z80/mkfnlib.sh ~/Workspace/fujinet-lib build/fnlib-h89 h89; z88dk from defoogi)
make h89-boot          # r2r/h89-cpm/fujiblst-boot.h8d: the same on a bootable 64K CP/M disk (made by CP/M in fujinet-go-h89-desktop's headless H89)
make h89-hdos          # r2r/h89-hdos/fujiblst.h8d: FUJIBLST.ABS for HDOS 2.0
```

```
make nes               # r2r/nes/fujiblst.nes (the lib: tools/nes/mkfnlib.sh ~/Workspace/fujinet-lib-experimental build/fnlib-nes, its add-nes branch)
```

`make intv` builds IntyBASIC 1.5 from `~/Workspace/IntyBASIC` (`IB_SRC`) and needs `as1600` on the PATH. The art is `gfx/intv/*.png` (drawn by `tools/drawart_intv.py`, converted by `tools/mkgfx_intv.py`); the sounds are the Mockingboard's tables (`tools/mksnd_intv.py`).

`make adam` builds fujinet-lib (the pinned fujinet-lib-experimental in `~/Workspace`, `FNLIB_EXP`) with zsdcc into `build/fnlib-adam` and links the program, which must end below C800h (`tools/adam_end.py` checks); z88dk comes from the defoogi container unless installed. The art is `gfx/tms/*.png` (drawn by `tools/drawart_tms.py`, converted by `tools/mkgfx_tms.py`) and `gfx/namco.bin`. `make adam TEST_SCENE=1` builds an offline scene in (see Testing; `touch src/test.c` to go back).

`make o2` runs `src/o2/build.sh`: Macroassembler AS (`~/asl/asl` and `p2bin`, or `ASL=`, `P2BIN=`) assembles a protocol 2 (APPBANK) image, a page at a time, and the checks in `tools/o2/` (from the O2 lobby and CONFIG) fail the build on a table read from the wrong page, a jump that tests the wrong instruction, a stack deeper than 8, and the like. `make o2-rig` builds `build/o2-rig/fujiblst.bin`, whose default server is the rig's.

`make arcadia` runs `src/arcadia/build.sh`: AS again, `CPU 2650`, an assembly a page (each holds the fixed halves, which must come out the same), linked into a protocol 2 (APPBANK) image by `tools/arc/mkblast.py`. The checks (`tools/arc/`, from M.U.L.E.'s Arcadia client) fail the build on a call chain deeper than 7 (the 2650's return stack is 8 and wraps silently), a register read after a call that clobbers it, and a branch that tests a constant just loaded. `tools/arc/fixreach.py src/arcadia/build.sh src/arcadia/src` widens relative branches that no longer reach. The art is `gfx/arcadia/*.png` (drawn by `tools/arc/drawart_arc.py`, converted by `tools/arc/mkgfx_arc.py`); the sounds `tools/arc/mksnd_arc.py`. `LOOK=0`/`2` builds the spike's other colours. `make arcadia-rig` builds `build/arcadia-rig/fujiblst.bin`, whose default server is the rig's.

`make astrocade` runs `src/astrocade/build.sh`: zmac 1.3 (built from `~/Workspace/zmac-1.3` with a bigger symbol table, `build/zmac/zmac`), one assembly, every page `PHASE`d to 2000H, padded into a protocol 2 (APPBANK) image and claimed. The checks (`tools/astro/`, from M.U.L.E.'s Astrocade client) fail the build on a page over 4K, the resident half over 2816 bytes, the variables running into the stack, and a jump from one page into another (`checkcalls.py`: only `FARCALL` crosses). The art is `gfx/astrocade/*.png` (drawn by `tools/astro/drawart_astro.py`, converted by `tools/astro/mkgfx_astro.py`); the sounds `tools/astro/mksnd_astro.py` (from `src/apple2/sndtab.s`, plus the jingles' new harmony and bass parts). `LOOK=1`/`2` builds the spike's other colours, `FOOTER=0` drops the gradient under the arena. `make astrocade-rig` builds `build/astrocade-rig/fujiblst.bin`, whose default server is the rig's.

`make coleco` runs `src/coleco/build.sh`: zmac 1.3 again, one assembly of a fixed 16K (the screen, the sprites, the sound, the link's mailbox, the main loop), the art in a fixed 6K, and 4K pages two to an 8K image page (page 0 the boot page, whose C000H is mapped at power-on), padded into an APPBANK image of 56K and claimed. The art is the Adam's (`gfx/tms/*.png` and `gfx/namco.bin`, `tools/coleco/mkgfx_coleco.py` turning the Adam's generated tables into zmac includes) and its tunes (`tools/coleco/mksnd_coleco.py` reads `src/adam/sound.c`). The checks (`tools/coleco/`, from M.U.L.E.'s ColecoVision client) fail the build on a region past its budget, the variables running into the stack, the mailbox pages not clear, or a label defined twice. `make coleco-rig` builds `build/coleco-rig/fujiblst.rom`, whose default server is the rig's.

The MS-DOS disk holds `FUJIBLST.EXE` and an `AUTOEXEC.BAT` that runs it. The art is `gfx/cga/*.png` (drawn by `tools/drawart_cga.py`, converted by `tools/mkgfx_cga.py`) and `gfx/namco.bin`, the Namco arcade font.

The Apple II disk holds three files:
- `FUJIBLST.SYSTEM`, our own loader (`tools/a2loader`)
- `FUJIBLST`, the program
- `FUJIBLST.GFX`, which goes to aux RAM: the art (`gfx/hgr/*.png`, made by `tools/drawart_hgr.py` from the 16-colour art in `gfx/dhgr/`, converted by `tools/mkgfx_hgr.py`), the Mockingboard's sounds and the menus' overlay

The Apple II's speed is timed on test scene 5 (four walking, blasts every second): `BOTS=0 ROOM= tools/a2rig/rig.sh tools/a2rig/bench4.txt OUTDIR`.

Main RAM is full: `src/apple2/fujiblst.cfg` says what goes where.

## Running a server

```
server/fujiblastd -stream :9910
server/blastbot -addr localhost:9910 -table dev1 -n 3 -start 4
```

`fujiblastd` serves tables `blast1`–`blast4` and the hidden `dev1`–`dev3`. It seats house bots at the public tables: `1 BOT`, `2 BOTS`, `3 BOTS` and `HUMANS ONLY` (`-bots blast1=1,blast2=2,blast3=3`, or `FUJIBLAST_BOTS`; `-bots ""` for none). A person is the host and starts the game; the bots never do. The rigs run it with `-bots ""`. It lists itself in the FujiNet lobby only when `FUJIBLAST_LOBBY` is set (lobby key 0x10).

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

## The MSX

- **Machine:** an MSX1 or later (50 or 60 Hz) with the FujiNet cartridge, which runs the ROM as a user ROM: mount `fujiblst[ASCII8].rom` in CONFIG and boot it (`b`), or the lobby does. The name ends in `[ASCII8].rom` because the firmware reads the mapper from it.
- **Controls:** joystick 1 or the keyboard: the arrows walk, SPACE (or the stick's button) is fire, RETURN, ESCAPE (the lobby: back to CONFIG) and BACKSPACE as they say, F1 turns the sound off and on.
- **Screen:** the Adam's (`src/tms9918/gfx.c`), the same art and sprites. Writes to the VDP go through the slow writer only, with the BIOS's interrupt held off for each burst: the interrupt reads the status register while the burst is going and bytes were lost (a shifted sprite table), and the interrupt's latency is too long to know the frame's blank is still ours for a flat-out write.
- **Sound:** the AY-3-8910: the Adam's tunes (`src/tms9918/tunes.h`; a divisor is the AY's period), jingles on channel A, effects on B and the noise, stepped by the BIOS's interrupt through the H.TIMI hook (`src/msx/msxint.asm`), which also keeps the 60 Hz clock on a 50 Hz machine.
- **Memory:** an ASCII8 megaROM (`src/msx/blast.asm`, `tools/msx/mkrom.py`): the program fixed at 4000h-9FFFh (473 bytes spare), the arena's cells in the window's bank 3 and the logo, trophies, sprites and font in bank 4 (`ART_BANK`, `src/tms9918/vdp.h`). The variables end at CF93h.
- **Network:** an N:TCP socket, spoken to in FujiBus packets by hand through the cartridge's registers (BFFCh-BFFFh) and never waited on: a status, then a read only when bytes wait, a write of what was queued (`src/msx/nstrans.c`, from M.U.L.E.'s). The frame layer is the Adam's.
- **Speed:** the benchmark scene (`make msxrom TEST_SCENE=5`, four walkers) redraws about 33 times a second in openMSX, as the Adam does; the animated tiles are redrawn one kind a frame.

## The Odyssey 2

- **Machine:** an Odyssey 2 or a Videopac G7000 (NTSC or PAL) with the FujiNet cartridge (firmware speaking protocol 2, APPBANK), which carries the program in pages of 2K, its RAM and the network.
- **Controls:** the left joystick walks; its button is fire. On the keyboard CLEAR leaves the table and M turns the sound off and on; a name is typed in (letters and digits, CLEAR rubs out, ENTER).
- **Screen:**
  - The arena's walls are the 8244's grid in fill mode: a band at every tile row writes the grid's ten columns afresh, so the grid shows our 11 rows of 16 x 16 tiles, each with a seam of 4 lines at its top. Ten of the thirteen columns fit; the view moves a column at a time to keep our bomber 3 to 6 columns in. A pillar and a block look alike; flames and burning blocks blink.
  - Each bomber is one of the four sprites, drawn as the 8244 font's man (standing and walking right and left, facing us, mirrored), in the seat's colour.
  - Bombs, duds and powerups (F, B, S, K, G) are the font's characters; the clock and the trophies are the HUD above the arena.
  - The text screens are 16 characters at the top and seven rows of twelve below, re-pointed by a band between rows.
  - The fixed half of the cartridge's image is RAM: its upper 512 bytes are the arena's display or the text screens', copied in when the screen changes.
- **Sound:** the 8244's one voice: effects, and short jingles note by note; an effect cuts a jingle.
- **Network:** the FujiNet's TCP socket, a mailbox transaction a frame (a WRITE of what is to be sent, else a READ; a STATUS now and then), never waited on. Every answer replaces the cartridge's reply, so a READ's bytes are decoded (COBS, the Fletcher sum, the patches straight into the state) before anything else is posted.
- **Speed:** the game logic runs 60 times a second (PAL: six jiffies every five frames; a slow pass is made up); in live play against three bots MAME runs about a pass a frame. The grid's bands take about a third of the 8048.

## The Emerson Arcadia 2001

- **Machine:** an Arcadia 2001 (NTSC; its PAL clones are untested: MAME runs them all at 60 Hz) with the FujiNet cartridge (firmware speaking protocol 2, APPBANK): the program in pages of 2K behind a fixed 8K, 1K of RAM (the state lives there) and the network.
- **Controls:** the left controller's disc walks (diagonals turn first, as everywhere); its side buttons are fire. CLEAR leaves the table, OPTION turns the sound off and on. A name is picked letter by letter: up and down turn the letter, fire or right takes it, left or CLEAR rubs out, ENTER (or START) ends.
- **Screen:**
  - The 2637's 13-row mode: 16 x 13 cells of 8 x 16 lines (each glyph row twice), about square on a TV. A tile is one cell, so the whole 13 x 11 arena shows, between black walls, with the HUD above (each seat's initial and wins, the clock) and our powers down the right.
  - The look is the blue field: black pillars (the ROM's solid block), red bricks, black bombs with a sparking fuse, yellow flames (bands across and down, flickering thick and thin), green letters for the powerups (F B S K G). Those four glyphs are user glyphs, rewritten in the vertical blank when they animate.
  - Each bomber is one of the four sprites, one colour each (white, green, red, yellow), 8 x 8 drawn twice as tall: walking four ways, cheering, knocked out. A sprite's picture is a user glyph too, rewritten when its frame changes. Kicked and thrown bombs are the bomb character, a tile at a time (no sprites are left).
  - The title's logo is block graphics.
- **Sound:** the one voice: effects, and the jingles an octave down (the divider is coarse up high); an effect cuts a jingle.
- **Network:** the O2's link, transcribed: the FujiNet's TCP socket, a mailbox transaction a frame, never waited on; a READ's bytes decoded (COBS, the Fletcher sum, the patches streamed straight into the state in cart RAM) a few a pass before anything else is posted; the screen picked only at a commit.
- **Speed:** the game logic runs 60 times a second (a slow pass is made up); in live play against three bots MAME runs about 0.9 passes a frame. Decoding the stream is the biggest cost (about 180 cycles a byte).

## The Bally Astrocade

- **Machine:** a Bally Astrocade (NTSC) with the FujiNet cartridge (firmware speaking protocol 2, APPBANK): the program in 4K pages behind a fixed half, and the network. Its only RAM is the 4K of screen RAM, so the picture is 84 lines and the state and every variable live past them.
- **Controls:** the first hand controller walks (diagonals turn first, as everywhere); its trigger is fire. On the keypad, C leaves the table and MR turns the sound off and on. A name is picked letter by letter: up and down turn the letter, the trigger or right takes it, left, C or CE rubs out, = ends.
- **Screen:**
  - 160 x 84 at 2 bits a pixel. A tile is 8 x 7 (about square: the pixels are taller than wide), so the whole 13 x 11 arena shows, framed in black, with each seat's bomber, name and wins on a strip above and the clock, the round and our powers down the right.
  - The look is the green field: white pillars, red bricks with white mortar, black bombs with a sparking fuse, white and red flames flickering thick and thin, and the powerups as icons on dark panels.
  - Colour: the line interrupt recolours the screen band by band (the ISS tracker's engine): the arena keeps black, green, red and white, while each row of the panel gets four colours of its own (the fire icon red and yellow, speed cyan, the glove pink...). The waiting room and the trophies give each seat's row its own strip colour, the title's logo runs from white through yellow to red, and an ember gradient fills the lines under the picture (the border's colour changed every three lines; no RAM, no drawing). A blast flashes the frame.
  - There are no sprites: the bombers are composed into the bitmap. A region (the bomber's old and new places together) is built a line at a time, each byte the tile under it with every bomber over it masked in, and written once, top to bottom, so nothing flickers and a bomber on its own bomb keeps its colours. The four seats are white, red, black and red/white stripes, from pre-shifted pictures. Kicked and thrown bombs are a bomb tile, a tile at a time.
- **Sound:** all three tones and the noise of the custom I/O chip, which no FujiNet client had used before. The jingles are in three parts: FujiBlast's tunes on one tone, a harmony and a bass (new here) on the others. The effects overlap, each on its own channels (a blast's noise and rumble never cut a footstep). Nothing modulates the master oscillator, so no voice retunes another.
- **Network:** the Arcadia's link, transcribed: the FujiNet's TCP socket, a mailbox transaction a frame, never waited on; a READ's bytes decoded (COBS, the Fletcher sum, the patches straight into the state) a few a pass before anything else is posted; the screen picked only at a commit.
- **Speed:** the game logic runs 60 times a second (a slow pass makes up to 8 jiffies). The picture is slower: in MAME, against three bots with everyone walking, about 18 redraws a second (composing the bombers is most of the work; the colour bands take about a tenth of the CPU).

## The ColecoVision

- **Machine:** a ColecoVision (NTSC) with the FujiNet cartridge (the firmware's APPBANK images: the program in 4K pages behind a fixed half, mapped by a read of the cartridge's page register) and the network. The console has 1K of RAM, so everything is assembly: 602 bytes of variables, the 247-byte state among them, and 68 bytes of stack at the deepest, measured.
- **Controls:** controller 1: the stick walks (diagonals turn first, as everywhere); either fire button is fire. On the keypad `*` leaves the table (and rubs out a letter in a name), `#` ends a name, `9` turns the sound off and on. A name is picked letter by letter as on the Astrocade: up and down turn the letter, fire or right takes it, left or `*` rubs out.
- **Screen:** the Adam's: the TMS9918A in Graphics II as a character map, 32 x 24 cells, each its own pattern and colours, the Namco font. A tile is 2 x 2 cells, the 13 x 11 arena framed in walls with the HUD above and our powers down the left. Each bomber is two sprites (the seat's colour behind, black with the eyes cut out in front); when three share lines they take turns at the front. Text is made of glyph sets by colour (a screen keeps to four colours: 59 glyphs a colour, stacked down from character 256), made the first time they are used. A bomb in the air is drawn on the cell grid, half a tile at a time.
- **Sound:** the SN76489, the Adam's tunes and effects, two voices stepped a frame at a time by the frame interrupt.
- **Network:** the Astrocade's link: the FujiNet's TCP socket, a mailbox transaction a frame, never waited on; a STATUS says how many bytes wait and a READ takes exactly those (a short read loses its bytes on older firmware); the stream's frames decoded (COBS, the Fletcher sum, the patches straight into the state) a few a pass.
- **Speed:** the game logic runs 60 times a second (a slow pass makes up to 8 jiffies). In MAME, against three bots with everyone walking, a pass takes about one and a half frames.

## The Atari 7800

- **Machine:** an Atari 7800 (NTSC or PAL; the clock is the frame's DLI, 60 jiffies a second on both) with the FujiNet cartridge: a SuperGame image with 16K of cart RAM (a "FUJI" claim of kind 5), 128K in eight banks. The program is the fixed bank plus bank 0 (menus, screens, the setup of a screen); the art is in banks 5 and 6, copied into RAM when a screen needs it; REPL (the first 8K of the window, the same in every bank) is the bombers' pixel pages, which MARIA reads whichever bank is in.
- **Controls:** the stick walks, fire (button 1 or 2) drops and throws. SELECT or RESET leaves the table, PAUSE turns the sound off and on. A name is turned out on a wheel: up and down change the letter (the last stop is `>`, done), fire or right takes it, left rubs out.
- **Screen:** MARIA, 25 zones of 8 lines, a zone a row of the Atari 800 port's screen. The arena and the menus' art are 160B cells (4 x 8 pixels, two bytes a line): each pixel picks one of 12 colours, so a tile has what the Apple II's has (the floor is the background, which costs nothing). Text is the Namco font in 320A, forty columns, the HUD's names each in its seat's colour. The bombers are two 160A objects (the seat's colour behind, black and the visor in front) read from the pages in REPL, so they move by changing a header, not by copying. A DLI where a zone's mode, character set or background differs (and the arena's four sets turn over every four jiffies).
- **Sound:** the cart's POKEY (the Atari's tunes, two voices stepped from the frame DLI).
- **Network:** the cartridge's mailbox driven by hand (a STATUS says how many bytes wait, a READ takes exactly those), under the Adam's COBS frame layer.
- **Speed:** in MAME, against three bots with everyone walking and bombs going off, about three passes of the game loop in four a frame (the logic catches up; a pass skips drawing when behind).

## The Atari 5200

- **Machine:** an Atari 5200 (NTSC) with the FujiNet cartridge: a flat 32K image (a "FUJI" claim of kind 0, the size), the cart's mailbox at `$B000-$B7FF`. Everything ANTIC reads is in the 16K of RAM (the crt0 copies the charsets there) but the BIOS font, which the text rows use.
- **Screen:** the Atari 800 port's, unchanged: `src/atari/gfx.c` and `gfxa.s` assemble for both (`hw.inc` and `atari.h` hold the addresses). The 5200's BIOS keeps no shadows in the 800's places, so the VBI ends in the crt0's (`vbi_body`) and the DLI stores CHBASE right after the WSYNC, as M.U.L.E.'s does.
- **Controls:** the analog stick walks, either fire button is fire. On the keypad START is RETURN (start, finish), `*` leaves the table (at the title, quits to CONFIG), `#` or PAUSE turns the sound off and on. A name is turned out on a wheel as on the 7800.
- **Sound:** POKEY, the Atari's tunes (two voices from the VBI; the serial clock's voices are free here).
- **Network:** the cartridge's mailbox driven by hand (`src/atari5200/nstrans.c`: `fn_regwr` and `fn_tx` make the hotspot reads), under the 7800's COBS frame layer.
- **Speed:** in MAME, against three bots, one pass of the game loop a frame.

## The Atari 2600

- **Machine:** an Atari 2600 (NTSC; `PAL=1 make atari2600` builds the PAL image: 312 lines, the 192-line picture centred) with the FujiNet cartridge, 128 bytes of RAM and no firmware change: only the blits the cartridge has (text, POKE, PATHPOKE). The image is 32K: fifteen 2K banks and the fixed half.
- **Controls:** joystick in port 1. Left, right, up, down walk; fire drops a bomb (or, with the glove, throws the one underfoot); walking into a bomb with the boots kicks it. RESET starts over (the socket is closed, the name takes the seat back). In the waiting room the host sets the rounds with left and right and starts with fire. With no appkeys, up/down turn the name wheel (fire adds a letter, left deletes, right is done) and a room list picks the table.
- **Screen:** the arena is the playfield, 13 columns of 3 bits (39 of the 40 playfield bits) and 11 rows of 14 lines; a row that holds a flame, bomb or power-up draws its pillars and bricks on every other entry in orange and the rest in its own colour. Your bomber is player 0 and never flickers; the other three share player 1, which is repositioned at a tile row's seam (a bomber whose window overlaps the previous one's is dropped that frame). The HUD is the cartridge's text: the timer and a trophy count a seat. The menus and the waiting room are the cartridge's text rows with a colour a row.
- **Sound:** one TIA voice, a step a frame: countdown beeps, GO, drop, boom, death, pick-up, round and game jingles.
- **Network:** the FujiNet's TCP socket. A hop a frame in the overscan (the walker, the link, the decoder), a compose unit in the vblank. The state is not kept: the map lives in the cartridge's planes (a row of 13 bytes in the free bytes after the playfield tables), the bits the screens need in zero page, everything else of the 247 bytes is dropped as it arrives. The decoder (COBS, the Fletcher sums) applies patches as they come, the map through a path buffer and one PATHPOKE a run; a bad frame asks for the whole state (`R`).
- **Zero page:** 118 bytes, about 89 persistent; the rest is a pool that the kernel's cells, player 1's job list and every hop's scratch share (`src/atari2600/fbdefs.inc` is the map and its rules).
- **Speed:** every frame is 262 lines (312 on PAL) except while the compose unit and a three-bomber seam overrun the vblank (a few percent of frames, 1-5 lines over). Walking is `src/move.c` exactly (checked against `internal/game/move.go`: 599 frames, no differences).
- **Not done:** the name and room are not written back to the appkeys; PAL's colours are NTSC's; hardware and the Go 2600 desktop are untested.

## The TRS-80 Color Computer

One disk, two programs: `AUTOEXEC.BAS` (HDB-DOS runs it) loads `FUJIBL3.BIN` on a CoCo 3 (its IRQ vector at `$FFF8` reads `$FE`) and `FUJIBLST.BIN` on a CoCo 1 or 2. CMOC 0.1.100 (`-O1`), the build from M.U.L.E.'s CoCo client: `tools/coco/lwasm-sect` puts a module's code in a section named by `FB_SECT`, `tools/coco/lwlink-fb.py` links with `src/coco/fujiblst.link` (or `fujiblst3.link`) and splits the result into what BASIC LOADMs (the low core) and what `bank.c` reads (the high core and the art). `bank_init` switches a CoCo 1/2 into all-RAM mode (the Disk ROM copied under itself, for DriveWire) and reads the rest by DSKCON, a few tries a file.

- **The CoCo 1/2** (64K, 0.9 MHz): PMODE 4 artifact colour, 128 colour pixels by 192 lines at `$0E00`, a pixel 2 bits, 4 a byte: the Atari's cell (4 clocks by 8 lines) a byte, so its art is used as it is, a tile 8 pixels (2 bytes) by 16 lines. Black, blue, orange and white (`CLEAR` swaps blue and orange, the start-up question asks which is which); the floor is black; the bombers are white, orange, blue and stripes. Text is 32 columns (the Atari's less 2 at each side), the 3-pixel font; BIG rows show 16 of their 20 characters; the HUD, whose 20 have to show, is drawn in the 5-pixel font 6 pixels apart. The arena is bytes 5-30 on lines 12-187, the powerups' counts in bytes 0-3. A bomber is composited over the tiles under it (`src/coco2/gfxa.asm`: roles through a 256-byte table a seat); where it was, the tiles are blitted again, and the bombers over them.
- **The CoCo 3** (128K, run at 1.79 MHz): the GIME's 320x192 in 16 colours, the Adam's art pixel for pixel in a palette of 16 of the 64 colours (RGB or composite, asked at start-up, `CLEAR` switches). The horizontal virtual screen puts the lines 256 bytes apart, so each 8K block of the MMU holds 32 whole lines (the screen is blocks 0-5; the arena's tiles start at line 16, so a tile row is in one block; a bomber on a block's edge is drawn in two parts) and drawing maps a block at `$C000` and puts the Disk ROM back after. The art is in blocks 6 (bombers, font) and 7 (tiles), at `$E000`. The layout is the Atari's: 40 cells of 8 pixels, the HUD two lines of text high; there is no bottom wall (the border is the wall's colour).
- **Sound:** the 6-bit DAC, the Atari's effects and jingles as steps of a tone or noise. The CoCo 1/2 has no timer, so the CPU makes the wave a slice of a jiffy at a time (2400 cycles in the arena, 9000 on the menus) and the volume falls a step a jiffy. The CoCo 3 plays them from the GIME's timer IRQ (about 4400 Hz) mixing two voices, effects and jingles, with the envelopes stepped once a jiffy; the timer's IRQ is on only while something sounds. The joystick reads change the DAC and the mux too, so they hold the sound off (`snd_gate`).
- **Controls:** the arrows (or the right stick) walk, SPACE (or the button) is fire, ENTER starts and finishes, `BREAK` leaves the table (at the title, a cold start: the lobby), `M` or SHIFT-CLEAR turns the sound off and on, `CLEAR` the colours.
- **Network:** an N: TCP socket over DriveWire, each call one write and one read (`dw_call`: the FujiNet answers its commands in order and keeps a reply until asked, so a command and the fetch of its reply go together), under the Apple II's COBS frame layer. The DriveWire ROM masks interrupts while it talks, so jiffies can go uncounted: the server's round timer puts them back (`clock_check`, as on the Apple II).
- **Speed:** in MAME, the four-walker scene draws 13 passes a second on a CoCo 1/2 and 27 on a CoCo 3 (the logic goes on at 60 Hz either way).
- **Not done:** the real machines (the artifact phase, the DriveWire timing and the DAC sound are untested on hardware; MAME's audio capture only records the CoCo 3's sound).

## The Sega Master System

A Sega-mapper image for the FujiNet SMS cartridge, which the SMS CONFIG boots: z88dk +sms with zsdcc (`make sms`, `src/sms/blast.asm`, `tools/sms/mkrom.py`, M.U.L.E.'s). The program is in banks 0-1 (`0000h-7FD7h`, about 22K used), the art in two banks that take turns in slot 2 (`8000h-AFFFh`: bank 2 the arena's, bank 3 the menus'), the cartridge's mailbox at `B000h` over them. Art: `tools/drawart_sms.py` draws `gfx/sms/*.png` (indexed: pixel = palette entry) and checks mock screens against the VDP's rules; `tools/mkgfx_sms.py` writes `gen/sms/` (planar tiles, name table words, palettes, frame tables; `gfxart1.c` and `gfxart2.c` are the two banks).

- **Screen:** Mode 4, 32x24 cells, the Adam's layout (HUD a black status bar on row 0, the arena at cell column 5, stats in columns 0-3). Tiles 0-255 are the sprites' (a set a screen group: arena or menus), 256-447 the background's (the set, then glyph cells made as text is drawn: a cell for each glyph, foreground and background in use, 48 kept in the arena). A tile is 4 cells (name table words, flips shared in the set); the animated tiles are name table writes, a few a frame (`anim_step`). The backdrop is sprite palette entry 0.
- **Sprites:** the VDP can't flip them, so left frames and each seat's colours are patterns of their own; a bomber is four 8x8 sprites (a line crosses two, so four abreast are the 8 a line), the results' big cheer 16.
- **Controls:** pad = walk, 1 = fire (again on your own bomb: throw), 2 = rub out, Pause = RETURN, 2+Pause = ESC (leave, quit), 1+Pause = sound; Reset = ESC. Name entry is the wheel.
- **Link:** `src/sms/nstrans.c` is the NES's on the SMS mailbox (`B000h-B7FFh`, plain stores), never waited on. The clock is the frame interrupt (`src/sms/smsint.asm`; a 50 Hz console counts every fifth frame twice); the sound is the Adam's (`src/adam/sound.c`, the tunes of `src/tms9918/tunes.h`) stepped by it.

## The Nintendo Entertainment System

An MMC3 image (mapper 4, 128K PRG, 16K CHR-RAM, 8K WRAM) that the FujiNet NES cartridge's CONFIG boots: cc65 (`make nes`, `src/nes/fujiblst-nes.cfg`). Art: `tools/drawart_nes.py` draws `gfx/nes/*.png` (indexed: pixel = 4 x palette + slot, so the converter reads the palette straight from the file) and checks mock screens against the 2C02's rules; `tools/mkgfx_nes.py` writes `gen/nes/` (CHR sets, name table cells, palettes).

- **Screen:** a name table of 8x8 cells at 2 bits a pixel, one palette to each 16x16 attribute block. A tile is 4 cells and owns its block, so each picks one of 4 palettes (the arena: stone, brick, fire, items); text is the Namco font in slot 1 of the palette its block has (the seat's colour). All the cells of the arena set (font, wall, every frame of every tile) fit one 4K pattern table, so the tiles' animation is name table writes (`anim_step`), no pattern is rewritten. The menus have their own set (font, logo, trophy). A bomber is two 8x16 sprites in its seat's palette (left frames are the right ones flipped): four abreast fill the 8 sprites a line.
- **The status bar** is black over the floor's green: the backdrop colour is one for the whole picture, so the MMC3's scanline interrupt writes it again at line 32 (`src/nes/crt0.s`: rendering off at dot ~150, the new colour in the hblank, the video address parked on a black palette entry, a line on set to row 4 and rendering on). Rows 0-3 are blank cells, so only the backdrop shows there.
- **Drawing** goes through a queue the NMI drains in the blank (about 88 bytes a frame); a whole screen is drawn with rendering off (`ppu_direct`). The NMI also counts the frames (60.000 Hz: one in 608 is not counted) and sends the sprites (`oam_lock` while the game writes them).
- **Controls:** pad = walk, A = fire (again on your own bomb: throw), B = rub out, START = RETURN, SELECT = ESC (at the title: quit), START + SELECT = sound. Name entry is the wheel.
- **Memory:** the fixed bank has the engine and the arena; R7 (`CODE2`, `$A000`) the menus, the screens, the link's glue and the sound; bank 0 (`$8000`, left there) the art and then `main` and the test scenes (`CODE0`). `tools/nes/checkrom.py` fails the build on the claim, the vectors and a read-modify-write of the mailbox.
- **Link:** `src/nes/nstrans.c` is the 5200's on the NES mailbox ($5000-$57FF, plain stores), never waited on.
- Colours are MesenCE's 2C02 palette (what FujiNet Go NES shows). Option A (black visor, white eyes) is the bomber's look; `OPTION` in `drawart_nes.py` picks another.

## The Heathkit H89

`FUJIBLST.COM` for Heath's CP/M 2.2.04 (a 62K system will do: FNDISK's) or `FUJIBLST.ABS` for HDOS 2.0 (with FN: loaded, LP: too), on an H17 disk: z88dk with zsdcc (`make h89-cpm`, `make h89-hdos`; `make h89-boot` is the lobby's bootable disk). No overlays: the image is about 24K; `tools/h89/h89check.py` cuts the variables off the `.COM` and checks the program ends under D806H (CP/M, with a stack) and D090H (HDOS); the program checks again when it starts (`src/h89/plat.c`, `src/h89/tpa.asm`).

- **Screen:** the H19's 80x24 characters and its 25th line, graphics mode on for good (lines, quadrants, half blocks, the checkerboard). A tile is 5x2 cells, square on the H19; the arena is 65x22 in a border, the stats in columns 68-79, the names, wins and clock on the 25th line. A bomber is 3x2 cells with its seat's digit, ours reversed. The other screens put a text row's 40 columns at columns 20-59 and a BIG row's letters in every other column. Art: `gfx/h89/art.txt` (cells: the byte sent and a mask), `tools/mkgfx_h89.py` (`--preview PNG` draws it with the H19's own ROM).
- **The line:** the H19 is on the CPU board's 8250 at 9600 baud, about 960 bytes a second: that, not the CPU, is what the screen can carry. `src/h89/h19.c` keeps the screen (`bg`), the sprites over it and a copy of what the terminal shows (`have`), with the columns of each row that may differ; a flush sends only the cells that differ, the cheapest cursor moves first (resend, BS, LF, else `ESC Y`), our bomber's rows before anything else, as much as the line takes in a few frames (`FILL`). The bytes go into a 128-byte ring that the 2 ms clock interrupt and the main loop drain (`src/h89/h89io.asm`, M.U.L.E.'s: XON/XOFF, never a BIOS wait).
- **Speed:** a 2 MHz Z80, of which the emulated machine leaves the program about three quarters (the clock interrupt has the rest). The logic runs every jiffy (the walking must match the server's); a pass that is late doesn't draw. The hot paths are assembly: the cell writers and the state scan (`h19cell.asm`), sending a row (`h19run.asm`), the link's per-byte work (`nsfast.asm`). Shared code takes three hooks only the H89 sets: `PLAT_SEATS_WHEN_DRAWN` (the other bombers are laid out only for a pass that draws), `TOWARD_STEP` (no smoothing: a character is 3 units), `PLAT_COMMIT_SPAN` (a commit copies only what was patched). In fujinet-go-h89-desktop: the four-walker scene draws about 15 times a second, a live game against three bots about 8 (the line about 450 bytes a second, the server's state about 350).
- **Controls:** the keypad: 8 2 4 6 walk and keep walking, 7 9 1 3 turn at the next opening, 5 stops; SPACE drops a bomb (again on your own bomb with the glove: throw); ESC leaves; M or f1 sound (the H19's bell, for blasts, deaths and the countdown); f5 or ERASE draws the screen again.
- **Link:** the H89-ESP32 card's 82C55 at D0h, FujiBus by hand and never waited on (`src/h89/nstrans.c`: a STATUS every 6 jiffies, a READ of the bytes waiting, queued WRITEs); fujinet-lib opens, closes and reads the appkeys, its `fuji_bus_call` replaced by `src/h89/fbport.c`'s, which times out on a card that doesn't answer.

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

For the MSX:

```
make msxrom TEST_SCENE=1                # as the Adam's: 1 the arena, 2 the waiting room, 3, 4, 5 four walkers (touch src/test.c to go back)
python3 tools/msx/msxsim.py r2r/msxrom/'fujiblst[ASCII8].rom' r2r/msxrom/fujiblst.map scenario.txt OUTDIR
KEYS=keys.txt SECS=60 PEEKS="msx_passes msx_skip" tools/msxrig/rig.sh OUTDIR
```

- `msx/msxsim.py` runs the ROM in openMSX (C-BIOS) with no FujiNet: `wait N`, `snap NAME` (the VRAM drawn by `tools/tmsshot.py`), `hold`, `press`, `key`, `peek SYMBOL`.
- `msxrig/rig.sh` plays live in a namespace of its own: the server, bots, fujinet-pc (the MSX build that mounts user ROMs, `FNPC=`, default `~/Workspace/fn-msx-romtype/build/dist`) and openMSX with its FujiNet extension: CONFIG mounts the ROM and boots it. `KEYS` lines are `SECONDS CMD ARG` (`type TEXT`, `down|up|tap` u d l r s e 1), `PEEKS` prints counters at the end, `WAV` records the sound, `EVERY` is the seconds between pictures.
- After a cut link (`cutproxy.py`) the client goes back to the title; in the rig the firmware (fujinet-pc) then took about 35 seconds over the next connect, so FIRE had to wait that long.

For the Odyssey 2:

```
python3 tools/o2sim/o2sim.py tools/o2sim/scenes/scene1.txt OUTDIR r2r/o2/fujiblst.bin
tools/o2rig/rig.sh SCENARIO OUTDIR
make o2-rig && ROM=build/o2-rig/fujiblst.bin NAME= ROOM= tools/o2rig/rig.sh SCENARIO OUTDIR
O2SIM_DEBUG=1 ... (a `trace FILE N` step), then tools/o2sim/traceprof.py FILE build/o2
```

- `o2sim.py` runs MAME's Odyssey 2 (`O2SIM_MACHINE=videopac`: the PAL Videopac) with the FujiNet cart device (`-cartslot fujinet`, from fujinet-firmware's `pico/o2/emu`): pictures, peeks, the stick and the keyboard. The scenes in `tools/o2sim/scenes` poke the test hook (`ipoke 36 A1`-`A4`): 1 the arena, 2 the waiting room, 3 a round's end, 4 the game's end, on test.c's state.
- `o2rig/rig.sh` plays live in a namespace of its own: the server, bots, a copy of fujinet-pc-rs232 on BoIP and MAME throttled (the cart's transactions take real time). A scenario presses fire at the title.
- `traceprof.py` sums a MAME debugger trace into cycles per routine and page.

For the Arcadia:

```
python3 tools/arcsim/arcsim.py tools/arcsim/scenes/scene1.txt OUTDIR r2r/arcadia/fujiblst.bin
make arcadia-rig && tools/arcrig/rig.sh tools/arcsim/scenes/game.txt OUTDIR
NAME= ROOM= TABLE=blast2 tools/arcrig/rig.sh tools/arcsim/scenes/nolobby.txt OUTDIR
CONFIG=1 tools/arcrig/rig.sh tools/arcsim/scenes/config.txt OUTDIR
ARCSIM_DEBUG=1 ... (a `cpu FILE N` step), then tools/arcsim/traceprof.py FILE build/arcadia-rig/obj
python3 tools/arc/preview.py OUT.png SHOT.png ...
```

- `arcsim.py` (M.U.L.E.'s, for this build) runs MAME's Arcadia with the FujiNet cart device (`-cartslot fujinet`): pictures, peeks and traces by symbol, the disc, the keypad and the panel, the screen as text, `test 1`-`4` (the offline scenes). `ARCSIM_WAV` records the sound (`tools/arc/sndcheck.py`).
- `arcrig/rig.sh` plays live in a namespace of its own: the server, bots, a copy of fujinet-pc-rs232 on BoIP and MAME throttled. `CONFIG=1` boots the cart's CONFIG (fujinet-config's `arcadia/build/config.bin`) and the image from the SD. The scenes in `tools/arcsim/scenes` play a game (`game.txt`), host one (`host.txt`, `BOTS=0` and bots started late by `EXTRA`), leave (`leave.txt`), lose the link (`cut.txt`, `hello.txt`) and reset the console (`reset.txt`).
- The counters `NBAD` (bad frames), `NLATE` and `NPASH:NPASL` (late and all arena passes) and `PDBG` (our walker's last jiffy) are for peeking.
- `preview.py` scales MAME's snapshots to a TV's 4:3.

For the Astrocade:

```
python3 tools/astrosim/astrosim.py tools/astrosim/scenes/scene1.txt OUTDIR r2r/astrocade/fujiblst.bin
make astrocade-rig && tools/astrorig/rig.sh tools/astrosim/scenes/game.txt OUTDIR
NAME= ROOM= tools/astrorig/rig.sh tools/astrosim/scenes/nolobby.txt OUTDIR
CONFIG=1 tools/astrorig/rig.sh tools/astrosim/scenes/config.txt OUTDIR
ASTROSIM_WAV=FILE.wav python3 tools/astrosim/astrosim.py tools/astrosim/scenes/sounds.txt OUTDIR
ASTROSIM_DEBUG=1 ... (a `cpu FILE N` step), then MARK=200D tools/astrosim/traceprof.py FILE LISTING
```

- `astrosim.py` (the Arcadia's arcsim, for MAME's astrocde with the FujiNet cart device) holds keypad 1 at the BIOS's menu, then plays the scenario: pictures, peeks and traces by symbol, the handle and the keypad, the screen as text (the 4x6 font read back from the pixels), `test 1`-`4` (the offline scenes), `test 5` the title, `test 6` the sound board (every sound in turn). `cpu` traces lead each line with the page mapped.
- `astrorig/rig.sh` plays live in a namespace of its own: the server, bots, a copy of fujinet-pc-rs232 on BoIP and MAME throttled. `CONFIG=1` boots the cart's CONFIG (fujinet-config's `astrocade/build/config.bin`) and the image from the SD. The scenes in `tools/astrosim/scenes` play a game (`game.txt`), host one (`host.txt`, `BOTS=0` and bots started late by `EXTRA`), the lobby-less path (`nolobby.txt`), lose the link (`cut.txt` with `tools/rigs/cutproxy.py`, `hello.txt` with `dropproxy.py`) and reset the console (`reset.txt`).
- The counters `NBAD` (bad frames) and `PDBG` (our walker's jiffies: a count, the handle, x, y, dir, acc) are for peeking.

For the Atari 5200:

```
tools/a52sim/build.sh [TEST_SCENE=n]        # a clean build (the lib first: tools/atari5200/mkfnlib.sh, outside defoogi)
tools/a52sim/shot.sh r2r/atari5200/fujiblst.a52 OUT 700     # run in MAME (~/Workspace/mame-a5200), snapshot at frame 700
tools/a52sim/rig.sh tools/a52sim/scenes/game.txt OUT         # live: server, bots, fujinet-pc, MAME in a namespace
NAME= ROOM= tools/a52sim/rig.sh tools/a52sim/scenes/nolobby.txt OUT
tools/a52sim/rig.sh tools/a52sim/scenes/pad.txt OUT          # the real stick, buttons and keypad (MAME's ports)
```

- `a52sim.py` is the 7800's scenario driver on MAME's a5200 (`pad` with the analog stick and keypad, `swaps N` the cart's swaps after a quit to CONFIG). `quit.txt` quits from the title.

For the Atari 7800:

```
tools/a78sim/build.sh [TEST_SCENE=n]        # a clean build (make doesn't see the flags change)
tools/a78sim/shot.sh r2r/atari7800/fujiblst.a78 OUT 700     # run in MAME, snapshot at frame 700 (DUMP="addr:len ..." prints memory)
python3 tools/a78sim/a78sim.py r2r/atari7800/fujiblst.a78 build/fujiblst-a78.map tools/a78sim/scenes/scene1.txt OUT   # offline scenes
tools/a78sim/rig.sh tools/a78sim/scenes/game.txt OUT         # live: server, bots, fujinet-pc, MAME in a namespace
NAME= ROOM= tools/a78sim/rig.sh tools/a78sim/scenes/nolobby.txt OUT
A78SIM_SYSTEM=a7800p tools/a78sim/rig.sh ...                 # a PAL console
```

- A test build (`TEST_SCENE=1`-`5`) leaves out the menus and the lib's appkeys: bank 0 is full. A production build has no scenes. `make atari7800 NMILOG=1` logs the DLIs' times (`nlog`); `DBG=-DDBGBG` / `-DDBGORD` colour the background by zone / DLI.
- `tools/a78sim/probe160b` is the MARIA probe that sized the display: what fits a line.

For the ColecoVision:

```
python3 tools/coleco/colsim.py SCENARIO OUTDIR r2r/coleco/fujiblst.rom build/coleco/fujiblst.lst
make coleco-rig && tools/colrig/rig.sh SCENARIO OUTDIR
NAME= ROOM= tools/colrig/rig.sh SCENARIO OUTDIR
COLSIM_WAV=FILE.wav python3 tools/coleco/colsim.py SCENARIO OUTDIR ...
```

- `colsim.py` (M.U.L.E.'s, for this build) runs MAME's coleco with the FujiNet cart device (`-cartslot fujinet`) and a scenario, a command a line: `test 1`-`4` (the offline scenes), `test 5` the title, `test 6` the sound board; `wait`, `waitfor`, `press`, `hold`, `key`, `reset`, `snap`, `peek`, `dump`, `vdump`, `fill`, `trace`, `show` (by symbol, from the listing).
- `colrig/rig.sh` plays live in a namespace of its own: the server, bots, a copy of fujinet-pc-rs232 on BoIP and MAME throttled; `EXTRA` runs a proxy (`tools/rigs/dropproxy.py`, `cutproxy.py`) between them. `NAME=` and `ROOM=` empty play the lobby-less path. `CONFIG=1` boots the cart's CONFIG (fujinet-config's `coleco/build/config.bin`) and the image from the SD as `fujiblst.col`; its fujinet-pc must know `.col` images (`FNPC=~/Workspace/fn-coleco-board/build/dist`: the shared rs232 build's mount stalls at LOADING 0).
- The counters `NBAD` (bad frames) and `PDBG` (our walker's jiffies) are for peeking.

For the Atari 2600:

```
tools/a26/build.sh                     # build/a26/fujiblst.bin (PAL=1: build/a26-pal)
python3 tools/a26/mkzp.py build/a26/netdec.lst build/a26/zp.lua
tools/a26rig/rig.sh OUTDIR SECS        # live: server, bots, fujinet-pc and MAME
LUA=frames tools/a26rig/rig.sh OUTDIR SECS     # every frame 262 lines (A26_LINES=312, MACHINE=a2600p, ROM=build/a26-pal/fujiblst.bin: PAL)
LUA=parity tools/a26rig/rig.sh OUTDIR SECS     # a walk's trace, to replay through internal/game's Walker
tools/a26/spike.sh [LOOK]              # the arena kernel on its own (a plain 4K ROM)
```

- `rig.sh` is the Arcadia's: a private network namespace with `fujiblastd`, bots, fujinet-pc (RS232 build) on BoIP and MAME's `a2600 -cartslot fujinet`. `NAME=` / `ROOM=` empty tests the name wheel and the room list; `ROOM=tcp://127.0.0.1:9911/?table=dev1` with `EXTRA="python3 tools/rigs/cutproxy.py 9911 9910 14"` (or `dropproxy.py`) cuts the link or loses the HELLO.
- `tools/a26sim/live.lua` takes snapshots (`A26_SNAPS=5,10`), logs zero-page cells (`A26_SHOW=PHASE,BX+1`), holds inputs (`A26_KEYS="12:left:50"`) and dumps the map from the planes (`A26_MAP=1`); `SNDOPT="-sound sdl -wavwrite f.wav"` records the sound.
- `tools/a26/checkrom.py` is the cartridge tree's with the two opcode lengths its table lacks (CPX/CPY zero page, DCP).

For the CoCo (MAME's `coco2b`, `MACHINE=coco3` for the CoCo 3; the build needs `make coco TEST=1` for the offline scenes):

```
python3 tools/cocosim/cocosim.py r2r/coco/fujiblst.dsk build/fujiblst-coco.map tools/cocosim/scenes/arena.txt OUT
MACHINE=coco3 python3 tools/cocosim/cocosim.py r2r/coco/fujiblst.dsk build/fujiblst-coco3.map SCENARIO OUT
tools/cocosim/rig.sh tools/cocosim/scenes/live.txt OUT       # live: server, bots, fujinet-pc-coco on the Becker port, MAME (MACHINE=coco3 COCO_BIOS=hdbdw3bc3)
python3 tools/cocosim/prof.py build/fujiblst-coco.map OUT/prof.trace    # instructions by function from a trace step
```

- `cocosim.py` is `a2sim.py` for MAME's CoCo: it boots the disk, types `RUN"AUTOEXEC"`, and plays the scenario (`test` scenes through the test hook, `boot` to start at once, `mame N` to wait for MAME's frame N, `snap`, `peek`, `dump`, `trace`, `buzz`). MAME 0.289 wants the VDG's and the GIME's character ROMs, which its ROM set lacks: the rig supplies blank ones. `COCO_MONITOR=0` is the composite monitor, `COCOWAV=file` records the sound.
- `scenes/bench4.txt` is the four-walker timing scene (`renders` after each wait). `make coco3` alone does not refresh `r2r/coco/fujiblst.dsk`: the disk depends on the CoCo 3 files, so use `make coco`.

For the NES (MAME's `nes` with the FujiNet cartridge's slot; MesenCE is what FujiNet Go NES runs; `make nes TEST_SCENE=n` builds an offline scene in, as everywhere):

```
python3 tools/nessim/nessim.py r2r/nes/fujiblst.nes build/fujiblst-nes.map SCENARIO OUT      # MAME: wait, snap, hold, press, key, peek, ppuwatch; NESWAV=file records the sound
python3 tools/nessim/mesensim.py r2r/nes/fujiblst.nes build/fujiblst-nes.map SCENARIO OUT    # MesenCE (the 2C02's OAM corruption, the status bar's split)
tools/nessim/rig.sh tools/nessim/scenes/game.txt OUT         # live: server, bots, fujinet-pc-rs232 on BoIP 9987, MAME throttled
```

- `nessim.py` and `mesensim.py` are M.U.L.E.'s (tools/nessim): the `test` scene command is not used here, the scene is the build's `TEST_SCENE`. The scenes in `tools/nessim/scenes`: `game.txt` (a live game, with `nes_passes` / `nes_skip` for the draw rate: 600 passes a 600 frames, about a third made up without drawing), `cut.txt` (`ROOM=tcp://127.0.0.1:9911/?table=dev1 EXTRA="python3 tools/rigs/cutproxy.py 9911 9910 62"`: the seat is back in 3 s), `nolobby.txt` (`NAME= ROOM=`: the name wheel), `sound.txt` (`snd_calls`, `snd_notes`).
- Checked: the status bar's split is black through line 31 and the arena exact from line 32 in both emulators, 120 frames in a row; the first versions had a white line at the split in MAME and one wrong fine-Y bit (`$2006`'s high byte can't set bit 14, so rendering resumes on a row's first line).

For the H89 (fujinet-go-h89-desktop's headless H89 and its FujiNet in process; `make h89-cpm TEST_SCENE=n` builds an offline scene in, as everywhere):

```
python3 tools/h89sim/h89sim.py r2r/h89-cpm/fujiblst.h8d r2r/h89-cpm/fujiblst.map SCENARIO OUT   # start, wait, snap, hold, press, key, type, keyname, peek, rate, pcprof (H89_PROF=1)
tools/h89rig/rig.sh tools/h89rig/live2.txt OUT                # live: server and bots in a network namespace; OS=hdos for HDOS
```

- `h89sim.py` is M.U.L.E.'s (`H89SIM_OS=hdos`, `H89SIM_FN0=1`). `rate N` prints the draws, the passes made up, the line's bytes and the link's a second; `tools/h89sim/bench4.txt` is the four-walker benchmark (scene 5). Never two at once: the H89's ports are fixed.

For the SMS (MAME's `sms1` with the FujiNet cartridge's slot, no BIOS; `make sms TEST_SCENE=n` builds an offline scene in, as everywhere; `SMSSIM_SYSTEM=smspal` is a 50 Hz console):

```
python3 tools/smssim/smssim.py r2r/sms/fujiblst.sms r2r/sms/fujiblst.map SCENARIO OUT   # wait, snap, hold, pad, press, key, peek, log, vram; SMSWAV=file records the sound
tools/smssim/rig.sh tools/smssim/scenes/game.txt OUT         # live: server, bots, fujinet-pc-rs232 on BoIP 9988, MAME throttled
```

- `smssim.py` is M.U.L.E.'s (its `test` scene command is not used: the scene is the build's `TEST_SCENE`). The scenes in `tools/smssim/scenes` are the NES's: `game.txt` (`sms_passes` / `sms_skip` give the draw rate: 600 passes a 600 frames, about half made up without drawing, ~31 drawn a second), `cut.txt` (`ROOM=tcp://127.0.0.1:9911/?table=dev1 EXTRA="python3 tools/rigs/cutproxy.py 9911 9910 62"`: the seat is back in 2 s), `nolobby.txt` (`NAME= ROOM=`: the name wheel), `sound.txt`.

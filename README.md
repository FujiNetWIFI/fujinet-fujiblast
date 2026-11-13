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
- the Atari 5200, with the FujiNet cartridge.

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



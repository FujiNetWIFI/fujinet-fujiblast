# FujiBlast for FujiNet 8-bit computers (MekkoGX).
#
#   make atari                     r2r/atari/fujiblst.atr (48K Atari 400/800, XL/XE)
#   make apple2                    r2r/apple2/fujiblst.po (128K //e, //c, IIgs; high-res)
#   make msdos                     r2r/msdos/fujiblst.img (128K PC, XT or PCjr with CGA)
#   make adam                      r2r/adam/fujiblst.ddp (Coleco Adam; TMS9918A)
#   make msxrom                    r2r/msxrom/fujiblst[ASCII8].rom (MSX, a FujiNet user ROM; TMS9918A)
#   make intv                      r2r/intv/fujiblst.rom (Intellivision, the FujiNet cartridge)
#   make o2                        r2r/o2/fujiblst.bin (Odyssey 2 / Videopac, the FujiNet cartridge)
#   make arcadia                   r2r/arcadia/fujiblst.bin (Emerson Arcadia 2001, the FujiNet cartridge)
#   make astrocade                 r2r/astrocade/fujiblst.bin (Bally Astrocade, the FujiNet cartridge)
#   make coleco                    r2r/coleco/fujiblst.col (ColecoVision, the FujiNet cartridge)
#   make atari7800                 r2r/atari7800/fujiblst.a78 (Atari 7800, the FujiNet cartridge; MARIA 160B)
#   defoogi make atari             the same in the toolchain container
#   make SERVER=host:9910          server used when the lobby gave none (remembered)
#   make server                    server/fujiblastd and server/blastbot (Go)
#   make proto                     src/proto.h from the server's constants
#
# src/ is platform neutral; src/platform-specific/*.h is what each platform
# implements in src/<platform>/. Art: gfx/*.png (tools/drawart.py draws
# them); tools/mkgfx_<platform>.py converts them into gen/ (gitignored).

PRODUCT = fujiblst
PLATFORMS = atari apple2 msdos adam msxrom atari7800

# the TMS9918A code (src/tms9918) is the Adam's and the MSX's
PLATFORM_COMBOS = adam+=tms9918 msxrom+=tms9918,msx

SRC_DIRS = src src/%PLATFORM% gen/%PLATFORM%
EXTRA_INCLUDE = src src/platform-specific gen

# fujinet-lib (appkeys): a release, fetched into _cache (works in defoogi)
FUJINET_LIB = 5.1.0

SERVER ?=
PYTHON ?= python3

# ---- Atari 400/800 (48K), XL/XE -------------------------------------------
# cc65, no DOS: picoboot loads FUJIBLST.COM, whose first chunk (src/atari/
# start.s) switches BASIC off. Memory: src/atari/fujiblst.cfg.
CFLAGS_EXTRA_ATARI += -Os -Cl
LDFLAGS_EXTRA_ATARI += -C src/atari/fujiblst.cfg -m build/fujiblst-atari.map
EXECUTABLE_EXTRA_DEPS_ATARI += src/atari/fujiblst.cfg
ifeq ($(PLATFORM),atari)
  EXTRA_INCLUDE += gen/atari
endif

# ---- Apple II (128K //e, //c, IIgs), high-res -----------------------------
# cc65, ProDOS: FUJIBLST.SYSTEM (tools/a2loader) loads FUJIBLST, which reads
# its art from FUJIBLST.GFX into aux RAM. Memory: src/apple2/fujiblst.cfg.
CFLAGS_EXTRA_APPLE2 += -Os -Cl
LDFLAGS_EXTRA_APPLE2 += -C src/apple2/fujiblst.cfg -m build/fujiblst-apple2.map
EXECUTABLE_EXTRA_DEPS_APPLE2 += src/apple2/fujiblst.cfg
ifeq ($(PLATFORM),apple2)
  EXTRA_INCLUDE += gen/apple2
  DISK_BASIC_SYSTEM =
endif

# Where the modules go: the room's screens, test scenes, main and
# the link below the HGR pages; the menus and appkeys in an overlay. (The
# language card, beside the blitter, has no room for input too.)
A2OBJ = build/$(PRODUCT)/apple2/src
$(A2OBJ)/screens.o $(A2OBJ)/test.o $(A2OBJ)/main.o $(A2OBJ)/apple2/nstrans.o: CFLAGS += --code-name LOWCODE --rodata-name LOWDATA
# the menus and the appkeys: an overlay, under other names (src/apple2/overlay.c)
A2OVL = -Dtitle=ov_title -Dname_entry=ov_name_entry -Dtable_list=ov_table_list \
        -Dappkey_load=ov_appkey_load -Dappkey_save_name=ov_appkey_save_name \
        -Dappkey_save_room=ov_appkey_save_room -Dappkey_clear_room=ov_appkey_clear_room
$(A2OBJ)/menus.o $(A2OBJ)/appkey.o: CFLAGS += --code-name OVERLAY --rodata-name OVERLAY $(A2OVL)

# FUJIBLST.SYSTEM is our own loader (tools/a2loader): it skips the HGR
# pages' zeros in the file, so the program can run up to $BE00.
apple2/disk-post:: build/a2loader.bin
	ac -p $(DISK) FUJIBLST.GFX BIN 0x0800 < $(EXECUTABLE).gfx
	ac -d $(DISK) FUJIBLST.SYSTEM
	ac -p $(DISK) FUJIBLST.SYSTEM SYS 0x2000 < build/a2loader.bin

build/a2loader.bin: tools/a2loader/loader.s tools/a2loader/loader.cfg
	@mkdir -p build
	ca65 -o build/a2loader.o tools/a2loader/loader.s
	ld65 -C tools/a2loader/loader.cfg -o $@ build/a2loader.o

# ---- MS-DOS (an IBM PC, XT, PCjr or compatible: CGA, 128K) -----------------
# Open Watcom, small model, from defoogi unless installed (tools/dos/ow).
# CGA's 320x200 4-colour mode, its palette set through the BIOS
# (src/msdos/gfx.c). The link to the server is FUJINET.SYS's serial port,
# driven by interrupt (src/msdos/fbport.c); fujinet-lib (5.1.0's msdos
# release, small model) opens it and reads the appkeys. FUJIBLST.EXE runs
# from AUTOEXEC.BAT on its disk, as the lobby starts it.
ifeq ($(PLATFORM),msdos)
  CC_OW2 = $(CURDIR)/tools/dos/ow wcc
  AS_OW2 = $(CURDIR)/tools/dos/ow wasm
  AR_OW2 = $(CURDIR)/tools/dos/ow wlib
  LD_OW2 = $(CURDIR)/tools/dos/ow wlink OPTION quiet
  EXTRA_INCLUDE += gen/msdos
endif
# (-ad names the object in the .d without its directory: -adt names it)
OPT ?= -os -oi -ol
CFLAGS_EXTRA_MSDOS += -zq -ms $(OPT) -s -zp1 -adt=$@ -DQUIT_HINT='"ESC: LOBBY"' -DKEY_END_NAME='"ENTER"' \
                      -DPLAT_MAP_NEXT -DMOVE_EVERY=6
ifeq ($(STATS),1)               # plays by itself, writes A:\STATS.TXT (tools/dosrig/86box.sh)
  CFLAGS_EXTRA_MSDOS += -DSTATS
  ASFLAGS_EXTRA_MSDOS += -dSTATS
endif
LDFLAGS_EXTRA_MSDOS += OPTION map=build/fujiblst-msdos.map OPTION stack=2048
ASFLAGS_EXTRA_MSDOS += -zq -0

msdos/disk-post::
	printf '@ECHO OFF\r\nFUJIBLST\r\n' > build/autoexec.bat
	mcopy -o -i $(DISK) build/autoexec.bat "::AUTOEXEC.BAT"

# ---- Coleco Adam (the TMS9918A) ---------------------------------------------
# z88dk's zcc with zsdcc, from the defoogi container unless installed
# (tools/z80/z88dk). fujinet-lib is fujinet-lib-experimental built with
# zsdcc (tools/z80/fnlib_adam.sh: the released Adam lib is sccz80, whose
# arguments come in the other order). The program runs from 0000h and must
# end below C800h, where the image's bootstrap sits (tools/adam_end.py);
# the VDP's interrupt is the Z80's NMI, the frame clock (src/adam/nmi.asm).
# Nothing prints, so z88dk's console (2.5K with its font) is left out.
FNLIB_EXP ?= $(HOME)/Workspace/fujinet-lib-experimental
FNLIB_COMMIT ?= a29e57f
ifeq ($(PLATFORM),adam)
  FUJINET_LIB := __UNDEFINED__
  CC_Z88DK = $(CURDIR)/tools/z80/z88dk zcc
  AS_Z88DK = $(CURDIR)/tools/z80/z88dk z80asm
  LD_Z88DK = $(CURDIR)/tools/z80/z88dk zcc
  EXTRA_INCLUDE += build/fnlib-adam/include gen/adam src/tms9918
  LIBS += -Lbuild/fnlib-adam -lfujinet-adam
endif
# (zcc hands a define on through a shell: a string is quoted twice)
CFLAGS_EXTRA_ADAM += -compiler=sdcc -SO3 -DQUIT_HINT='"\"ESC: LOBBY\""' -DTAGLINE='"\"BOMBS AND BLASTS FOR 2-4\""' \
                     -DHINT_LR='"\"(L/R)\""' -DNAME_HELP='"\"LETTERS, DIGITS, THEN RETURN\""'
LDFLAGS_EXTRA_ADAM += -compiler=sdcc -m -pragma-define:CRT_ENABLE_NMI=2 -pragma-define:CRT_INITIALIZE_BSS=1 \
                      -pragma-define:CRT_ENABLE_STDIO=0 -pragma-define:CLIB_DEFAULT_SCREEN_MODE=-1 \
                      -pragma-define:CLIB_EXIT_SCREEN_MODE=-1 -pragma-redirect:fputc_cons=l_ret
EXECUTABLE_EXTRA_DEPS_ADAM += build/fnlib-adam/fujinet-adam.lib
# an offline scene built in (src/test.c): make adam TEST_SCENE=1 (then touch src/test.c to go back)
ifneq ($(TEST_SCENE),)
  CFLAGS_EXTRA_ADAM += -DTEST_SCENE=$(TEST_SCENE) -DTEST_ARG=$(or $(TEST_ARG),0)
endif

adam/r2r-post::
	@python3 tools/adam_end.py r2r/adam/$(PRODUCT).map

build/fnlib-adam/fujinet-adam.lib: tools/z80/fnlib_adam.sh
	tools/z80/fnlib_adam.sh $(FNLIB_EXP) $(FNLIB_COMMIT) build/fnlib-adam

.PHONY: gen-atari7800 gen-atari gen-apple2 gen-msdos gen-adam gen-msxrom server proto intv o2 o2-rig arcadia arcadia-rig astrocade astrocade-rig coleco coleco-rig FORCE

# ---- Intellivision (the FujiNet cartridge) ----------------------------------
# IntyBASIC, not mekkogx: src/intv/Makefile builds it (the compiler too).
intv:
	$(MAKE) -C src/intv $(if $(SERVER),SERVER=$(SERVER))

# ---- Odyssey 2 / Videopac (the FujiNet cartridge) ---------------------------
# 8048 assembly (Macroassembler AS), not mekkogx: src/o2/build.sh builds the
# APPBANK image and runs its checks. o2-rig: a copy whose default server is
# the rig's (build/o2-rig/fujiblst.bin), gen/server left alone.
o2:
	$(if $(SERVER),DEFSRV=$(SERVER)) src/o2/build.sh

o2-rig:
	DEFSRV=127.0.0.1:9910 OUT=build/o2-rig/obj R2R=build/o2-rig src/o2/build.sh

# ---- Emerson Arcadia 2001 (the FujiNet cartridge) ---------------------------
# 2650 assembly (Macroassembler AS), not mekkogx: src/arcadia/build.sh builds
# the APPBANK image and runs its checks. arcadia-rig: a copy whose default
# server is the rig's (build/arcadia-rig/fujiblst.bin), gen/server left alone.
arcadia:
	$(if $(SERVER),DEFSRV=$(SERVER)) src/arcadia/build.sh

arcadia-rig:
	DEFSRV=127.0.0.1:9910 OUT=build/arcadia-rig/obj R2R=build/arcadia-rig src/arcadia/build.sh

# ---- Bally Astrocade (the FujiNet cartridge) ---------------------------------
# Z80 assembly (zmac 1.3), not mekkogx: src/astrocade/build.sh builds the
# APPBANK image and runs its checks (LOOK=, FOOTER= its colours).
# astrocade-rig: a copy whose default server is the rig's
# (build/astrocade-rig/fujiblst.bin), gen/server left alone.
astrocade:
	$(if $(SERVER),DEFSRV=$(SERVER)) bash src/astrocade/build.sh

astrocade-rig:
	DEFSRV=127.0.0.1:9910 OUT=build/astrocade-rig/obj R2R=build/astrocade-rig bash src/astrocade/build.sh

# ---- ColecoVision (the FujiNet cartridge) -------------------------------------
# Z80 assembly (zmac 1.3), not mekkogx: src/coleco/build.sh builds the APPBANK
# image (the Adam's art and sound, the Astrocade's logic) and runs its checks.
# coleco-rig: a copy whose default server is the rig's
# (build/coleco-rig/fujiblst.rom), gen/server left alone.
coleco:
	$(if $(SERVER),DEFSRV=$(SERVER)) bash src/coleco/build.sh

coleco-rig:
	DEFSRV=127.0.0.1:9910 OUT=build/coleco-rig/obj R2R=build/coleco-rig bash src/coleco/build.sh

# ---- MSX (MSX1 and up): a ROM the FujiNet cartridge runs ----------------------
# The Adam's screen and game on the MSX's TMS9918A (src/tms9918 is shared),
# zcc with zsdcc as above. An ASCII8 megaROM (src/msx/blast.asm,
# tools/msx/mkrom.py): the program fixed at 4000h-9FFFh, the art in two
# banks of the window at A000h. fujinet-lib is fujinet-lib-experimental built
# with zsdcc (tools/z80/fnlib_msx.sh); the link to the server is the
# cartridge's registers, driven by hand (src/msx/nstrans.c).
ifeq ($(PLATFORM),msxrom)
  FUJINET_LIB := __UNDEFINED__
  CC_Z88DK = $(CURDIR)/tools/z80/z88dk zcc
  AS_Z88DK = $(CURDIR)/tools/z80/z88dk z80asm
  LD_Z88DK = $(CURDIR)/tools/z80/z88dk zcc
  EXTRA_INCLUDE += build/fnlib-msx/include gen/msxrom src/tms9918
  LIBS += -Lbuild/fnlib-msx -lfujinet-msx
endif
CFLAGS_EXTRA_MSXROM += -compiler=sdcc -SO3 -DQUIT_HINT='"\"ESC: LOBBY\""' -DTAGLINE='"\"BOMBS AND BLASTS FOR 2-4\""' \
                       -DHINT_LR='"\"(L/R)\""' -DNAME_HELP='"\"LETTERS, DIGITS, THEN RETURN\""'
LDFLAGS_EXTRA_MSXROM += -compiler=sdcc -m -pragma-define:CRT_MSX_CUSTOM_HEADER=1 -Ca-I$(CURDIR)/src/msx/crt \
                        -pragma-define:CRT_ENABLE_STDIO=0 -pragma-redirect:fputc_cons=l_ret
EXECUTABLE_EXTRA_DEPS_MSXROM += build/fnlib-msx/fujinet-msx.lib src/msx/crt/msx_rom_header.asm
ifneq ($(TEST_SCENE),)
  CFLAGS_EXTRA_MSXROM += -DTEST_SCENE=$(TEST_SCENE) -DTEST_ARG=$(or $(TEST_ARG),0)
endif

msxrom/r2r-post::
	@python3 tools/msx/mkrom.py r2r/msxrom $(PRODUCT)

build/fnlib-msx/fujinet-msx.lib: tools/z80/fnlib_msx.sh tools/z80/msx-portio.c
	tools/z80/fnlib_msx.sh $(FNLIB_EXP) $(FNLIB_COMMIT) build/fnlib-msx

# ---- Atari 7800 (the FujiNet 7800 cartridge) ---------------------------------
# cc65, a SuperGame image with the cart's RAM, 128K in 16K banks
# (src/atari7800/fujiblst-a78.cfg, DESIGN.md). The screen is MARIA's: 25 zones
# of 8 lines, 160B cells (12 colours a pixel) for the arena and menu art, 320A
# text, the bombers two 160A objects each from the pages in REPL, which every
# bank has (tools/atari7800/mkrom.py copies it). fujinet-lib is
# fujinet-lib-experimental's atari7800 target (tools/atari7800/mkfnlib.sh);
# the link to the server is the cartridge's mailbox, driven by hand
# (src/atari7800/nstrans.c). The test hook / scenes as everywhere (TEST_SCENE).
FNLIB_7800 ?= $(HOME)/Workspace/fnlib-7800
ifeq ($(PLATFORM),atari7800)
  FUJINET_LIB := __UNDEFINED__
  LIBS += build/fnlib-a78/fujinet-a78.lib
  EXTRA_INCLUDE += gen/atari7800 src/atari7800 build/fnlib-a78/include
endif
CFLAGS_EXTRA_ATARI7800 += -Os -Cl -DPLAT_NO_TEST -DPLAT_NAME_WHEEL -DKEY_END_NAME='"PAUSE"' -DQUIT_HINT='"SELECT: LOBBY"' \
                          -DTAGLINE='"BOMBS AND BLASTS FOR 2-4"' -DHINT_LR='"(L/R)"' \
                          -DNAME_HELP='"UP/DOWN: LETTER  FIRE: ADD  >: DONE"'
CFLAGS_EXTRA_ATARI7800 += $(DBG)
ifeq ($(NMILOG),1)
  ASFLAGS += -Wa -DNMILOG
endif
LDFLAGS_EXTRA_ATARI7800 += -C src/atari7800/fujiblst-a78.cfg -m build/fujiblst-a78.map
# the phases' code that doesn't run every frame: bank 0 (always the one in the window)
A7OBJ = build/$(PRODUCT)/atari7800
$(A7OBJ)/src/menus.o $(A7OBJ)/src/screens.o $(A7OBJ)/src/appkey.o $(A7OBJ)/src/test.o $(A7OBJ)/src/atari7800/gfxsetup.o: CFLAGS += --code-name OVERLAY1 --rodata-name OVL1RO
# constants only the main thread reads (never the NMI, which may run with the art's bank in)
$(A7OBJ)/gen/atari7800/gfxdata.o $(A7OBJ)/src/main.o $(A7OBJ)/src/play.o: CFLAGS += --rodata-name OVL1RO
EXECUTABLE_EXTRA_DEPS_ATARI7800 += src/atari7800/fujiblst-a78.cfg build/fnlib-a78/fujinet-a78.lib gen/atari7800/gfxdata.c
ifneq ($(TEST_SCENE),)
  CFLAGS_EXTRA_ATARI7800 += -DTEST_SCENE=$(TEST_SCENE) -DTEST_ARG=$(or $(TEST_ARG),0) -DNO_APPKEYS -DNO_MENUS
endif

# REPL into every bank, the checks, the .a78 (r2r/atari7800/fujiblst.a78)
atari7800/executable-post::
	python3 tools/atari7800/mkrom.py build/fujiblst-a78.map $(EXECUTABLE) $(EXECUTABLE:.bin=.a78)

build/fnlib-a78/fujinet-a78.lib: tools/atari7800/mkfnlib.sh $(wildcard $(FNLIB_7800)/common/*.c $(FNLIB_7800)/bus/atari7800/* $(FNLIB_7800)/include/*.h)
	tools/atari7800/mkfnlib.sh $(FNLIB_7800) build/fnlib-a78

gen-atari7800: gen/config.h gen/atari7800/gfxdata.c build/fnlib-a78/fujinet-a78.lib

gen/atari7800/gfxdata.c: tools/mkgfx_a78.py tools/drawart_a78.py tools/drawart_dhgr.py tools/a78/palette.py gfx/namco.bin
	@mkdir -p gen/atari7800
	$(PYTHON) tools/mkgfx_a78.py gen/atari7800

r2r/atari/$(PRODUCT): gen-atari
r2r/atari7800/$(PRODUCT): gen-atari7800
r2r/apple2/$(PRODUCT): gen-apple2
r2r/msdos/$(PRODUCT): gen-msdos
r2r/adam/$(PRODUCT): gen-adam
r2r/msxrom/$(PRODUCT): gen-msxrom

gen-atari: gen/config.h gen/atari/gfxdata.s

gen/config.h: FORCE
	@mkdir -p gen
	@if [ -n "$(SERVER)" ]; then echo "$(SERVER)" > gen/server.new; \
	  cmp -s gen/server.new gen/server || mv gen/server.new gen/server; rm -f gen/server.new; fi
	@[ -f gen/server ] || echo "localhost:9910" > gen/server
	@printf '/* GENERATED by the Makefile. */\n#define DEFAULT_SERVER "%s"\n' "$$(cat gen/server)" > gen/config.h.new
	@cmp -s gen/config.h.new gen/config.h || mv gen/config.h.new gen/config.h
	@rm -f gen/config.h.new

gen-apple2: gen/config.h gen/apple2/gfxdata.s

gen/apple2/gfxdata.s: tools/mkgfx_hgr.py tools/hgr.py tools/drawart_hgr.py tools/artlayout.py tools/png.py gfx/hgr/tiles.png gfx/hgr/sprites.png gfx/hgr/logo.png gfx/hgr/wall.png gfx/hgr/font3.png gfx/hgr/font5.png
	@mkdir -p gen/apple2
	$(PYTHON) tools/mkgfx_hgr.py gen/apple2

gen-msdos: gen/config.h gen/msdos/gfxdata.c

gen/msdos/gfxdata.c: tools/mkgfx_cga.py tools/artlayout.py tools/png.py gfx/cga/tiles.png gfx/cga/sprites.png gfx/cga/logo.png gfx/cga/cells.png gfx/namco.bin
	@mkdir -p gen/msdos
	$(PYTHON) tools/mkgfx_cga.py gen/msdos

gen-adam: gen/config.h gen/adam/gfxdata.c build/fnlib-adam/fujinet-adam.lib

gen/adam/gfxdata.c gen/adam/gfxart1.c gen/adam/gfxart2.c &: tools/mkgfx_tms.py tools/artlayout.py tools/png.py gfx/tms/tiles.png gfx/tms/sprites.png gfx/tms/logo.png gfx/tms/cells.png gfx/namco.bin
	@mkdir -p gen/adam
	$(PYTHON) tools/mkgfx_tms.py gen/adam

gen-msxrom: gen/config.h gen/msxrom/gfxdata.c build/fnlib-msx/fujinet-msx.lib

gen/msxrom/gfxdata.c gen/msxrom/gfxart1.c gen/msxrom/gfxart2.c &: tools/mkgfx_tms.py tools/artlayout.py tools/png.py gfx/tms/tiles.png gfx/tms/sprites.png gfx/tms/logo.png gfx/tms/cells.png gfx/namco.bin
	@mkdir -p gen/msxrom
	$(PYTHON) tools/mkgfx_tms.py gen/msxrom

gen/atari/gfxdata.s: tools/mkgfx_atari.py tools/artlayout.py tools/png.py gfx/tiles.png gfx/glyphs.png gfx/logo.png gfx/sprites.png
	@mkdir -p gen/atari
	$(PYTHON) tools/mkgfx_atari.py gen/atari

server:
	cd server && go build -o fujiblastd ./cmd/fujiblastd && go build -o blastbot ./cmd/blastbot

proto:
	cd server && go run ./cmd/fujiblastd -cheader > ../src/proto.h

include mekkogx/toplevel-rules.mk

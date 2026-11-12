# FujiBlast for FujiNet 8-bit computers (MekkoGX).
#
#   make atari                     r2r/atari/fujiblst.atr (48K Atari 400/800, XL/XE)
#   make apple2                    r2r/apple2/fujiblst.po (128K //e, //c, IIgs; double hi-res)
#   make msdos                     r2r/msdos/fujiblst.img (128K PC, XT or PCjr with CGA)
#   make intv                      r2r/intv/fujiblst.rom (Intellivision, the FujiNet cartridge)
#   defoogi make atari             the same in the toolchain container
#   make SERVER=host:9910          server used when the lobby gave none (remembered)
#   make server                    server/fujiblastd and server/blastbot (Go)
#   make proto                     src/proto.h from the server's constants
#
# src/ is platform neutral; src/platform-specific/*.h is what each platform
# implements in src/<platform>/. Art: gfx/*.png (tools/drawart.py draws
# them); tools/mkgfx_<platform>.py converts them into gen/ (gitignored).

PRODUCT = fujiblst
PLATFORMS = atari apple2 msdos

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

# ---- Apple II (128K //e, //c, IIgs), double hi-res -------------------------
# cc65, ProDOS: FUJIBLST.SYSTEM (tools/a2loader) loads FUJIBLST, which reads
# its art from FUJIBLST.GFX into aux RAM. Memory: src/apple2/fujiblst.cfg.
CFLAGS_EXTRA_APPLE2 += -Os -Cl
LDFLAGS_EXTRA_APPLE2 += -C src/apple2/fujiblst.cfg -m build/fujiblst-apple2.map
EXECUTABLE_EXTRA_DEPS_APPLE2 += src/apple2/fujiblst.cfg
ifeq ($(PLATFORM),apple2)
  EXTRA_INCLUDE += gen/apple2
  DISK_BASIC_SYSTEM =
endif

# Where the modules go: the room's screens, test scenes, main, start-up and
# the link below the DHGR pages; the menus and appkeys in an overlay; input
# in the language card beside the blitter.
A2OBJ = build/$(PRODUCT)/apple2/src
$(A2OBJ)/screens.o $(A2OBJ)/test.o $(A2OBJ)/main.o $(A2OBJ)/apple2/plat.o $(A2OBJ)/apple2/nstrans.o: CFLAGS += --code-name LOWCODE --rodata-name LOWDATA
# the menus and the appkeys: an overlay, under other names (src/apple2/overlay.c)
A2OVL = -Dtitle=ov_title -Dname_entry=ov_name_entry -Dtable_list=ov_table_list \
        -Dappkey_load=ov_appkey_load -Dappkey_save_name=ov_appkey_save_name \
        -Dappkey_save_room=ov_appkey_save_room -Dappkey_clear_room=ov_appkey_clear_room
$(A2OBJ)/menus.o $(A2OBJ)/appkey.o: CFLAGS += --code-name OVERLAY --rodata-name OVERLAY $(A2OVL)
$(A2OBJ)/apple2/input.o: CFLAGS += --code-name LC

# FUJIBLST.SYSTEM is our own loader (tools/a2loader): it skips the DHGR
# pages' zeros in the file, so the program can run up to $BE00.
apple2/disk-post:: build/a2loader.bin
	ac -p $(DISK) FUJIBLST.GFX BIN 0x6000 < $(EXECUTABLE).gfx
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

.PHONY: gen-atari gen-apple2 gen-msdos server proto intv FORCE

# ---- Intellivision (the FujiNet cartridge) ----------------------------------
# IntyBASIC, not mekkogx: src/intv/Makefile builds it (the compiler too).
intv:
	$(MAKE) -C src/intv $(if $(SERVER),SERVER=$(SERVER))

r2r/atari/$(PRODUCT): gen-atari
r2r/apple2/$(PRODUCT): gen-apple2
r2r/msdos/$(PRODUCT): gen-msdos

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

gen/apple2/gfxdata.s: tools/mkgfx_apple2.py tools/artlayout.py tools/png.py gfx/dhgr/tiles.png gfx/dhgr/sprites.png gfx/dhgr/logo.png gfx/dhgr/wall.png gfx/dhgr/font3.png gfx/dhgr/font5.png
	@mkdir -p gen/apple2
	$(PYTHON) tools/mkgfx_apple2.py gen/apple2

gen-msdos: gen/config.h gen/msdos/gfxdata.c

gen/msdos/gfxdata.c: tools/mkgfx_cga.py tools/artlayout.py tools/png.py gfx/cga/tiles.png gfx/cga/sprites.png gfx/cga/logo.png gfx/cga/cells.png gfx/namco.bin
	@mkdir -p gen/msdos
	$(PYTHON) tools/mkgfx_cga.py gen/msdos

gen/atari/gfxdata.s: tools/mkgfx_atari.py tools/artlayout.py tools/png.py gfx/tiles.png gfx/glyphs.png gfx/logo.png gfx/sprites.png
	@mkdir -p gen/atari
	$(PYTHON) tools/mkgfx_atari.py gen/atari

server:
	cd server && go build -o fujiblastd ./cmd/fujiblastd && go build -o blastbot ./cmd/blastbot

proto:
	cd server && go run ./cmd/fujiblastd -cheader > ../src/proto.h

include mekkogx/toplevel-rules.mk

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
#   make atari2600                 r2r/atari2600/fujiblst.bin (Atari 2600, the FujiNet cartridge; PAL=1 for PAL)
#   make astrocade                 r2r/astrocade/fujiblst.bin (Bally Astrocade, the FujiNet cartridge)
#   make coleco                    r2r/coleco/fujiblst.col (ColecoVision, the FujiNet cartridge)
#   make atari7800                 r2r/atari7800/fujiblst.a78 (Atari 7800, the FujiNet cartridge; MARIA 160B)
#   make atari5200                 r2r/atari5200/fujiblst.a52 (Atari 5200, the FujiNet cartridge; the 800's screens)
#   make sms                       r2r/sms/fujiblst.sms (Sega Master System, the FujiNet SMS cartridge; Mode 4, 4 bits a pixel)
#   make nes                       r2r/nes/fujiblst.nes (NES, the FujiNet NES cartridge; 2 bits a pixel, a status bar split)
#   make coco                      r2r/coco/fujiblst.dsk (TRS-80 CoCo 1/2 artifact colour and CoCo 3 16 colours, one disk)
#   make h89-cpm                   r2r/h89-cpm/fujiblst.h8d (Heathkit H89, CP/M: FUJIBLST.COM on an H17 disk; the H19's characters)
#   make h89-boot                  r2r/h89-cpm/fujiblst-boot.h8d (the same on a bootable 64K CP/M disk)
#   make h89-hdos                  r2r/h89-hdos/fujiblst.h8d (Heathkit H89, HDOS 2.0: FUJIBLST.ABS on an H17 disk)
#   defoogi make atari             the same in the toolchain container
#   make SERVER=host:9910          server used when the lobby gave none (remembered)
#   make server                    server/fujiblastd and server/blastbot (Go)
#   make proto                     src/proto.h from the server's constants
#
# src/ is platform neutral; src/platform-specific/*.h is what each platform
# implements in src/<platform>/. Art: gfx/*.png (tools/drawart.py draws
# them); tools/mkgfx_<platform>.py converts them into gen/ (gitignored).

PRODUCT = fujiblst
PLATFORMS = atari apple2 msdos adam msxrom atari7800 atari5200 nes sms coco h89-cpm h89-hdos

# the TMS9918A code (src/tms9918) is the Adam's and the MSX's; src/h89 and
# gen/h89 are both H89 systems'
PLATFORM_COMBOS = adam+=tms9918 msxrom+=tms9918,msx h89-cpm+=h89 h89-hdos+=h89

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

# ---- TRS-80 Color Computer: CoCo 1/2 (64K, PMODE 4 artifact colour) --------
# CMOC, from M.U.L.E.'s CoCo client. 64K, all-RAM once running
# (src/coco/bank.c). CMOC has no section pragma: tools/coco/lwasm-sect moves
# a module's code and constants to the section named by FB_SECT, and
# tools/coco/lwlink-fb.py links with src/coco/fujiblst.link and splits the
# result into fujiblst.bin (LOADM'd) and fujiblhi.bin (the high core and the
# art, loaded by bank.c). AUTOEXEC.BAS picks the CoCo 3 build on a CoCo 3.
COCO_TOOLS = $(CURDIR)/tools/coco
# -O1: CMOC 0.1.100's -O2 drops index reloads (a[i]; b[i]; read b[a[i]])
CFLAGS_EXTRA_COCO += -O1 -funsigned-char -fomit-frame-pointer -Wno-const \
                     --lwasm=$(COCO_TOOLS)/lwasm-sect
ASFLAGS_EXTRA_COCO += --lwasm=$(COCO_TOOLS)/lwasm-sect
LDFLAGS_EXTRA_COCO += --lwlink=$(COCO_TOOLS)/lwlink-fb.py --initial-s=8000
CFLAGS_EXTRA_COCO += -DKEY_END_NAME='"ENTER"' -DQUIT_HINT='"BREAK: LOBBY"'
# (32 columns where the Atari has 40, a BIG row shows 16 of its 20: the longer texts are shortened;
# the CoCo 3's are the Atari's)
ifeq ($(PLATFORM),coco)
  CFLAGS_EXTRA_COCO += -DHOST_COL=12 -DHINT_LR='"(L/R)"' -DTAGLINE='"BOMBS AND BLASTS FOR 2-4"' \
                       -DNAME_HELP='"LETTERS, DIGITS, THEN ENTER"' -DNAME_PROMPT='"YOUR NAME?"' -DMSG_NO='"SERVER SAID NO"'
endif
EXTRA_INCLUDE_COCO += src/coco/include src/coco gen/coco
# the test scenes (src/test.c, tools/cocosim) take 1.8K the low core hasn't: make coco TEST=1
ifneq ($(TEST),1)
  CFLAGS_EXTRA_COCO += -DPLAT_NO_TEST
endif
EXECUTABLE_EXTRA_DEPS_COCO += src/coco/fujiblst.link $(COCO_TOOLS)/lwlink-fb.py
DECB ?= $(firstword $(wildcard $(HOME)/Workspace/toolshed/build/unix/decb/decb) decb)
ifeq ($(PLATFORM),coco)
  SRC_DIRS = src src/coco src/coco2 gen/coco
  DISK_TOOL := $(DECB)
  # the disk carries the CoCo 3 build too (the second pass, run first)
  DISK_EXTRA_DEPS += $(foreach s,.bin hi.bin a.bin b.bin,r2r/coco3/$(PRODUCT)$(s))
endif
ifneq ($(TEST_SCENE),)
  CFLAGS_EXTRA_COCO += -DTEST_SCENE=$(TEST_SCENE) -DTEST_ARG=$(or $(TEST_ARG),0)
endif

COOBJ = build/$(PRODUCT)/coco
# The low core (BASIC loads it: $2600-$7BFF) holds the program; the graphics
# engine and its art, and the arena (play.c), go in the high core, which
# bank_init reads; the bombers' art in the room above.
$(COOBJ)/src/coco2/gfx.o $(COOBJ)/src/coco2/gfxa.o $(COOBJ)/gen/coco/gfxdata.o $(COOBJ)/src/play.o: export FB_SECT = hi
$(COOBJ)/gen/coco/gfxspr.o:                     export FB_SECT = art

# The loader: HDB-DOS runs AUTOEXEC.BAS, which LOADMs the low core. BASIC
# programs start at $2601, where FUJIBLST.BIN goes: PCLEAR1 moves this one
# down out of the way first. A CoCo 3 (its IRQ vector is $FEF7, so $FFF8
# reads $FE) gets the CoCo 3 build, on the same disk.
coco/disk-post::
	printf '10 PCLEAR1:IF PEEK(&HFFF8)=&HFE THEN LOADM"FUJIBL3":EXEC\n20 LOADM"FUJIBLST":EXEC\n' > build/autoexec.bas
	$(DECB) copy -0 -a -l -r build/autoexec.bas $(DISK),AUTOEXEC.BAS
	$(DECB) copy -b -2 -r r2r/coco/$(PRODUCT)hi.bin $(DISK),FUJIBLHI.BIN
	$(call coco3-files,$(DISK))

# ---- CoCo 3 (GIME 320x192, 16 colours) ---------------------------------------
# The CoCo sources again with -DCOCO3 (gfx3.c for gfx.c). tools/coco/coco3.mk
# is platforms/coco.mk renamed, for separate build/ and r2r/ directories.
# `make coco` also puts FUJIBL3*.BIN on the CoCo disk; `make coco3` makes a
# CoCo 3-only disk.
ifeq ($(PLATFORM),coco3)
  SRC_DIRS = src src/coco src/coco3 gen/coco3
  # fujinet-lib's coco build, found the way the coco pass finds it
  # (common.mk would look for a coco3 one)
  FNLIB_COCO3 := $(FUJINET_LIB)
  FUJINET_LIB := __UNDEFINED__
  define _nl


  endef
  $(eval $(subst |,$(_nl),$(shell PLATFORM=coco CACHE_DIR=_cache $(CURDIR)/mekkogx/fnlib.py $(FNLIB_COCO3) | tr '\n' '|')))
  DISK_TOOL := $(DECB)
  DISK_EXECUTABLES =
  DISK_EXTRA_DEPS += $(EXECUTABLE)
  export FB_LINK = coco3
endif
CFLAGS_EXTRA_COCO3 += $(CFLAGS_EXTRA_COCO) -DCOCO3 -DUSING_FUJINET_LIB=1
ASFLAGS_EXTRA_COCO3 += $(ASFLAGS_EXTRA_COCO)
LDFLAGS_EXTRA_COCO3 += $(LDFLAGS_EXTRA_COCO)
EXTRA_INCLUDE_COCO3 += src/coco/include src/coco gen/coco3
EXECUTABLE_EXTRA_DEPS_COCO3 += $(EXECUTABLE_EXTRA_DEPS_COCO) src/coco/fujiblst3.link

C3OBJ = build/$(PRODUCT)/coco3
$(C3OBJ)/src/coco3/gfx3.o $(C3OBJ)/src/coco3/gfx3a.o $(C3OBJ)/gen/coco3/gfxdata.o $(C3OBJ)/src/play.o: export FB_SECT = hi
# block 6 (sprites, font) and block 7 (tiles) of the MMU, loaded by bank.c
$(C3OBJ)/gen/coco3/gfxspr.o:                    export FB_SECT = art
$(C3OBJ)/gen/coco3/gfxtile.o:                   export FB_SECT = art2

# The CoCo 3 files, on its own disk and on the CoCo's.
define coco3-files
	$(DECB) copy -b -2 -r r2r/coco3/$(PRODUCT).bin $(1),FUJIBL3.BIN
	$(DECB) copy -b -2 -r r2r/coco3/$(PRODUCT)hi.bin $(1),FUJIBL3H.BIN
	$(DECB) copy -b -2 -r r2r/coco3/$(PRODUCT)a.bin $(1),FUJIBL3A.BIN
	$(DECB) copy -b -2 -r r2r/coco3/$(PRODUCT)b.bin $(1),FUJIBL3B.BIN
endef
coco3/disk-post::
	printf '10 PCLEAR1:LOADM"FUJIBL3":EXEC\n' > build/autoexec3.bas
	$(DECB) copy -0 -a -l -r build/autoexec3.bas $(DISK),AUTOEXEC.BAS
	$(call coco3-files,$(DISK))

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

.PHONY: gen-atari5200 gen-atari7800 gen-atari gen-apple2 gen-msdos gen-adam gen-msxrom server proto intv o2 o2-rig arcadia arcadia-rig atari2600 atari2600-rig astrocade astrocade-rig coleco coleco-rig FORCE

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

# 6502 assembly (Macroassembler AS), not cc65: tools/a26/build.sh builds the
# 32K FujiNet-cart image (fifteen 2K banks + the fixed half) and runs its
# checks. atari2600-rig: a copy whose default server is the rig's.
atari2600:
	$(if $(SERVER),DEFSRV=$(SERVER)) tools/a26/build.sh
	mkdir -p r2r/atari2600 && cp build/a26$(if $(PAL),-pal)/fujiblst.bin r2r/atari2600/fujiblst.bin

atari2600-rig:
	DEFSRV=127.0.0.1:9910 tools/a26/build.sh

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

# ---- Sega Master System (the FujiNet SMS cartridge) ----------------------------
# z88dk +sms with zsdcc, from defoogi unless installed (tools/z80/z88dk). A
# Sega-mapper image (src/sms/blast.asm, tools/sms/mkrom.py): the program in
# banks 0-1 (0000h-7FD7h), the art in two banks that take turns in slot 2
# (8000h-AFFFh), the cartridge's mailbox at B000h over them. The screen is
# Mode 4, 4 bits a pixel, 32 x 24 cells of the Adam's layout (src/sms/gfx.c;
# tools/drawart_sms.py has the art and the rules, tools/mkgfx_sms.py turns
# it into gen/sms); the link to the server is the mailbox driven by hand
# (src/sms/nstrans.c). fujinet-lib is fujinet-lib-experimental's add-sms
# branch (FNLIB_SMS, a worktree) built with zsdcc (tools/z80/mkfnlib.sh).
# TEST_SCENE as everywhere.
FNLIB_SMS ?= $(HOME)/Workspace/fnlib-sms
ifeq ($(PLATFORM),sms)
  FUJINET_LIB := __UNDEFINED__
  CC_Z88DK = $(CURDIR)/tools/z80/z88dk zcc
  AS_Z88DK = $(CURDIR)/tools/z80/z88dk z80asm
  LD_Z88DK = $(CURDIR)/tools/z80/z88dk zcc
  EXTRA_INCLUDE += build/fnlib-sms/include gen/sms src/tms9918
  LIBS += -Lbuild/fnlib-sms -lfujinet-sms
endif
CFLAGS_EXTRA_SMS += -compiler=sdcc -SO3 -DPLAT_NO_TEST -DPLAT_NAME_WHEEL -DKEY_END_NAME='"\"PAUSE\""' \
                    -DQUIT_HINT='"\"2+PAUSE: QUIT\""' -DTAGLINE='"\"BOMBS AND BLASTS FOR 2-4\""' \
                    -DHINT_LR='"\"(L/R)\""' -DNAME_HELP='"\"UP/DOWN: LETTER  1: ADD  >: DONE\""'
LDFLAGS_EXTRA_SMS += -compiler=sdcc -m -pragma-define:CRT_ENABLE_STDIO=0 -pragma-define:CLIB_FOPEN_MAX=0 \
                     -pragma-define:CRT_INITIALIZE_BSS=1 -pragma-redirect:fputc_cons=l_ret
EXECUTABLE_EXTRA_DEPS_SMS += build/fnlib-sms/fujinet-sms.lib src/sms/blast.asm
ifneq ($(TEST_SCENE),)
  CFLAGS_EXTRA_SMS += -DTEST_SCENE=$(TEST_SCENE) -DTEST_ARG=$(or $(TEST_ARG),0)
endif
SMOBJ = build/$(PRODUCT)/sms
$(SMOBJ)/gen/sms/gfxart1.o: CFLAGS += --constseg=ART1
$(SMOBJ)/gen/sms/gfxart2.o: CFLAGS += --constseg=ART2

sms/r2r-post::
	python3 tools/sms/mkrom.py r2r/sms $(PRODUCT)

build/fnlib-sms/fujinet-sms.lib: tools/z80/mkfnlib.sh $(wildcard $(FNLIB_SMS)/common/*.c $(FNLIB_SMS)/bus/sms/* $(FNLIB_SMS)/include/*.h)
	tools/z80/mkfnlib.sh $(FNLIB_SMS) build/fnlib-sms sms

.PHONY: gen-sms
gen-sms: gen/config.h gen/sms/gfxdata.c build/fnlib-sms/fujinet-sms.lib

gen/sms/gfxdata.c gen/sms/gfxdata.h gen/sms/gfxcodes.h gen/sms/gfxart1.c gen/sms/gfxart2.c&: tools/mkgfx_sms.py tools/drawart_sms.py tools/drawart_nes.py tools/artlayout.py tools/png.py gfx/namco.bin gfx/sms/tiles.png gfx/sms/sprites.png gfx/sms/bigwin.png gfx/sms/logo.png gfx/sms/cells.png gfx/sms/trophy.png
	@mkdir -p gen/sms
	$(PYTHON) tools/mkgfx_sms.py gen/sms

# ---- Heathkit H89: CP/M 2.2.04 or HDOS 2.0, the H19 terminal ----------------
# z88dk with zsdcc, from defoogi unless installed (tools/z80/z88dk).
# FUJIBLST.COM (CP/M) or FUJIBLST.ABS (HDOS) on a 100K H17 disk (hz89, the
# format Heath's CP/M gives a hard-sectored disk); h89-boot makes a bootable
# 64K CP/M disk with the game on it (the lobby's). The screen is the H19's
# characters, sent by difference over its 9600-baud line (src/h89/h19.c,
# gfx.c; art gfx/h89/art.txt, tools/mkgfx_h89.py). The link to the server
# is the H89-ESP32 card's 82C55, FujiBus by hand, never waited on
# (src/h89/fbport.c, nstrans.c); fujinet-lib (FNLIB_H89, with bus/h89) opens
# and closes, built with zsdcc (tools/z80/mkfnlib.sh), its fuji_bus_call
# replaced by fbport.c's. The console, clock and keyboard layers are
# M.U.L.E.'s client's (src/h89/h89io.asm, input.c). No overlays: the
# variables must end under D806H (the 62K CP/M FNDISK needs; a stack too)
# and D090H (HDOS with FN: loaded): tools/h89/h89check.py says how close.
#   HDOS: the program at H89_HDOS_ORG, its stack under it from 2280H (the
#   startup pops a word it never pushed: SP two under); the variables after
#   the image, out of the file (src/h89/tpa.asm checks and clears them).
# H89_RIG_KEYS=1: ` is ESC (for the rig). H89_PROF=1: the clock interrupt
# keeps the PCs it interrupted (h89sim pcprof). TEST_SCENE as everywhere.
H89_HDOS_ORG = 0x2480
FNLIB_H89 ?= $(HOME)/Workspace/fujinet-lib
Z80_TOOLS = $(CURDIR)/tools/z80
ifneq ($(filter h89-cpm h89-hdos,$(PLATFORM)),)
  FUJINET_LIB := __UNDEFINED__
  CC_Z88DK = $(CURDIR)/tools/z80/z88dk zcc
  AS_Z88DK = $(CURDIR)/tools/z80/z88dk z80asm
  LD_Z88DK = $(CURDIR)/tools/z80/z88dk zcc
  EXTRA_INCLUDE += src/h89 gen/h89 build/fnlib-h89/include
  LIBS += -Lbuild/fnlib-h89 -lfujinet-h89
endif
H89_CFLAGS = -compiler=sdcc -SO3 -D__H89__ -DMOVE_EVERY=6 -DPLAT_SEATS_WHEN_DRAWN -DTOWARD_STEP=UNIT -DPLAT_MAP_NEXT -DPLAT_COMMIT_SPAN \
             -DQUIT_HINT='"\"ESC: QUIT\""' \
             -DTAGLINE='"\"BOMBS AND BLASTS FOR 2-4\""' -DNAME_HELP='"\"LETTERS, DIGITS, THEN RETURN\""'
ifeq ($(H89_RIG_KEYS),1)
  H89_CFLAGS += -DH89_RIG_KEYS
endif
ifneq ($(TEST_SCENE),)
  H89_CFLAGS += -DTEST_SCENE=$(TEST_SCENE) -DTEST_ARG=$(or $(TEST_ARG),0)
endif
H89_ASFLAGS = $(if $(filter 1,$(H89_PROF)),-DH89_PROF=1)
H89_LDFLAGS = -compiler=sdcc -m -pragma-define:CRT_ENABLE_STDIO=0 -pragma-define:CLIB_FOPEN_MAX=0 \
              -pragma-define:CRT_ENABLE_COMMANDLINE=0
CFLAGS_EXTRA_H89_CPM   += $(H89_CFLAGS)
ASFLAGS_EXTRA_H89_CPM  += $(H89_ASFLAGS)
LDFLAGS_EXTRA_H89_CPM  += $(H89_LDFLAGS) -pragma-define:CRT_INITIALIZE_BSS=1 -pragma-define:noprotectmsdos=1
CFLAGS_EXTRA_H89_HDOS  += $(H89_CFLAGS)
ASFLAGS_EXTRA_H89_HDOS += $(H89_ASFLAGS) -DH89_HDOS
LDFLAGS_EXTRA_H89_HDOS += $(H89_LDFLAGS) -pragma-define:CRT_ORG_CODE=$(H89_HDOS_ORG) \
                          -pragma-define:REGISTER_SP=$(shell printf '0x%X' $$(($(H89_HDOS_ORG) - 2))) \
                          -pragma-define:CLIB_EXIT_STACK_SIZE=0 \
                          -pragma-define:CRT_INITIALIZE_BSS=0 -pragma-define:CRT_TRIM_BSS=1
EXECUTABLE_EXTRA_DEPS_H89_CPM  += build/fnlib-h89/fujinet-h89.lib
EXECUTABLE_EXTRA_DEPS_H89_HDOS += build/fnlib-h89/fujinet-h89.lib

# 0D806H: the BDOS of a 62K CP/M, the one FNDISK needs (a 64K one has E006H)
h89-cpm/r2r-post::
	@mkdir -p $(CACHE_PLATFORM)/disk
	python3 tools/h89/h89check.py $(R2R_PD)/$(PRODUCT) --os cpm --top 0xD806
	rm -f $(R2R_PD)/FUJIBLST.COM            # (-create-app's, the variables still on: not the one)
	cp $(R2R_PD)/$(PRODUCT).com $(CACHE_PLATFORM)/disk/FUJIBLST.COM
	cd $(CACHE_PLATFORM)/disk && rm -f disk && $(Z80_TOOLS)/z88dk z88dk-appmake +cpmdisk -f hz89 \
	  --container raw --no-com-file -b FUJIBLST.COM -o disk
	python3 -I tools/h89/cpm-dir-fixup.py $(CACHE_PLATFORM)/disk/disk
	mv $(CACHE_PLATFORM)/disk/disk $(R2R_PD)/$(PRODUCT).h8d

# 0D090H: what HDOS 2.0 leaves with FN: loaded (S.SYSM - S.OMAX; CB0DH with LP:
# too: src/h89/tpa.asm says so on the machine)
h89-hdos/r2r-post::
	@mkdir -p $(CACHE_PLATFORM)/disk
	python3 tools/h89/h89check.py $(R2R_PD)/$(PRODUCT) --os hdos --top 0xD090
	cp $(R2R_PD)/$(PRODUCT).abs $(CACHE_PLATFORM)/disk/disk.bin
	cd $(CACHE_PLATFORM)/disk && rm -f disk.h8d && $(Z80_TOOLS)/z88dk z88dk-appmake +hdos --dumb \
	  -b disk.bin -o FUJIBLST.ABS
	python3 -I tools/h89/hdos-dir-fixup.py $(CACHE_PLATFORM)/disk/disk.h8d FUJIBLST.ABS $(CACHE_PLATFORM)/disk/disk.bin
	mv $(CACHE_PLATFORM)/disk/disk.h8d $(R2R_PD)/$(PRODUCT).h8d

# A disk that boots: fujinet-go-h89-desktop's 64K CP/M system disk with the
# development tools taken off and FUJIBLST.COM put on, by CP/M itself in
# that project's headless H89 (tools/h89/mkboot.sh; it needs the rig, built).
.PHONY: h89-boot gen-h89
h89-boot: r2r/h89-cpm/$(PRODUCT)
	tools/h89/mkboot.sh r2r/h89-cpm/$(PRODUCT).com r2r/h89-cpm/$(PRODUCT)-boot.h8d

build/fnlib-h89/fujinet-h89.lib: tools/z80/mkfnlib.sh $(wildcard $(FNLIB_H89)/common/*.c $(FNLIB_H89)/bus/h89/* $(FNLIB_H89)/include/*.h)
	tools/z80/mkfnlib.sh $(FNLIB_H89) build/fnlib-h89 h89

gen-h89: gen/config.h gen/h89/gfxdata.c build/fnlib-h89/fujinet-h89.lib

gen/h89/gfxdata.c gen/h89/gfxcodes.h&: tools/mkgfx_h89.py gfx/h89/art.txt
	@mkdir -p gen/h89
	$(PYTHON) tools/mkgfx_h89.py gen/h89

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

# ---- Atari 5200 (the FujiNet 5200 cartridge) ----------------------------------
# cc65, a flat 32K cart image (src/atari5200/fujiblst-a52.cfg) that the 5200
# CONFIG boots. The screens are the Atari 800's (src/atari, assembled for the
# 5200 by hw.inc/atari.h): the same display list, charsets and players, the
# charsets copied from the cart into RAM by the crt0 (src/atari5200/crt0.s).
# fujinet-lib is fujinet-lib-experimental's atari5200 target
# (tools/atari5200/mkfnlib.sh); the link to the server is the cartridge's
# mailbox, driven by hand (src/atari5200/nstrans.c). TEST_SCENE as everywhere.
FNLIB_5200 ?= $(HOME)/Workspace/fnlib-5200
ifeq ($(PLATFORM),atari5200)
  FUJINET_LIB := __UNDEFINED__
  LIBS += build/fnlib-a52/fujinet-a52.lib
  EXTRA_INCLUDE += gen/atari5200 build/fnlib-a52/include
endif
CFLAGS_EXTRA_ATARI5200 += -Os -Cl -DPLAT_NO_TEST -DPLAT_NAME_WHEEL -DKEY_END_NAME='"START"' -DQUIT_HINT='"*: LOBBY"' \
                          -DTAGLINE='"BOMBS AND BLASTS FOR 2-4"' -DHINT_LR='"(L/R)"' \
                          -DNAME_HELP='"UP/DOWN: LETTER  FIRE: ADD  >: DONE"'
CFLAGS_EXTRA_ATARI5200 += $(DBG)
LDFLAGS_EXTRA_ATARI5200 += -C src/atari5200/fujiblst-a52.cfg -m build/fujiblst-a52.map
EXECUTABLE_EXTRA_DEPS_ATARI5200 += src/atari5200/fujiblst-a52.cfg build/fnlib-a52/fujinet-a52.lib gen/atari5200/gfxdata.s
ifneq ($(TEST_SCENE),)
  CFLAGS_EXTRA_ATARI5200 += -DTEST_SCENE=$(TEST_SCENE) -DTEST_ARG=$(or $(TEST_ARG),0)
endif

# the checks, the .a52 (r2r/atari5200/fujiblst.a52)
atari5200/executable-post::
	python3 tools/atari5200/mkrom.py build/fujiblst-a52.map $(EXECUTABLE) $(EXECUTABLE:.bin=.a52)

build/fnlib-a52/fujinet-a52.lib: tools/atari5200/mkfnlib.sh $(wildcard $(FNLIB_5200)/common/*.c $(FNLIB_5200)/bus/atari5200/* $(FNLIB_5200)/include/*.h)
	tools/atari5200/mkfnlib.sh $(FNLIB_5200) build/fnlib-a52

gen-atari5200: gen/config.h gen/atari5200/gfxdata.s build/fnlib-a52/fujinet-a52.lib

# the same art as the 800's
gen/atari5200/gfxdata.s: tools/mkgfx_atari.py tools/artlayout.py tools/png.py gfx/tiles.png gfx/glyphs.png gfx/logo.png gfx/sprites.png
	@mkdir -p gen/atari5200
	$(PYTHON) tools/mkgfx_atari.py gen/atari5200

# ---- NES (the FujiNet NES cartridge) -------------------------------------------
# cc65, an MMC3 image (.nes) the NES CONFIG boots: 128K PRG, 16K CHR-RAM, 8K
# WRAM (src/nes/fujiblst-nes.cfg). The screen is a name table of 8x8 cells at
# 2 bits a pixel, a 16x16 tile one attribute block (tools/drawart_nes.py has
# the art and the rules, tools/mkgfx_nes.py turns it into gen/nes). The
# backdrop is black for the status bar and green below it: the MMC3's
# scanline interrupt writes it again (src/nes/crt0.s). fujinet-lib is
# fujinet-lib-experimental's nes target (tools/nes/mkfnlib.sh); the link to
# the server is the cartridge's mailbox, driven by hand (src/nes/nstrans.c).
# TEST_SCENE as everywhere.
FNLIB_NES ?= $(HOME)/Workspace/fujinet-lib-experimental
ifeq ($(PLATFORM),nes)
  FUJINET_LIB := __UNDEFINED__
  LIBS += build/fnlib-nes/fujinet-nes.lib
  EXTRA_INCLUDE += gen/nes src/tms9918 build/fnlib-nes/include
endif
CFLAGS_EXTRA_NES += -Os -Cl -DPLAT_NO_TEST -DPLAT_NAME_WHEEL -DKEY_END_NAME='"START"' -DQUIT_HINT='"SELECT: QUIT"' \
                    -DTAGLINE='"BOMBS AND BLASTS FOR 2-4"' -DHINT_LR='"(L/R)"' \
                    -DNAME_HELP='"UP/DOWN: LETTER  A: ADD  >: DONE"'
CFLAGS_EXTRA_NES += $(DBG)
LDFLAGS_EXTRA_NES += -C src/nes/fujiblst-nes.cfg -m build/fujiblst-nes.map
EXECUTABLE_EXTRA_DEPS_NES += src/nes/fujiblst-nes.cfg build/fnlib-nes/fujinet-nes.lib gen/nes/gfxdata.c
ifneq ($(TEST_SCENE),)
  CFLAGS_EXTRA_NES += -DTEST_SCENE=$(TEST_SCENE) -DTEST_ARG=$(or $(TEST_ARG),0)
endif

# R7's bank ($A000, never switched out) takes the menus, the screens, the
# link's glue and the sound: the fixed bank has the engine, the arena and the
# interrupts
NESOBJ = build/$(PRODUCT)/nes
$(NESOBJ)/src/menus.o $(NESOBJ)/src/screens.o $(NESOBJ)/src/appkey.o $(NESOBJ)/src/net.o $(NESOBJ)/src/nes/sound.o: CFLAGS += --code-name CODE2 --rodata-name CODE2

# main and the test scenes (TEST_SCENE builds) go after the art in bank 0, which stays at $8000
$(NESOBJ)/src/test.o $(NESOBJ)/src/main.o: CFLAGS += --code-name CODE0 --rodata-name CODE0

# the claim, the vectors and no read-modify-write of the mailbox
nes/executable-post::
	python3 tools/nes/checkrom.py build/fujiblst-nes.map $(EXECUTABLE)

build/fnlib-nes/fujinet-nes.lib: tools/nes/mkfnlib.sh $(wildcard $(FNLIB_NES)/common/*.c $(FNLIB_NES)/bus/nes/* $(FNLIB_NES)/include/*.h)
	tools/nes/mkfnlib.sh $(FNLIB_NES) build/fnlib-nes

.PHONY: gen-nes
gen-nes: gen/config.h gen/nes/gfxdata.c build/fnlib-nes/fujinet-nes.lib

gen/nes/gfxdata.c gen/nes/gfxdata.h gen/nes/gfxcodes.h&: tools/mkgfx_nes.py tools/drawart_nes.py tools/artlayout.py tools/png.py gfx/namco.bin gfx/nes/tiles.png gfx/nes/sprites.png gfx/nes/bigwin.png gfx/nes/logo.png gfx/nes/cells.png
	@mkdir -p gen/nes
	$(PYTHON) tools/mkgfx_nes.py gen/nes

r2r/atari/$(PRODUCT): gen-atari
r2r/nes/$(PRODUCT): gen-nes
r2r/sms/$(PRODUCT): gen-sms
r2r/h89-cpm/$(PRODUCT): gen-h89
r2r/h89-hdos/$(PRODUCT): gen-h89
# CoCo: the CoCo 1/2 disk takes the CoCo 3 build too (second pass, coco3.mk)
r2r/coco/$(PRODUCT): gen-coco r2r/coco3/$(PRODUCT)
r2r/coco3/$(PRODUCT): gen-coco3 FORCE
	$(MAKE) -f tools/coco/coco3.mk r2r
.PHONY: coco3 gen-coco gen-coco3
coco3: r2r/coco3/$(PRODUCT)
gen-coco: gen/config.h gen/coco/gfxdata.c
gen-coco3: gen/config.h gen/coco3/gfxdata.c

gen/coco/gfxdata.c gen/coco/gfxspr.c&: tools/mkgfx_coco.py tools/artlayout.py tools/png.py gfx/namco.bin
	@mkdir -p gen/coco
	$(PYTHON) tools/mkgfx_coco.py gen/coco

gen/coco3/gfxdata.c gen/coco3/gfxspr.c gen/coco3/gfxtile.c&: tools/mkgfx_coco3.py tools/artlayout.py tools/png.py gfx/namco.bin
	@mkdir -p gen/coco3
	$(PYTHON) tools/mkgfx_coco3.py gen/coco3
r2r/atari5200/$(PRODUCT): gen-atari5200
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

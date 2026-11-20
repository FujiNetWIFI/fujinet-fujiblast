# Independent target: the MekkoGX legacy library resolver is never invoked.
CC = $(or $(AMIGA_CC),m68k-amigaos-gcc)
PYTHON ?= python3
LIBDIR = _cache/fujinet-nio-lib/amiga-0.2.0
ifneq ($(RESPONSIVE),)
override LIVE = 1
override PERF = 1
endif
OUT = build/amiga$(if $(AUDIO),-audio)$(if $(TEST_SCENE),-scene$(TEST_SCENE))$(if $(ACCEPT),-accept)$(if $(LIVE),-live)$(if $(RESPONSIVE),-responsive)$(if $(PERF),-perf)
PROGRAM = $(OUT)/fujiblast-nio
CPPFLAGS = -Isrc -Isrc/platform-specific -Isrc/amiga -Igen/amiga -I$(LIBDIR)/include -DAMIGA_ASYNC -DPLAT_MUSIC -D__AMIGA__ -D__fastcall__= -DQUIT_HINT='"ESC: QUIT"' -DKEY_END_NAME='"RETURN"'
CFLAGS = -m68000 -mcrt=nix13 -O2 -Wall -Wextra -Wno-unused-parameter -MMD -MP
LDFLAGS = -m68000 -mcrt=nix13 -Wl,-Map,$(OUT)/fujiblast.map
ifneq ($(TEST_SCENE),)
CPPFLAGS += -DTEST_SCENE=$(TEST_SCENE) -DTEST_ARG=1
endif
ifneq ($(AUDIO),)
CPPFLAGS += -DAMIGA_AUDIO
endif
ifneq ($(ACCEPT),)
CPPFLAGS += -DAMIGA_ACCEPT
endif
ifneq ($(LIVE),)
CPPFLAGS += -DAMIGA_LIVE
endif
ifneq ($(RESPONSIVE),)
CPPFLAGS += -DAMIGA_RESPONSIVE
endif
ifneq ($(PERF),)
CPPFLAGS += -DAMIGA_PERF
endif
SRC = $(wildcard src/*.c src/amiga/*.c src/nio/*.c) gen/amiga/gfxdata.c gen/amiga/palette.c gen/amiga/music.c
OBJ = $(patsubst %.c,$(OUT)/%.o,$(SRC)) $(OUT)/src/amiga/worker_entry.o
.PHONY: all config test clean
all: $(PROGRAM)
ifeq ($(strip $(ACCEPT)$(TEST_SCENE)$(LIVE)$(PERF)$(AUDIO)),)
	@mkdir -p r2r/amiga
	cp $(PROGRAM) r2r/amiga/fujiblast-nio
endif
$(LIBDIR)/.verified $(LIBDIR)/fujinet-nio-amiga.a $(LIBDIR)/include/fujinet-nio.h &: tools/amiga/fetch_nio.py
	$(PYTHON) $<
gen/amiga/gfxdata.c gen/amiga/gfxcodes.h gen/amiga/palette.c &: tools/mkgfx_amiga.py tools/mkgfx_vga.py tools/vgapal.py tools/png.py $(wildcard gfx/vga/*.png) gfx/namco.bin
	$(PYTHON) tools/mkgfx_amiga.py gen/amiga
gen/amiga/music.c: tools/mkmus_amiga.py tools/mus.py music/play.txt
	$(PYTHON) tools/mkmus_amiga.py music/play.txt $@
.PHONY: FORCE
config: FORCE
	@mkdir -p gen/amiga
	@$(PYTHON) tools/amiga/config.py '$(SERVER)' '$(LOBBY_SERVER)'
$(OUT)/src/appkey.o $(OUT)/src/amiga/setup.o: config
$(OBJ): tools/amiga/amiga.mk $(LIBDIR)/.verified $(LIBDIR)/include/fujinet-nio.h gen/amiga/gfxcodes.h
$(OUT)/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
$(OUT)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(PROGRAM): $(OBJ) $(LIBDIR)/fujinet-nio-amiga.a
	$(CC) $(LDFLAGS) $(OBJ) $(LIBDIR)/fujinet-nio-amiga.a -lamiga -o $@
test: $(LIBDIR)/.verified gen/amiga/music.c
	$(PYTHON) -m unittest discover -s tests/amiga -p 'test_*.py' -v
-include $(OBJ:.o=.d)

# a2rig: AppleWin's core and FujiNet, from a built fujinet-go-apple2-desktop
# (its build/core static libraries, as its boot_smoke test links them).
A2D ?= $(HOME)/Workspace/fujinet-go-apple2-desktop
B = $(A2D)/build/core
LIBS = $(B)/libapple2session.a $(B)/applewin-libretro/applewin_libretro.a \
       $(B)/applewin-common2/libcommon2.a $(B)/applewin-source/libappleii.a \
       $(B)/applewin-libyaml/libyaml.a $(B)/applewin-minizip/libminizip.a \
       $(B)/applewin-resource/libapple2roms.a $(B)/applewin-libwindows/libwindows.a

a2rig: a2rig.c $(LIBS)
	cc -O2 -Wall -I$(A2D)/core/applewin -c a2rig.c -o a2rig.o
	c++ -o $@ a2rig.o $(LIBS) -lz -lSDL3 -lpthread -ldl

clean:
	rm -f a2rig a2rig.o

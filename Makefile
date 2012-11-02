PROFILE=0

TARGET	= tgemu

OBJS	= $(RESOBJS) game.o text.o file.o \
  src/fileio.o \
  src/pce.o \
  src/psg.o \
  src/render.o \
  src/system.o \
  src/unzip.o \
  src/vce.o \
  src/vdc.o \
  src/cpu/h6280.o \

LIBS	= -lgame -lc -lgcc -Lzlib-1.2.7 -lz

include ../../libgame/libgame.mk
CFLAGS += -Isrc -Isrc/cpu -Izlib-1.2.7 -fno-strict-aliasing -DLSB_FIRST -DFAST_MEM -O3 -funroll-loops -fomit-frame-pointer -DNATIVE_KEYS

ifeq ($(PROFILE),1)
CFLAGS += -finstrument-functions -DPROFILE
OBJS += profile.o
profile.o: profile.c
	$(TOOLCHAIN)gcc -O2 -c $< -o $@
endif

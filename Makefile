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

LIBS	= -lgame -lz -lc -lgcc

include ../../libgame/libgame.mk
CFLAGS += -Isrc -Isrc/cpu -DLSB_FIRST -DFAST_MEM -O3 -funroll-loops -fomit-frame-pointer

ifeq ($(PROFILE),1)
CFLAGS += -finstrument-functions -DPROFILE
OBJS += profile.o
profile.o: profile.c
	$(TOOLCHAIN)gcc -O2 -c $< -o $@
endif

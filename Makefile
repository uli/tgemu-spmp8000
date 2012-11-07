LIBSPMP8K = ../..
PROFILE=0

TARGET	= tgemu

OBJS	= game.o text.o file.o \
  src/fileio.o \
  src/pce.o \
  src/psg.o \
  src/render.o \
  src/system.o \
  src/unzip.o \
  src/vce.o \
  src/vdc.o \
  src/cpu/h6280.o \

ifeq ($(PROFILE),1)
OBJS += profile.o
endif

LIBS	= -lgame -lz -lc -lgcc

include $(LIBSPMP8K)/main.cfg
include $(LIBGAME)/libgame.mk

CFLAGS += -Isrc -Isrc/cpu -DLSB_FIRST -DFAST_MEM -O3 -funroll-loops -fomit-frame-pointer

ifeq ($(PROFILE),1)
CFLAGS += -finstrument-functions -DPROFILE
profile.o: profile.c
	$(CC) -O2 -c $< -o $@
endif

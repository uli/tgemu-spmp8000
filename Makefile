
#ifdef $(NEWLIB)
#undef $(NEWLIB)
#endif

TARGET	= tgemu

TOOLS_DIR = ../../bin

include ../libgame/libgame.cfg

# resources
RESOBJS	= font.o

OBJS	= $(RESOBJS) game.o \
  src/fileio.o \
  src/pce.o \
  src/psg.o \
  src/render.o \
  src/system.o \
  src/unzip.o \
  src/vce.o \
  src/vdc.o \
  src/cpu/h6280.o \

LIBS	= -lgame -lemu -lfreetype -lc -lgcc -Lzlib-1.2.7 -lz -Lfreetype-flat

include ../libgame/libgame.mk
CFLAGS += -Isrc -Isrc/cpu -Izlib-1.2.7 -Ifreetype-flat/include -Isrc/spmp -fno-strict-aliasing -DLSB_FIRST -DFAST_MEM -O3 -funroll-loops

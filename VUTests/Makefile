EE_BIN = test.elf
EE_OBJS = main.o clip.o mac.o
EE_LIBS = -lc

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

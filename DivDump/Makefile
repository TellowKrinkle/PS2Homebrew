EE_BIN = test.elf
EE_OBJS = main.o div.o div-vu0.o
EE_LIBS = -lc $(PS2SDK)/ports/lib/libz.a
EE_INCS = -I$(PS2SDK)/ports/include
EE_DVP = dvp-as

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

%.o: %.vsm
	$(EE_DVP) $< -o $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

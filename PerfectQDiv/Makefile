EE_BIN = test.elf
EE_OBJS = main.o VUPointScan.o
EE_LIBS = -lc -ldraw -ldma -lgraph -lpacket
EE_DVP = dvp-as

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

%.o: %.vsm
	$(EE_DVP) $< -o $@

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

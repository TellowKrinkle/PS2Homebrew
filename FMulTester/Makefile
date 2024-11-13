EE_BIN = test.elf
EE_OBJS = main.o irx/usbd_irx.o irx/bdm_irx.o irx/bdmfs_fatfs_irx.o irx/usbmass_bd_irx.o
EE_LIBS = -lc $(PS2SDK)/ports/lib/libz.a
EE_INCS = -I$(PS2SDK)/ports/include

all: $(EE_BIN)

clean:
	rm -f $(EE_BIN) $(EE_OBJS) irx/*_irx.*

irx/%_irx.c: $(PS2SDK)/iop/irx/%.irx
	bin2c $< $@ $*_irx

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

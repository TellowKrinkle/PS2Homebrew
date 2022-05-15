# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

EE_BIN = ule_loader.elf
EE_OBJS = main.o cdfs_irx.o loader_elf.o
EE_LIBS = -lpad -lpatches -lc

BIN2S = $(PS2SDK)/bin/bin2s

all: $(EE_BIN)

clean:
	$(MAKE) -C loader clean
	rm -f $(EE_BIN) $(EE_OBJS) cdfs_irx.s loader_elf.s

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

reset:
	ps2client reset

cdfs_irx.s: $(PS2SDK)/iop/irx/cdfs.irx
	$(BIN2S) $< $@ cdfs_irx

loader/loader.elf: loader
	$(MAKE) -C $<

loader_elf.s: loader/loader.elf
	$(BIN2S) $< $@ loader_elf

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal

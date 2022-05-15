#include <tamtypes.h>
#include <string.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libcdvd-common.h>
#include <stdio.h>
#include <sbv_patches.h>
#include <iopcontrol.h>

#define ELF_PT_LOAD 1

typedef struct
{
	u8 ident[16];  // struct definition for ELF object header
	u16 type;
	u16 machine;
	u32 version;
	u32 entry;
	u32 phoff;
	u32 shoff;
	u32 flags;
	u16 ehsize;
	u16 phentsize;
	u16 phnum;
	u16 shentsize;
	u16 shnum;
	u16 shstrndx;
} elf_header_t;
//------------------------------
typedef struct
{
	u32 type;  // struct definition for ELF program section header
	u32 offset;
	void *vaddr;
	u32 paddr;
	u32 filesz;
	u32 memsz;
	u32 flags;
	u32 align;
} elf_pheader_t;

extern u8 cdfs_irx[];
extern int size_cdfs_irx;

extern u8 loader_elf[];
extern int size_loader_elf;

void loadModules()
{
	SifInitRpc(0);
    while (!SifIopReset("", 0)) {
    }
    while (!SifIopSync()) {
    }
    SifInitRpc(0);
    SifLoadFileInit();
    sbv_patch_enable_lmb();

	int ret;
	sceCdInit(SCECdINoD); // SCECdINoD init without check for a disc. Reduces risk of a lockup if the drive is in a erroneous state.
	SifExecModuleBuffer(cdfs_irx, size_cdfs_irx, 0, NULL, &ret);
}

int main(int argc, const char *argv[])
{
	loadModules();
	char* exe = (char*)"cdfs:/ULE.ELF";
	char* exe_argv[2] = {exe, exe};
	elf_header_t* eh = (elf_header_t *)loader_elf;
	elf_pheader_t* eph = (elf_pheader_t *)(loader_elf + eh->phoff);

	for (int i = 0; i < eh->phnum; i++) {
		if (eph[i].type != ELF_PT_LOAD)
			continue;

		void* pdata = loader_elf + eph[i].offset;
		memcpy(eph[i].vaddr, pdata, eph[i].filesz);

		if (eph[i].memsz > eph[i].filesz)
			memset(eph[i].vaddr + eph[i].filesz, 0,
			       eph[i].memsz - eph[i].filesz);
	}

	SifExitRpc();
	FlushCache(0);
	FlushCache(2);
	ExecPS2((void *)eh->entry, NULL, 2, exe_argv);
}
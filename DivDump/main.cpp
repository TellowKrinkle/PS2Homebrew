#include <kernel.h>
#include <tamtypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#if 1
#include <zlib.h>
#define OPENARGS "wb1"
#define EXT ".bin.gz"
#else
#define gzFile FILE*
#define gzopen fopen
#define gzclose fclose
#define gzfwrite fwrite
#define OPENARGS "wb"
#define EXT
#endif

extern "C" {
	void divTest(u32 divisor, u32* buffer);
	extern const char vudivbeg, vudivend;
}

static char* const vu0code = reinterpret_cast<char*>(0x11000000);

int main(int argc, const char* argv[]) {
	memcpy(vu0code, &vudivbeg, &vudivend - &vudivbeg);
	FlushCache(0);
	const char* fbase = argc > 1 ? argv[1] : "host:/";
	gzFile file = nullptr;
	static u32 buffer[0x800000 / 32];
	u32 b = 0x800000;
	if (argc > 2) {
		char* res;
		unsigned long req = strtoul(argv[2], &res, 0);
		if (res != argv[2] && req >= (1 << 23) && req < (1 << 24)) {
			b = req;
		} else {
			printf("Bad start point %s, ignoring\n", argv[2]);
		}
	}
	for (; b < (1 << 24); b++) {
		if ((b & 0xff) == 0) {
			if (file)
				gzclose(file);
			char filename[256];
			snprintf(filename, sizeof(filename), "%s/div_%06X" EXT, fbase, b);
			file = gzopen(filename, OPENARGS);
			if (!file) {
				printf("Failed to open %s\n", filename);
				break;
			}
		}
		printf("Processing %X...\n", b);
		divTest(b, buffer);
		printf("Writing    %X...\n", b);
		gzfwrite(buffer, 1, sizeof(buffer), file);
	}
	if (file)
		gzclose(file);
	printf("Sleeping\n");
	SleepThread();
}

#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char * argv[]) {
	int ret = EXIT_FAILURE;
	if (argc <= 3) {
		fprintf(stderr, "Usage: %s ps2in.bin ps2out.bin output.bin\n", argv[0]);
		return EXIT_FAILURE;
	}
	FILE* in0 = fopen(argv[1], "rb");
	if (!in0) {
		fprintf(stderr, "Failed to open %s for reading: %s\n", argv[1], strerror(errno));
		return EXIT_FAILURE;
	}
	FILE* in1 = fopen(argv[2], "rb");
	if (!in1) {
		fprintf(stderr, "Failed to open %s for reading: %s\n", argv[2], strerror(errno));
		goto close0;
	}
	FILE* out = fopen(argv[3], "wb");
	if (!out) {
		fprintf(stderr, "Failed to open %s for writing: %s\n", argv[3], strerror(errno));
		goto close1;
	}
	while (1) {
		uint32_t b0[8192];
		uint32_t b1[4096];
		uint32_t bo[8192 + 4096];
		size_t r0 = fread(b0, 8, sizeof(b0) / 8, in0);
		size_t r1 = fread(b1, 4, sizeof(b1) / 4, in1);
		if (r0 == 0 && !feof(in0)) {
			fprintf(stderr, "Failed to read %s: %s\n", argv[1], strerror(ferror(in0)));
			goto close;
		}
		if (r1 == 0 && !feof(in1)) {
			fprintf(stderr, "Failed to read %s: %s\n", argv[2], strerror(ferror(in1)));
			goto close;
		}
		if (r0 != r1) {
			if (r0 > r1)
				fprintf(stderr, "%s longer than %s\n", argv[1], argv[2]);
			else
				fprintf(stderr, "%s longer than %s\n", argv[2], argv[1]);
			goto close;
		}
		if (r0 == 0)
			break;
		for (size_t i = 0; i < r0; i++) {
			bo[i * 3 + 0] = b0[i * 2 + 0] | 0x3f800000;
			bo[i * 3 + 1] = b0[i * 2 + 1] | 0x3f800000;
			bo[i * 3 + 2] = b1[i * 1 + 0];
		}
		if (fwrite(bo, 12, r0, out) != r0) {
			fprintf(stderr, "Failed to write %s: %s\n", argv[3], strerror(ferror(out)));
			goto close;
		}
	}
	ret = EXIT_SUCCESS;
close:
	fclose(out);
close1:
	fclose(in1);
close0:
	fclose(in0);
	return ret;
}

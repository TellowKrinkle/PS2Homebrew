#include <kernel.h>
#include <tamtypes.h>
#include <stdio.h>
#include <string.h>

#include <dirent.h>
#include <packet.h>
#include <stdlib.h>
#include <fcntl.h>
#include <iterator>
#include <zlib.h>
#include <loadfile.h>

#define DECL_IRX(name) \
	extern unsigned int size_##name##_irx; \
	extern unsigned char name##_irx[]

#define LOAD_IRX(name, hname) \
	int name##_irx_id = SifExecModuleBuffer(name##_irx, size_##name##_irx, 0, NULL, NULL); \
	printf(hname " Loaded: %d\n", name##_irx_id)

DECL_IRX(usbd);
DECL_IRX(bdm);
DECL_IRX(bdmfs_fatfs);
DECL_IRX(usbmass_bd);

static void load_usb_irx() {
	LOAD_IRX(bdm, "BDM");
	LOAD_IRX(bdmfs_fatfs, "BDMFS FATFS");
	LOAD_IRX(usbd, "USBD");
	LOAD_IRX(usbmass_bd, "USBMass");
}

struct alignas(16) TestResult {
	u32 inputs[4];
	u32 cop1[4];
	u32 cop2[4];
};

static void test(u32* input, u32* output, u32* end) {
	asm volatile(
		".set noreorder      \n\t"
		"pref   0,   0(%[i]) \n\t"
		"lui    $13, 0x3f80  \n\t"
		"pref   0,  64(%[i]) \n\t"
		"pextlw $13, $13, $13\n\t"
		"pref   0,   0(%[o]) \n\t"
		"pextlw $13, $13, $13\n\t"

		// First iteration
		"lq     $8,    0(%[i])  \n\t" // $8  = A1 B1 A2 B2
		"lq     $9,   16(%[i])  \n\t" // $9  = A3 B3 A4 B4
		"por    $8,  $8,  $13   \n\t" // $8 |= 0x3f800000
		"por    $9,  $9,  $13   \n\t" // $9 |= 0x3f800000
		"pextlw $10, $9,  $8    \n\t" // $10 = A1 A3 B1 B3
		"pextuw $11, $9,  $8    \n\t" // $11 = A2 A4 B2 B4
		"pextlw $8,  $11, $10   \n\t" // $8  = A1 A2 A3 A4
		"pextuw $9,  $11, $10   \n\t" // $9  = B1 B2 B3 B4
		"qmtc2  $8,  $vf1       \n\t" // $vf1= A1 A2 A3 A4
		"qmtc2  $9,  $vf2       \n\t" // $vf2= B1 B2 B3 B4
		"pref   0,   128(%[i])  \n\t"
		"vmul   $vf1, $vf1, $vf2\n\t" // $vf1= C1 C2 C3 C4
		"addi   %[i], %[i], 32  \n\t" // Advance input
		"pref   0,    64(%[o])  \n\t"
		"beq    %[i], %[end], 1f\n\t"
		"addi   %[o], %[o], 16  \n\t" // Advance output

		// Loop
		"\n0:\n\t"
		"lq     $8,    0(%[i])  \n\t" // $8  = [1] A1 B1 A2 B2
		"lq     $9,   16(%[i])  \n\t" // $9  = [1] A3 B3 A4 B4
		"por    $8,  $8,  $13   \n\t" // $8 |= [1] 0x3f800000
		"por    $9,  $9,  $13   \n\t" // $9 |= [1] 0x3f800000
		"pextlw $10, $9,  $8    \n\t" // $10 = [1] A1 A3 B1 B3
		"pextuw $11, $9,  $8    \n\t" // $11 = [1] A2 A4 B2 B4
		"qmfc2  $12, $vf1       \n\t" // $12 = [0] C1 C2 C3 C4
		"pextlw $8,  $11, $10   \n\t" // $8  = [1] A1 A2 A3 A4
		"pextuw $9,  $11, $10   \n\t" // $9  = [1] B1 B2 B3 B4
		"qmtc2  $8,  $vf1       \n\t" // $vf1= [1] A1 A2 A3 A4
		"qmtc2  $9,  $vf2       \n\t" // $vf2= [1] B1 B2 B3 B4
		"sq     $12, -16(%[o])  \n\t" // STORE [0] C1 C2 C3 C4
		"vmul   $vf1, $vf1, $vf2\n\t" // $vf1= [1] C1 C2 C3 C4
		"pref   0,   128(%[i])  \n\t"
		"addi   %[i], %[i], 32  \n\t" // Advance input
		"pref   0,    64(%[o])  \n\t"
		"bne    %[i], %[end], 0b\n\t"
		"addi   %[o], %[o], 16  \n\t" // Advance output

		// Finish up final iteration
		"\n1:\n\t"
		"qmfc2  $12, $vf1     \n\t" // $12 = [0] C1 C2 C3 C4
		"sq     $12, -16(%[o])\n\t" // STORE [0] C1 C2 C3 C4
		".set reorder\n\t"

		: [i]"+r"(input), [o]"+r"(output), [end]"+r"(end)
		:
		: "memory", "8", "9", "10", "11", "12", "13"
	);
}

ALIGNED(16) u32 input[1024 * 256 * 2];
ALIGNED(16) u32 output[1024 * 256];

bool wait_usb_ready() {
	printf("Waiting for USB to be ready...\n");
	bool ok = false;
	int tries = 0;

	for (; tries < 10; tries++) {
		if (DIR* dir = opendir("mass:/")) {
			closedir(dir);
			ok = true;
			break;
		}
		sleep(1);
	}

	printf("USB ready after %d attempts.\n", tries + 1);
	if (!ok)
		printf("USB not ready after 10 seconds :(.\n Try a smaller FAT32 partition?.\n");

	return ok;
}

static bool endsWith(const char* string, const char* end) {
	size_t endlen = strlen(end);
	size_t len = strlen(string);
	if (endlen > len)
		return false;
	return 0 == memcmp(string + (len - endlen), end, endlen);
}

void run(const char* ipath, const char* opath) {
	if (0 == strncmp(ipath, "mass", 4) || 0 == strncmp(ipath, "mass", 4)) {
		load_usb_irx();
		if (!wait_usb_ready())
			return;
	}
	bool ogz = endsWith(opath, ".gz");
	gzFile ifile = gzopen(ipath, "rb");
	if (!ifile) {
		printf("Failed to open %s\n", ipath);
		return;
	}
	gzFile ofile = gzopen(opath, ogz ? "wb1" : "wbT");
	if (!ofile) {
		gzclose(ifile);
		printf("Failed to open %s\n", opath);
		return;
	}
	u64 total = 0;
	while (true) {
		int count = gzfread(input, 8, sizeof(input) / 8, ifile);
		printf("fread returned %d\n", count);
		if (count <= 0)
			break;
		test(input, output, std::end(input));
		printf("Writing output... ");
		fflush(stdout);
		gzfwrite(output, 4, count, ofile);
		total += count;
		printf("wrote 0x%x items (total: 0x%llx)\n", count, total);
	}
	gzclose(ofile);
	gzclose(ifile);
}

int main(int argc, const char* argv[]) {
	if (argc <= 2) {
		float* fi = reinterpret_cast<float*>(input);
		float* fo = reinterpret_cast<float*>(output);
		for (u32 i = 0; i < 256; i++) {
			fi[i] = i;
		}
		test(input, output, std::end(input));
		for (u32 i = 0; i < 128; i++) {
			printf("%4g * %4g = %4g\n", fi[i * 2], fi[i * 2 + 1], fo[i]);
		}
	} else {
		run(argv[1], argv[2]);
	}
	printf("Sleeping\n");
	SleepThread();
}

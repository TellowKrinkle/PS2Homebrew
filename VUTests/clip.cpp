#include "vutests.h"

#include <tamtypes.h>
#include <stdio.h>

struct alignas(16) ClipTest {
	u32 test[4];
	u32 result;
};

static constexpr ClipTest clipTests[] = {
	{{0x3f800000, 0xbf800000, 0x40000000, 0x3f800000}, 0b010000},
	{{0x3f800000, 0xbf800000, 0x40000000, 0xbf7fffff}, 0b011001},
	{{0x0007ffff, 0x8007ffff, 0x00800000, 0x00000000}, 0b010000},
	{{0x0007ffff, 0x8007ffff, 0x80800000, 0x80000000}, 0b100000},
	{{0xffffffff, 0x7fffffff, 0x7ffffff0, 0x7fffffff}, 0b000000},
	{{0xffffffff, 0x7fffffff, 0x7ffffff0, 0xfffffffe}, 0b000110},
	{{0x7f7fffff, 0xff800000, 0xfffffff0, 0xfffffffe}, 0b000000},
	{{0xffffffff, 0x7fffffff, 0x7ffffff0, 0x7fffff00}, 0b010110},
	{{0x7f7fffff, 0xff800001, 0x7f800000, 0x7f7fffff}, 0b011000},
	{{0x7f7fffff, 0xff800001, 0x7f800000, 0xff800000}, 0b001000},
};

struct PrintClipFlags {
	char str[8];
	PrintClipFlags(u32 flags) {
		for (u32 i = 0; i < 3; i++) {
			u32 i2 = i * 2;
			if (flags & (1 << i2))
				str[i2 + 0] = '+';
			else if (flags & (2 << i2))
				str[i2 + 0] = '-';
			else
				str[i2 + 0] = ' ';
			if (flags & (3 << i2))
				str[i2 + 1] = 'x' + i;
			else
				str[i2 + 1] = ' ';
		}
		str[6] = 0;
		str[7] = 0;
	}
};

bool testClip() {
	bool ok = true;
	for (const ClipTest& test : clipTests) {
		u32 out;
		asm(
			"lqc2   $vf1, (%[test])\n\t"
			"vclipw $vf1, $vf1     \n\t"
			"vnop                  \n\t"
			"vnop                  \n\t"
			"vnop                  \n\t"
			"vnop                  \n\t"
			"cfc2   %[out], $18    \n\t"
			: [out]"=r"(out)
			: [test]"r"(&test)
		);
		if ((out & 0x3f) != test.result) {
			ok = false;
			printf("VCLIP %08x %08x %08x %08x => %s != %s\n",
				test.test[0], test.test[1], test.test[2], test.test[3],
				PrintClipFlags(out).str, PrintClipFlags(test.result).str
			);
		}
	}
	return ok;
}

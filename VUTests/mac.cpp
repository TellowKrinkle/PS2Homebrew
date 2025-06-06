#include "vutests.h"

#include <stdio.h>

/// The flags from a math operation
struct MACTestFlags {
	u32 cop1;     ///< Each byte represents cop1 flags for the output at that position
	u16 cop2stat; ///< COP2 status flags register (with sticky bits cleared before the operation)
	u16 cop2mac;  ///< COP2 MAC flags register
};

struct alignas(16) MACTest2Arg {
	u32 in0[4];    ///< First input
	u32 in1[4];    ///< Second input
	u32 out[4];    ///< Output
	MACTestFlags flags;
};

struct alignas(16) MACTest3Arg {
	u32 in0[4];     ///< First input
	u32 in1[4];     ///< Second input
	u32 in2[4];     ///< Third (ACC) input
	u32 out[4];     ///< Output
	MACTestFlags flags;  ///< Flags for madd
	MACTestFlags nflags; ///< Flags for msub, with in1 negated
};

struct COP1Result {
	u32 flags;
};

struct COP2Result {
	u32 status;
	u32 mac;
};

template <COP2Result(*Fn)(u32(&)[4], const u32(&)[4], const u32(&)[4]), bool Inv = false>
const bool runTestCOP2(const char* name, const char* op, ArrayRef<const MACTest2Arg> tests) {
	bool ok = true;
	for (const MACTest2Arg& test : tests) {
		u32 res[4];
		u32 inv[4];
		for (u32 i = 0; i < 4; i++)
			inv[i] = test.in1[i] ^ 0x80000000;
		const u32(&in0)[4] = test.in0;
		const u32(&in1)[4] = Inv ? inv : test.in1;
		COP2Result flags = Fn(res, in0, in1);
		for (u32 i = 0; i < 4; i++) {
			if (res[i] != test.out[i]) {
				printf("%s %08X %s %08X => %08X != %08X\n", name, in0[i], op, in1[i], res[i], test.out[i]);
				ok = false;
			}
			u32 mask = 0x8888 >> i;
			if ((flags.mac & mask) != (test.flags.cop2mac & mask)) {
				printf("%s %08X %s %08X MAC FLAGS %s != %s\n",
					name, in0[i], op, in1[i],
					PrintCOP2MAC(flags.mac, 3 - i).str, PrintCOP2MAC(test.flags.cop2mac, 3 - i).str);
				ok = false;
			}
		}
		if (flags.status != test.flags.cop2stat) {
			printf("%s {%08X %08X %08X %08X} %s {%08X %08X %08X %08X} STATUS FLAGS %s != %s\n",
				name, in0[0], in0[1], in0[2], in0[3],
				op,   in1[0], in1[1], in1[2], in1[3],
				PrintCOP2Status(flags.status).str, PrintCOP2Status(test.flags.cop2stat).str);
			ok = false;
		}
	}
	return ok;
}

template <COP1Result(*Fn)(u32(&)[4], const u32(&)[4], const u32(&)[4]), bool Inv = false>
const bool runTestCOP1(const char* name, const char* op, ArrayRef<const MACTest2Arg> tests) {
	bool ok = true;
	for (const MACTest2Arg& test : tests) {
		u32 res[4];
		u32 inv[4];
		for (u32 i = 0; i < 4; i++)
			inv[i] = test.in1[i] ^ 0x80000000;
		const u32(&in0)[4] = test.in0;
		const u32(&in1)[4] = Inv ? inv : test.in1;
		COP1Result flags = Fn(res, in0, in1);
		for (u32 i = 0; i < 4; i++) {
			if (res[i] != test.out[i]) {
				printf("%s %08X %s %08X => %08X != %08X\n", name, in0[i], op, in1[i], res[i], test.out[i]);
				ok = false;
			}
			u32 ps2      = (flags.flags     >> (i * 8)) & 0xff;
			u32 expected = (test.flags.cop1 >> (i * 8)) & 0xff;
			if (ps2 != expected) {
				printf("%s %08X %s %08X FLAGS %s != %s\n", name, in0[i], op, in1[i], PrintCOP1Flags(ps2).str, PrintCOP1Flags(expected).str);
				ok = false;
			}
		}
	}
	return ok;
}

enum class Op2Arg {
	Add = 0,
	Sub = 1,
	Mul = 2,
};

enum class Op3Arg {
	MAdd = 0,
	MSub = 1,
};

template <Op2Arg Op>
static COP2Result doOp2COP2(u32(&c)[4], const u32(&a)[4], const u32(&b)[4]) {
	COP2Result res;
	asm(
		"ctc2 $0,   $16       \n\t"
		"lqc2 $vf1, (%[a])    \n\t"
		"lqc2 $vf2, (%[b])    \n"
		".if %[op] == 0       \n\t"
		"vadd $vf1, $vf1, $vf2\n"
		".elseif %[op] == 1   \n\t"
		"vsub $vf1, $vf1, $vf2\n"
		".elseif %[op] == 2   \n\t"
		"vmul $vf1, $vf1, $vf2\n"
		".endif               \n\t"
		"sqc2 $vf1, (%[c])    \n\t"
		"cfc2 %[stat], $16    \n\t"
		"cfc2 %[mac],  $17    \n\t"
		: [stat]"=r"(res.status), [mac]"=r"(res.mac)
		: [a]"r"(&a), [b]"r"(&b), [c]"r"(&c), [op]"i"(static_cast<u32>(Op))
		: "memory"
	);
	return res;
}

template <Op2Arg Op, int Off>
static u32 doOp2COP1Helper(u32(&c)[4], const u32(&a)[4], const u32(&b)[4]) {
	u32 res;
	asm(
		"ctc1  $0,  $31         \n\t"
		"lwc1  $f0, %[off](%[a])\n\t"
		"lwc1  $f1, %[off](%[b])\n"
		".if %[op] == 0         \n\t"
		"add.s $f0, $f0, $f1    \n"
		".elseif %[op] == 1     \n\t"
		"sub.s $f0, $f0, $f1    \n"
		".elseif %[op] == 2     \n\t"
		"mul.s $f0, $f0, $f1    \n"
		".endif                 \n\t"
		"swc1  $f0, %[off](%[c])\n\t"
		"cfc1  %[f],  $31       \n\t"
		: [f]"=r"(res)
		: [a]"r"(&a), [b]"r"(&b), [c]"r"(&c), [off]"i"(Off), [op]"i"(static_cast<u32>(Op))
		: "f0", "f1", "memory"
	);
	return res;
}

template <Op2Arg Op>
static COP1Result doOp2COP1(u32(&c)[4], const u32(&a)[4], const u32(&b)[4]) {
	u32 x = doOp2COP1Helper<Op,  0>(c, a, b);
	u32 y = doOp2COP1Helper<Op,  4>(c, a, b);
	u32 z = doOp2COP1Helper<Op,  8>(c, a, b);
	u32 w = doOp2COP1Helper<Op, 12>(c, a, b);
	u32 res = (((x & 0x78) >>  3) | ((x & 0x3C000) >> 10))
	        | (((y & 0x78) <<  5) | ((y & 0x3C000) >>  2))
	        | (((z & 0x78) << 13) | ((z & 0x3C000) <<  6))
	        | (((w & 0x78) << 21) | ((w & 0x3C000) << 14));
	return { res };
}

static constexpr MACTest2Arg addTests[] = {
	{
		{0x00000000, 0x00000000, 0x80000000, 0x80000000},
		{0x00000000, 0x80000000, 0x00000000, 0x80000000},
		{0x00000000, 0x00000000, 0x00000000, 0x80000000},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00303, /*COP2MAC*/ 0x001F}
	}, {
		{0x007FFFFF, 0x00000000, 0x80000000, 0x807FFFFF},
		{0x007FFFFF, 0x807FFFFF, 0x007FFFFF, 0x807FFFFF},
		{0x00000000, 0x00000000, 0x00000000, 0x80000000},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00303, /*COP2MAC*/ 0x001F}
	}, {
		{0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000},
		{0x3F800000, 0x40000000, 0x34000000, 0x33800000},
		{0x40000000, 0x40400000, 0x3F800001, 0x3F800000},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00000, /*COP2MAC*/ 0x0000}
	}, {
		{0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000},
		{0xB4000000, 0xB3800000, 0xB3000000, 0xB2800000},
		{0x3F7FFFFE, 0x3F7FFFFF, 0x3F800000, 0x3F800000},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00000, /*COP2MAC*/ 0x0000}
	}, {
		{0x3F800000, 0x7FFFFFFF, 0x00800000, 0x00800001},
		{0xBF800000, 0xFFFFFFFF, 0x80800000, 0x80800000},
		{0x00000000, 0x00000000, 0x00000000, 0x00000000},
		{/*COP1*/ 0x11000000, /*COP2STAT*/ 00505, /*COP2MAC*/ 0x010F}
	}, {
		{0xBF800000, 0xFFFFFFFF, 0x80800000, 0x80800001},
		{0x3F800000, 0x7FFFFFFF, 0x00800000, 0x00800000},
		{0x00000000, 0x00000000, 0x00000000, 0x80000000},
		{/*COP1*/ 0x11000000, /*COP2STAT*/ 00707, /*COP2MAC*/ 0x011F}
	}, {
		{0x00900001, 0x00900000, 0x80900001, 0x80900000},
		{0x80900000, 0x808FFFFF, 0x00900000, 0x008FFFFF},
		{0x00000000, 0x00000000, 0x80000000, 0x80000000},
		{/*COP1*/ 0x11111111, /*COP2STAT*/ 00707, /*COP2MAC*/ 0x0F3F}
	}, {
		{0x7F7FFFFF, 0x7F800000, 0xFF7FFFFF, 0xFF7FFFFF},
		{0x7F7FFFFF, 0x7F7FFFFF, 0xFF7FFFFE, 0xFF800000},
		{0x7FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00202, /*COP2MAC*/ 0x0030}
	}, {
		{0x7FFFFFFF, 0x7F800000, 0xFFFFFFFF, 0xFF7FFFFF},
		{0x7FFFFFFF, 0x7F800000, 0xFFFFFFFE, 0xFF800001},
		{0x7FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
		{/*COP1*/ 0x22222222, /*COP2STAT*/ 01212, /*COP2MAC*/ 0xF030}
	}, {
		{0x3F800000, 0x7FFDDDDD, 0x7FFDDDDD, 0xF4800000},
		{0xBCF776F9, 0xB4480000, 0xFF800000, 0x7FFDDDDD},
		{0x3F784449, 0x7FFDDDDD, 0x7F7BBBBA, 0x7FFDDDDB},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00000, /*COP2MAC*/ 0x0000}
	},
};

bool testAdd() {
	bool res = true;
	res &= runTestCOP1<doOp2COP1<Op2Arg::Add>>("ADD.S", "+", addTests);
	res &= runTestCOP2<doOp2COP2<Op2Arg::Add>>("VADD",  "+", addTests);
	return res;
}

bool testSub() {
	bool res = true;
	res &= runTestCOP1<doOp2COP1<Op2Arg::Sub>, true>("SUB.S", "-", addTests);
	res &= runTestCOP2<doOp2COP2<Op2Arg::Sub>, true>("VSUB",  "-", addTests);
	return res;
}

static constexpr MACTest2Arg mulTests[] = {
	{
		{0x00000000, 0x00000000, 0x80000000, 0x80000000},
		{0x00000000, 0x80000000, 0x00000000, 0x80000000},
		{0x00000000, 0x80000000, 0x80000000, 0x00000000},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00303, /*COP2MAC*/ 0x006F}
	}, {
		{0x007FFFFF, 0x00000000, 0x80000000, 0x807FFFFF},
		{0x007FFFFF, 0x807FFFFF, 0x007FFFFF, 0x807FFFFF},
		{0x00000000, 0x80000000, 0x80000000, 0x00000000},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00303, /*COP2MAC*/ 0x006F}
	}, {
		{0x7FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
		{0x00000000, 0x80000000, 0x00000000, 0x80000000},
		{0x00000000, 0x80000000, 0x80000000, 0x00000000},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00303, /*COP2MAC*/ 0x006F}
	}, {
		{0x7FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
		{0x007FFFFF, 0x807FFFFF, 0x007FFFFF, 0x807FFFFF},
		{0x00000000, 0x80000000, 0x80000000, 0x00000000},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00303, /*COP2MAC*/ 0x006F}
	}, {
		{0x00800000, 0x80800000, 0x00800000, 0x80800000},
		{0x3F000000, 0x3F000000, 0xBF000000, 0xBF000000},
		{0x00000000, 0x80000000, 0x80000000, 0x00000000},
		{/*COP1*/ 0x11111111, /*COP2STAT*/ 00707, /*COP2MAC*/ 0x0F6F}
	}, {
		{0x20000000, 0xA0000000, 0x20000000, 0xA0400000},
		{0x1F800000, 0x1F800000, 0x9F800000, 0x9FC00000},
		{0x00000000, 0x80000000, 0x80000000, 0x00900000},
		{/*COP1*/ 0x00111111, /*COP2STAT*/ 00707, /*COP2MAC*/ 0x0E6E}
	}, {
		{0x5F800000, 0xDF800000, 0x7FFFFFFF, 0xFFFFFFFE},
		{0x5F800000, 0xDF800001, 0x3F800000, 0xBF800001},
		{0x7F800000, 0x7F800001, 0x7FFFFFFF, 0x7FFFFFFF},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00000, /*COP2MAC*/ 0x0000}
	}, {
		{0x5FC00000, 0x7F800000, 0x7FFFFFFF, 0xFFFFFFFF},
		{0x5FC00000, 0x7F800000, 0xFFFFFFFF, 0xBF800001},
		{0x7FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF, 0x7FFFFFFF},
		{/*COP1*/ 0x22222222, /*COP2STAT*/ 01212, /*COP2MAC*/ 0xF020}
	}, {
		{0x00800000, 0x7FFFFFFF, 0x3F9E4791, 0x43480000},
		{0x7FFFFFFF, 0x00800000, 0x7F800000, 0x43480000},
		{0x40FFFFFE, 0x40FFFFFF, 0x7F9E4791, 0x471C4000},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00000, /*COP2MAC*/ 0x0000}
	}, {
		{0x3F800400, 0x3F800400, 0x3FAAAB00, 0x3FAAAB00},
		{0x3F800001, 0x3F800002, 0x3F800003, 0x3F800004},
		{0x3F800401, 0x3F800401, 0x3FAAAB03, 0x3FAAAB05},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00000, /*COP2MAC*/ 0x0000}
	}, {
		{0x3F800000, 0x3F800000, 0x3F800000, 0xA0400000},
		{0x3F800001, 0x3F800002, 0x3F800003, 0x9FC00002},
		{0x3F800001, 0x3F800001, 0x3F800002, 0x00900001},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00000, /*COP2MAC*/ 0x0000}
	}, {
		{0x408F0000, 0x40CF0000, 0x3F480000, 0x3F080000},
		{0x7E8FFFFF, 0x7ECFFFFF, 0x00C80000, 0x00C80000},
		{0x7FA0DFFE, 0x7FFFFFFF, 0x009C4000, 0x00000000},
		{/*COP1*/ 0x11002200, /*COP2STAT*/ 01515, /*COP2MAC*/ 0x4101}
	}
};

bool testMul() {
	bool res = true;
	res &= runTestCOP1<doOp2COP1<Op2Arg::Mul>>("MUL.S", "*", mulTests);
	res &= runTestCOP2<doOp2COP2<Op2Arg::Mul>>("VMUL",  "*", mulTests);
	return res;
}

template <COP2Result(*Fn)(u32(&)[4], const u32(&)[4], const u32(&)[4], const u32(&)[4]), bool Inv = false>
const bool runTestCOP2(const char* name, const char* op, ArrayRef<const MACTest3Arg> tests) {
	bool ok = true;
	for (const MACTest3Arg& test : tests) {
		u32 res[4];
		u32 inv[4];
		for (u32 i = 0; i < 4; i++)
			inv[i] = test.in1[i] ^ 0x80000000;
		const u32(&in0)[4] = test.in0;
		const u32(&in1)[4] = Inv ? inv : test.in1;
		const u32(&in2)[4] = test.in2;
		COP2Result flags = Fn(res, in0, in1, in2);
		const MACTestFlags& expectedFlags = Inv ? test.nflags : test.flags;
		for (u32 i = 0; i < 4; i++) {
			if (res[i] != test.out[i]) {
				printf("%s %08X %s %08X * %08X => %08X != %08X\n", name, in2[i], op, in0[i], in1[i], res[i], test.out[i]);
				ok = false;
			}
			u32 mask = 0x8888 >> i;
			if ((flags.mac & mask) != (expectedFlags.cop2mac & mask)) {
				printf("%s %08X %s %08X * %08X MAC FLAGS %s != %s\n",
					name, in2[i], op, in0[i], in1[i],
					PrintCOP2MAC(flags.mac, 3 - i).str, PrintCOP2MAC(expectedFlags.cop2mac, 3 - i).str);
				ok = false;
			}
		}
		if (flags.status != expectedFlags.cop2stat) {
			printf("%s {%08X %08X %08X %08X} %s {%08X %08X %08X %08X} * {%08X %08X %08X %08X} STATUS FLAGS %s != %s\n",
				name, in2[0], in2[1], in2[2], in2[3],
				op,   in0[0], in0[1], in0[2], in0[3],
				      in1[0], in1[1], in1[2], in1[3],
				PrintCOP2Status(flags.status).str, PrintCOP2Status(expectedFlags.cop2stat).str);
			ok = false;
		}
	}
	return ok;
}

template <COP1Result(*Fn)(u32(&)[4], const u32(&)[4], const u32(&)[4], const u32(&)[4]), bool Inv = false>
const bool runTestCOP1(const char* name, const char* op, ArrayRef<const MACTest3Arg> tests) {
	bool ok = true;
	for (const MACTest3Arg& test : tests) {
		u32 res[4];
		u32 inv[4];
		for (u32 i = 0; i < 4; i++)
			inv[i] = test.in1[i] ^ 0x80000000;
		const u32(&in0)[4] = test.in0;
		const u32(&in1)[4] = Inv ? inv : test.in1;
		const u32(&in2)[4] = test.in2;
		COP1Result flags = Fn(res, in0, in1, in2);
		const MACTestFlags& expectedFlags = Inv ? test.nflags : test.flags;
		for (u32 i = 0; i < 4; i++) {
			if (res[i] != test.out[i]) {
				printf("%s %08X %s %08X * %08X => %08X != %08X\n", name, in2[i], op, in0[i], in1[i], res[i], test.out[i]);
				ok = false;
			}
			u32 ps2      = (flags.flags        >> (i * 8)) & 0xff;
			u32 expected = (expectedFlags.cop1 >> (i * 8)) & 0xff;
			if (ps2 != expected) {
				printf("%s %08X %s %08X * %08X FLAGS %s != %s\n", name, in2[i], op, in0[i], in1[i], PrintCOP1Flags(ps2).str, PrintCOP1Flags(expected).str);
				ok = false;
			}
		}
	}
	return ok;
}

template <Op3Arg Op>
static COP2Result doOp3COP2(u32(&d)[4], const u32(&a)[4], const u32(&b)[4], const u32(&c)[4]) {
	COP2Result res;
	asm(
		"lqc2   $vf1, (%[c])    \n\t"
		"vsubax $ACC, $vf1, $vf0\n\t"
		"vnop                   \n\t"
		"ctc2   $0,   $16       \n\t"
		"lqc2   $vf1, (%[a])    \n\t"
		"lqc2   $vf2, (%[b])    \n"
		".if %[op] == 0         \n\t"
		"vmadd  $vf1, $vf1, $vf2\n"
		".elseif %[op] == 1     \n\t"
		"vmsub  $vf1, $vf1, $vf2\n"
		".endif                 \n\t"
		"sqc2   $vf1, (%[d])    \n\t"
		"cfc2   %[stat], $16    \n\t"
		"cfc2   %[mac],  $17    \n\t"
		: [stat]"=r"(res.status), [mac]"=r"(res.mac)
		: [a]"r"(&a), [b]"r"(&b), [c]"r"(&c), [d]"r"(&d), [op]"i"(static_cast<u32>(Op))
		: "memory"
	);
	return res;
}

template <Op3Arg Op, int Off>
static u32 doOp3COP1Helper(u32(&d)[4], const u32(&a)[4], const u32(&b)[4], const u32(&c)[4]) {
	u32 res;
	asm(
		"lwc1   $f0, %[off](%[c])\n\t"
		"mtc1   $0,  $f1         \n\t"
		"suba.s $f0, $f1         \n\t"
		"ctc1   $0,  $31         \n\t"
		"lwc1   $f0, %[off](%[a])\n\t"
		"lwc1   $f1, %[off](%[b])\n"
		".if %[op] == 0          \n\t"
		"madd.s $f0, $f0, $f1    \n"
		".elseif %[op] == 1      \n\t"
		"msub.s $f0, $f0, $f1    \n"
		".endif                  \n\t"
		"swc1   $f0, %[off](%[d])\n\t"
		"cfc1   %[f],  $31       \n\t"
		: [f]"=r"(res)
		: [a]"r"(&a), [b]"r"(&b), [c]"r"(&c), [d]"r"(&d), [off]"i"(Off), [op]"i"(static_cast<u32>(Op))
		: "f0", "f1", "memory"
	);
	return res;
}

template <Op3Arg Op>
static COP1Result doOp3COP1(u32(&d)[4], const u32(&a)[4], const u32(&b)[4], const u32(&c)[4]) {
	u32 x = doOp3COP1Helper<Op,  0>(d, a, b, c);
	u32 y = doOp3COP1Helper<Op,  4>(d, a, b, c);
	u32 z = doOp3COP1Helper<Op,  8>(d, a, b, c);
	u32 w = doOp3COP1Helper<Op, 12>(d, a, b, c);
	u32 res = (((x & 0x78) >>  3) | ((x & 0x3C000) >> 10))
	        | (((y & 0x78) <<  5) | ((y & 0x3C000) >>  2))
	        | (((z & 0x78) << 13) | ((z & 0x3C000) <<  6))
	        | (((w & 0x78) << 21) | ((w & 0x3C000) << 14));
	return { res };
}

static constexpr MACTest3Arg maddTests[] = {
	{
		{0x00000000, 0x00000000, 0x80000000, 0x80000000},
		{0x00000000, 0x80000000, 0x00000000, 0x80000000},
		{0x00000000, 0x00000000, 0x00000000, 0x00000000},
		{0x00000000, 0x00000000, 0x00000000, 0x00000000},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00301, /*COP2MAC*/ 0x000F},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00301, /*COP2MAC*/ 0x000F},
	}, {
		{0x00000000, 0x00000000, 0x80000000, 0x80000000},
		{0x00000000, 0x80000000, 0x00000000, 0x80000000},
		{0x80000000, 0x80000000, 0x80000000, 0x80000000},
		{0x00000000, 0x80000000, 0x80000000, 0x00000000},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00303, /*COP2MAC*/ 0x006F},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00303, /*COP2MAC*/ 0x006F},
	}, {
		{0x80000000, 0x807FFFFF, 0x80000000, 0x807FFFFF},
		{0x80000000, 0x80000000, 0x807FFFFF, 0x807FFFFF},
		{0x807FFFFF, 0x80000000, 0x80000000, 0x007FFFFF},
		{0x00000000, 0x00000000, 0x00000000, 0x00000000},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00101, /*COP2MAC*/ 0x000F},
		{/*COP1*/ 0x00000000, /*COP2STAT*/ 00301, /*COP2MAC*/ 0x000F},
	}, {
		{0x80000000, 0x80800000, 0x80000000, 0x80800000},
		{0x80000000, 0x80000000, 0x80800000, 0x80800000},
		{0x00800000, 0x80000000, 0x80000000, 0x007FFFFF},
		{0x00800000, 0x00000000, 0x00000000, 0x00000000},
		{/*COP1*/ 0x01000000, /*COP2STAT*/ 00501, /*COP2MAC*/ 0x0007},
		{/*COP1*/ 0x01000000, /*COP2STAT*/ 00701, /*COP2MAC*/ 0x0007},
	}, {
		{0x80000000, 0x80800000, 0x80000000, 0x80800000},
		{0x80000000, 0x80000000, 0x80800000, 0x80800000},
		{0x00800000, 0x3F800000, 0xBF800000, 0xBF800000},
		{0x00800000, 0x3F800000, 0xBF800000, 0xBF800000},
		{/*COP1*/ 0x01000000, /*COP2STAT*/ 00702, /*COP2MAC*/ 0x0030},
		{/*COP1*/ 0x01000000, /*COP2STAT*/ 00702, /*COP2MAC*/ 0x0030},
	}, {
		{0xA0400000, 0xA0400000, 0xA0400000, 0xA0400000},
		{0x1FC00000, 0x1FC00000, 0x9FC00000, 0x9FC00002},
		{0x00900000, 0x00900001, 0x808FFFFF, 0x80900000},
		{0x00000000, 0x00000000, 0x00000000, 0x00000000},
		{/*COP1*/ 0x11111100, /*COP2STAT*/ 00705, /*COP2MAC*/ 0x070F},
		{/*COP1*/ 0x11111100, /*COP2STAT*/ 00705, /*COP2MAC*/ 0x070F},
	}, {
		{0xA0400000, 0xA0400000, 0xA0400000, 0xA0400000},
		{0x9FC00000, 0x9FC00000, 0x1FC00000, 0x1FC00002},
		{0x80900000, 0x80900001, 0x008FFFFF, 0x00900000},
		{0x00000000, 0x80000000, 0x80000000, 0x80000000},
		{/*COP1*/ 0x11111100, /*COP2STAT*/ 00707, /*COP2MAC*/ 0x077F},
		{/*COP1*/ 0x11111100, /*COP2STAT*/ 00707, /*COP2MAC*/ 0x077F},
	}, {
		{0xFFFFFFFF, 0x7F800000, 0x7FFFFFFF, 0x7FFFFFFF},
		{0xFFFFFFFF, 0x7F800000, 0x3F800001, 0x3F800000},
		{0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF},
		{0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x00000000},
		{/*COP1*/ 0x00222222, /*COP2STAT*/ 01111, /*COP2MAC*/ 0xE001},
		{/*COP1*/ 0x00222222, /*COP2STAT*/ 01311, /*COP2MAC*/ 0xE001},
	}, {
		{0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF},
		{0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000},
		{0x3F800000, 0x00000000, 0x73800000, 0x74000000},
		{0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF},
		{/*COP1*/ 0x22000000, /*COP2STAT*/ 01010, /*COP2MAC*/ 0x1000},
		{/*COP1*/ 0x22000000, /*COP2STAT*/ 01210, /*COP2MAC*/ 0x1000},
	},
};

bool testMAdd() {
	bool res = true;
	res &= runTestCOP1<doOp3COP1<Op3Arg::MAdd>>("MADD.S", "+", maddTests);
	res &= runTestCOP2<doOp3COP2<Op3Arg::MAdd>>("VMADD",  "+", maddTests);
	return res;
}

bool testMSub() {
	bool res = true;
	res &= runTestCOP1<doOp3COP1<Op3Arg::MSub>, true>("MSUB.S", "-", maddTests);
	res &= runTestCOP2<doOp3COP2<Op3Arg::MSub>, true>("VMSUB",  "-", maddTests);
	return res;
}

struct COP2FlagOp {
	u16 stat;
	u16 mac;
	const char* op;
};

static bool checkCOP2Flags(const COP2FlagOp* expected, const u32* stat, const u32* mac, u32 count) {
	bool statOK = true;
	bool macOK = true;
	for (u32 i = 0; i < count; i++) {
		statOK &= stat[i] == expected[i].stat;
		macOK &= mac[i] == expected[i].mac;
	}
	if (!statOK) {
		for (u32 i = 0; i < 4; i++) {
			const char* eq = stat[i] == expected[i].stat ? "==" : "!=";
			printf("%s STATUS FLAGS %s %s %s\n", expected[i].op, PrintCOP2Status(stat[i]).str, eq, PrintCOP2Status(expected[i].stat).str);
		}
	}
	if (!macOK) {
		for (u32 i = 0; i < 4; i++) {
			u32 expmac = expected[i].mac;
			const char* eq = mac[i] == expmac ? "==" : "!=";
			printf("%s MAC FLAGS %s %s %s %s %s %s %s %s %s\n", expected[i].op,
				PrintCOP2MAC(mac[i], 3).str, PrintCOP2MAC(mac[i], 2).str, PrintCOP2MAC(mac[i], 1).str, PrintCOP2MAC(mac[i], 0).str, eq,
				PrintCOP2MAC(expmac, 3).str, PrintCOP2MAC(expmac, 2).str, PrintCOP2MAC(expmac, 1).str, PrintCOP2MAC(expmac, 0).str);
		}
	}
	return statOK && macOK;
}

struct COP1FlagOp {
	u32 stat;
	const char* op;
};

static bool checkCOP1Flags(const COP1FlagOp* expected, const u32* stat, u32 count) {
	bool ok = true;
	for (u32 i = 0; i < count; i++)
		ok &= processFlagsCOP1(stat[i]) == expected[i].stat;
	if (!ok) {
		for (u32 i = 0; i < count; i++) {
			u32 flags = processFlagsCOP1(stat[i]);
			const char* eq = flags == expected[i].stat ? "==" : "!=";
			printf("%s STATUS FLAGS %s %s %s\n", expected[i].op, PrintCOP1Flags(flags).str, eq, PrintCOP1Flags(expected[i].stat).str);
		}
	}
	return ok;
}

static bool testCOP2Flags() {
	alignas(16) static constexpr u32 data[] = {
		0x7FFFFFFE, 0x7FFFFFFE, 0x7FFFFFFE, 0x7FFFFFFE,
		0x00000000, 0x00000000, 0x7FFFFFFE, 0x7FFFFFFE,
		0x7FFFFFFE, 0x7FFFFFFE, 0x00000000, 0x00000000,
	};
	static constexpr COP2FlagOp expected[] = {
		{00000, 0x0000, "VADD.XY --OO"},
		{01010, 0x2000, "VADD.XZ --OO"},
		{05050, 0x2000, "DIV     D   "},
		{05040, 0x0000, "VADD.W  OO--"},
	};
	constexpr u32 count = sizeof(expected) / sizeof(*expected);
	u32 stat[count], mac[count];
	asm(
		"ctc2    $0,   $16        \n\t"
		"lqc2    $vf1,  0(%[buf]) \n\t"
		"lqc2    $vf2, 16(%[buf]) \n\t"
		"lqc2    $vf3, 32(%[buf]) \n\t"
		"vadd.xy $vf4, $vf1, $vf2 \n\t"
		"vmove   $vf5, $vf4       \n\t"
		"cfc2    $8,   $16        \n\t"
		"cfc2    $9,   $17        \n\t"
		"sw      $8,    0(%[stat])\n\t"
		"sw      $9,    0(%[mac]) \n\t"
		"vadd.xz $vf4, $vf1, $vf2 \n\t"
		"vmove   $vf5, $vf4       \n\t"
		"cfc2    $8,   $16        \n\t"
		"cfc2    $9,   $17        \n\t"
		"sw      $8,    4(%[stat])\n\t"
		"sw      $9,    4(%[mac]) \n\t"
		"vdiv    $Q, $vf1x, $vf0x \n\t"
		"vwaitq                   \n\t"
		"cfc2    $8,   $16        \n\t"
		"cfc2    $9,   $17        \n\t"
		"sw      $8,    8(%[stat])\n\t"
		"sw      $9,    8(%[mac]) \n\t"
		"vadd.w  $vf4, $vf1, $vf3 \n\t"
		"vmove   $vf5, $vf4       \n\t"
		"cfc2    $8,   $16        \n\t"
		"cfc2    $9,   $17        \n\t"
		"sw      $8,   12(%[stat])\n\t"
		"sw      $9,   12(%[mac]) \n\t"
		"vsqrt   $Q,   $vf0x      \n\t"
		:
		: [buf]"r"(data), [stat]"r"(stat), [mac]"r"(mac)
		: "memory", "8", "9"
	);
	return checkCOP2Flags(expected, stat, mac, count);
}

static bool testCOP1Flags() {
	static constexpr u32 data[] = {
		0x00000000, 0xFFFFFFFE, 0x00800000, 0x00800001
	};
	static constexpr COP1FlagOp expected[] = {
		{0x22, "ADD.S  O"},
		{0x66, "DIV.S  D"},
		{0x57, "SUB.S  U"},
		{0x9F, "SQRT.S I"},
		{0x8F, "MUL.S  -"},
		{0x0F, "SQRT.S -"},
	};
	constexpr u32 count = sizeof(expected) / sizeof(*expected);
	u32 stat[count];
	asm(
		"ctc1  $0,  $31       \n\t"
		"lwc1  $f0,  0(%[buf])\n\t"
		"lwc1  $f1,  4(%[buf])\n\t"
		"lwc1  $f2,  8(%[buf])\n\t"
		"lwc1  $f3, 12(%[buf])\n\t"
		"add.s $f4, $f1, $f1  \n\t"
		"cfc1  $8,  $31       \n\t"
		"div.s $f4, $f1, $f0  \n\t"
		"sw    $8,  0(%[stat])\n\t"
		"cfc1  $8,  $31       \n\t"
		"sub.s $f4, $f3, $f2  \n\t"
		"sw    $8,  4(%[stat])\n\t"
		"cfc1  $8,  $31       \n\t"
		"sqrt.s $f4, $f1      \n\t"
		"sw    $8,  8(%[stat])\n\t"
		"cfc1  $8,  $31       \n\t"
		"mul.s $f4, $f0, $f1  \n\t"
		"sw    $8, 12(%[stat])\n\t"
		"cfc1  $8,  $31       \n\t"
		"sqrt.s $f4, $f0      \n\t"
		"sw    $8, 16(%[stat])\n\t"
		"cfc1  $8,  $31       \n\t"
		"sw    $8, 20(%[stat])\n\t"
		:
		: [buf]"r"(data), [stat]"r"(stat)
		: "memory", "8", "f0", "f1", "f2", "f3", "f4"
	);
	return checkCOP1Flags(expected, stat, count);
}

bool testFlags() {
	bool ok = true;
	ok &= testCOP1Flags();
	ok &= testCOP2Flags();
	return ok;
}

bool testAccCop1() {
	static constexpr u32 data[] = {
		0x00000000, 0x7FFFFFFF, 0x3F800000, 0xC0800000
	};
	static constexpr COP1FlagOp expected[] = {
		{0x22, "ADDA.S  O"},
		{0x02, "ADD.S   -"},
		{0x22, "MSUBA.S -"},
		{0x22, "MADD.S  O"},
		{0x02, "ADDA.S  -"},
		{0x22, "ADD.S   O"},
		{0x02, "MSUB.S  -"},
	};
	static constexpr struct {
		u32 val;
		const char* op;
	} expectedResult[] = {
		{0x7FFFFFFF, "(7FFFFFFF ADDA.S + 7FFFFFFF) MSUB.S - (7FFFFFFF * 3F800000)"},
		{0xFFFFFFFF, "(7FFFFFFF ADDA.S + 7FFFFFFF) MSUBA.S - (7FFFFFFF * 3F800000) MADD.S + (7FFFFFFF * C0800000)"},
		{0x74000000, "(7FFFFFFF ADDA.S + 00000000) MSUB.S - (3F800000 * 7FFFFFFF)"},
	};
	constexpr u32 count = sizeof(expected) / sizeof(*expected);
	constexpr u32 countRes = sizeof(expectedResult) / sizeof(*expectedResult);
	u32 stat[count];
	u32 val[countRes];
	asm(
		"ctc1    $0,  $31        \n\t"
		"lwc1    $f0,  0(%[buf]) \n\t"
		"lwc1    $f1,  4(%[buf]) \n\t"
		"lwc1    $f2,  8(%[buf]) \n\t"
		"lwc1    $f3, 12(%[buf]) \n\t"
		"adda.s  $f1, $f1        \n\t"
		"cfc1    $8,  $31        \n\t"
		"add.s   $f4, $f2, $f3   \n\t"
		"sw      $8,   0(%[stat])\n\t"
		"cfc1    $8,  $31        \n\t"
		"msub.s  $f4, $f1, $f2   \n\t"
		"msuba.s $f1, $f2        \n\t"
		"sw      $8,   4(%[stat])\n\t"
		"swc1    $f4,  0(%[out]) \n\t"
		"cfc1    $8,  $31        \n\t"
		"madd.s  $f4, $f1, $f3   \n\t"
		"sw      $8,   8(%[stat])\n\t"
		"cfc1    $8,  $31        \n\t"
		"adda.s  $f1, $f0        \n\t"
		"swc1    $f4,  4(%[out]) \n\t"
		"sw      $8,  12(%[stat])\n\t"
		"cfc1    $8,  $31        \n\t"
		"add.s   $f4, $f1, $f1   \n\t"
		"sw      $8,  16(%[stat])\n\t"
		"cfc1    $8,  $31        \n\t"
		"msub.s  $f4, $f2, $f1   \n\t"
		"sw      $8,  20(%[stat])\n\t"
		"cfc1    $8,  $31        \n\t"
		"swc1    $f4,  8(%[out]) \n\t"
		"sw      $8,  24(%[stat])\n\t"
		:
		: [buf]"r"(data), [stat]"r"(stat), [out]"r"(val)
		: "memory", "8", "f0", "f1", "f2", "f3", "f4"
	);

	bool ok = checkCOP1Flags(expected, stat, count);
	for (u32 i = 0; i < countRes; i++) {
		if (val[i] != expectedResult[i].val) {
			printf("%s => %08X != %08X\n", expectedResult[i].op, val[i], expectedResult[i].val);
			ok = false;
		}
	}
	return ok;
}

bool testAccCop2() {
	alignas(16) static constexpr u32 data[] = {
		0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF,
		0x7FFFFFFF, 0x00000000, 0x7FFFFFFF, 0x00000000,
		0x00000000, 0x7FFFFFFF, 0x00000000, 0x7FFFFFFF,
		0xBF800000, 0x3F000000, 0xBF800001, 0x00000000,
		0xBF800001, 0xBF800000, 0xBF800000, 0xBF800000,
	};
	static constexpr COP2FlagOp expected[] = {
		{01010, 0xA000, "VADDA      O-O-"},
		{01010, 0x5000, "VADD       -O-O"},
		{01310, 0xC000, "VMADDA.xyw -OOO"},
		{01312, 0xE080, "VMADD      O---"},
	};
	static constexpr struct {
		u32 val;
		const char* op;
	} expectedResult[] = {
		{0xFFFFFFFF, "(7FFFFFFF VADDA + 7FFFFFFF) VMADDA + (7FFFFFFF * BF800000) VMADD + (BF800001 * 7FFFFFFF)"},
		{0x7FFFFFFF, "(7FFFFFFF VADDA + 00000000) VMADDA + (7FFFFFFF * 3F000000) VMADD + (BF800000 * 7FFFFFFF)"},
		{0x7FFFFFFF, "(7FFFFFFF VADDA + 7FFFFFFF) VMADD + (BF800000 * 7FFFFFFF)"},
		{0x74000000, "(7FFFFFFF VADDA + 00000000) VMADDA + (7FFFFFFF * 00000000) VMADD + (BF800000 * 7FFFFFFF)"},
	};
	constexpr u32 count = sizeof(expected) / sizeof(*expected);
	constexpr u32 countRes = sizeof(expectedResult) / sizeof(*expectedResult);
	alignas(16) u32 val[countRes];
	u32 stat[count], mac[count];
	asm(
		"ctc2       $0,   $16        \n\t"
		"lqc2       $vf1,  0(%[buf]) \n\t"
		"lqc2       $vf2, 16(%[buf]) \n\t"
		"vadda      $ACC, $vf1, $vf2 \n\t"
		"lqc2       $vf2, 32(%[buf]) \n\t"
		"vnop                        \n\t"
		"vnop                        \n\t"
		"vnop                        \n\t"
		"cfc2       $8,   $16        \n\t"
		"cfc2       $9,   $17        \n\t"
		"vadd       $vf4, $vf1, $vf2 \n\t"
		"lqc2       $vf2, 48(%[buf]) \n\t"
		"sw         $8,    0(%[stat])\n\t"
		"sw         $9,    0(%[mac]) \n\t"
		"vmove      $vf5, $vf4       \n\t"
		"cfc2       $8,   $16        \n\t"
		"cfc2       $9,   $17        \n\t"
		"vmadda.xyw $ACC, $vf1, $vf2 \n\t"
		"lqc2       $vf2, 64(%[buf]) \n\t"
		"sw         $8,    4(%[stat])\n\t"
		"sw         $9,    4(%[mac]) \n\t"
		"vnop                        \n\t"
		"cfc2       $8,   $16        \n\t"
		"cfc2       $9,   $17        \n\t"
		"vmadd      $vf4, $vf2, $vf1 \n\t"
		"sw         $8,    8(%[stat])\n\t"
		"sw         $9,    8(%[mac]) \n\t"
		"sqc2       $vf4,  0(%[out]) \n\t"
		"cfc2       $8,   $16        \n\t"
		"cfc2       $9,   $17        \n\t"
		"sw         $8,   12(%[stat])\n\t"
		"sw         $9,   12(%[mac]) \n\t"
		:
		: [buf]"r"(data), [stat]"r"(stat), [mac]"r"(mac), [out]"r"(val)
		: "memory", "8", "9"
	);
	bool ok = checkCOP2Flags(expected, stat, mac, count);
	for (u32 i = 0; i < countRes; i++) {
		if (val[i] != expectedResult[i].val) {
			printf("%s => %08X != %08X\n", expectedResult[i].op, val[i], expectedResult[i].val);
			ok = false;
		}
	}
	return ok;
}

bool testAcc() {
	bool ok = true;
	ok &= testAccCop1();
	ok &= testAccCop2();
	return ok;
}

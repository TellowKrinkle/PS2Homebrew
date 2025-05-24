#include "vutests.h"

#include <stdio.h>

struct Flags {
	u16 cop1;
	u16 cop2;
};

struct alignas(16) DivTestArg {
	u32 in0;
	u32 in1;
	u32 out;
	Flags flags;
};

struct SqrtTestArg {
	u32 in;
	u32 out;
	Flags flags;
};

enum class DivOp { Div = 0, RSqrt = 1 };

template <DivOp Op>
bool testDivOp(const char* cop1op, const char* cop2op, const char* wrap0, const char* wrap1, ArrayRef<const DivTestArg> tests) {
	asm volatile("vaddw $vf1, $vf0, $vf0w\n\t"); // Clear mac status flags
	bool ok = true;
	for (const DivTestArg& arg : tests) {
		u32 cop1, cop2, stat1, stat2;
		asm(
			"ctc2    $0,   $16       \n\t"
			"ctc1    $0,   $31       \n\t"
			"lqc2    $vf1, 0(%[arg]) \n\t"
			"lwc1    $f0,  0(%[arg]) \n\t"
			"lwc1    $f1,  4(%[arg]) \n"
			".if %[op] == 0          \n\t"
			"vdiv    $Q, $vf1x, $vf1y\n\t"
			"div.s   $f0, $f0, $f1   \n"
			".elseif %[op] == 1      \n\t"
			"vrsqrt  $Q, $vf1x, $vf1y\n\t"
			"rsqrt.s $f0, $f0, $f1   \n"
			".endif                  \n\t"
			"vwaitq                  \n\t"
			"cfc2    %[cop2],  $22   \n\t"
			"cfc2    %[stat2], $16   \n\t"
			"mfc1    %[cop1],  $f0   \n\t"
			"cfc1    %[stat1], $31   \n\t"
			: [cop1]"=r"(cop1), [cop2]"=r"(cop2), [stat1]"=r"(stat1), [stat2]"=r"(stat2)
			: [arg]"r"(&arg), [op]"i"(static_cast<u32>(Op))
			: "f0", "f1"
		);
		u32 c1flags = processFlagsCOP1(stat1);
		ok &= cop1 == arg.out && cop2 == arg.out && c1flags == arg.flags.cop1 && stat2 == arg.flags.cop2;
		if (cop1 != arg.out)
			printf("%s %08X / %s%08X%s => %08X != %08X\n", cop1op, arg.in0, wrap0, arg.in1, wrap1, cop1, arg.out);
		if (c1flags != arg.flags.cop1)
			printf("%s %08X / %s%08X%s STATUS FLAGS %s != %s\n", cop1op, arg.in0, wrap0, arg.in1, wrap1, PrintCOP1Flags(c1flags).str, PrintCOP1Flags(arg.flags.cop1).str);
		if (cop2 != arg.out)
			printf("%s %08X / %s%08X%s => %08X != %08X\n", cop2op, arg.in0, wrap0, arg.in1, wrap1, cop2, arg.out);
		if (stat2 != arg.flags.cop2)
			printf("%s %08X / %s%08X%s STATUS FLAGS %s != %s\n", cop2op, arg.in0, wrap0, arg.in1, wrap1, PrintCOP2Status(stat2).str, PrintCOP2Status(arg.flags.cop2).str);
	}
	return ok;
}

static constexpr DivTestArg testsDiv[] = {
	{0x3F800000, 0x3F800000, 0x3F800000, {0x00, 00000}},
	{0x3F800000, 0x7FFFFFFF, 0x00000000, {0x00, 00000}},
	{0x3F800000, 0x00800000, 0x7E800000, {0x00, 00000}},
	{0x40000000, 0x00800000, 0x7F000000, {0x00, 00000}},
	{0x40800000, 0x00800000, 0x7F800000, {0x00, 00000}},
	{0x41000000, 0x00800000, 0x7FFFFFFF, {0x00, 00000}},
	{0x40FFFFFF, 0x00800000, 0x7FFFFFFF, {0x00, 00000}},
	{0x40FFFFFE, 0x00800000, 0x7FFFFFFE, {0x00, 00000}},
	{0x40FFFFFF, 0x7FFFFFFF, 0x00800000, {0x00, 00000}},
	{0x40FFFFFE, 0x7FFFFFFF, 0x00000000, {0x00, 00000}},
	{0x00000000, 0x00000000, 0x7FFFFFFF, {0x88, 02020}},
	{0x007FFFFF, 0x007FFFFF, 0x7FFFFFFF, {0x88, 02020}},
	{0x00800000, 0x007FFFFF, 0x7FFFFFFF, {0x44, 04040}},
	{0x007FFFFF, 0x00800000, 0x00000000, {0x00, 00000}},
	{0x80000000, 0x80000000, 0x7FFFFFFF, {0x88, 02020}},
	{0x007FFFFF, 0x807FFFFF, 0xFFFFFFFF, {0x88, 02020}},
	{0x80800000, 0x007FFFFF, 0xFFFFFFFF, {0x44, 04040}},
	{0x807FFFFF, 0x00800000, 0x80000000, {0x00, 00000}},
	{0x40000000, 0x3FB504F3, 0x3FB504F3, {0x00, 00000}},
	{0x40490FDA, 0x3FB504F2, 0x400E2C19, {0x00, 00000}},
	{0x40490FDA, 0x3FB504F3, 0x400E2C18, {0x00, 00000}},
	{0x40490FDA, 0x3FB504F4, 0x400E2C18, {0x00, 00000}},
};

bool testDiv() {
	return testDivOp<DivOp::Div>("DIV.S", "VDIV", "", "", testsDiv);
}

static constexpr DivTestArg testsRSqrt[] = {
	{0x3F800000, 0x3F800000, 0x3F800000, {0x00, 00000}},
	{0x3F800000, 0x7FFFFFFF, 0x1F3504F3, {0x00, 00000}},
	{0x3F800000, 0xFFFFFFFF, 0x1F3504F3, {0x88, 02020}},
	{0x3F800000, 0x00800000, 0x5F000000, {0x00, 00000}},
	{0x3F800000, 0x007FFFFF, 0x7FFFFFFF, {0x44, 04040}},
	{0x3F800000, 0x807FFFFF, 0x7FFFFFFF, {0xCC, 06060}},
	{0x40490FDA, 0x40000000, 0x400E2C18, {0x00, 00000}},
	{0x40000001, 0x40000001, 0x3FB504F4, {0x00, 00000}},
	{0x40000002, 0x40000002, 0x3FB504F5, {0x00, 00000}},
	{0x40000003, 0x40000003, 0x3FB504F6, {0x00, 00000}},
	{0x40000004, 0x40000004, 0x3FB504F6, {0x00, 00000}},
	{0x40000005, 0x40000005, 0x3FB504F8, {0x00, 00000}},
};

bool testRSqrt() {
	return testDivOp<DivOp::RSqrt>("RSQRT.S", "VRSQRT", "sqrt(", ")", testsRSqrt);
}

static constexpr SqrtTestArg testsSqrt[] = {
	{0x3F800000, 0x3F800000, {0x00, 00000}},
	{0x7FFFFFFF, 0x5FB504F3, {0x00, 00000}},
	{0xFFFFFFFF, 0x5FB504F3, {0x88, 02020}},
	{0x00800000, 0x20000000, {0x00, 00000}},
	{0x007FFFFF, 0x00000000, {0x00, 00000}},
	{0x807FFFFF, 0x00000000, {0x88, 02020}},
	{0x40000000, 0x3FB504F3, {0x00, 00000}},
	{0x40490FDA, 0x3FE2DFC4, {0x00, 00000}},
	{0x40000001, 0x3FB504F4, {0x00, 00000}},
	{0x40000002, 0x3FB504F4, {0x00, 00000}},
	{0x40000003, 0x3FB504F5, {0x00, 00000}},
	{0x40000004, 0x3FB504F6, {0x00, 00000}},
	{0x40000005, 0x3FB504F6, {0x00, 00000}},
};

bool testSqrt() {
	asm volatile("vaddw $vf1, $vf0, $vf0w\n\t"); // Clear mac status flags
	bool ok = true;
	for (const SqrtTestArg& arg : testsSqrt) {
		u32 cop1, cop2, stat1, stat2;
		asm(
			"ctc2   $0,   $16   \n\t"
			"ctc1   $0,   $31   \n\t"
			"qmtc2  %[arg], $vf1\n\t"
			"mtc1   %[arg], $f0 \n\t"
			"vsqrt  $Q, $vf1x   \n\t"
			"sqrt.s $f0, $f0    \n\t"
			"vwaitq             \n\t"
			"cfc2 %[cop2],  $22 \n\t"
			"cfc2 %[stat2], $16 \n\t"
			"mfc1 %[cop1],  $f0 \n\t"
			"cfc1 %[stat1], $31 \n\t"
			: [cop1]"=r"(cop1), [cop2]"=r"(cop2), [stat1]"=r"(stat1), [stat2]"=r"(stat2)
			: [arg]"r"(arg.in)
			: "f0"
		);
		u32 c1flags = processFlagsCOP1(stat1);
		ok &= cop1 == arg.out && cop2 == arg.out && c1flags == arg.flags.cop1 && stat2 == arg.flags.cop2;
		if (cop1 != arg.out)
			printf("SQRT.S %08X => %08X != %08X\n", arg.in, cop1, arg.out);
		if (c1flags != arg.flags.cop1)
			printf("SQRT.S %08X STATUS FLAGS %s != %s\n", arg.in, PrintCOP1Flags(c1flags).str, PrintCOP1Flags(arg.flags.cop1).str);
		if (cop2 != arg.out)
			printf("VSQRT %08X => %08X != %08X\n", arg.in, cop2, arg.out);
		if (stat2 != arg.flags.cop2)
			printf("VSQRT %08X STATUS FLAGS %s != %s\n", arg.in, PrintCOP2Status(stat2).str, PrintCOP2Status(arg.flags.cop2).str);
	}
	return ok;
}

#include <kernel.h>
#include <tamtypes.h>
#include <stdio.h>
#include <string.h>

#include <packet.h>
#include <stdlib.h>
#include <fcntl.h>

struct alignas(16) TestResult {
	u32 inputs[4];
	u32 cop1[4];
	u32 cop2[4];
};

static bool test(u32 multiplicand, u32 base, u32 loops, TestResult* buffer) {
	asm volatile(
		// Register Allocation:
		// $8:   Temporary
		// $9:   Temporary
		// $10:  COP1 Result
		// $11:  COP2 Result
		// $12:  Current Value
		// $13:  Advance (4, 4, 4, 4)
		// $14:  Compare Target (~0)
		// $f1:  Multiplicand
		// $vf1: Multiplicand
		// $f2-$f5:   Temporary
		// $vf2-$vf4: Temporary
		".set noreorder             \n\t"
		"add    $8, %[base], 2      \n\t" // $8 = Base + 2
		"or     $13, $0, 4          \n\t" // Advance = 4
		"pextlw $14, %[multiplicand], %[multiplicand]\n\t"
		"mtc1  %[multiplicand], $f1 \n\t" // Copy multiplicand to COP1
		"add    $9, %[base], 1      \n\t" // $9 = Base + 1
		"mtc1   %[base], $f2        \n\t" // Send  COP2 0x
		"pextlw $14, $14, $14       \n\t" // $14 = (multiplicand, multiplicand, multiplicand, multiplicand)
		"mtc1   $8, $f4             \n\t" // Send  COP2 0z
		"add    $10, %[base], 3     \n\t" // $10 = Base + 3
		"mtc1   $9, $f3             \n\t" // Send  COP2 0y
		"pextlw $8, $8, %[base]     \n\t" // $8 = (Base, Base + 2, x, x)
		"qmtc2  $14, $vf1           \n\t" // Copy multiplicand to COP2
		"pextlw $9, $10, $9         \n\t" // $9 = (Base + 1, Base + 3, x, x)
		"sq     $0,  32(%[scratch]) \n\t" // Zero  COP1 2
		"pextlw $13, $13, $13       \n\t" // Advance = (4, 4, x, x)
		"mul.s  $f2, $f1, $f2       \n\t" // Calc  COP1 0x
		"pextlw $12, $9, $8         \n\t" // Current = (Base, Base + 1, Base + 2, Base + 3)
		"mtc1   $10, $f5            \n\t" // Send  COP2 0w
		"pextlw $13, $13, $13       \n\t" // Advance = (4, 4, 4, 4)
		"mul.s  $f3, $f1, $f3       \n\t" // Calc  COP1 0y
		"por    $8,  $0,  $0        \n\t" // Zero  COP2 2
		"qmtc2  $12, $vf2           \n\t" // Send  COP2 0
		"paddw  $12, $12, $13       \n\t" // Next       1
		"vmul   $vf2, $vf1, $vf2    \n\t" // Calc  COP2 0
		"add   %[loops], %[loops], 1\n\t" // We need one extra loop
		"sq     $12, 16(%[scratch]) \n\t" // Store COP1 1
		"nor    $14, $0,  $0        \n\t" // Set up $14

		"\n0:\n\t"
		"swc1  $f2,  0(%[scratch])\n\t" // Store COP1 0x
		"lq    $10, 32(%[scratch])\n\t" // Load  COP1 2
		"mul.s $f4, $f1, $f4      \n\t" // Calc  COP1 0z
		"por   $11, $8,  $0       \n\t" // Save  COP2 2
		"lwc1  $f2, 16(%[scratch])\n\t" // Load  COP1 1x
		"pceqw $9,  $8,  $10      \n\t" // Compare    2
		"swc1  $f3,  4(%[scratch])\n\t" // Store COP1 0y
		"qmtc2 $12, $vf3          \n\t" // Send  COP2 1
		"mul.s $f5, $f1, $f5      \n\t" // Calc  COP1 0w
		"ppach $9,  $9,  $9       \n\t" // Pack       2
		"lwc1  $f3, 20(%[scratch])\n\t" // Load  COP1 1y
		"paddw $12, $12, $13      \n\t" // Next       2
		"swc1  $f4,  8(%[scratch])\n\t" // Store COP1 0z
		"qmfc2 $8,  $vf2          \n\t" // Recv  COP2 0
		"mul.s $f2, $f1, $f2      \n\t" // Calc  COP1 1x
		"bne   $9,  $14, 1f       \n\t" // Check      2
		"lwc1  $f4, 24(%[scratch])\n\t" // Load  COP1 1z
		"vmul  $vf3, $vf1, $vf3   \n\t" // Calc  COP2 1
		"swc1  $f5, 12(%[scratch])\n\t" // Store COP1 0w
		"sq    $12, 32(%[scratch])\n\t" // Store COP1 2
		"mul.s $f3, $f1, $f3      \n\t" // Calc  COP1 1y
		"lwc1  $f5, 28(%[scratch])\n\t" // Load  COP1 1w

		"swc1  $f2, 16(%[scratch])\n\t" // Store COP1 1x
		"lq    $10,  0(%[scratch])\n\t" // Load  COP1 0
		"mul.s $f4, $f1, $f4      \n\t" // Calc  COP1 1z
		"por   $11, $8,  $0       \n\t" // Save  COP2 0
		"lwc1  $f2, 32(%[scratch])\n\t" // Load  COP1 2x
		"pceqw $9,  $8,  $10      \n\t" // Compare    0
		"swc1  $f3, 20(%[scratch])\n\t" // Store COP1 1y
		"qmtc2 $12, $vf4          \n\t" // Send  COP2 2
		"mul.s $f5, $f1, $f5      \n\t" // Calc  COP1 1w
		"ppach $9,  $9,  $9       \n\t" // Pack       0
		"lwc1  $f3, 36(%[scratch])\n\t" // Load  COP1 2y
		"paddw $12, $12, $13      \n\t" // Next       0
		"swc1  $f4, 24(%[scratch])\n\t" // Store COP1 1z
		"qmfc2 $8,  $vf3          \n\t" // Recv  COP2 1
		"mul.s $f2, $f1, $f2      \n\t" // Calc  COP1 2x
		"bne   $9,  $14, 1f       \n\t" // Check      0
		"lwc1  $f4, 40(%[scratch])\n\t" // Load  COP1 2z
		"vmul  $vf4, $vf1, $vf4   \n\t" // Calc  COP2 2
		"swc1  $f5, 28(%[scratch])\n\t" // Store COP1 1w
		"sq    $12,  0(%[scratch])\n\t" // Store COP1 0
		"mul.s $f3, $f1, $f3      \n\t" // Calc  COP1 2y
		"sub %[loops], %[loops], 1\n\t" // Decrement Loop Counter
		"lwc1  $f5, 44(%[scratch])\n\t" // Load  COP1 2w

		"swc1  $f2, 32(%[scratch])\n\t" // Store COP1 2x
		"lq    $10, 16(%[scratch])\n\t" // Load  COP1 1
		"mul.s $f4, $f1, $f4      \n\t" // Calc  COP1 2z
		"por   $11, $8,  $0       \n\t" // Save  COP2 1
		"lwc1  $f2,  0(%[scratch])\n\t" // Load  COP1 0x
		"pceqw $9,  $8,  $10      \n\t" // Compare    1
		"swc1  $f3, 36(%[scratch])\n\t" // Store COP1 2y
		"qmtc2 $12, $vf2          \n\t" // Send  COP2 0
		"mul.s $f5, $f1, $f5      \n\t" // Calc  COP1 2w
		"ppach $9,  $9,  $9       \n\t" // Pack       1
		"lwc1  $f3,  4(%[scratch])\n\t" // Load  COP1 0y
		"paddw $12, $12, $13      \n\t" // Next       1
		"swc1  $f4, 40(%[scratch])\n\t" // Store COP1 2z
		"qmfc2 $8,  $vf4          \n\t" // Recv  COP2 2
		"mul.s $f2, $f1, $f2      \n\t" // Calc  COP1 0x
		"bne   $9,  $14, 1f       \n\t" // Check      1
		"lwc1  $f4,  8(%[scratch])\n\t" // Load  COP1 0z
		"vmul  $vf2, $vf1, $vf2   \n\t" // Calc  COP2 1
		"swc1  $f5, 44(%[scratch])\n\t" // Store COP1 2w
		"sq    $12, 16(%[scratch])\n\t" // Store COP1 1
		"mul.s $f3, $f1, $f3      \n\t" // Calc  COP1 0y
		"bne   %[loops], $0, 0b   \n\t" // Jump to Loop Start
		"lwc1  $f5, 12(%[scratch])\n\t" // Load  COP1 0w

		"beq $0, $0, 2f\n\t" // Jump to end
		"nop           \n\t"

		"\n1:\n\t"                   // $12 has been advanced 3x past this value, back it up
		"paddw $9, $13, $13    \n\t" // $9 = 2x Advance
		"sq $10, 16(%[scratch])\n\t" // Store COP1
		"psubw $8, $12, $13    \n\t" // Subtract 1x Advance
		"sq $11, 32(%[scratch])\n\t" // Store COP2
		"psubw $8, $8,  $9     \n\t" // Subtract 2x Advance
		"sq $8,   0(%[scratch])\n\t" // Store Input 
		
		"\n2:\n\t"
		".set reorder\n\t"
		: [loops]"+r"(loops), [base]"+r"(base)
		: [scratch]"r"(buffer), [multiplicand]"r"(multiplicand)
		: "memory", "f1", "f2", "f3", "f4", "f5", "8", "9", "10", "11", "12", "13", "14"
	);
	return loops;
}

static const u32 tests[] = {
	0x3f800000,
	0x3f7fffff,
	0x80000000,
	0x00000000,
	0x7fffffff,
	0x7f800000,
	0xffffffff,
	0xff800000,
	0x3faaaaaa,
	0x3f955555,
	0x00800000,
	0x00ffffff,
	0x3f924924,
	0x40249249,
	0x40c92492,
	0x50db6db6,
	0x60b6db6d,
	0x70edb6db,
	0x71888888,
	0x7fcccccc,
	0xbff0f0f0,
	0xbfff00ff,
};

int main(void) {
	TestResult res;
	for (u32 t : tests) {
		printf("Testing %x\n", t);
		if (test(t, 0x00000000, ((1ull << 32) + 12ull) / 12ull, &res)) {
			for (int i = 0; i < 4; i++) {
				printf("%08x * %08x = %08x[COP1], %08x[COP2]\n", t, res.inputs[i], res.cop1[i], res.cop2[i]);
			}
		}
	}
	printf("Sleeping\n");
	SleepThread();
}

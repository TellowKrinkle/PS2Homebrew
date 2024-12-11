#include <kernel.h>
#include <tamtypes.h>
#include <stdio.h>
#include <string.h>

#include <packet.h>
#include <stdlib.h>
#include <fcntl.h>
#include <iterator>

struct alignas(16) MemoryVector {
	u32 value[4];
	MemoryVector() = default;
	constexpr MemoryVector(u32 a, u32 b, u32 c, u32 d): value{a, b, c, d} {}
	constexpr MemoryVector(u32 splat): MemoryVector(splat, splat, splat, splat) {}
};

// We're super ALU constrained.
// PS2 can do one vector ALU op per cycle, leaving one op per cycle for literally anything else.
// Anything that can be done by the LSU is pretty much free.
struct TestConfig {
	struct {
		MemoryVector shift;  // ~0u to multiply by two, 0 to not
		MemoryVector negate; // xor'd with value to negate if necessary
		MemoryVector zero;   // and'd with value to zero, mask, or neither
	} booth[8];
	MemoryVector negbit[3];  // The extra bit from negation of booth 5, 6, and 7
	MemoryVector b;
	// Add these values to transform integers to floats
	MemoryVector ai2f;
	MemoryVector bi2f;
	MemoryVector ci2f;
	MemoryVector c00000400{0x00000400};
	MemoryVector c00000800{0x00000800};
	MemoryVector cfffff000{0xfffff000};
	MemoryVector cffff8000{0xffff8000};
};

struct alignas(16) TestResult {
	u32 a[4];
	u32 b[4];
	u32 cop2[4];
	u32 expected[4];
};

#if 0
// Assembly algorithm with nice register names and tracking of what executes in the same cycle
// Variable names:
// - bx: Booth result for partial product x
// - lx: Low part of xth CSA application
// - hx: High part of xth CSA application
// - tx: Arbitrary temporary variable
0:
	paddw  af, a, ai2f     | load shift1
	psllw  b1, a, 2
	pand   t0, b1, shift1  | load negate1
	paddw  b1, b1, t0      | load zero1
	pxor   b1, b1, negate1 | qmtc2 af
	pand   b1, b1, zero1   | load shift2

	psllw  b2, a, 4
	pand   t0, b2, shift2  | load negate2
	paddw  b2, b2, t0      | load zero2
	pxor   b2, b2, negate2
	pand   b2, b2, zero2   | load shift3

	psllw  b3, a, 6
	pand   t0, b3, shift3  | load negate3
	paddw  b3, b3, t0      | load zero3
	pxor   b3, b3, negate3
	pand   b3, b3, zero3

	pxor   t0, b1, b2
	pand   b1, b1, b2
	pxor   l0, t0, b3
	pand   t0, t0, b3
	por    h0, t0, b1
	psllw  h0, h0, 1       | load shift4

	psllw  b4, a, 8
	pand   t0, b4, shift4  | load negate4
	paddw  b4, b4, t0      | load zero4   // zero4 includes ~0x7ff mask if nonzero
	pxor   b4, b4, negate4
	pand   b4, b4, zero4   | load shift5

	psllw  b5, a, 10
	pand   t0, b5, shift5  | load negate5
	paddw  b5, b5, t0      | load zero5
	pxor   b5, b5, negate5
	pand   b5, b5, zero5   | load shift6

	psllw  b6, a, 12
	pand   t0, b6, shift6  | load negate6
	paddw  b6, b6, t0      | load zero6
	pxor   b6, b6, negate6 | load ~0xfff
	pand   b6, b6, zero6

	pand   t1, b5, ~0xfff
	pxor   t0, b4, t1
	pand   b4, b4, t1
	pxor   l1, t0, b6
	pand   t0, t0, b6
	por    h1, t0, b4      | load 0x800
	psllw  h1, h1, 1       | load negbit6

	pand   t0, b5, 0x800
	por    t0, t0, negbit6 | load 0x400
	por    h1, h1, t0      | load negbit5
	pand   b5, b5, 0x400
	paddw  b5, b5, negbit5 | load shift7

	psllw  b7, a, 14
	pand   t0, b7, shift7  | load negate7
	paddw  b7, b7, t0      | load zero7
	pxor   b7, b7, negate7
	pand   b7, b7, zero7
	por    b7, b7, b5

	pxor   t0, l1, h1
	pand   l1, l1, h1
	pxor   l3, t0, b7
	pand   t0, t0, b7
	por    h3, t0, l1      | load shift0
	psllw  h3, h3, 1

	pand   t0, a,  shift0  | load negate0
	paddw  b0, a,  t0      | load zero0
	pxor   b0, b0, negate0
	pand   b0, b0, zero0

	pxor   t0, l0, h0
	pand   l0, l0, h0
	pxor   l2, t0, b0
	pand   t0, t0, b0
	por    h2, t0, l0
	psllw  h2, h2, 1

	pxor   t0, l3, h3
	pand   l3, l3, h3
	pxor   l4, t0, h2
	pand   t0, t0, h2
	por    h4, t0, l3
	psllw  h4, h4, 1

	pextlw  t1, $0, a
	pmultuw t1, t1, b
	pextuw  t2, $0, a
	pmultuw t2, t2, b

	pxor   t0, l4, h4
	vmul   cf, af, bf      | Weird pmultuw aftereffects (wide integer insts delay here)
	pand   l4, l4, h4
	pxor   l5, t0, l2
	pand   t0, t0, l2
	por    h5, t0, l4
	psllw  h5, h5, 1

	ppacw  t0, t2, t1
	qfsrv  t1, t1, t1
	qfsrv  t2, t2, t2      | load negbit7
	ppacw  t1, t2, t1      | load ~0x7fff

	paddw  h5, h5, negbit7
	pand   h5, h5, ~0x7fff | qmfc2 cf
	paddw  h5, h5, l5
	pnor   t0, t0, t0
	pand   h5, h5, t0
	psllw  h5, h5, 16
	psraw  h5, h5, 31      | load ci2f
	paddw  m0, t1, h5

	psrlw  m0, m0, shift
	paddw  m0, m0, ci2f
	pceqw  t0, m0, c
	ppach  t0, t0, t0      | load aflt

	bne    t0, ~0, 2f      | 1: sub loops, loops, 1
	bne    loops, $0, 0b   | next a

	beq $0, $0, 3f         | nop

2:
	sq  a,   0(out)
	sq  b,  16(out)
	sq  c,  32(out)
	sq  m0, 48(out)
	beq $0, $0, 1b
	add out, out, 64

3:
#endif

template <int Shift>
static TestResult* testasm(u32 base, u32 loops, TestResult* output, TestResult* oend, const TestConfig& config) {
	oend--;
	asm volatile(
		// Register Allocation:
		// $8: Temporary
		// $9: A
		// $10-$15: Temporary
		// $16: B
		// $17: Step
		// $18: ~0ull
		// $24-$25: Temporary
		// $vf1: Af
		// $vf2: Bf
		// $vf3: Cf
		".set noreorder\n\t"
		"mtsab  $0, 2               \n\t" // Prepare funnel shift for >> 16
		"ori    $17, $0,  4         \n\t" // $17 = 4
		"nor    $18, $0,  $0        \n\t" // $18 = ~0ull
		"add    $9,  %[base], 2     \n\t"
		"add    $8,  %[base], 1     \n\t"
		"add    $10, %[base], 3     \n\t"
		"lq     $16,  432(%[config])\n\t" // Load  B
		"pextlw $9,  $9,  %[base]   \n\t" // $9  = (base + 0, base + 2)
		"lq     $24,  464(%[config])\n\t" // Load bi2f
		"pextlw $8,  $10, $8        \n\t" // $8  = (base + 1, base + 3)
		"lq     $25,  448(%[config])\n\t" // Load ai2f
		"pextlw $9,  $8,  $9        \n\t" // $9  = A = (base, base + 1, base + 2, base + 3)
		"paddw  $24, $24, $16       \n\t" // $24 = Bf
		"pextlw $17, $17, $17       \n\t" // $17 = (4, 4)
		"qmtc2  $24, $vf2           \n\t" // $vf2 = Bf
		"pextlw $17, $17, $17       \n\t" // $17 = (4, 4, 4, 4)

		"\n0:\n\t"
		// Booth 1 -> $11
		"paddw  $15, $9,  $25      \n\t" // Calc   Af
		"lq     $24,  48(%[config])\n\t" // Load   1 Shift
		"psllw  $11, $9,   2       \n\t" // Shift  1
		"pand   $8,  $11, $24      \n\t"
		"lq     $24,  64(%[config])\n\t" // Load   1 Negate
		"paddw  $11, $11, $8       \n\t" // Double 1
		"lq     $25,  80(%[config])\n\t" // Load   1 Zero
		"pxor   $11, $11, $24      \n\t" // Negate 1
		"qmtc2  $15, $vf1          \n\t" // Send   Af
		"pand   $11, $11, $25      \n\t" // Zero   1
		"lq     $24,  96(%[config])\n\t" // Load   2 Shift

		// Booth 2 -> $12
		"psllw  $12, $9,   4       \n\t" // Shift  2
		"pand   $8,  $12, $24      \n\t"
		"lq     $24, 112(%[config])\n\t" // Load   2 Negate
		"paddw  $12, $12, $8       \n\t" // Double 2
		"lq     $25, 128(%[config])\n\t" // Load   2 Zero
		"pxor   $12, $12, $24      \n\t" // Negate 2
		"pand   $12, $12, $25      \n\t" // Zero   2
		"lq     $24, 144(%[config])\n\t" // Load   3 Shift

		// Booth 3 -> $13
		"psllw  $13, $9,   6       \n\t" // Shift  3
		"pand   $8,  $13, $24      \n\t"
		"lq     $24, 160(%[config])\n\t" // Load   3 Negate
		"paddw  $13, $13, $8       \n\t" // Double 3
		"lq     $25, 176(%[config])\n\t" // Load   3 Zero
		"pxor   $13, $13, $24      \n\t" // Negate 3
		"pand   $13, $13, $25      \n\t" // Zero   3

		// CSA $11 + $12 + $13 -> $10 $11
		"pxor   $8,  $11, $12      \n\t"
		"pand   $11, $11, $12      \n\t"
		"pxor   $10, $8,  $13      \n\t" // CSA    0 Low
		"pand   $8,  $8,  $13      \n\t"
		"por    $11, $11, $8       \n\t"
		"psllw  $11, $11, 1        \n\t" // CSA    0 High
		"lq     $24, 192(%[config])\n\t" // Load   4 Shift

		// Booth 4 -> $13
		"psllw  $13, $9,   8       \n\t" // Shift  4
		"pand   $8,  $13, $24      \n\t"
		"lq     $24, 208(%[config])\n\t" // Load   4 Negate
		"paddw  $13, $13, $8       \n\t" // Double 4
		"lq     $25, 224(%[config])\n\t" // Load   4 Zero
		"pxor   $13, $13, $24      \n\t" // Negate 4
		"pand   $13, $13, $25      \n\t" // Zero   4
		"lq     $24, 240(%[config])\n\t" // Load   5 Shift

		// Booth 5 -> $14
		"psllw  $14, $9,  10       \n\t" // Shift  5
		"pand   $8,  $14, $24      \n\t"
		"lq     $24, 256(%[config])\n\t" // Load   5 Negate
		"paddw  $14, $14, $8       \n\t" // Double 5
		"lq     $25, 272(%[config])\n\t" // Load   5 Zero
		"pxor   $14, $14, $24      \n\t" // Negate 5
		"pand   $14, $14, $25      \n\t" // Zero   5
		"lq     $24, 288(%[config])\n\t" // Load   6 Shift

		// Booth 6 -> $15
		"psllw  $15, $9,  12       \n\t" // Shift  6
		"pand   $8,  $15, $24      \n\t"
		"lq     $24, 304(%[config])\n\t" // Load   6 Negate
		"paddw  $15, $15, $8       \n\t" // Double 6
		"lq     $25, 320(%[config])\n\t" // Load   6 Zero
		"pxor   $15, $15, $24      \n\t" // Negate 6
		"lq     $24, 528(%[config])\n\t" // Load   0xfffff000
		"pand   $15, $15, $25      \n\t" // Zero   6

		// CSA $13 + $14 + $15 -> $12 $13
		"pand   $12, $14, $24      \n\t"
		"pxor   $8,  $13, $12      \n\t"
		"pand   $13, $13, $12      \n\t"
		"pxor   $12, $8,  $15      \n\t" // CSA    1 Low
		"pand   $8,  $8,  $15      \n\t"
		"por    $13, $13, $8       \n\t"
		"lq     $24, 512(%[config])\n\t" // Load   0x00000800
		"psllw  $13, $13, 1        \n\t" // CSA    1 High
		"lq     $25, 400(%[config])\n\t" // Load   6 Bit

		// Add low bits of booth 5 to $13
		"pand   $8,  $14, $24      \n\t"
		"por    $8,  $8,  $25      \n\t"
		"lq     $24, 496(%[config])\n\t" // Load   0x00000400
		"por    $13, $13, $8       \n\t"
		"lq     $25, 384(%[config])\n\t" // Load   5 Bit
		"pand   $14, $14, $24      \n\t"
		"paddw  $14, $14, $25      \n\t"
		"lq     $24, 336(%[config])\n\t" // Load   7 Shift

		// Booth 7 -> $15
		"psllw  $15, $9,  14       \n\t" // Shift  7
		"pand   $8,  $15, $24      \n\t"
		"lq     $24, 352(%[config])\n\t" // Load   7 Negate
		"paddw  $15, $15, $8       \n\t" // Double 7
		"lq     $25, 368(%[config])\n\t" // Load   7 Zero
		"pxor   $15, $15, $24      \n\t" // Negate 7
		"pand   $15, $15, $25      \n\t" // Zero   7
		"por    $15, $15, $14      \n\t"

		// CSA $12 + $13 + $15 -> $12 $13
		"pxor   $8,  $12, $13      \n\t"
		"pand   $13, $12, $13      \n\t"
		"pxor   $12, $8,  $15      \n\t" // CSA    3 Low
		"pand   $8,  $8,  $15      \n\t"
		"por    $13, $13, $8       \n\t"
		"lq     $24,   0(%[config])\n\t" // Load   0 Shift
		"psllw  $13, $13, 1        \n\t" // CSA    3 High

		// Booth 0 -> $14
		"pand   $8,  $9,  $24      \n\t" // Shift  0
		"lq     $24,  16(%[config])\n\t" // Load   0 Negate
		"paddw  $14, $9,  $8       \n\t" // Double 0
		"lq     $25,  32(%[config])\n\t" // Load   0 Zero
		"pxor   $14, $14, $24      \n\t" // Negate 0
		"pand   $14, $14, $25      \n\t" // Zero   0

		// CSA $10 + $11 + $14 -> $10 $11
		"pxor   $8,  $10, $11      \n\t"
		"pand   $11, $10, $11      \n\t"
		"pxor   $10, $8,  $14      \n\t" // CSA    2 Low
		"pand   $8,  $8,  $14      \n\t"
		"por    $11, $11, $8       \n\t"
		"psllw  $11, $11, 1        \n\t" // CSA    2 High

		// CSA $11 + $12 + $13 -> $11 $12
		"pxor   $8,  $11, $12      \n\t"
		"pand   $12, $11, $12      \n\t"
		"pxor   $11, $8,  $13      \n\t" // CSA    4 Low
		"pand   $8,  $8,  $13      \n\t"
		"por    $12, $12, $8       \n\t"
		"psllw  $12, $12, 1        \n\t" // CSA    4 High

		// Multiply a * b -> $14 $15
		"pextlw  $14, $0,  $9      \n\t"
		"pmultuw $14, $14, $16     \n\t"
		"pextuw  $15, $0,  $9      \n\t"
		"pmultuw $15, $15, $16     \n\t"

		// CSA $10 + $11 + $12 -> $10 $11
		"pxor   $8,  $10, $11      \n\t"
		"vmul   $vf3, $vf1, $vf2   \n\t" // Mul    Cf
		"pand   $11, $10, $11      \n\t"
		"pxor   $10, $8,  $12      \n\t" // CSA    5 Low
		"pand   $8,  $8,  $12      \n\t"
		"por    $11, $11, $8       \n\t"
		"psllw  $11, $11, 1        \n\t" // CSA    5 High

		// Pack multiply results ($13: Low, $14: High)
		"ppacw  $13, $15, $14      \n\t" // $13 = RealLo
		"qfsrv  $14, $14, $14      \n\t"
		"qfsrv  $15, $15, $15      \n\t"
		"lq     $24, 416(%[config])\n\t" // Load   7 Bit
		"ppacw  $14, $15, $14      \n\t" // $14 = RealMid
		"lq     $25, 544(%[config])\n\t" // Load   0xffff8000

		// Combine CSA results and adjust final result
		"paddw  $11, $11, $24      \n\t"
		"pand   $11, $11, $25      \n\t"
		"paddw  $11, $11, $10      \n\t" // $11 = PS2Lo16
		"qmfc2  $15, $vf3          \n\t" // $15 = COP2Flt
		"pnor   $13, $13, $13      \n\t" // $13 = ~RealLo
		"pand   $11, $11, $13      \n\t" // $11 = PS2Lo16 & ~RealLo
		"psllw  $11, $11, 16       \n\t"
		"psraw  $11, $11, 31       \n\t" // $11 = (PS2Lo16 & ~RealLo & 0x8000) ? -1 : 0
		"lq     $24, 480(%[config])\n\t" // $24 = ci2f
		"paddw  $11, $11, $14      \n\t" // $11 = PS2Mid = RealMid - ((PS2 & 0x8000) != (Real & 0x8000) ? 1 : 0)

		"psrlw  $11, $11, %[shift] \n\t" // $11 = PS2Hi
		"paddw  $11, $11, $24      \n\t" // $11 = PS2Flt
		"pceqw  $10, $11, $15      \n\t" // $10 = PS2Flt == COP2Flt
		"ppach  $10, $10, $10      \n\t"
		"lq     $25, 448(%[config])\n\t" // Load ai2f
		"bne    $10, $18, 2f       \n"   // if (any(PS2Mid != COP2Mid)) goto write
		"1:\t"
		"sub  %[loops], %[loops], 1\n\t"
		"bne  %[loops], $0, 0b     \n\t" // if (loops) goto loop
		"paddw  $9,  $9,  $17      \n\t" // a += step

		"beq    $0,  $0, 3f\n\t"
		"nop               \n\t"

		"\n2:\n"
		"sqc2 $vf1,  0(%[out])  \n\t"
		"sqc2 $vf2, 16(%[out])  \n\t"
		"sqc2 $vf3, 32(%[out])  \n\t"
		"sq   $11,  48(%[out])  \n\t"
		"bne %[out], %[oend], 1b\n\t"
		"add %[out], %[out], 64 \n\t"

		"\n3:\n"
		".set reorder\n\t"

		: [loops]"+r"(loops), [base]"+r"(base), [out]"+r"(output)
		: [config]"r"(&config), [oend]"r"(oend), [shift]"i"(Shift)
		: "memory", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "24", "25"
	);
	return output;
}

static u32 test(u32 b) {
	u32 fails = 0;
	TestConfig config;
	config.b = b;
	config.ai2f  = 0x3f000000;
	config.bi2f  = 0x3f000000;
	config.ci2f  = 0x3f000000;
	for (u32 i = 0; i < 8; i++) {
		u32 test = ((b << 1) >> (i * 2)) & 7;
		u32 bit = 1 << (i * 2);
		u32 shift = test == 3 || test == 4 ? ~0u : 0;
		u32 neg   = test >= 4 && test <= 6 ? ~0u : 0;
		u32 zero  = test >= 1 && test <= 6 ? ~0u : 0;
		if (i == 4) { zero &= ~0x7ffu; }
		config.booth[i].shift  = shift;
		config.booth[i].negate = neg & -bit;
		config.booth[i].zero   = zero;
		if (i >= 5) { config.negbit[i - 5] = neg & bit; }
	}
	u32 crossover = static_cast<u32>(((1ull << 47) - 1) / b);
	static TestResult results[4096];
	// Due to bit loss, the actual crossover may be crossover + 1, so add one extra in case
	u32 loops = ((crossover - 0x800000) + 4) / 4;
	TestResult* res = testasm<7>(0x800000, loops, results, std::end(results), config);
	for (TestResult* r = results; r < res; r++) {
		for (u32 i = 0; i < 4; i++) {
			if (r->cop2[i] == r->expected[i] || r->expected[i] > 0x3fffffff) {
				continue;
			}
			fails++;
			printf("\t%08x * %08x = %08x[COP2] != %08x[EMU]\n", r->a[i], r->b[i], r->cop2[i], r->expected[i]);
		}
	}
	u32 base = crossover + 1;
	loops = ((1 << 24) - base + 3) / 4;
	config.ci2f = 0x3f800000;
	res = results;
	if (loops)
		res = testasm<8>(base, loops, res, std::end(results), config);
	for (TestResult* r = results; r < res; r++) {
		for (u32 i = 0; i < 4; i++) {
			if (r->cop2[i] == r->expected[i] || r->expected[i] <= 0x3fffffff || r->a[i] > 0xffffff) {
				continue;
			}
			fails++;
			printf("\t%08x * %08x = %08x[COP2] != %08x[EMU]\n", r->a[i], r->b[i], r->cop2[i], r->expected[i]);
		}
	}
	return fails;
}

int main(int argc, const char* argv[]) {
	u64 fails = 0;
	u32 b = 1 << 23;
	if (argc > 1) {
		char* res;
		unsigned long req = strtoul(argv[1], &res, 0);
		if (res != argv[1] && req >= (1 << 23) && req < (1 << 24)) {
			b = req;
		} else {
			printf("Bad start point %s, ignoring\n", argv[1]);
		}
	}
	for (; b < (1 << 24); b++) {
		if ((b & 15) == 0) {
			printf("[Fails: %lld] Testing %06x...\n", fails, b);
		}
		fails += test(b);
	}
	printf("Sleeping\n");
	SleepThread();
}

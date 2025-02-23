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

struct ConstantData {
	MemoryVector one{1};
	MemoryVector advance{4};
	MemoryVector exp127{0x3f800000};
	MemoryVector exp255{0x7f800000};
	MemoryVector adj0{~(1u << 24)};
	MemoryVector testP{(1 << 23) - 1};
	MemoryVector mask{(1 << 24) - 1};
	MemoryVector pad{};
	MemoryVector bit[26][2];

	constexpr ConstantData() : bit{} {
		for (uint32_t i = 0; i < 26; i++) {
			bit[i][0] = MemoryVector( 1u << i);
			bit[i][1] = MemoryVector(~0u << i);
		}
	}
};

#if 0
// Assembly algorithm with nice register names and tracking of what executes in the same cycle
0:
	addiu buf, bufbase, 0
	addiu cur, const, offsetof 1 << 22
	pand  cs1, val, 1 << 23 | lq cs0, mantissa(const)
	pceqw cs1, cs1, $0      | lq q1,  adj0(const)
	pand  cs0, cs0, val
	por   cs0, cs0, 1 << 23
	pand  cs1, cs1, cs0     | lq qbn, one(const)
	paddw cs0, cs0, cs1

	pxor  qbp, one, cs0
	pxor  cs1, qbp, q1
	pand  cc1, qbp, q1  | lq t0, (1 << 24) - 1
	paddw cc1, cc1, cc1 | lq q0, ~0u << 24

	pand  qbp, cs1, t0
	pand  cs0, cs1, q0
	paddw cs0, cs0, cc1 | lq t0,  (1 << 23) - 1
	por   cs0, cs0, qbp | lq qb,  1 << 24

	pcgtw qbp, cs0, t0  | lq q0,  1 << 25
	pand  qb,  qb,  qbp
	paddw q1,  q0,  qb

	psllw cs0, cs1, 1
	psllw cc0, cc1, 1
	psraw qb,  qb,  1
	paddw q0,  q0,  qb
	pnor  q0,  q0,  q0
	pand  q0,  q0,  qbp
	pand  qbn, qbp, one
	paddw cc0, cc0, qbn

	pxor  qbn, cc0, cs0
	pxor  cs1, qbn, q0
	pand  cc1, qbn, q0
	pand  qbn, cc0, cs0
	por   cc1, cc1, qbn
	paddw cc1, cc1, cc1

	pnor  qbn, qbp, qbp
	pand  cs0, cs0, qbn
	pand  cc0, cc0, qbn
	pand  qbn, cs1, qbp
	por   cs0, cs0, qbn
	pand  qbn, cc1, qbp | lq t0, (1 << 24) - 1
	por   cc0, cc0, qbn | lq q1, ~0 << 24

	pand  qbp, cs0, t0
	pand  cs0, cs0, q1
	paddw cs0, cs0, cc0 | lq t0,  (1 << 23) - 1
	por   cs0, cs0, qbp

	pcgtw qbp, cs0, t0  | lq qb,   1 << 23
	pcgtw qbn, q1,  cs0 | lq t0,  ~0 << 23
	pand  qb,  qb,  qbp
	pand  q1,  t0,  qbn
	por   qb,  qb,  q1

	paddw q0,  q1,  qb
1:
	addiu cur, cur, -64 | vsqrt $Q, $vf1x
	psllw cs0, cs1, 1
	psllw cc0, cc1, 1
	psraw qb,  qb,  1
	paddw q1,  q1,  qb
	por   qbn, qbn, qbp
	pxor  q1,  q1,  qbp
	pand  q1,  q1,  qbn
	pand  qbp, qbp, one
	paddw cc0, cc0, qbp

	pxor  qbp, cc0, cs0
	pxor  cs1, qbp, q1
	pand  cc1, qbp, q1
	pand  qbp, cc0, cs0
	por   cc1, cc1, qbp
	paddw cc1, cc1, cc1

	pnor  qbp, qbn, qbn | cfc2 q1, $22
	pand  cs0, cs0, qbp
	pand  cc0, cc0, qbp
	pand  qbp, cs1, qbn
	por   cs0, cs0, qbp | sw   q1, 0(buf)
	pand  qbp, cc1, qbn | lq   t0, (1 << 24) - 1
	por   cc0, cc0, qbp | lq   q1, ~0u << 24

	pand  qbp, cs0, t0
	pand  cs0, cs0, q1
	paddw cs0, cs0, cc0 | lq   t0,  (1 << 23) - 1
	por   cs0, cs0, qbp

	pcgtw qbp, cs0, t0  | lq   qb,  32(cur)
	pcgtw qbn, q1,  cs0 | lq   cc0, 48(cur)

	pand  qb,  qb,  qbp
	pand  q1,  cc0, qbn | lq  cs0, exp255(const)
	por   qb,  qb,  q1

	addiu buf, buf, 4   | vmr32 $vf1, $vf1
	paddw q1,  q0,  qb

	; Second time, but swap q1 and q0 (and no vsqrt)
	bne   cur, end, 1b

	pand  cc0, cs0, val | lq cs1, exp127(const) ; cc0 = exponent(val)
	pceqw cc1, cc0, $0                          ; cc1 = exponent(val) == 0
	pnor  cc1, cc1, cc1                         ; cc1 = exponent(val) != 0
	paddw cc0, cc0, cs1                         ; cc0 = exponent(val) + 127
	psrlw cc0, cc0, 1
	pand  cc0, cc0, cs0                         ; cc0 = (exponent(val) + 127) >> 1
	psrlw q0,  q0,  2
	pand  q0,  q0,  t0                          ; q0  = mantissa(result)
	por   q0,  q0,  cc0 | lq q1,  0(bufbase)
	pand  q0,  q0,  cc1 | lq cs0, advance(const)
	pceqw cc0, q0,  q1
	ppach cc0, cc0, cc0
	bne   cc0, -1,  3f

2:
	addiu loops, loops, -1
	paddw val, val, cs0 | bne loops, $0, 0b
	b 4f

3:
	sq val, 0(out)
	sq q0, 16(out)
	sq q1, 32(out)
	add out, out, 48
	bne out, oend, 2b

4:
#endif

struct alignas(16) TestResult {
	u32 val[4];
	u32 expected[4];
	u32 cop2[4];
};

static TestResult* testasm(u32 base, u32 loops, TestResult* output, TestResult* oend) {
	static constexpr ConstantData constants;
	u32 tmp[12];
	oend--;
	asm volatile(
		// Register Allocation
		// $8:  Quotient 0
		// $9:  Quotient 1
		// $10: Sum   0
		// $11: Carry 0
		// $12: Sum   1
		// $13: Carry 1
		// $14: Quotient Bit Positive
		// $15: Quotient Bit Negative
		// $16: Quotient Bit
		// $17: Temporary
		// $18: ~0ull
		// $24: Temp Buffer Write
		// $25: Constant Read
		".set noreorder\n\t"
		"addiu  $9,  %[base], 2     \n\t"
		"addiu  $8,  %[base], 1     \n\t"
		"addiu  $10, %[base], 3     \n\t"
		"nor    $18, $0,  $0        \n\t" // $18 = ~0ull
		"pextlw %[base], $9, %[base]\n\t"
		"lq     $17, 736(%[cbuf])   \n\t" // $17 = 1 << 23
		"pextlw $8,  $10, $8        \n\t"
		"pextlw %[base], $8, %[base]\n\t" // base = (base, base + 1, base + 2, base + 3)

		"\n0:\n\t"
		"addiu $24, %[bufbase], 0\n\t" // $24 = tmp
		"addiu $25, %[cbuf], 704 \n\t" // $25 = &const.bit[22]
		"lq    $11, -48(%[cbuf]) \n\t" // $10 = 0x007fffff
		"pand  $12, $17, %[base] \n\t" // $12 = val & 0x800000
		"lq    $8,  -64(%[cbuf]) \n\t" // $8  = 0xfeffffff
		"pceqw $12, $12, $0      \n\t" // $12 = (val & 0x800000) == 0
		"pand  $10, $11, %[base] \n\t" // $10 = (val & 0x7fffff)
		"por   $10, $10, $17     \n\t" // $10 = mantissa(val)
		"paddw $10, $10, $10     \n\t" // $10 = mantissa(val) << 1
		"lq    $15,-128(%[cbuf]) \n\t" // $15 = 1
		"pand  $12, $12, $10     \n\t"
		"paddw $10, $10, $12     \n\t" // if ((val & 0x800000) == 0) $10 <<= 1

		"qmtc2 %[base], $vf1     \n\t" // $vf1 = val
		"pxor  $14, $15, $10     \n\t"
		"pxor  $12, $14, $8      \n\t" // $12 = CSALo($10, $15, $8)
		"lq    $17, -32(%[cbuf]) \n\t" // $17 = 0x00ffffff
		"pand  $13, $14, $8      \n\t"
		"lq    $8,  784(%[cbuf]) \n\t" // $16 = ~0 << 24
		"paddw $13, $13, $13     \n\t" // $13 = CSAHi($10, $15, $8)

		"pand  $14, $12, $17     \n\t"
		"pand  $10, $12, $8      \n\t"
		"lq    $17, -48(%[cbuf]) \n\t" // $17 = 0x007fffff
		"paddw $10, $10, $13     \n\t"
		"lq    $16, 768(%[cbuf]) \n\t" // $16 =  1 << 24
		"por   $10, $10, $14     \n\t"


		"lq    $8,  800(%[cbuf]) \n\t" // $8  = Quotient 0
		"pcgtw $14, $10, $17     \n\t" // $14 = Quotient Bit 1 Positive
		"pand  $16, $16, $14     \n\t" // $16 = Quotient Bit 1
		"paddw $9,  $8,  $16     \n\t" // $9  = Quotient 1

		"psllw $10, $12, 1       \n\t" // $10 = Next Sum
		"psllw $11, $13, 1       \n\t" // $11 = Next Carry
		"psraw $16, $16, 1       \n\t"
		"paddw $8,  $8,  $16     \n\t" // $8  = Adjust
		"pnor  $8,  $8,  $8      \n\t"
		"pand  $8,  $8,  $14     \n\t" // $8  = QB1 Positive ? ~Adjust : 0
		"pand  $15, $14, $15     \n\t" // $15 = QB1 Positive ? 1 : 0
		"paddw $11, $11, $15     \n\t" // Carry += $15

		"pxor  $15, $10, $11     \n\t"
		"pxor  $12, $15, $8      \n\t" // $12 = CSALo($10, $11, $8)
		"pand  $13, $15, $8      \n\t"
		"pand  $15, $10, $11     \n\t"
		"por   $13, $13, $15     \n\t"
		"paddw $13, $13, $13     \n\t" // $13 = CSAHi($10, $11, $8)

		"pnor  $15, $14, $14     \n\t" // $15 = QB1 Zero
		"pand  $10, $10, $15     \n\t"
		"pand  $11, $11, $15     \n\t"
		"pand  $15, $12, $14     \n\t"
		"por   $10, $10, $15     \n\t" // $10 = QB1 Zero ? $10 : $12
		"lq    $17, -32(%[cbuf]) \n\t" // $17 = 0x00ffffff
		"pand  $15, $13, $14     \n\t"
		"lq    $8,  784(%[cbuf]) \n\t" // $8  = ~0 << 24
		"por   $11, $11, $15     \n\t" // $11 = QB1 Zero ? $11 : $13

		"pand  $14, $10, $17     \n\t"
		"pand  $10, $10, $8      \n\t"
		"lq    $17, -48(%[cbuf]) \n\t" // $17 = 0x007fffff
		"paddw $10, $10, $11     \n\t"
		"por   $10, $10, $14     \n\t" // $10 = partialAdd($10, $11)

		"lq    $16, 736(%[cbuf]) \n\t" // $16 =  1 << 23
		"pcgtw $14, $10, $17     \n\t" // $14 = Quotient Bit 2 Positive
		"lq    $17, 752(%[cbuf]) \n\t" // $17 = ~0 << 23
		"pcgtw $15, $8,  $10     \n\t" // $15 = Quotient Bit 2 Negative
		"pand  $16, $16, $14     \n\t"
		"pand  $8,  $17, $15     \n\t"
		"por   $16, $16, $8      \n\t" // $16 = Quotient Bit 2
		"paddw $8,  $9,  $16     \n\t" // $8  = Quotient 2

		"\n1:\n\t"
		"vsqrt $Q, $vf1x         \n\t"
		"addiu $25, $25, -64     \n\t"
		"lq    $17,-128(%[cbuf]) \n\t" // $17 = 1
		"psllw $10, $12, 1       \n\t" // $10 = Next Sum
		"psllw $11, $13, 1       \n\t" // $11 = Next Carry
		"psraw $16, $16, 1       \n\t"
		"paddw $9,  $9,  $16     \n\t" // $9  = Adjust
		"por   $15, $15, $14     \n\t" // $15 = Quotient Bit N NonZero
		"pxor  $9,  $9,  $14     \n\t" // $9  = QBN Positive ? ~Adjust : Adjust
		"pand  $9,  $9,  $15     \n\t" // $9  = QBN Positive ? ~Adjust : QBN Negative ? Adjust : 0
		"pand  $14, $14, $17     \n\t" // $15 = QBN Positive ? 1 : 0
		"paddw $11, $11, $14     \n\t" // Carry += $15

		"pxor  $14, $10, $11     \n\t"
		"pxor  $12, $14, $9      \n\t" // $12 = CSALo($10, $11, $9)
		"pand  $13, $14, $9      \n\t"
		"pand  $14, $10, $11     \n\t"
		"por   $13, $13, $14     \n\t"
		"paddw $13, $13, $13     \n\t" // $13 = CSAHi($10, $11, $9)

		"cfc2  $9,  $22          \n\t" // $9 = $Q
		"pnor  $14, $15, $15     \n\t" // $14 = QBN Zero
		"pand  $10, $10, $14     \n\t"
		"pand  $11, $11, $14     \n\t"
		"pand  $14, $12, $15     \n\t"
		"sw    $9,    0($24)     \n\t" // tmp[i] = $Q
		"por   $10, $10, $14     \n\t" // $10 = QBN Zero ? $10 : $12
		"lq    $17, -32(%[cbuf]) \n\t" // $17 = 0x00ffffff
		"pand  $14, $13, $15     \n\t"
		"lq    $9,  784(%[cbuf]) \n\t" // $9  = ~0 << 24
		"por   $11, $11, $14     \n\t" // $11 = QBN Zero ? $11 : $13

		"pand  $14, $10, $17     \n\t"
		"pand  $10, $10, $9      \n\t"
		"lq    $17, -48(%[cbuf]) \n\t" // $17 = 0x007fffff
		"paddw $10, $10, $11     \n\t"
		"por   $10, $10, $14     \n\t" // $10 = partialAdd($10, $11)

		"lq    $16,  64($25)     \n\t" // $16 =  1 << (24 - N)
		"pcgtw $14, $10, $17     \n\t" // $14 = Quotient Bit N Positive
		"lq    $17,  80($25)     \n\t" // $17 = ~0 << (24 - N)
		"pcgtw $15, $9,  $10     \n\t" // $15 = Quotient Bit N Negative
		"pand  $16, $16, $14     \n\t"
		"pand  $9,  $17, $15     \n\t"
		"por   $16, $16, $9      \n\t" // $16 = Quotient Bit N
		"paddw $9,  $8,  $16     \n\t" // $8  = Quotient N

		"vmr32 $vf1, $vf1        \n\t" // Rotate vf1
		"addiu $24, $24, 4       \n\t" // Advance Tmp Pointer

		"lq    $17,-128(%[cbuf]) \n\t" // $17 = 1
		"psllw $10, $12, 1       \n\t" // $10 = Next Sum
		"psllw $11, $13, 1       \n\t" // $11 = Next Carry
		"psraw $16, $16, 1       \n\t"
		"paddw $8,  $8,  $16     \n\t" // $8  = Adjust
		"por   $15, $15, $14     \n\t" // $15 = Quotient Bit N NonZero
		"pxor  $8,  $8,  $14     \n\t" // $8  = QBN Positive ? ~Adjust : Adjust
		"pand  $8,  $8,  $15     \n\t" // $8  = QBN Positive ? ~Adjust : QBN Negative ? Adjust : 0
		"pand  $14, $14, $17     \n\t" // $15 = QBN Positive ? 1 : 0
		"paddw $11, $11, $14     \n\t" // Carry += $15

		"pxor  $14, $10, $11     \n\t"
		"pxor  $12, $14, $8      \n\t" // $12 = CSALo($10, $11, $8)
		"pand  $13, $14, $8      \n\t"
		"pand  $14, $10, $11     \n\t"
		"por   $13, $13, $14     \n\t"
		"paddw $13, $13, $13     \n\t" // $13 = CSAHi($10, $11, $8)

		"pnor  $14, $15, $15     \n\t" // $14 = QBN Zero
		"pand  $10, $10, $14     \n\t"
		"pand  $11, $11, $14     \n\t"
		"pand  $14, $12, $15     \n\t"
		"por   $10, $10, $14     \n\t" // $10 = QBN Zero ? $10 : $12
		"lq    $17, -32(%[cbuf]) \n\t" // $17 = 0x00ffffff
		"pand  $14, $13, $15     \n\t"
		"lq    $8,  784(%[cbuf]) \n\t" // $8  = ~0 << 24
		"por   $11, $11, $14     \n\t" // $11 = QBN Zero ? $11 : $13

		"pand  $14, $10, $17     \n\t"
		"pand  $10, $10, $8      \n\t"
		"lq    $17, -48(%[cbuf]) \n\t" // $17 = 0x007fffff
		"paddw $10, $10, $11     \n\t"
		"por   $10, $10, $14     \n\t" // $10 = partialAdd($10, $11)

		"lq    $16,  32($25)     \n\t" // $16 =  1 << (24 - N)
		"pcgtw $14, $10, $17     \n\t" // $14 = Quotient Bit N Positive
		"lq    $11,  48($25)     \n\t" // $11 = ~0 << (24 - N)
		"pcgtw $15, $8,  $10     \n\t" // $15 = Quotient Bit N Negative
		"pand  $16, $16, $14     \n\t"
		"lq    $10, -80(%[cbuf]) \n\t" // $10 = 0x7f800000
		"pand  $8,  $11, $15     \n\t"
		"por   $16, $16, $8      \n\t" // $16 = Quotient Bit N
		"bne   $25, %[cbuf], 1b  \n\t"
		"paddw $8,  $9,  $16     \n\t" // $9  = Quotient N

		"lq    $12, -96(%[cbuf]) \n\t" // $12 = 0x3f800000
		"pand  $11, $10, %[base] \n\t" // $11 = exponent(val)
		"pceqw $13, $11, $0      \n\t" // $13 = exponent(val) == 0
		"pnor  $13, $13, $13     \n\t" // $13 = exponent(val) != 0
		"paddw $11, $11, $12     \n\t" // $11 = exponent(val) + 127
		"psrlw $11, $11, 1       \n\t"
		"pand  $11, $11, $10     \n\t" // $11 = exponent(result)
		"psrlw $8,  $8,  2       \n\t"
		"pand  $8,  $8,  $17     \n\t" // $8  = mantissa(result) if not zero
		"lq    $9,  0(%[bufbase])\n\t" // $9  = ps2 result
		"por   $8,  $8,  $11     \n\t"
		"lq    $10,-112(%[cbuf]) \n\t" // $10 = 4
		"pand  $8,  $8,  $13     \n\t" // $8  = mantissa(result)
		"lq    $17, 736(%[cbuf]) \n\t" // $17 = 1 << 23
		"pceqw $11, $8,  $9      \n\t"
		"ppach $11, $11, $11     \n\t"
		"bne   $11, $18, 3f      \n\t"
		"addiu %[loops], %[loops], -1\n\t"

		"\n2:\n\t"
		"bne   %[loops], $0, 0b     \n\t"
		"paddw %[base], %[base], $10\n\t"

		"beq   $0, $0, 4f\n\t"

		"\n3:\n\t"
		"sq    %[base],  0(%[out]) \n\t"
		"sq    $8,      16(%[out]) \n\t"
		"sq    $9,      32(%[out]) \n\t"
		"bne   %[out], %[oend], 2b\n\t"
		"addiu %[out], %[out], 48 \n\t"

		"\n4:\n"
		".set reorder\n\t"

		: [loops]"+r"(loops), [base]"+r"(base), [out]"+r"(output)
		: [cbuf]"r"(constants.bit), [oend]"r"(oend), [bufbase]"r"(tmp)
		: "memory", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "24", "25"
	);
	return output;
}

int main(int argc, const char* argv[]) {
	u32 i = 0;
	u32 fails = 0;
	do {
		u32 loops = 1 << 22;
		printf("[Fails: %d] Testing %08x...\n", fails, i);
		static TestResult results[4096];
		TestResult* res = testasm(i, loops, results, std::end(results));
		for (TestResult* r = results; r < res; r++) {
			for (u32 i = 0; i < 4; i++) {
				if (r->cop2[i] == r->expected[i])
					continue;
				fails++;
				printf("\tsqrt(%08x) = %08x[COP2] != %08x[EMU]\n", r->val[i], r->cop2[i], r->expected[i]);
			}
		}
		i += loops * 4;
	} while (i);
	printf("Sleeping\n");
	SleepThread();
}

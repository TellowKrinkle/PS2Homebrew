#include <kernel.h>
#include <tamtypes.h>
#include <stdio.h>
#include <string.h>

#include <packet.h>
#include <stdlib.h>
#include <fcntl.h>

static u32 test() {
	alignas(64) static u32 buffer[33];
	u32 cycles;
	asm volatile(
		".set noreorder          \n\t"
		"ori   $8, $0, 1         \n\t"
		"sw    $8, 128(%[buffer])\n\t"
		"\n0:\n\t"
		"mfc0 %[cycles], $9\n\t"
		"mfc0 %[cycles], $9\n\t"
		"mfc0 %[cycles], $9\n\t" // The above instructions can't run at the same time as this one, guaranteeing that this one will be the first of a superscalar cycle
		"vnop              \n\t" // vnop can run at the same time as mfc0, so it will always pair with it

		// Put your test instructions here
		// You can use the first 128 bytes of buffer for testing load/store

		"vnop                  \n\t" // Either this will pair with the instruction above or the mfc0, but either way the cfc0 below will run exactly one cycle after the previous instruction finishes
		"mfc0 $8, $9           \n\t"
		"sub  %[cycles], $8, %[cycles]\n\t"
		"lw  $8, 128(%[buffer])\n\t"
		"bne $8, $0, 0b        \n\t" // Loop once so the first fills icache and the second one we actually measure
		"sw  $0, 128(%[buffer])\n\t"
		".set reorder\n\t"
		: [cycles]"=&r"(cycles)
		: [buffer]"r"(buffer)
		: "8", "9", "10", "11", "12", "13", "14", "15"
	);
	return cycles - 1;
}

int main(void) {
	u32 res = test();
	printf("%d cycles\n", res);
	printf("Sleeping\n");
	SleepThread();
}

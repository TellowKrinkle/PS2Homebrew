#include <kernel.h>
#include <tamtypes.h>
#include <stdio.h>
#include <string.h>

#include <packet.h>
#include <stdlib.h>
#include <fcntl.h>
#include <iterator>

#include "vutests.h"

int main(int argc, const char* argv[]) {
	bool ok = true;
	ok &= testClip();
	ok &= testFlags();
	ok &= testAdd();
	ok &= testSub();
	ok &= testMul();
	ok &= testMAdd();
	ok &= testMSub();
	ok &= testAcc();
	ok &= testDiv();
	ok &= testSqrt();
	ok &= testRSqrt();
	if (ok) {
		printf("All tests passed\n");
	}
	printf("Sleeping\n");
	SleepThread();
}

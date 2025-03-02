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
	ok &= testAdd();
	ok &= testSub();
	ok &= testMul();
	ok &= testMAdd();
	ok &= testMSub();
	if (ok) {
		printf("All tests passed\n");
	}
	printf("Sleeping\n");
	SleepThread();
}

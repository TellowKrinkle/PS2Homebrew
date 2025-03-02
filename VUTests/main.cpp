#include <kernel.h>
#include <tamtypes.h>
#include <stdio.h>
#include <string.h>

#include <packet.h>
#include <stdlib.h>
#include <fcntl.h>
#include <iterator>

bool testClip();

int main(int argc, const char* argv[]) {
	bool ok = true;
	ok &= testClip();
	if (ok) {
		printf("All tests passed\n");
	}
	printf("Sleeping\n");
	SleepThread();
}

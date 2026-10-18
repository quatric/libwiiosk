/* Runs tests/selftest on the desktop. */
#include <stdio.h>

#include "tests.h"

int main(void)
{
	run_tests();
	printf("%s %d/%d\n", failed ? "FAIL" : "PASS", passed, passed + failed);
	return failed != 0;
}

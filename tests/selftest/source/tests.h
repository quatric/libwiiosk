#ifndef __SELFTEST_TESTS_H__
#define __SELFTEST_TESTS_H__

/* Scripted checks of the keyboard logic; portable, no video needed. */
extern int passed, failed;
extern char first_failure[96];
void run_tests(void);

#endif

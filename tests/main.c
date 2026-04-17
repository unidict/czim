//
//  main.c
//  tests
//
//  Test runner entry point
//

#include "unity.h"

extern void run_header_tests(void);
extern void run_entry_tests(void);
extern void run_archive_tests(void);

int main(void) {
    UNITY_BEGIN();

    run_header_tests();
    run_entry_tests();
    run_archive_tests();

    return UNITY_END();
}

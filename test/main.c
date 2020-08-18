/**
 * $Id: main.cpp,v 1.3 2012/11/23 10:19:34 rcano Exp $
 * Main function for the test file.
 * Includes test utils
 */

#include <stdlib.h>
#include <check.h>

Suite *test_suite(void);
SRunner *test_runner;
int quiet = 0;
int no_fork = 1;

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;
    int number_failed;
    Suite *s = test_suite();
    test_runner = srunner_create(s);

    if (no_fork)
    {
        srunner_set_fork_status(test_runner, CK_NOFORK);
    }

    srunner_run_all(test_runner, CK_VERBOSE);
    number_failed = srunner_ntests_failed(test_runner);
    srunner_free(test_runner);
    test_runner = NULL;
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

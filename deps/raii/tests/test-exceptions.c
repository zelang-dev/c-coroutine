#include "raii.h"
#include "test_assert.h"
#include <assert.h>

/* Basic try.. catch */
int test_basic_catch(void) {
    int caught = 0;

    try {
        raise(SIGINT);
    } catch (sig_int) {
        caught = 1;
    } end_trying;

    ASSERT_EQ(1, caught);
    return 0;
}

void test_types_pt2(int *caught) {
    *caught = 0;

    try {
        raise(SIGILL);
    } catch (sig_segv) {
        *caught = 1;
    } end_trying;
}

int test_types(void) {
    int caught;

    try {
        test_types_pt2(&caught);
    } catch (sig_ill) {
    } end_trying;

    /* Different type: should not be caught */
    ASSERT_EQ(0, caught);
    return 0;
}

/* test of the exception any subtyped system */
int test_subtypes(void) {
    int caught = 0;

    try {
        raise(SIGABRT);
    } catch_any {
        caught = 1;
    } end_trying;

    ASSERT_EQ(1, caught);
    return 0;
}

/* test the finally {} functionality */
void test_finally_pt2(int *ran_finally) {
    try {
        raise(SIGSEGV);
    } catch (sig_ill) {
    } finally {
        *ran_finally = 1;
    } end_try;
}

/* test rethrowing an exception after it is caught */
int test_rethrow_pt2(int *caught_1, int *finally_1) {
    try {
        raise(SIGSEGV);
    } catch (sig_segv) {
        *caught_1 = 1;
        rethrow();
    } finally {
        *finally_1 = 1;
    } end_try;

    assert(0);
    return 0;
}

int test_rethrow(void) {
    int caught_1, caught_2;
    int finally_1, finally_2;

    caught_1 = 0;
    caught_2 = 0;
    finally_1 = 0;
    finally_2 = 0;

    try {
        test_rethrow_pt2(&caught_1, &finally_1);
    } catch_any{
        caught_2 = 1;
    } finally {
        finally_2 = 1;
    } end_try;

    ASSERT_EQ( true, caught_1 && caught_2 && finally_1 && finally_2);
    return 0;
}

/* test throw inside finally block */
int test_throw_in_finally_pt2(int *caught) {
    try {
        raise(SIGSEGV);
    } catch (sig_segv) {
        *caught = 1;
    } finally {
        raise(SIGILL);
    } end_try;

    assert(0);
    return 0;
}

int test_throw_in_finally(void) {
    int caught_1, caught_2;
    int i;

    for (i = 0; i < 10; ++i) {

        caught_1 = 0;
        caught_2 = 0;

        try {
            test_throw_in_finally_pt2(&caught_1);
        } catch (sig_ill) {
            caught_2 = 1;
        } end_trying;

        ASSERT_EQ(true, caught_1 && caught_2);
    }
    return 0;
}

/* test for assert */
int test_assert(void) {
    int caught = 0;

    try {
        assert(1 == 2);
        raise(SIGABRT);
    } catch (sig_abrt) {
        caught = 1;
    } end_trying;

    ASSERT_EQ(1, caught);
    return 0;
}

int test_finally(void) {
    int ran_finally;
    int caught;

    /* In normal conditions, finally should be run after the try block */

    ran_finally = 0;

    try {
    } finality {
        ran_finally = 1;
    } end_try;
    ASSERT_EQ(1, ran_finally);

    /* If we catch an exception, finally should run */

    ran_finally = 0;

    try {
        raise(SIGILL);
    } catch (sig_ill) {
    } finally {
        ran_finally = 1;
    } end_try;

    ASSERT_EQ(1, ran_finally);

    /* If we have try .. finally with no catch, the finally block
     * should run and the exception be passed higher up the stack. */

    caught = 0;
    ran_finally = 0;

    try {
        test_finally_pt2(&ran_finally);
    } finality {
        caught = 1;
    } end_trying;

    ASSERT_EQ( true, caught && ran_finally);
    return 0;
}

int test_list(void)
{
    test_basic_catch();
    test_types();
    test_subtypes();
    test_finally();
    test_rethrow();
    test_throw_in_finally();
    test_assert();

    return 0;
}

int main(int argc, char **argv) {

    ASSERT_FUNC(test_list());

    return 0;
}

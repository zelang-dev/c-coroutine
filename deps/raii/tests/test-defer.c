#include "raii.h"
#include "test_assert.h"

char number[20];
int g_print(void *args) {
    ASSERT_NOTNULL(args);
    int arg = raii_value(args).integer;
    ASSERT_EQ(true, arg >= 0);
    printf("Defer in g = %d.\n\n", arg);
}

int f_print(void *args) {
    ASSERT_NULL(args);
    const char *err = _get_message();
    ASSERT_STR("4", err);
    puts("In defer in f");
    fflush(stdout);
    if (_recover(err)) {
        printf("Recovered in f = %s\n\n", err);
        fflush(stdout);
    }
}

void g(int i) {
    if (i > 3) {
        puts("Panicking!\n");
        snprintf(number, 20, "%d", i);
        _panic(number);
    }

    guard {
      _defer(g_print, &i);
      printf("Printing in g = %d.\n", i);
      g(i + 1);
    } guarded;
}

void f()
guard {
    _defer(f_print, NULL);
    puts("Calling g.");
    g(0);
    puts("Returned normally from g.");
} guarded;

int test_main() {
    f();
    puts("Returned normally from f.");
    return 0;
}

int main(int argc, char **argv) {

    ASSERT_FUNC(test_main());

    return EXIT_SUCCESS;
}

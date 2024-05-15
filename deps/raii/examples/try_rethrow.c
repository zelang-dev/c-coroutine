#include "raii.h"

static int idiv(int a, int b) {
    if (b == 0)
        throw(division_by_zero);

    return a / b;
}

int main(void) {
    try {
        idiv(1, 0);
        printf("never reached\n");
    } catch (division_by_zero) {
        printf("catch: exception %s (%s:%d) caught\n", err, err_file, err_line);
        rethrow();
        printf("never reached\n");
    } end_trying;

    return 0;
}

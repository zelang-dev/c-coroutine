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
    } catch_if {
        if (caught(bad_alloc)) {
            printf("catch: exception %s (%s:%d) caught\n", err, err_file, err_line);
        } else if (caught(division_by_zero)) {
            printf("catch: exception %s (%s:%d) caught\n", err, err_file, err_line);
        }
    } finally {
        if (err)
            printf("finally: try failed -> %s (%s:%d)\n", err, err_file, err_line);
        else
            printf("finally: try succeeded\n");
    } end_try;

    return 0;
}

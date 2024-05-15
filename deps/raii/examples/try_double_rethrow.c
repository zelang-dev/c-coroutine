
#include "raii.h"

static int div_err(int a, int b) {
    if (b == 0)
        throw(division_by_zero);

    return a / b;
}

int main(int argc, char **argv) {
    try {
        div_err(1, 0);
        printf("never reached\n");
    } catch (division_by_zero) {
        printf("catch: exception %s (%s:%d) caught\n", err, err_file, err_line);
        rethrow();
        printf("never reached\n");
    } finally {
        if (err) {
            printf("finally: try failed -> %s (%s:%d)\n", err, err_file, err_line);
            rethrow();
            printf("never reached\n");
        } else
            printf("finally: try succeeded\n");
    } end_try;

    return 0;
}

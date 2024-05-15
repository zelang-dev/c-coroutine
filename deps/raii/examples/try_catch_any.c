#include "raii.h"

static int idiv(int a, int b) {
    *(int *)0 = b;
}

int main(int argc, char **argv) {
    try {
        idiv(1, 0);
        printf("never reached\n");
    } catch_any {
        printf("catch_any: exception %s (%s:%d) caught\n", err, err_file, err_line);
    } end_trying;

    return 0;
}

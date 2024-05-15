#include "raii.h"

static void pfree(void *p) {
    printf("freeing protected memory pointed by '%s'\n", (char *)p);
    free(p);
}

int main(int argc, char **argv) {
    try {
        char *p = 0;
        p = calloc_full(raii_init(), 1, 3, pfree);
        if (p)
            strcpy(p, "p");

        int a = 1;
        int b = 0.00000;
        float x = a / b;
        printf("never reached %f\n", x);

        free(p);
    } catch_any {
        printf("catch_any: exception %s (%s:%d) caught\n", err, err_file, err_line);
    } end_trying;

    return 0;
}

#include "raii.h"

static int div_err(int a, int b) {
    if (b == 0)
        throw(division_by_zero);

    return a / b;
}

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

        p = calloc_full(raii_init(), 1, 4, pfree); /* still protected */
        if (p)
            strcpy(p, "p2");

        div_err(1, 0);
        printf("never reached\n");

        free(p);
    } catch_any {
        printf("catch_any: exception %s (%s:%d) caught\n", err, err_file, err_line);
    } end_trying;

    return 0;
}

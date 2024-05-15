#include "raii.h"

static void pfree(void *p) {
    printf("freeing protected memory pointed by '%s'\n", (char *)p);
    free(p);
}

int main(int argc, char **argv) {
    try {
        char *p = 0;
        p = malloc_full(raii_init(), 3, pfree);
        if (p)
            strcpy(p, "p");

        *(int *)0 = 0;
        printf("never reached\n");

        free(p);
    } catch_any {
        printf("catch: exception %s (%s:%d) caught\n", err, err_file, err_line);
    } end_trying;

    return 0;
}

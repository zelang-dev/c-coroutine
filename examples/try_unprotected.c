#include "../include/coroutine.h"

static int div_err(int a, int b)
{
    if (b == 0)
        throw(division_by_zero);

    return a / b;
}

static void pfree(void *p)
{
    printf("freeing protected memory pointed by '%s'\n", (char *)p);
    free(p);
}

int co_main(int argc, char **argv)
{
    try
    {
        char *p1 = 0;
        protected(p1, pfree);
        char *p2 = 0;
        protected(p2, pfree);

        p1 = malloc(sizeof("p1"));
        if (p1)
            strcpy(p1, "p1");

        p2 = malloc(sizeof("p2"));
        if (p2)
            strcpy(p2, "p2");

        free(p1);
        free(p2);
        unprotected(p1); /* unprotected p2 too */

        div_err(1, 0);
        printf("never reached\n");
    }
    catch_any
    {
        printf("catch_any: exception %s (%s:%d) caught\n", err, err_file, err_line);
    }
    end_try;

    return 0;
}

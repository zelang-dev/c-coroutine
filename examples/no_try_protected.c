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
    char *p = 0;

    p = co_new(3);
    if (p)
        strcpy(p, "p");
    div_err(1, 0);
    printf("never reached\n");

    free(p);

    return 0;
}

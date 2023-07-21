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

int main(void)
{
    char *p = 0;
    protected(p, pfree);

    p = malloc(sizeof("p"));
    if (p)
        strcpy(p, "p");
    div_err(1, 0);
    printf("never reached\n");

    free(p);
    unprotected(p);

    return 0;
}

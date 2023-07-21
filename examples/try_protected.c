#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/exception.h"

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
    try
    {
        char *p = 0;
        protected(p, pfree);

        p = malloc(sizeof("p"));
        if (p)
            strcpy(p, "p");

        free(p);

        p = malloc(sizeof("p2")); /* still protected */
        if (p)
            strcpy(p, "p2");

        div_err(1, 0);
        printf("never reached\n");

        free(p);
        unprotected(p);
    }
    catch_any
    {
        printf("catch_any: exception %s (%s:%d) caught\n", err, err_file, err_line);
    }
    end_try;

    return 0;
}

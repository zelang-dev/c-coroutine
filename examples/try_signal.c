#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "../include/exception.h"

static void pfree(void *p)
{
    printf("freeing protected memory pointed by '%s'\n", (char *)p);
    free(p);
}

int main(void)
{
    ex_signal_std();

    try
    {
        char *p = 0;
        protected(p, pfree);

        p = malloc(sizeof("p"));
        if (p)
            strcpy(p, "p");

        *(int *)0 = 0;
        printf("never reached\n");

        free(p);
        unprotected(p);
    }
    catch (sig_segv)
    {
        printf("catch: exception %s (%s:%d) caught\n", err, err_file, err_line);
    }
    catch_any
    {
        printf("catch_any: exception %s (%s:%d) caught\n", err, err_file, err_line);
    }
    end_try;

    return 0;
}

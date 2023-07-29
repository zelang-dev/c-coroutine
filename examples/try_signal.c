#include "../include/coroutine.h"
#include <signal.h>

static void pfree(void *p)
{
    printf("freeing protected memory pointed by '%s'\n", (char *)p);
    free(p);
}

int co_main(int argc, char **argv)
{
    try
    {
        char *p = 0;
        p = co_malloc_full(co_active(), 3, pfree);
        if (p)
            strcpy(p, "p");

        *(int *)0 = 0;
        printf("never reached\n");

        free(p);
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

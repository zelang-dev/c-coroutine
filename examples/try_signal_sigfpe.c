#include "../include/coroutine.h"

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

        int a = 1;
        int b = 0.00000;
        float x = a / b;
        printf("never reached %f\n", x);

        free(p);
    }
    catch (sig_fpe)
    {
        printf("catch: exception %s (%s:%d) caught\n", err, err_file, err_line);
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

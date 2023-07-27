#include "../include/coroutine.h"

static int div_err(int a, int b)
{
    if (b == 0)
        throw(division_by_zero);

    return a / b;
}

int co_main(int argc, char **argv)
{
    try
    {
        div_err(1, 0);
        printf("never reached\n");
    }
    catch (out_of_memory)
    {
        printf("catch: exception %s (%s:%d) caught\n", err, err_file, err_line);
    }
    finally
    {
        if (err)
            printf("finally: try failed -> %s (%s:%d)\n", err, err_file, err_line);
        else
            printf("finally: try succeeded\n");
    }
    end_try;

    return 0;
}

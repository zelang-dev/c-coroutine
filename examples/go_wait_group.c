#include "../include/coroutine.h"

void worker(int id)
{
    printf("Worker %d starting\n", id);

    co_sleep(1000);
    printf("Worker %d done\n", id);
}

void *func(void *args)
{
    worker(co_value(args).integer);
    return 0;
}

int co_main(int argc, char **argv)
{
    co_hast_t *wg = co_wait_group();
    for (int i = 1; i <= 5; i++)
    {
        int arg = i;
        co_go(func, &arg);
    }
    co_wait(wg);
}

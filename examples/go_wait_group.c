#include "../include/coroutine.h"

void *worker(void *arg)
{
    // int id = co_value(arg).integer;
    int id = co_id();
    printf("Worker %d starting\n", id);

    co_sleep(1000);
    printf("Worker %d done\n", id);
    return 0;
}

int co_main(int argc, char **argv)
{
    co_hast_t *wg = co_wait_group();
    for (int i = 1; i <= 5; i++)
    {
        co_go(worker, &i);
    }
    co_wait(wg);
    return 0;
}

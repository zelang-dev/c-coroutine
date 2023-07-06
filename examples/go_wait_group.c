#include "../include/coroutine.h"

void *worker(void *arg)
{
    // int id = co_value(arg).integer;
    int id = co_id();
    printf("Worker %d starting\n", id);

    co_sleep(1000);
    printf("Worker %d done\n", id);
    if (id == 4)
        return (void *)32;
    else if (id == 3)
        return (void *)"hello world\0";

    return 0;
}

int co_main(int argc, char **argv)
{
    int cid[5];
    co_ht_group_t *wg = co_wait_group();
    for (int i = 1; i <= 5; i++)
    {
       cid[i-1] = co_go(worker, &i);
    }
    co_ht_result_t *wgr = co_wait(wg);

    printf("\nWorker # %d returned: %d\n", cid[2], co_group_get_result(wgr, cid[2]).integer);
    printf("\nWorker # %d returned: %s\n", cid[1], co_group_get_result(wgr, cid[1]).string);
    return 0;
}

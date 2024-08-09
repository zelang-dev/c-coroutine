#include "coroutine.h"

void *worker(void *arg) {
    int wid = args_get(arg, 0).integer + 1;
    int id = co_id();

    printf("Worker %d starting\n", wid);
    co_info_active();

    sleep_for(1000);

    printf("Worker %d done\n", wid);
    co_info_active();

    if (id == 4)
        return (values_t*)32;
    else if (id == 3)
        return "hello world";

    return 0;
}

int co_main(int argc, char **argv) {
    int cid[100], i;

    wait_group_t *wg = work_group();
    for (i = 0; i < 100; i++) {
        cid[i] = co_go(worker, args_for("i", i));
    }
    wait_result_t *wgr = co_wait(wg);

    printf("\nWorker # %d returned: %d\n", cid[3], work_group_result(wgr, cid[3]).integer);
    printf("\nWorker # %d returned: %s\n", cid[2], work_group_result(wgr, cid[2]).char_ptr);
    return 0;
}

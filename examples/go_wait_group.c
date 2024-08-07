#include "coroutine.h"

void *worker(void *arg) {
    int wid = args_get(arg, 0).integer;
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
    int cid[5], i;
    wait_group_t *wg = work_group();
    for (i = 1; i <= 5; i++) {
        cid[i - 1] = co_go(worker, args_for("i", i));
    }
    wait_result_t *wgr = co_wait(wg);

    printf("\nWorker # %d returned: %d\n", cid[2], work_group_result(wgr, cid[2]).integer);
    printf("\nWorker # %d returned: %s\n", cid[1], work_group_result(wgr, cid[1]).char_ptr);
    return 0;
}

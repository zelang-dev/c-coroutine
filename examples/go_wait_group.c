#include "coroutine.h"

void *worker(void *arg) {
    int wid = get_args(arg, 0).integer;
    int id = co_id();
    printf("Worker %d starting\n", wid);

    co_sleep(1000);
    printf("Worker %d done\n", wid);
    if (id == 4)
        return 32;
    else if (id == 3)
        return "hello world";

    return 0;
}

int co_main(int argc, char **argv) {
    int cid[5];
    wait_group_t *wg = co_wait_group();
    for (int i = 1; i <= 5; i++) {
        cid[i - 1] = co_go(worker, args_for("i", i));
    }
    wait_result_t *wgr = co_wait(wg);

    printf("\nWorker # %d returned: %d\n", cid[2], co_group_get_result(wgr, cid[2]).integer);
    printf("\nWorker # %d returned: %s\n", cid[1], co_group_get_result(wgr, cid[1]).char_ptr);
    return 0;
}


#include "coroutine.h"

void *worker(void *arg) {
    int count = get_args(arg, 0).integer;
    char *text = get_args(arg, 1).char_ptr;

    for (int i = 0; i < count; i++) {
        printf("%s\n", text);
        co_sleep(10);
    }
    return 0;
}

int co_main(int argc, char **argv) {
    co_go(worker, args_for("is", 4, "a"));
    co_go(worker, args_for("is", 2, "b"));
    co_go(worker, args_for("is", 3, "c"));

    co_sleep(100);

    return 0;
}

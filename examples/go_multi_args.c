
#include "coroutine.h"

void *worker(args_t arg) {
    int i, count = args_get(arg, 0).integer;
    char *text = args_get(arg, 1).char_ptr;

    for (i = 0; i < count; i++) {
        printf("%s\n", text);
        sleep_for(10);
    }
    return 0;
}

int co_main(int argc, char **argv) {
    go_for(worker, "is", 4, "a");
    go_for(worker, "is", 2, "b");
    go_for(worker, "is", 3, "c");

    sleep_for(100);

    return 0;
}

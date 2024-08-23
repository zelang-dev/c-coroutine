
#include "coroutine.h"

void *worker(void *arg) {
    int count = args_get(arg, 0).integer;
    char *text = args_get(arg, 1).char_ptr;

    for (int i = 0; i < count; i++) {
        printf("%s\n", text);
        sleep_for(10);
    }
    return 0;
}

int co_main(int argc, char **argv) {
    go(worker, args_for("is", 4, "a"));
    go(worker, args_for("is", 2, "b"));
    go(worker, args_for("is", 3, "c"));

    sleep_for(100);

    return 0;
}

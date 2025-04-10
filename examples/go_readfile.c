#include "coroutine.h"

void_t worker(args_t args) {
    printf("%s\n", args->char_ptr);
    sleep_for(500);
    return "done";
}

int co_main(int argc, char *argv[]) {
    go_for(worker, 1, "hello world");
    string text = fs_readfile(__FILE__);
    printf("\n%s\n", text);

    return 0;
}


#include "../include/coroutine.h"

void *worker(void *arg) {
    uv_args_t *co_args = (uv_args_t *)arg;
    int count = co_args->args[0].value.integer;
    char *text = co_args->args[1].value.char_ptr;

    for (int i = 0; i < count; i++) {
        printf("%s\n", text);
        co_sleep(10);
    }
    return 0;
}

int co_main(int argc, char **argv) {
    uv_args_t *a = co_arguments(2, true);
    uv_args_t *b = co_arguments(2, true);
    uv_args_t *c = co_arguments(2, true);

    a->args[0].value.integer = 4;
    a->args[1].value.char_ptr = "a";

    b->args[0].value.integer = 2;
    b->args[1].value.char_ptr = "b";

    c->args[0].value.integer = 3;
    c->args[1].value.char_ptr = "c";

    co_go(worker, a);
    co_go(worker, b);
    co_go(worker, c);

    co_sleep(100);

    return 0;
}

#include "coroutine.h"

int fibonacci(channel_t *c, channel_t *quit) {
    int x = 0;
    int y = 1;
    for_select {
        select_case(c) {
            chan_send(c, &x);
            unsigned long tmp = x + y;
            x = y;
            y = tmp;
        } select_case_if(quit) {
            chan_recv(quit);
            puts("quit");
            return 0;
        } select_break;
    } select_end;
}

void *func(args_t args) {
    args_deferred(args);
    channel_t *c = args[0].object;
    channel_t *quit = args[1].object;
    int i;

    defer(delete, c);

    for (i = 0; i < 10; i++) {
        printf("%d\n", chan_recv(c).integer);
    }
    chan_send(quit, 0);

    printf("\nChannel `quit` type is: %d, validity: %d\n", type_of(quit), is_instance(quit));

    delete(c);
    delete(quit);

    printf("Channel `quit` freed, validity: %d\n", is_instance(quit));
    printf("Channel `c` type is: %d accessed after freed!\n\n", type_of(c));
    delete(quit);

    return 0;
}

int co_main(int argc, char **argv) {
    args_t params = array(2, channel(), channel());
    go(func, params);
    return fibonacci(params[0].object, params[1].object);
}

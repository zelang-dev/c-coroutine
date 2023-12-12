#include "../include/coroutine.h"

int fibonacci(channel_t *c, channel_t *quit) {
    int x = 0;
    int y = 1;
    for_select {
        select_case(c) {
            co_send(c, &x);
            unsigned long tmp = x + y;
            x = y;
            y = tmp;
        } select_case_if(quit) {
            co_recv(quit);
            puts("quit");
            return 0;
        } select_break;
    } select_end;
}

void *func(void *args) {
    channel_t *c = get_args(args, 0).object;
    channel_t *quit = get_args(args, 1).object;

    defer(delete, c);

    for (int i = 0; i < 10; i++) {
        printf("%d\n", co_recv(c).integer);
    }
    co_send(quit, 0);

    printf("\nChannel `quit` type is: %d, validity: %d\n", type_of(quit), is_instance(quit));

    delete(c);
    delete(quit);

    printf("Channel `quit` freed, validity: %d\n", is_instance(quit));
    printf("Channel `c` type is: %d accessed after freed!\n\n", type_of(c));
    delete(quit);

    return 0;
}

int co_main(int argc, char **argv) {
    args_t *params = args_for("pp", channel(), channel());
    co_go(func, params);
    return fibonacci(params->args[0].value.object, params->args[1].value.object);
}

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
    channel_t *c = ((channel_t **)args)[0];
    channel_t *quit = ((channel_t **)args)[1];

    for (int i = 0; i < 10; i++) {
        printf("%d\n", co_recv(c).integer);
    }
    co_send(quit, 0);

    printf("\nChannel `quit` type is: %d, validity: %d\n", type_of(quit), is_instance(quit));

    channel_free(c);
    channel_free(quit);

    printf("Channel `quit` freed, validity: %d\n", is_instance(quit));
    printf("Channel `c` type is: %d accessed after freed!\n\n", type_of(c));

    return 0;
}

int co_main(int argc, char **argv) {
    channel_t *args[2];
    channel_t *c = channel();
    channel_t *quit = channel();

    args[0] = c;
    args[1] = quit;
    co_go(func, args);
    return fibonacci(c, quit);
}

#include "../include/coroutine.h"

int fibonacci(channel_t *c, channel_t *quit)
{
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

void *func(void *args)
{
    channel_t *c = ((channel_t **)args)[0];
    channel_t *quit = ((channel_t **)args)[1];
    for (int i = 0; i < 10; i++)
    {
        printf("%d\n", co_recv(c)->value.integer);
    }
    co_send(quit, 0);

    return 0;
}

int co_main(int argc, char **argv)
{
    channel_t *args[2];
    channel_t *c = co_make();
    channel_t *quit = co_make();

    args[0] = c;
    args[1] = quit;
    co_go(func, args);
    return fibonacci(c, quit);
}

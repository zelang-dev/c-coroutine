#include <stdio.h>
#include <stdlib.h>
#include "../coroutine.h"

void *co_2(void *arg)
{
    channel_t *c = (channel_t *)arg;
    printf("co_2 start\n");
    for (int i = 0; i < 10; i++)
    {
        unsigned long v = co_value(channel_recv(c)).big_int;
        printf("received: %lu\n", v);
    }
    printf("co_2 end\n");

    return 0;
}

int co_main(int argc, char **argv)
{
    channel_t *c = channel_create(sizeof(unsigned long), 3);
    coroutine_create(co_2, (void *)c, 0);
    for (unsigned long i = 0; i < 10; i++)
    {
        printf("going to send number: %lu\n", i);
        channel_send(c, &i);
        printf("send success: %lu\n", i);
    }
    printf("co_main end\n");

    return 0;
}

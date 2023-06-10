#include <stdio.h>
#include <stdlib.h>
#include "../coroutine.h"

void *co_2(void *arg)
{
    channel_t *c = (channel_t *)arg;
    printf("co_2 start\n");
    for (int i = 0; i < 10; i++)
    {
        void *v = channel_recv(c);
        printf("received: %lu\n", co_value(v).big_int);
    }
    printf("co_2 end\n");

    return 0;
}

int co_main(int argc, char **argv)
{
    channel_t *c = channel_create(sizeof(co_value_t), 3);
    int s;
    coroutine_create(co_2, (void *)c, 0);
    for (unsigned long i = 0; i < 10; i++)
    {
        printf("going to send number: %lu\n", i);
        s = channel_send(c, &i);
        printf("status: %d send success: %lu\n", s, i);
    }
    printf("co_main end\n");

    return 0;
}

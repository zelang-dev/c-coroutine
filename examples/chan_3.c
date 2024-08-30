#include "coroutine.h"
void *co_2(void *arg) {
    channel_t *c = (channel_t *)arg;
    int i;

    printf("co_2 start\n");
    for (i = 0; i < 10; i++) {
        printf("received: %lu\n", chan_recv(c).u_long);
    }
    printf("co_2 end\n");

    return 0;
}

int co_main(int argc, char **argv) {
    channel_t *c = channel_buf(3);
    int i, s;

    go(co_2, (void *)c);
    for (i = 0; i < 10; i++) {
        printf("going to send number: %u\n", i);
        s = chan_send(c, &i);
        printf("status: %d send success: %u\n", s, i);
    }
    printf("co_main end\n");

    return 0;
}

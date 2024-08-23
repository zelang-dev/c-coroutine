#include "coroutine.h"

channel_t *c;

void *delay_co(void *arg) {
    int v = atoi(c_char_ptr(arg));
    sleep_for(v);
    printf("awake after %d ms\n", v);
    chan_send(c, 0);

    return 0;
}

int co_main(int argc, char **argv) {
    int i, n;
    c = channel();

    n = 0;
    for (i = 1; i < argc; i++) {
        n++;
        printf("x");
        go(delay_co, &argv[i]);
    }

    /* wait for n tasks to finish */
    for (i = 0; i < n; i++) {
        printf("y");
        chan_recv(c);
    }

    channel_free(c);
    exit(0);
}

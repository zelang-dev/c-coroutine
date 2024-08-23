#include "coroutine.h"

int quiet = 0;
int goal = 0;
int buffer = 0;

void *prime_co(void *arg) {
    channel_t *c, *nc;
    int p, i;
    c = arg;

    p = chan_recv(c).integer;
    if (p > goal)
        return 0;
    if (!quiet) {
        printf("%s ", co_get_name());
        printf("%d\n", p);
    }
    nc = channel_buf(buffer);
    go(prime_co, nc);
    for (;;) {
        i = chan_recv(c).integer;
        if (i % p)
            chan_send(nc, &i);
    }

    return 0;
}

int co_main(int argc, char **argv) {
    int i;
    channel_t *c;

    if (argc > 1)
        goal = atoi(argv[1]);
    else
        goal = 100;
    printf("goal=%d\n", goal);

    c = channel_buf(buffer);
    go(prime_co, c);
    for (i = 2;; i++)
        chan_send(c, &i);

    return 0;
}

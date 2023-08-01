#include "../include/coroutine.h"

void *func(void *arg)
{
    promise *p = (promise *)arg;
    printf("\tstarted thread\n");
#ifdef _WIN32
    Sleep(3);
#else
    usleep(3 * 1000);
#endif
    printf("\tthread will set promise\n");
    promise_set(p, (void *)42);
    printf("\tstopping thread\n");
    return NULL;
}

int co_main(int argc, char **argv)
{
    long t;
    printf("main thread\n");
    future *f = future_create(func);
    promise *p = promise_create();
    future_start(f, p);

    printf("got result from future: %d\n", promise_get(p).integer);

    promise_close(p);
    future_close(f);

    return 0;
}

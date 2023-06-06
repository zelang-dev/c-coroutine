#include <stdio.h>
#include <assert.h>
#include "../coroutine.h"

void *hello_world(void *args)
{
    co_defer(CO_DEFER(puts), "all done");
    puts("Hello");
    // Suspension point that send `1` and receive `15`
    int value = co_value(co_yielding_get(co_active(), (void *)1)).integer;
    assert(value == 15);
    puts("World");
    int p = *(int*)(args);
    p++;
    *(int*)args = p;
    return args;
}

int co_main(int argc, char **argv)
{
    co_routine_t *coro;
    int i = 30;
    int v = 15;
    coro = co_start(hello_world, &i);
    i++;
    assert(co_resuming_set(coro, &v) == (void *)1);     // Verifying return value
    assert(co_resuming(coro) == (void *)-1);    // Invalid call
    assert(co_returning(coro).integer == 32); // returning final value
    assert(i == 32);
    co_delete(coro);
    return EXIT_SUCCESS;
}

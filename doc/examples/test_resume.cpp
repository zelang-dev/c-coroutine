#include <stdio.h>
#include <assert.h>
#include "c_coro.h"

void *hello_world(void *args)
{
    puts("Hello");
    co_yielding(co_active(), (void *)1); // Suspension point that returns the value `1`
    puts("World");
    int p = *(int*)(args);
    p++;
    *(int*)args = p;
    return args;
}

int main()
{
    co_routine_t *coro;
    int i = 30;
    coro = co_start(hello_world, &i);
    i++;
    assert(co_resuming(coro) == (void *)1);     // Verifying return value
    assert(co_resuming(coro) == (void *)-1);    // Invalid call
    assert(co_returning(coro).integer == 32); // returning final value
    co_delete(coro);
    return EXIT_SUCCESS;
}

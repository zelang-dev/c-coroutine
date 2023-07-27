#include "../include/coroutine.h"

int div(int x, int y)
{
    if (y == 0)
        co_panic("no good");
    return x / y;
}

int mul(int x, int y)
{
    return x * y;
}

void func(void *arg)
{
    const char *err = co_recovered();
    if (NULL != err)
        printf("panic occurred: %s\n", err);
}

void divByZero()
{
    co_defer_recover(func, NULL);
    printf("%d", div(1, 0));
}

int co_main(int argc, char **argv)
{
    divByZero();
    printf("Although panicked. We recovered. We call mul() func\n");
    printf("mul func result: %d\n", mul(5, 10));
    return 0;
}

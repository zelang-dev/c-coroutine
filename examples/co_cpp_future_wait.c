#include "../include/coroutine.h"

// a non-optimized way of checking for prime numbers:
void *is_prime(void *arg)
{
    int x = co_value(arg).integer;
    for (int i = 2; i < x; ++i) if (x % i == 0) return (void *)false;
    return (void *)true;
}

int co_main(int argc, char **argv)
{
    int prime = 194232491;
    // call function asynchronously:
    future *f = co_async(is_prime, &prime);
    printf("checking...\n");
    co_async_wait(f);

    printf("\n194232491 ");
    if (co_async_get(f).boolean) // guaranteed to be ready (and not block) after wait returns
        printf("is prime!\n");
    else
        printf("is not prime.\n");

    return 0;
}

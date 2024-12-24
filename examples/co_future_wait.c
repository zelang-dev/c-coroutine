#include "coroutine.h"

// a non-optimized way of checking for prime numbers:
void *is_prime(args_t arg) {
    int i, x = get_arg(args_deferred(arg)).integer;
    for (i = 2; i < x; ++i) if (x % i == 0) return thrd_value(false);
    return thrd_value(true);
}

int co_main(int argc, char **argv) {
    // call function asynchronously:
    future fut = thrd_launch(is_prime, args_ex(1, 194232491));

    printf("checking...\n");
    thrd_until(fut);

    printf("\n194232491 ");
    if (thrd_get(fut).boolean) // guaranteed to be ready (and not block) after `until` returns
        printf("is prime.\n");
    else
        printf("is not prime.\n");

    return 0;
}

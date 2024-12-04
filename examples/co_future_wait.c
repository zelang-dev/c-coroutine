#include "coroutine.h"

// a non-optimized way of checking for prime numbers:
void *is_prime(args_t arg) {
    int i, x = get_arg(arg).integer;
    for (i = 2; i < x; ++i) if (x % i == 0) return thrd_value(false);
    return thrd_value(true);
}

int co_main(int argc, char **argv) {
    int prime = 194232491;
    // call function asynchronously:
    future *fut = thrd_async(is_prime, thrd_data(prime));

    printf("checking...\n");
    thrd_wait(fut, co_yield_info);

    printf("\n194232491 ");
    if (thrd_get(fut).boolean) // guaranteed to be ready (and not block) after wait returns
        printf("is prime.\n");
    else
        printf("is not prime.\n");

    return 0;
}

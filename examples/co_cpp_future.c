#include "../include/coroutine.h"

void *is_prime(void *arg) {
    int x = co_value(arg).integer;
    printf("Calculating. Please, wait...\n");
    for (int i = 2; i < x; ++i)
        if (x % i == 0)
            return (void *)false;

    return (void *)true;
}

int co_main(int argc, char **argv) {
    int prime = 313222313;
    future *f = co_async(is_prime, &prime);
    printf("Checking whether 313222313 is prime.\n");

    bool ret = co_async_get(f).boolean; // waits for is_prime to return

    if (ret)
        printf("It is prime!\n");
    else
        printf("It is not prime.\n");

    return 0;
}

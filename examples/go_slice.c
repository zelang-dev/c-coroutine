
#include "../include/coroutine.h"

int co_main(int argc, char **argv) {
    array_t *primes = array_long(6, 2, 3, 5, 7, 11, 13);
    co_defer(array_free, primes);

    println(1, slice(primes, 1, 4));

    return 0;
}

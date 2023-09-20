
#include "../include/coroutine.h"

int co_main(int argc, char **argv) {
    as_array_long(pow, 8, 1, 2, 4, 8, 16, 32, 64, 128);

    ranging(v in pow)
        printf("2**%s = %d\n", indic(v), has(v).integer);

    return 0;
}

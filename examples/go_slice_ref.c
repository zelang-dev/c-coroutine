
#include "../include/coroutine.h"

int co_main(int argc, char **argv) {
    array_t *names = array_str(4, "John", "Paul", "George", "Ringo");
    co_defer(delete, names);

    println(1, names);

    slice_t *a, *b;
    a = slice(names, 0, 2);
    b = slice(names, 1, 3);
    println(2, a, b);

    $$(b, 0, "XXXX");
    println(2, a, b);

    println(1, names);

    return 0;
}

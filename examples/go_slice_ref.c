
#include "../include/coroutine.h"

int co_main(int argc, char **argv) {
    as_array_string(names, 4, "John", "Paul", "George", "Ringo");

    println(1, names);

    as_slice(a, names, 0, 2);
    as_slice(b, names, 1, 3);
    println(2, a, b);

    $$(b, 0, "XXXX");
    println(2, a, b);

    println(1, names);

    return 0;
}

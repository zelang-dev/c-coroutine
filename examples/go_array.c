
#include "../include/coroutine.h"

int co_main(int argc, char **argv) {
    array_t *list = array_str(2, "Hello", "World");

    printf("%s %s\n", $(list, 0).str, $(list, 1).str);
    println(1, list);
    delete(list);

    list = array_long(6, 2, 3, 5, 7, 11, 13);
    co_defer(delete, list);

    println(1, list);

    return 0;
}

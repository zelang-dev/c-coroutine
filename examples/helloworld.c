
#include "uv_coro.h"

int uv_main(int argc, char **argv) {
    printf("Now quitting.\n");
    yielding();

    return coro_err_code();
}

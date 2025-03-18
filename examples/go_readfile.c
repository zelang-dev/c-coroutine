#include "uv_coro.h"

int uv_main(int argc, char *argv[]) {
    string text = fs_readfile(__FILE__);
    printf("\n%s\n", text);

    return 0;
}

#include "../include/coroutine.h"

int co_main(int argc, char *argv[]) {
    string text = fs_readfile(__FILE__);
    printf("%s\n", text);

    return 0;
}

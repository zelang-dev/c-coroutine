#include "../include/coroutine.h"

int co_main(int argc, char *argv[])
{

    co_fs_open(__FILE__, O_RDONLY, 0);
    return 0;
}

/* *****hello world***** */

#include "../include/coroutine.h"

int co_main(int argc, char *argv[]) {
    uv_file fd = -1;
    int status = co_fs_close(fd);
    printf("\nFile closed, status: %d\n", status);
    fd = co_fs_open(__FILE__, O_RDONLY, 0);
    printf("\nFile opened, fd: %d\n", fd);
    return 0;
}

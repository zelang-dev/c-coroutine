#include "../include/coroutine.h"

int co_main(int argc, char *argv[]) {
    uv_file fd = -1;

    printf("\nFile closed, status: %d\n", co_fs_close(fd));
    fd = co_fs_open(__FILE__, O_RDONLY, 0);
    printf("\nFile opened, fd: %d\n", fd);
    printf("\nFile read: %s\n", co_fs_read(fd, 393));
    printf("\nFile really closed, status: %d\n", co_fs_close(fd));
    return 0;
}

/******hello  world******/

/******hello  world******/

#include "../include/coroutine.h"

int co_main(int argc, char *argv[]) {
    uv_file fd = -1;
    uv_stat_t *stat;

    int status = co_fs_close(fd);
    printf("\nFile closed, status: %d\n", status);
    fd = co_fs_open(__FILE__, O_RDONLY, 0);
    printf("\nFile opened, fd: %d\n", fd);
    stat = co_fs_fstat(fd);
    printf("\nFile size: %lu\n", stat->st_size);
    status = co_fs_close(fd);
    printf("\nFile really closed, status: %d\n", status);
    return 0;
}

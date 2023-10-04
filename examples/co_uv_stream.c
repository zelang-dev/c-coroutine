#include "../include/coroutine.h"

int co_main(int argc, char *argv[]) {
    uv_file fd = 1;
    uv_tty_t *tty = tty_create(fd);

    printf("Created tty with fd: %d\n", fd);
    int status = stream_write((uv_stream_t *)tty, "hello world\n");
    printf("Wrote to stream, status: %d\n", status);

    return 0;
}

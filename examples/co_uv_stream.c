#include "../include/coroutine.h"

int co_main(int argc, char *argv[]) {
    uv_file fd = 1;
    uv_tty_t *tty = tty_create(fd);

    printf("Created tty with fd: %d\n", fd);
    int status = stream_write(stream(tty), "hello world\n");
    printf("Wrote to stream - tty, status: %d\n", status);

    uv_pipe_t *pipe = pipe_create(false);
    status = uv_pipe_open(pipe, STDERR_FILENO);
    printf("Created and opened pipe, status: %d\n", status);
    status = stream_write(stream(pipe), "ABCDEF\n");
    printf("Wrote to stream - pipe, status: %d\n", status);

    uv_file fdd[2];
    uv_pipe_t *read1 = pipe_create(false);
    uv_pipe_t *read2 = pipe_create(false);

    printf("Create uv_pipe pair, status: %d\n", uv_pipe(fdd, UV_NONBLOCK_PIPE, UV_NONBLOCK_PIPE));

    printf("Opened uv_pipe pair 1 - 1 for writing, status: %d\n", uv_pipe_open(read1, fdd[1]));
    printf("Opened uv_pipe pair 2 - 0 for reading, status: %d\n", uv_pipe_open(read2, fdd[0]));

    printf("Wrote to pair pipe stream, status: %d\n", stream_write(stream(read1), "ABC"));
    printf("Read from stream - pipe: %s\n", stream_read(stream(read2)));
    return 0;
}

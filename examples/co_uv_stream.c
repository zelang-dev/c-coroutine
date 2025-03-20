#include "uv_coro.h"

int uv_main(int argc, char **argv) {
    uv_file fd = 1;
    uv_tty_t *tty = tty_create(fd);

    printf("Created tty with fd: %d\n", fd);
    sleepfor(1);
    int status = stream_write(streamer(tty), "hello world\n");
    printf("Wrote to stream - tty, status: %d\n", status);

#ifndef _WIN32
    uv_pipe_t *pipe = pipe_create(false);
    status = uv_pipe_open(pipe, STDOUT_FILENO);
    printf("\nCreated and opened pipe, status: %d\n", status);
    status = stream_write(streamer(pipe), "ABCDEF\n");
    printf("\nWrote to stream - pipe, status: %d\n", status);
#endif

    uv_file fdd[2];
    uv_pipe_t *read1 = pipe_create(false);
    uv_pipe_t *read2 = pipe_create(false);

    printf("Create uv_pipe pair, status: %d\n", uv_pipe(fdd, UV_NONBLOCK_PIPE, UV_NONBLOCK_PIPE));

    printf("Opened uv_pipe pair 1 - 1 for writing, status: %d\n", uv_pipe_open(read1, fdd[1]));
    printf("Opened uv_pipe pair 2 - 0 for reading, status: %d\n", uv_pipe_open(read2, fdd[0]));

    printf("\nWrote to pair pipe stream, status: %d\n", stream_write(streamer(read1), "ABC"));
    printf("\nRead from stream - pipe: %s\n", stream_read(streamer(read2)));
    return 0;
}

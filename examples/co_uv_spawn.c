#include "uv_coro.h"

void _on_process_exit(int64_t exit_status, int term_signal) {
    fprintf(stderr, "\nProcess exited with status %" PRId64 ", signal %d\n", exit_status, term_signal);
}

int uv_main(int argc, char **argv) {
    coro_stacksize_set(Kb(64));
    spawn_t *child = spawn("./child", (argc > 0 && !is_empty(argv[1]) ? argv[1] : "test-dir"),
                          spawn_opts(NULL, NULL, 0, 0, 0, 3,
                                     stdio_fd(0, UV_IGNORE),
                                     stdio_fd(1, UV_INHERIT_FD),
                                     stdio_fd(2, UV_INHERIT_FD)));

    if (!spawn_exit(child, _on_process_exit))
        fprintf(stderr, "\nLaunched process with ID %d\n", spawn_pid(child));

    return 0;
}

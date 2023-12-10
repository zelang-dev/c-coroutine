#include "../../../include/coroutine.h"

void on_exit(int64_t exit_status, int term_signal) {
    fprintf(stderr, "Process exited with status %" PRId64 ", signal %d\n", exit_status, term_signal);
}

int co_main(int argc, char **argv) {
    spawn_t *child = spawn("child", (argc > 0 && !is_empty(argv[1]) ? argv[1] : "test-dir"),
                          spawn_opts(NULL, NULL, 0, 0, 0, 3,
                                     stdio_fd(0, UV_IGNORE),
                                     stdio_fd(1, UV_INHERIT_FD),
                                     stdio_fd(2, UV_INHERIT_FD)));

    if (!spawn_exit(child, on_exit))
        fprintf(stderr, "\t\t\tLaunched process with ID %d\n", child->process->pid);

    return 0;
}

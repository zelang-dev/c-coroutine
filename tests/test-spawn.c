#include "assertions.h"

int _on_exit(int64_t exit_status, int term_signal) {
    ASSERT_EQ(0, exit_status);
    ASSERT_EQ(0, term_signal);
}

int _on_output(string_t buf) {
    ASSERT_TRUE(is_str_in(buf, "This is stdout"));
    ASSERT_TRUE(is_str_in(buf, "Sleeping..."));
    ASSERT_TRUE(is_str_in(buf, "`test-dir`"));
    ASSERT_FALSE(is_str_in(buf, "Exiting"));
}

TEST(spawn) {
    spawn_t child = spawn("./child", "test-dir",
                          spawn_opts(nullptr, nullptr, 0, 0, 0, 3,
                                     stdio_piperead(), stdio_pipewrite(), stdio_fd(2, UV_INHERIT_FD))
    );

    ASSERT_TRUE(is_process(child));
    ASSERT_EQ(0, spawn_atexit(child, (spawn_cb)_on_exit));
    ASSERT_EQ(0, spawn_out(child, (stdout_cb)_on_output));
    ASSERT_TRUE(spawn_pid(child) > 0);
    ASSERT_TRUE(is_spawning(child));

    while (is_spawning(child))
        yield();

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(spawn);

    return result;
}

int uv_main(int argc, char **argv) {
    TEST_FUNC(list());
}
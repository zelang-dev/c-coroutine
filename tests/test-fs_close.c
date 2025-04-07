#include "assertions.h"

void_t worker(params_t args) {
    ASSERT_WORKER(is_str_eq("hello world", args->char_ptr));
    sleepfor(1000);
    return "done";
}

TEST(fs_close) {
    rid_t res = go(worker, 1, "hello world");
    ASSERT_TRUE((res > coro_id()));
    ASSERT_FALSE(result_is_ready(res));
    uv_file fd = fs_open(__FILE__, O_RDONLY, 0);
    ASSERT_TRUE((fd > 0));
    ASSERT_EQ(0, fs_close(fd));
    ASSERT_FALSE(result_is_ready(res));
    while (!result_is_ready(res))
        yielding();

    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "done");
    ASSERT_EQ(INVALID_FD, fs_close(fd));

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(fs_close);

    return result;
}

int uv_main(int argc, char **argv) {
    TEST_FUNC(list());
}

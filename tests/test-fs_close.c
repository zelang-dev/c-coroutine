#include "assertions.h"

void *worker(params_t args) {
    ASSERT_WORKER(is_str_eq("hello world", args->char_ptr));
    return "done";
}

TEST(fs_close) {
    rid_t res = go(worker, 1, "hello world");
    ASSERT_TRUE((res > coro_id()));
    uv_file fd = fs_open(__FILE__, O_RDONLY, 0);
    ASSERT_TRUE((fd > 0));
    int close_status = fs_close(fd);
    ASSERT_EQ(0, close_status);
    ASSERT_STR(result_for(res).char_ptr, "done");
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

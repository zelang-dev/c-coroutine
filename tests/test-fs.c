#include "assertions.h"

char buf[] = "blablabla\n";
char *path = "write.tmp";

/******hello  world******/

void *worker(params_t args) {
    ASSERT_WORKER(is_str_eq("hello world", args->char_ptr));
    sleepfor(1);
    return "done";
}

void *worker2(params_t args) {
    sleepfor(1);
    return "hello world";
}

TEST(fs_close) {
    rid_t res = go(worker, 1, "hello world");
    ASSERT_TRUE((res > coro_id()));
    ASSERT_FALSE(result_is_ready(res));
    uv_file fd = fs_open(__FILE__, O_RDONLY, 0);
    ASSERT_TRUE((fd > 0));
    int close_status = fs_close(fd);
    ASSERT_EQ(0, close_status);
    while (!result_is_ready(res))
        yielding();

    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "done");
    ASSERT_EQ(-9, fs_close(-1));

    return 0;
}

TEST(fs_open) {
    ASSERT_EQ(fs_open("does_not_exist", O_RDONLY, 0), UV_ENOENT);
    printf("\nWill indicate memory leak at exit, no `fs_close()`.\n");
    ASSERT_TRUE((fs_open(__FILE__, O_RDONLY, 0) > 0));

    return 0;
}

TEST(fs_write) {
    rid_t res = go(worker2, 0);
    uv_file fd = fs_open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    ASSERT_FALSE(result_is_ready(res));
    ASSERT_TRUE((fd > 0));
    int status = fs_write(fd, buf, 0);
    ASSERT_EQ(10, status);
    ASSERT_EQ(0, fs_close(fd));
    ASSERT_EQ(0, fs_unlink(path));
    ASSERT_STR(result_for(res).char_ptr, "hello world");

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(fs_close);
    //EXEC_TEST(fs_open);
    //EXEC_TEST(fs_write);

    return result;
}

int uv_main(int argc, char **argv) {
    TEST_FUNC(list());
}

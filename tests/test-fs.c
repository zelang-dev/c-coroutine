#include "assertions.h"

char buf[] = "blablabla";
char *path = "write.tmp";

/******hello  world******/

void_t worker(params_t args) {
    ASSERT_WORKER(is_str_eq("hello world", args->char_ptr));
    sleepfor(1000);
    return "done";
}

void_t worker2(params_t args) {
    sleepfor(250);
    return "hello world";
}

void_t worker3(params_t args) {
    sleepfor(args->u_int);
    return "finish";
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

TEST(fs_open) {
    ASSERT_EQ(fs_open("does_not_exist", O_RDONLY, 0), UV_ENOENT);
    printf("\nWill indicate memory leak at exit, no `fs_close()`.\n");
    ASSERT_TRUE((fs_open(__FILE__, O_RDONLY, 0) > 0));

    return 0;
}

TEST(fs_write) {
    rid_t res = go(worker2, 0);
    uv_file fd = fs_open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    ASSERT_FALSE(result_is_ready(res));
    ASSERT_TRUE((fd > 0));
    int status = fs_write(fd, buf, 0);
    ASSERT_EQ(9, status);
    ASSERT_EQ(0, fs_close(fd));
    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "hello world");

    return 0;
}

TEST(fs_read) {
    rid_t res = go(worker3, 1, 250);
    uv_file fd = fs_open(path, O_RDONLY, 0);
    ASSERT_FALSE(result_is_ready(res));
    ASSERT_TRUE((fd > 0));
    string text = fs_read(fd, 6);
    ASSERT_STR("bla", text);
    ASSERT_EQ(0, fs_close(fd));
    ASSERT_EQ(0, fs_unlink(path));
    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "finish");

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(fs_write);
    EXEC_TEST(fs_read);
    EXEC_TEST(fs_open);
    EXEC_TEST(fs_close);

    return result;
}

int uv_main(int argc, char **argv) {
    TEST_FUNC(list());
}

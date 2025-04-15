#include "assertions.h"

/******hello  world******/

char buf[] = "blablabla";
char *path = "write.tmp";

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
    while (!result_is_ready(res)) {
        yielding();
    }

    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "done");
    ASSERT_EQ(INVALID_FD, fs_close(fd));

    return 0;
}

void_t worker2(params_t args) {
    sleepfor(1);
    return "hello world";
}

TEST(fs_write_read) {
    rid_t res = go(worker2, 0);
    uv_file fd = fs_open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    ASSERT_FALSE(result_is_ready(res));
    ASSERT_TRUE((fd > 0));
    ASSERT_EQ(9, fs_write(fd, buf, 0));
    ASSERT_STR("bla", fs_read(fd, 6));
    ASSERT_EQ(0, fs_close(fd));
    ASSERT_EQ(0, fs_unlink(path));
    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "hello world");

    return 0;
}

void_t worker3(params_t args) {
    ASSERT_WORKER(($size(args) == 2));
    sleepfor(args[0].u_int);
    ASSERT_WORKER((args[0].u_int == 1));
    ASSERT_WORKER(is_str_eq("worker", args[1].char_ptr));
    return "finish";
}

TEST(fs_open) {
    rid_t res = go(worker3, 2, 1, "worker");
    ASSERT_EQ(fs_open("does_not_exist", O_RDONLY, 0), UV_ENOENT);
    printf("\nWill indicate memory leak at exit, no `fs_close()`.\n");
    uv_file fd = fs_open(__FILE__, O_RDONLY, 0);
    ASSERT_TRUE((fd > 0));
    ASSERT_STR("/******hello  world******/", str_trim(fs_read(fd, 27), 26));
    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "finish");

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(fs_close);
    EXEC_TEST(fs_write_read);
    EXEC_TEST(fs_open);

    return result;
}

int uv_main(int argc, char **argv) {
    TEST_FUNC(list());
}

#include "coroutine.h"
#include "assertions.h"

char buf[] = "blablabla";
char *path = "write.tmp";

/******hello  world******/

void_t worker(args_t args) {
    ASSERT_TRUE(is_str_eq("hello world", args->char_ptr));
    sleep_for(1000);
    return "done";
}

void_t worker2(args_t args) {
    sleep_for(250);
    return "hello world";
}

TEST(fs_close) {
    u32 res = go_for(worker, 1, "hello world");
    uv_file fd = fs_open(__FILE__, O_RDONLY, 0);
    ASSERT_TRUE((fd > 0));
    ASSERT_EQ(0, fs_close(fd));
        co_yield();

    return 0;
}

TEST(fs_open) {
    ASSERT_EQ(fs_open("does_not_exist", O_RDONLY, 0), UV_ENOENT);
    printf("\nWill indicate memory leak at exit, no `fs_close()`.\n");
    ASSERT_TRUE((fs_open(__FILE__, O_RDONLY, 0) > 0));

    return 0;
}

TEST(fs_write) {
    go_for(worker2, 0);
    uv_file fd = fs_open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    ASSERT_TRUE((fd > 0));
    int status = fs_write(fd, buf, 0);
    ASSERT_EQ(9, status);
    ASSERT_EQ(0, fs_close(fd));
    ASSERT_EQ(-9, fs_close(fd));

    return 0;
}

void_t worker3(args_t args) {
    sleep_for(args->u_int);
    return "finish";
}

TEST(fs_read) {
    go_for(worker3, 1, 500);
    uv_file fd = fs_open(path, O_RDONLY, 0);
    ASSERT_TRUE((fd > 0));
    string text = fs_read(fd, 6);
    ASSERT_STR("bla", text);
    ASSERT_EQ(0, fs_close(fd));
    //ASSERT_EQ(0, fs_unlink(path));

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(fs_read);
    EXEC_TEST(fs_write);
    EXEC_TEST(fs_open);
    EXEC_TEST(fs_close);

    return result;
}

int co_main(int argc, char **argv) {
    TEST_FUNC(list());
}
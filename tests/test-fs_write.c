#include "assertions.h"

char buf[] = "blablabla";
char *path = "write.tmp";

void_t worker2(params_t args) {
    sleepfor(250);
    return "hello world";
}

TEST(fs_write) {
    rid_t res = go(worker2, 0);
    uv_file fd = fs_open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    ASSERT_FALSE(result_is_ready(res));
    ASSERT_TRUE((fd > 0));
    ASSERT_EQ(9, fs_write(fd, buf, 0));
    ASSERT_EQ(0, fs_close(fd));
    ASSERT_EQ(0, fs_unlink(path));
    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "hello world");

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(fs_write);

    return result;
}

int uv_main(int argc, char **argv) {
    TEST_FUNC(list());
}

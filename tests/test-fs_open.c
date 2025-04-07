#include "assertions.h"

char buf[] = "blablabla";
char *path = "write.tmp";

void_t worker2(params_t args) {
    sleepfor(250);
    return "hello world";
}

TEST(fs_open) {
    rid_t res = go(worker2, 0);
    ASSERT_EQ(fs_open("does_not_exist", O_RDONLY, 0), UV_ENOENT);
    ASSERT_FALSE(result_is_ready(res));
    printf("\nWill indicate memory leak at exit, no `fs_close()`.\n");
    ASSERT_TRUE((fs_open(__FILE__, O_RDONLY, 0) > 0));
    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "hello world");

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(fs_open);

    return result;
}

int uv_main(int argc, char **argv) {
    TEST_FUNC(list());
}

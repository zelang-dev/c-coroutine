#include "assertions.h"

/******hello  world******/

void_t worker3(params_t args) {
    sleepfor(args->u_int);
    return "finish";
}

TEST(fs_read) {
    rid_t res = go(worker3, 1, 250);
    uv_file fd = fs_open(__FILE__, O_RDONLY, 0);
    ASSERT_FALSE(result_is_ready(res));
    ASSERT_TRUE((fd > 0));
    string text = fs_read(fd, 27);
    ASSERT_STR("/******hello  world******/", str_trim(text, 26));
    ASSERT_EQ(0, fs_close(fd));
    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "finish");

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(fs_read);

    return result;
}

int uv_main(int argc, char **argv) {
    TEST_FUNC(list());
}

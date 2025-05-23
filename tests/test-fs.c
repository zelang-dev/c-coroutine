#include "assertions.h"

/******hello  world******/

string buf = "blablabla";
string path = "write.temp";
string dir_path = "tmp_dir";
string ren_path = "tmp_temp";
string scan_path = "scandir";
string_t watch_path = "watchdir";

void_t worker(params_t args) {
    ASSERT_WORKER(is_str_eq("hello world", args->char_ptr));
    sleepfor(500);
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
    ASSERT_WORKER(($size(args) == 0));
    sleepfor(300);
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
    while (!result_is_ready(res)) {
        yielding();
    }

    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "hello world");

    return 0;
}

void_t worker3(params_t args) {
    ASSERT_WORKER(($size(args) == 2));
    sleepfor(args[0].u_int);
    ASSERT_WORKER((args[0].u_int == 25));
    ASSERT_WORKER(is_str_eq("worker", args[1].char_ptr));
    return "finish";
}

TEST(fs_open) {
    rid_t res = go(worker3, 2, 25, "worker");
    ASSERT_EQ(fs_open("does_not_exist", O_RDONLY, 0), UV_ENOENT);
    printf("\nWill indicate memory leak at exit, no `fs_close()`.\n");
    uv_file fd = fs_open(__FILE__, O_RDONLY, 0);
    ASSERT_TRUE((fd > 0));
    ASSERT_STR("/******hello  world******/", str_trim(fs_read(fd, 27), 26));
    while (!result_is_ready(res)) {
        yielding();
    }

    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "finish");

    return 0;
}

void_t worker_misc(params_t args) {
    ASSERT_WORKER(($size(args) > 1));
    sleepfor(args[0].u_int);
    ASSERT_WORKER(is_str_in("mkdir,rmdir,rename,writefile,scandir,unlink,event", args[1].char_ptr));
    return args[1].char_ptr;
}

TEST(fs_mkdir) {
    rid_t res = go(worker_misc, 2, 250, "mkdir");
    ASSERT_EQ(0, fs_mkdir(dir_path, 0));
    ASSERT_FALSE(result_is_ready(res));
    while (!result_is_ready(res))
        yielding();

    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "mkdir");
    ASSERT_EQ(UV_EEXIST, fs_mkdir(dir_path, 0));
    return 0;

}

TEST(fs_rename) {
    rid_t res = go(worker_misc, 2, 200, "rename");
    ASSERT_EQ(0, fs_rename(dir_path, ren_path));
    ASSERT_FALSE(result_is_ready(res));
    ASSERT_EQ(0, fs_rmdir(ren_path));
    while (!result_is_ready(res))
        yielding();

    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "rename");

    return 0;
}

TEST(fs_scandir) {
    char filepath[SCRAPE_SIZE] = nil;
    scandir_t *dir_files = nil;
    int i = 0;
    rid_t res = go(worker_misc, 2, 5000, "scandir");
    ASSERT_EQ(0, fs_mkdir(scan_path, 0));
    ASSERT_FALSE(result_is_ready(res));

    for (i = 1; i < 6; i++) {
        snprintf(filepath, SCRAPE_SIZE, "%s/file%d.txt", scan_path, i);
        ASSERT_EQ(1, fs_writefile(filepath, " "));
    }

    ASSERT_FALSE(result_is_ready(res));
    ASSERT_NOTNULL((dir_files = fs_scandir(scan_path, 0)));
    ASSERT_FALSE(result_is_ready(res));
    ASSERT_XEQ(5, dir_files->count);

    i = 1;
    foreach_scandir(file in dir_files) {
        snprintf(filepath, SCRAPE_SIZE, "file%d.txt", i);
        ASSERT_TRUE(is_str_eq(filepath, file->name));
        snprintf(filepath, SCRAPE_SIZE, "%s/%s", scan_path, file->name);
        printf("deleting: %s\n", filepath);
        ASSERT_EQ(0, fs_unlink(filepath));
        i++;
    }

    ASSERT_EQ(0, fs_rmdir(scan_path));
    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "scandir");

    return 0;
}

void watch_handler(string_t filename, int events, int status) {
    RAII_HERE;
    if (events & UV_RENAME)
        printf("renamed -%s\n", filename);

    if (events & UV_CHANGE)
        printf("changed - %s\n", filename);
        //ASSERT_WORKER(is_str_in("mkdir,rmdir,rename,writefile,scandir,unlink,event", args[1].char_ptr));
}

TEST(fs_watch) {
    char filepath[SCRAPE_SIZE] = nil;
    scandir_t *dir_files = nil;
    int i = 0;
    rid_t res = go(worker_misc, 2, 500, "event");
    ASSERT_EQ(0, fs_mkdir(watch_path, 0));
    ASSERT_FALSE(result_is_ready(res));

    fs_watch(watch_path, watch_handler);
    ASSERT_FALSE(result_is_ready(res));

    for (i = 1; i < 10; i++) {
        snprintf(filepath, SCRAPE_SIZE, "%s/file%d.txt", watch_path, 1);
        ASSERT_EQ(0, fs_writefile(filepath, ""));
        sleepfor(5);
        ASSERT_EQ(0, fs_unlink(filepath));
    }
    yielding();

    ASSERT_EQ(0, fs_rmdir(watch_path));
    ASSERT_TRUE(result_is_ready(res));
    ASSERT_STR(result_for(res).char_ptr, "event");

    return 0;
}

TEST(list) {
    int result = 0;

    EXEC_TEST(fs_close);
    EXEC_TEST(fs_write_read);
    EXEC_TEST(fs_open);
    EXEC_TEST(fs_mkdir);
    EXEC_TEST(fs_rename);
    EXEC_TEST(fs_scandir);
    EXEC_TEST(fs_watch);

    return result;
}

int uv_main(int argc, char **argv) {
    TEST_FUNC(list());
}
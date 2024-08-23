#include "coroutine.h"

void test(void_t args) {
    url_t *url = parse_url("http://secret:hideout@zelang.dev:80/this/is/a/very/deep/directory/structure/and/file.html?lots=1&of=2&parameters=3&too=4&here=5#some_page_ref123");
    fileinfo_t *fileinfo = pathinfo(url->path);

    printf("[uv_type] => %d\n", url->uv_type);
    printf("[scheme] => %s\n", url->scheme);
    printf("[host] => %s\n", url->host);
    printf("[user] => %s\n", url->user);
    printf("[pass] => %s\n", url->pass);
    printf("[port] => %d\n", url->port);
    printf("[path] => %s\n", url->path);
    printf("[query] => %s\n", url->query);
    printf("[fragment] => %s\n\n", url->fragment);

    printf("[dirname] => %s\n", fileinfo->dirname);
    printf("[base] => %s\n", fileinfo->base);
    printf("[extension] => %s\n", fileinfo->extension);
    printf("[filename] => %s\n\n", fileinfo->filename);

    if (url->query) {
       int i = 0;
       char **token = co_str_split(url->query, "&", &i);

       for (int x = 0; x < i; x++) {
           char **parts = co_str_split(token[x], "=", NULL);
           printf("%s = %s\n", parts[0], parts[1]);
       }
    }

    printf("New String: %s\n", str_replace(fileinfo->dirname,
                                           "directory",
                                           co_concat_by(3, "testing ", "this ", "thing")));
}

int co_main(int argc, char *argv[]) {
    launch(test, NULL);
    return 0;
}

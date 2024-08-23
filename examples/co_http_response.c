#include "coroutine.h"

int co_main(int argc, char *argv[]) {
    http_t *parser = http_for(HTTP_RESPONSE, NULL, 1.1);
    string response = co_concat_by(15, "HTTP/1.1 200 OK", CRLF,
                                   "Date: ", http_std_date(0), CRLF,
                                   "Content-Type: text/html; charset=utf-8", CRLF,
                                   "Content-Length: 11", CRLF,
                                   "Server: uv_server", CRLF,
                                   "X-Powered-By: ZeLang", CRLF, CRLF,
                                   "hello world");


    if (is_str_eq(response, http_response(parser, "hello world", STATUS_OK, NULL, "x-powered-by=ZeLang;")))
        printf("\nThey match!\n\n%s\n", response);

    return 0;
}

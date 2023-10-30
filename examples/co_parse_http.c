#include "../include/coroutine.h"

int co_main(int argc, char *argv[]) {
    http_t *parser = http_for(HTTP_BOTH, NULL, 1.1);

    char raw[] = "HTTP/1.1 200 OK\n\
Date: Tue, 12 Apr 2016 13:58:01 GMT\n\
Server: Apache/2.2.14 (Ubuntu)\n\
X-Powered-By: PHP/5.3.14 ZendServer/5.0\n\
Set-Cookie: ZDEDebuggerPresent=php,phtml,php3; path=/\n\
Set-Cookie: PHPSESSID=6sf8fa8rlm8c44avk33hhcegt0; path=/; HttpOnly\n\
Expires: Thu, 19 Nov 1981 08:52:00 GMT\n\
Cache-Control: no-store, no-cache, must-revalidate, post-check=0, pre-check=0\n\
Pragma: no-cache\n\
Vary: Accept-Encoding\n\
Content-Encoding: gzip\n\
Content-Length: 192\n\
Content-Type: text/xml";

    parse_http(parser, raw);

    CO_ASSERT(is_str_eq("Tue, 12 Apr 2016 13:58:01 GMT", get_header(parser, "Date", "")));
    CO_ASSERT(is_str_eq("Apache/2.2.14 (Ubuntu)", get_header(parser, "Server", "")));
    CO_ASSERT(is_str_eq("PHP/5.3.14 ZendServer/5.0", get_header(parser, "X-Powered-By", "")));
    CO_ASSERT(is_str_eq("PHPSESSID=6sf8fa8rlm8c44avk33hhcegt0; path=/; HttpOnly", get_header(parser, "Set-Cookie", "")));
    CO_ASSERT(is_str_eq("Thu, 19 Nov 1981 08:52:00 GMT", get_header(parser, "Expires", "")));
    CO_ASSERT(is_str_eq("no-store, no-cache, must-revalidate, post-check=0, pre-check=0", get_header(parser, "Cache-Control", "")));
    CO_ASSERT(is_str_eq("no-cache", get_header(parser, "Pragma", "")));
    CO_ASSERT(is_str_eq("Accept-Encoding", get_header(parser, "Vary", "")));
    CO_ASSERT(is_str_eq("gzip", get_header(parser, "Content-Encoding", "")));
    CO_ASSERT(is_str_eq("192", get_header(parser, "Content-Length", "")));
    CO_ASSERT(is_str_eq("text/xml", get_header(parser, "Content-Type", "")));
    CO_ASSERT(is_str_eq("HTTP/1.1", parser->protocol));
    CO_ASSERT(is_str_eq("OK", parser->message));
    CO_ASSERT(is_str_eq("/", get_variable(parser, "Set-Cookie", "path", "")));

    CO_ASSERT(200 == parser->code);

    CO_ASSERT(has_header(parser, "Pragma"));
    CO_ASSERT(has_flag(parser, "Cache-Control", "no-store"));
    CO_ASSERT(has_variable(parser, "Set-Cookie", "PHPSESSID"));
}
